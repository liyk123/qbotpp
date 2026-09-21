#include "qbot_Instance.h"
#include "qbot_tools.h"
#include <drogon/drogon.h>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <spdlog/spdlog.h>

#define QBOT_TAG "\033[36mQBot\033[0m "

using namespace std::literals;
namespace DispatchType = qbot::DispatchType;
using tools::JsonMethod;
using tools::HttpMethodType;
using DispatchMap = std::unordered_map<std::string_view, std::vector<qbot::DispatchAction>>;

constexpr auto QBotUniversalUrl = "https://api.bot.qq.com";
constexpr auto QBotSandboxUrl = "https://sandbox.api.sgroup.qq.com";
constexpr auto GROUP_AND_C2C_EVENT = 1U << 25;
constexpr auto GROUP_MEMBER_EVENT = 1U << 24;
constexpr auto INTERACTION = 1U << 26;
constexpr auto SCENE_TAG_LIST = std::array{ "users"sv,"groups"sv };

enum class opcode : std::int32_t
{
    // 服务端进行消息推送
    Dispatch = 0,
    // 客户端或服务端发送心跳
    Heartbeat = 1,
    // 客户端发送鉴权
    Identify = 2,
    // 客户端恢复连接
    Resume = 6,
    // 服务端通知客户端重新连接
    Reconnect = 7,
    // 当identify或resume的时候，如果参数有错，服务端会返回该消息
    Invalid = 9,
    // 当客户端与网关建立ws连接之后，网关下发的第一条消息
    Hello = 10,
    // 当发送心跳成功之后，就会收到该消息
    HeartbeatACK = 11,
    // 仅用于 http 回调模式的回包，代表机器人收到了平台推送的数据
    HTTPCallbackACK = 12,
    // 开放平台对机器人服务端进行验证
    CallbackAuth = 13
};

struct qbot::Instance::Impl
{
    std::shared_mutex tokenMutex{};
    std::string accessToken{};
    drogon::HttpClientPtr apiClient{};
    drogon::WebSocketClientPtr wsClient{};
    bool sandbox{};
    std::string appId{};
    std::string clientSecret{};
    std::string sessionId{};
    std::atomic_flag needResume{};
    std::atomic_llong seq{ 0 };
    DispatchMap dispatchMap{
        {DispatchType::C2CMessageCreate,{}},
        {DispatchType::C2CMsgReceived,{}},
        {DispatchType::C2CMsgReject,{}},
        {DispatchType::FriendAdd,{}},
        {DispatchType::FriendDel,{}},
        {DispatchType::GroupAddRobot,{}},
        {DispatchType::GroupAtMessageCreate,{}},
        {DispatchType::GroupDelRobot,{}},
        {DispatchType::GroupJoinRequest,{}},
        {DispatchType::GroupMemberAdd,{}},
        {DispatchType::GroupMemberRemove,{}},
        {DispatchType::GroupMessageCreate,{}},
        {DispatchType::GroupMsgReceive,{}},
        {DispatchType::GroupMsgReject,{}},
        {DispatchType::InteractionCreate,{}},
    };
    std::string getAccessToken();
    void setAccessToken(const std::string token);
    void initAndStart(const Json::Value& config);
};

static qbot::Instance* getInstance()
{
    return drogon::app().getPlugin<qbot::Instance>();
}

static std::unique_ptr<qbot::Instance::Impl>& getInstanceImpl()
{
    return drogon::app().getPlugin<qbot::Instance>()->m_pImpl;
}

static drogon::Task<std::pair<std::string, std::uint64_t>> getAccessTokenAsync(const std::string appId, const std::string clientSecret)
{
    static auto client = drogon::HttpClient::newHttpClient(QBotUniversalUrl);
    auto req = drogon::HttpRequest::newCustomHttpRequest(nlohmann::json{
        {"appId", appId},
        {"clientSecret", clientSecret}
    });
    req->setPath("/app/getAppAccessToken");
    auto resp = co_await client->sendRequestCoro(req);
    SPDLOG_INFO(QBOT_TAG "{} {} {}", req->methodString(), req->path(), resp->body());
    auto data = resp->as<nlohmann::json>();
    co_return{ data.value("access_token", ""),std::stoull(data.value("expires_in","30")) };
}

static drogon::Task<> getAccessTokenAsyncEveryExpiredTime(const std::string appId, const std::string clientSecret)
{
    auto [token, expiredTime] = co_await getAccessTokenAsync(appId, clientSecret);
    if (token != getInstanceImpl()->getAccessToken()) [[unlikely]]
    {
        co_await drogon::switchThreadCoro(drogon::app().getLoop());
        getInstanceImpl()->setAccessToken(token);
    }
    auto time = std::chrono::seconds{ expiredTime > 30 ? expiredTime - 30 : expiredTime };
    drogon::app().getLoop()->runAfter(time, drogon::async_func([appId, clientSecret]()  {
        return getAccessTokenAsyncEveryExpiredTime(appId, clientSecret);
    }));
}

static drogon::Task<std::string> getGatewayAsync(const std::string token)
{
    auto&& client = getInstanceImpl()->apiClient;
    auto req = drogon::HttpRequest::newHttpJsonRequest({});
    req->setPath("/gateway");
    req->addHeader("Authorization", "QQBot " + token);
    auto resp = co_await client->sendRequestCoro(req);
    SPDLOG_INFO(QBOT_TAG "{} {} {}", req->methodString(), req->path(), resp->body());
    co_return resp->as<nlohmann::json>().value("url", "");
}

static void SendHeartbeat(const drogon::WebSocketConnectionPtr& connection)
{
    auto payload = nlohmann::json{
        {"op", opcode::Heartbeat},
        {"d", getInstanceImpl()->seq.load()}
    };
    SPDLOG_INFO(QBOT_TAG "SEND {}", payload.dump());
    connection->send(payload.dump());
}

static nlohmann::json DispatchReady(const nlohmann::json& data)
{
    getInstanceImpl()->sessionId.assign(data["d"]["session_id"].get<std::string>());
    return {
        {"op", opcode::Heartbeat},
        {"d", nullptr}
    };
}

static nlohmann::json DispatchResumed(const nlohmann::json& data)
{
    return {
        {"op", opcode::Heartbeat},
        {"d", getInstanceImpl()->seq.load()}
    };
}

template<drogon::HttpMethod method = drogon::Post>
static drogon::Task<nlohmann::json> CallQBotApiAsync(const std::string path, const nlohmann::json data)
{
    auto&& client = getInstanceImpl()->apiClient;
    auto req = drogon::HttpRequest::newCustomHttpRequest(JsonMethod{ data, HttpMethodType<method>{} });
    req->setPath(path);
    req->addHeader("Authorization", "QQBot " + getInstanceImpl()->getAccessToken());
    drogon::HttpResponsePtr resp = co_await client->sendRequestCoro(req);
    SPDLOG_INFO(QBOT_TAG "{} {} {} {}", req->methodString(), path, req->body(), resp->body());
    co_return resp->as<nlohmann::json>();
}

template<qbot::SceneType scene>
static drogon::Task<nlohmann::json> SendMessageAsync(const nlohmann::json payload, const std::string openId)
{
    auto path = std::format("/v2/{}/{}/messages", SCENE_TAG_LIST[scene], openId);
    return CallQBotApiAsync(std::move(path), std::move(payload));
}

template<qbot::SceneType scene>
static drogon::Task<nlohmann::json> UploadFileAsync(const nlohmann::json payload, const std::string openId)
{
    auto path = std::format("/v2/{}/{}/files", SCENE_TAG_LIST[scene], openId);
    return CallQBotApiAsync(std::move(path), std::move(payload));
}

template<qbot::SceneType scene>
static drogon::Task<nlohmann::json> UploadPartPrepareAysnc(const nlohmann::json payload, const std::string openId)
{
    auto path = std::format("/v2/{}/{}/upload_prepare", SCENE_TAG_LIST[scene], openId);
    return CallQBotApiAsync(std::move(path), std::move(payload));
}

template<qbot::SceneType scene>
static drogon::Task<nlohmann::json> UploadPartFinishAysnc(const nlohmann::json payload, const std::string openId)
{
    auto path = std::format("/v2/{}/{}/upload_part_finish", SCENE_TAG_LIST[scene], openId);
    return CallQBotApiAsync(std::move(path), std::move(payload));
}

template<qbot::SceneType scene>
static drogon::Task<nlohmann::json> DeleteMessageAsync(const nlohmann::json payload, const std::string openId)
{
    auto path = std::format("/v2/{}/{}/messages/{}", SCENE_TAG_LIST[scene], openId, payload.get<std::string_view>());
    return CallQBotApiAsync<drogon::Delete>(std::move(path), std::move(payload));
}

static drogon::Task<nlohmann::json> GetJoinRequestList(const nlohmann::json payload, const std::string openId)
{
    auto path = std::format("/v2/groups/{}/join_request_list", openId);
    return CallQBotApiAsync<drogon::Get>(std::move(path), std::move(payload));
}

static drogon::Task<nlohmann::json> ApprovalJoinRequest(const nlohmann::json& payload, const std::string& groupId, const std::string& userId)
{
    auto path = std::format("/v2/groups/{}/approval_join_request/{}", groupId, userId);
    return CallQBotApiAsync(std::move(path), std::move(payload));
}

template<drogon::HttpMethod method> requires (method == drogon::Get || method == drogon::Post)
static drogon::Task<nlohmann::json> RestrictChatSetting(const nlohmann::json payload, const std::string openId)
{
    auto path = std::format("/v2/groups/{}/restrict_chat_setting", openId);
    return CallQBotApiAsync<method>(std::move(path), std::move(payload));
}

template<drogon::HttpMethod method> requires (method == drogon::Get || method == drogon::Post)
static drogon::Task<nlohmann::json> JoinApprovalStrategy(const nlohmann::json payload)
{
    auto path = "/v2/groups/join_approval_strategy";
    return CallQBotApiAsync<method>(std::move(path), std::move(payload));
}

template<drogon::HttpMethod method> requires (method == drogon::Patch || method == drogon::Delete)
static drogon::Task<nlohmann::json> JoinApprovalStrategy(const nlohmann::json payload, const std::string strategyId)
{
    auto path = std::format("/v2/groups/join_approval_strategy/{}", strategyId);
    return CallQBotApiAsync<method>(std::move(path), std::move(payload));
}

static drogon::Task<nlohmann::json> GetSelfDetails()
{
    return CallQBotApiAsync<drogon::Get>("/users/@me", {});
}

template<drogon::HttpMethod method> requires (method == drogon::Get || method == drogon::Put)
static drogon::Task<nlohmann::json> Menu(const nlohmann::json payload)
{
    auto path = "/v2/menu";
    return CallQBotApiAsync<method>(std::move(path), std::move(payload));
}

template<drogon::HttpMethod method> requires (method == drogon::Get || method == drogon::Post)
static drogon::Task<nlohmann::json> Panels(const nlohmann::json payload)
{
    auto path = "/v2/panels";
    nlohmann::json ret = co_await CallQBotApiAsync<method>(std::move(path), std::move(payload));
    co_return ret;
}

template<drogon::HttpMethod method> requires (method == drogon::Get || method == drogon::Put || method == drogon::Delete)
static drogon::Task<nlohmann::json> Panels(const nlohmann::json& payload, const std::string& panelId)
{
    auto path = std::format("/v2/panels/{}", panelId);
    return CallQBotApiAsync<method>(std::move(path), std::move(payload));
}

static drogon::Task<nlohmann::json> UpdatePanelsTarget(const nlohmann::json& payload, const std::string& panelId)
{
    auto path = std::format("/v2/panels/{}/target", panelId);
    return CallQBotApiAsync<drogon::Put>(std::move(path), std::move(payload));
}

static void OnDispatchReceived(const nlohmann::json& data, const drogon::WebSocketConnectionPtr& connection)
{
    auto type = data["t"].get<std::string_view>();
    if (type == DispatchType::Ready)
    {
        auto payload = DispatchReady(data);
        SPDLOG_INFO(QBOT_TAG "SEND {}", payload.dump());
        connection->send(payload.dump());
        return;
    }
    if (type == DispatchType::Resumed)
    {
        auto payload = DispatchResumed(data);
        SPDLOG_INFO(QBOT_TAG "SEND {}", payload.dump());
        connection->send(payload.dump());
        return;
    }
    for (auto&& action : getInstanceImpl()->dispatchMap.at(type))
    {
        action(data);
    }
}

static void OnReconnectReceived(const drogon::WebSocketConnectionPtr& connection)
{
    SPDLOG_WARN(QBOT_TAG "Need Resume");
    getInstanceImpl()->needResume.test_and_set();
}

static void SendIdentify(const drogon::WebSocketConnectionPtr& connection)
{
    auto payload = nlohmann::json{
        {"op", opcode::Identify},
        {"d", {
            {"token","QQBot " + getInstanceImpl()->getAccessToken()},
            {"intents",GROUP_AND_C2C_EVENT | GROUP_MEMBER_EVENT | INTERACTION}
        }},
        {"shard", nullptr},
        {"properties", nullptr}
    };
    SPDLOG_INFO(QBOT_TAG "SEND {}", payload.dump());
    connection->send(payload.dump());
}

static void SendResume(const drogon::WebSocketConnectionPtr& connection)
{
    auto payload = nlohmann::json{
        {"op", opcode::Resume},
        {"d", {
            {"token", "QQBot " + getInstanceImpl()->getAccessToken()},
            {"session_id", getInstanceImpl()->sessionId},
            {"seq", getInstanceImpl()->seq.load()}
        }}
    };
    SPDLOG_INFO(QBOT_TAG "SEND {}", payload.dump());
    connection->send(payload.dump());
}


static void OnHelloReceived(const drogon::WebSocketConnectionPtr& connection)
{
    (getInstanceImpl()->needResume.test() ? SendResume : SendIdentify)(connection);
    getInstanceImpl()->needResume.clear();
}

static void OnHearbeatACKReceived(const drogon::WebSocketConnectionPtr& connection)
{
    drogon::app().getLoop()->runAfter(5s, [connection] {
        if (!connection->connected()) [[unlikely]]
        {
            return;
        }
        SendHeartbeat(connection);
    });
}

static void QBotCloseHandler(const drogon::WebSocketConnectionPtr& connection)
{
    SPDLOG_WARN(QBOT_TAG "Close");
    connection->forceClose();
}

static void QBotTextHandler(const std::string& msg, const drogon::WebSocketConnectionPtr& connection)
{
    if (nlohmann::json data = nlohmann::json::parse(msg, nullptr, false); !data.is_discarded())
    {
        if (data["s"].is_number())
        {
            getInstanceImpl()->seq.store(data["s"].get<std::int64_t>());
        }
        switch (opcode(data["op"].get<std::int32_t>()))
        {
            case opcode::Dispatch:
                OnDispatchReceived(data, connection);
                break;
            case opcode::Reconnect:
                OnReconnectReceived(connection);
                break;
            case opcode::Hello:
                OnHelloReceived(connection);
                break;
            case opcode::HeartbeatACK:
                OnHearbeatACKReceived(connection);
                break;
            default:
                break;
        }
    }
}

static void MessageHandler(std::string&& msg, const drogon::WebSocketClientPtr& client, const drogon::WebSocketMessageType& type)
{
    SPDLOG_INFO(QBOT_TAG "RECV {}", msg);
    auto connection = client->getConnection();
    switch (type)
    {
        [[likely]] case drogon::WebSocketMessageType::Text:
            QBotTextHandler(msg, connection);
            break;
        [[unlikely]] case drogon::WebSocketMessageType::Close:
            QBotCloseHandler(connection);
            break;
        default:
            break;
    }
}

static void ClosedHandler(const drogon::WebSocketClientPtr& client)
{
    auto&& gateway = *client->getConnection()->getContext<std::string>();
    SPDLOG_INFO(QBOT_TAG "reconnect to {}", gateway);
    auto newClient = tools::ConnectToWSServer(gateway, MessageHandler, ClosedHandler, [gateway](drogon::ReqResult r, const drogon::HttpResponsePtr& resp, const drogon::WebSocketClientPtr& client) {
        if (r != drogon::ReqResult::Ok)
        {
            SPDLOG_ERROR(QBOT_TAG "{} {} {}", gateway, (int)r, resp->body());
            return;
        }
        SPDLOG_INFO(QBOT_TAG "{} is connected!", gateway);
        client->getConnection()->setContext(std::make_shared<std::string>(gateway));
    });
    drogon::app().getPlugin<qbot::Instance>()->m_pImpl->wsClient = newClient;
}

namespace qbot {

    Instance::Instance() = default;

    Instance::~Instance() = default;

    void Instance::initAndStart(const Json::Value& config)
    {
        SPDLOG_WARN(QBOT_TAG "init");
        m_pImpl = std::make_unique<Impl>();
        m_pImpl->initAndStart(config);
    }

    void Instance::shutdown()
    {
        SPDLOG_WARN(QBOT_TAG "down");
    }

    std::string Instance::getAppId() const
    {
        return m_pImpl->appId;
    }

    std::string Instance::Impl::getAccessToken()
    {
        std::shared_lock lock(tokenMutex);
        return accessToken;
    }

    void Instance::Impl::setAccessToken(const std::string token)
    {
        std::unique_lock lock(tokenMutex);
        accessToken = token;
    }

    void Instance::Impl::initAndStart(const Json::Value& config)
    {
        sandbox = config.get("sandbox", false).asBool();
        appId = config.get("appId", "").asString();
        clientSecret = config.get("clientSecret", "").asString();
        apiClient = drogon::HttpClient::newHttpClient(sandbox ? QBotSandboxUrl : QBotUniversalUrl);
        drogon::app().registerBeginningAdvice(drogon::async_func([this]() -> drogon::Task<> {
            co_await getAccessTokenAsyncEveryExpiredTime(appId, clientSecret);
            auto gateway = co_await getGatewayAsync(accessToken);
            if (gateway.empty())
            {
                SPDLOG_ERROR("Empty Gateway! Please check the error message.");
                drogon::app().quit();
                co_return;
            }
            wsClient = tools::ConnectToWSServer(gateway, MessageHandler, ClosedHandler, [gateway](drogon::ReqResult r, const drogon::HttpResponsePtr& resp, const drogon::WebSocketClientPtr& client) {
                if (r != drogon::ReqResult::Ok)
                {
                    SPDLOG_ERROR("{} {} {}", gateway, (int)r, resp->body());
                    return;
                }
                SPDLOG_INFO("{} is connected!", gateway);
                client->getConnection()->setContext(std::make_shared<std::string>(gateway));
            });
        }));
    }

    template<fixstr::fixed_string type>
    void Instance::registerDispatchAction(DispatchAction&& action)
    {
        LOG_INFO << type << " " << action;
        m_pImpl->dispatchMap[type].emplace_back(std::move(action));
    }

    drogon::Task<nlohmann::json> Instance::sendC2CMessageAsync(const nlohmann::json payload, const std::string openId) const
    {
        return SendMessageAsync<c2c>(std::move(payload), std::move(openId));
    }

    drogon::Task<nlohmann::json> Instance::sendGroupMessageAsync(const nlohmann::json payload, const std::string openId) const
    {
        return SendMessageAsync<group>(std::move(payload), std::move(openId));
    }

    template<qbot::SceneType scene>
    static drogon::Task<nlohmann::json> UploadBufferFileAsync(const std::string buf, const std::string name, const FileType type, const std::string openId)
    {
        auto prePayload = nlohmann::json{
            {"file_type", type},
            {"file_size", buf.size()},
            {"file_name", name},
            {"md5", drogon::utils::getMd5(buf)},
            {"sha1", drogon::utils::getSha1(buf)},
            {"md5_10m", drogon::utils::getMd5(buf.substr(0,FILE_POS_MD5_10M))}
        };
        auto preData = co_await UploadPartPrepareAysnc<scene>(std::move(prePayload), openId);
        std::vector<drogon::Task<void>> tasks;
        std::size_t partPos = 0;
        auto uploadId = preData["upload_id"].get<std::string>();
        for (auto&& part : preData["parts"])
        {
            auto blockSize = part["block_size"].get<std::size_t>();
            auto task = [](const std::string& bufPart, const nlohmann::json& part, const std::string& uploadId, const std::string& openId) -> drogon::Task<> {
                auto url = part["presigned_url"].get<std::string>();
                co_await tools::SendHttpRequestAsync(url, bufPart, HttpMethodType<drogon::Put>{});
                auto partPayload = nlohmann::json{
                    {"upload_id", uploadId},
                    {"part_index", part["part_index"]},
                    {"block_size", part["block_size"]},
                    {"md5", drogon::utils::getMd5(bufPart)}
                };
                co_await UploadPartFinishAysnc<scene>(std::move(partPayload), openId);
            };
            tasks.emplace_back(std::move(task(buf.substr(partPos, blockSize), part, uploadId, openId)));
            partPos += blockSize;
        }
        co_await drogon::when_all(std::move(tasks));
        auto finishPayload = nlohmann::json{
            {"file_type", type},
            {"file_name", name},
            {"upload_id", uploadId}
        };
        nlohmann::json ret = co_await UploadFileAsync<scene>(std::move(finishPayload), openId);
        co_return ret;
    }

    drogon::Task<nlohmann::json> Instance::uploadC2CBufferFileAsync(const std::string buf, const std::string name, const FileType type, const std::string openId)
    {
        return UploadBufferFileAsync<c2c>(std::move(buf), std::move(name), type, std::move(openId));
    }

    drogon::Task<nlohmann::json> Instance::uploadGroupBufferFileAsync(const std::string buf, const std::string name, const FileType type, const std::string openId)
    {
        return UploadBufferFileAsync<group>(std::move(buf), std::move(name), type, std::move(openId));
    }

    template<qbot::SceneType scene>
    static drogon::Task<nlohmann::json> UploadUrlFileAsync(const std::string url, const FileType type, const std::string openId)
    {
        auto payload = nlohmann::json{
            {"file_type", type},
            {"url", url},
        };
        return UploadFileAsync<scene>(std::move(payload), std::move(openId));
    }

    drogon::Task<nlohmann::json> Instance::uploadC2CUrlFileAsync(const std::string url, const FileType type, const std::string openId) const
    {
        return UploadUrlFileAsync<c2c>(std::move(url), type, std::move(openId));
    }

    drogon::Task<nlohmann::json> Instance::uploadGroupUrlFileAsync(const std::string url, const FileType type, const std::string openId) const
    {
        return UploadUrlFileAsync<group>(std::move(url), type, std::move(openId));
    }

    void Instance::export_functions()
    {
        registerDispatchAction<DispatchType::C2CMessageCreate>({});
        registerDispatchAction<DispatchType::GroupMessageCreate>({});
        registerDispatchAction<DispatchType::GroupAtMessageCreate>({});
        registerDispatchAction<DispatchType::GroupAddRobot>({});
        registerDispatchAction<DispatchType::GroupDelRobot>({});
        registerDispatchAction<DispatchType::FriendAdd>({});
        registerDispatchAction<DispatchType::FriendDel>({});
        registerDispatchAction<DispatchType::GroupMemberAdd>({});
        registerDispatchAction<DispatchType::GroupMemberRemove>({});
        registerDispatchAction<DispatchType::GroupJoinRequest>({});
    }
}