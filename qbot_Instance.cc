#include "qbot_Instance.h"
#include <drogon/drogon.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#define QBOT_TAG "\033[36mQBot\033[0m "

using namespace std::literals;
using qbot::DispatchType;
using qbot::JsonMethod;
using qbot::HttpMethodType;

constexpr auto QBotUniversalUrl = "https://api.bot.qq.com";
constexpr auto QBotSandboxUrl = "https://sandbox.api.sgroup.qq.com";
constexpr auto GROUP_AND_C2C_EVENT = 1U << 25 | 1U << 24;

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

static qbot::Instance* getInstance()
{
    return drogon::app().getPlugin<qbot::Instance>();
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
    if (token != getInstance()->getAccessToken()) [[unlikely]]
    {
        co_await drogon::switchThreadCoro(drogon::app().getLoop());
        getInstance()->setAccessToken(token);
    }
    auto time = std::chrono::seconds{ expiredTime > 30 ? expiredTime - 30 : expiredTime };
    drogon::app().getLoop()->runAfter(time, drogon::async_func([appId, clientSecret]()  {
        return getAccessTokenAsyncEveryExpiredTime(appId, clientSecret);
    }));
}

static drogon::Task<std::string> getGatewayAsync(const std::string token)
{
    auto client = getInstance()->getApiClient();
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
        {"d", getInstance()->seq().load()}
    };
    SPDLOG_INFO(QBOT_TAG "SEND {}", payload.dump());
    connection->send(payload.dump());
}

static nlohmann::json DispatchReady(const nlohmann::json& data)
{
    getInstance()->sessionId().assign(data["d"]["session_id"].get<std::string>());
    return {
        {"op", opcode::Heartbeat},
        {"d", nullptr}
    };
}

static nlohmann::json DispatchResumed(const nlohmann::json& data)
{
    return {
        {"op", opcode::Heartbeat},
        {"d", getInstance()->seq().load()}
    };
}

template<drogon::HttpMethod method = drogon::Post>
static drogon::Task<nlohmann::json> CallQBotApiAsync(const std::string& path, const nlohmann::json& data, const std::string& token)
{
    auto client = getInstance()->getApiClient();
    auto req = drogon::HttpRequest::newCustomHttpRequest(JsonMethod{ data, HttpMethodType<method>{} });
    req->setPath(path);
    req->addHeader("Authorization", "QQBot " + token);
    drogon::HttpResponsePtr resp = co_await client->sendRequestCoro(req);
    SPDLOG_INFO(QBOT_TAG "{} {} {} {}", req->methodString(), path, req->body(), resp->body());
    co_return resp->as<nlohmann::json>();
}

static drogon::Task<nlohmann::json> SendC2CMessageAsync(const nlohmann::json& payload, const std::string& openId, const std::string token)
{
    auto path = std::format("/v2/users/{}/messages", openId);
    co_return co_await CallQBotApiAsync(path, payload, token);
}

static drogon::Task<nlohmann::json> SendGroupMessageAsync(const nlohmann::json& payload, const std::string& openId, const std::string token)
{
    auto path = std::format("/v2/groups/{}/messages", openId);
    co_return co_await CallQBotApiAsync(path, payload, token);
}

static drogon::Task<nlohmann::json> UploadC2CFileAsync(const nlohmann::json& payload, const std::string& openId, const std::string token)
{
    auto path = std::format("/v2/users/{}/files", openId);
    co_return co_await CallQBotApiAsync(path, payload, token);
}

static drogon::Task<nlohmann::json> UploadC2CPartPrepareAysnc(const nlohmann::json& payload, const std::string& openId, const std::string token)
{
    auto path = std::format("/v2/users/{}/upload_prepare", openId);
    co_return co_await CallQBotApiAsync(path, payload, token);
}

static drogon::Task<nlohmann::json> UploadC2CPartFinishAysnc(const nlohmann::json& payload, const std::string& openId, const std::string token)
{
    auto path = std::format("/v2/users/{}/upload_part_finish", openId);
    co_return co_await CallQBotApiAsync(path, payload, token);
}

static drogon::Task<nlohmann::json> UploadGroupFileAsync(const nlohmann::json& payload, const std::string& openId, const std::string token)
{
    auto path = std::format("/v2/groups/{}/files", openId);
    co_return co_await CallQBotApiAsync(path, payload, token);
}

static drogon::Task<nlohmann::json> UploadGroupPartPrepareAysnc(const nlohmann::json& payload, const std::string& openId, const std::string token)
{
    auto path = std::format("/v2/groups/{}/upload_prepare", openId);
    co_return co_await CallQBotApiAsync(path, payload, token);
}

static drogon::Task<nlohmann::json> UploadGroupPartFinishAysnc(const nlohmann::json& payload, const std::string& openId, const std::string token)
{
    auto path = std::format("/v2/groups/{}/upload_part_finish", openId);
    co_return co_await CallQBotApiAsync(path, payload, token);
}

static drogon::Task<nlohmann::json> DeleteGroupMessageAsync(const nlohmann::json& payload, const std::string& openId, const std::string token)
{
    auto path = std::format("/v2/groups/{}/messages/{}", openId, payload.get<std::string_view>());
    co_return co_await CallQBotApiAsync<drogon::Delete>(path, {}, token);
}

static drogon::Task<nlohmann::json> DeleteC2CMessageAsync(const nlohmann::json& payload, const std::string& openId, const std::string token)
{
    auto path = std::format("/v2/users/{}/messages/{}", openId, payload.get<std::string_view>());
    co_return co_await CallQBotApiAsync<drogon::Delete>(path, {}, token);
}

static drogon::Task<nlohmann::json> GetJoinRequestList(const nlohmann::json& payload, const std::string& openId, const std::string token)
{
    auto path = std::format("/v2/groups/{}/join_request_list", openId);
    co_return co_await CallQBotApiAsync<drogon::Get>(path, payload, token);
}

static drogon::Task<nlohmann::json> ApprovalJoinRequest(const nlohmann::json& payload, const std::string& groupId, const std::string& userId, const std::string token)
{
    auto path = std::format("/v2/groups/{}/approval_join_request/{}", groupId, userId);
    co_return co_await CallQBotApiAsync(path, payload, token);
}

template<drogon::HttpMethod method> requires (method == drogon::Get || method == drogon::Post)
static drogon::Task<nlohmann::json> RestrictChatSetting(const nlohmann::json& payload, const std::string& openId, const std::string token)
{
    auto path = std::format("/v2/groups/{}/restrict_chat_setting", openId);
    nlohmann::json ret = co_await CallQBotApiAsync<method>(path, payload, token);
    co_return ret;
}

template<drogon::HttpMethod method> requires (method == drogon::Get || method == drogon::Post)
static drogon::Task<nlohmann::json> JoinApprovalStrategy(const nlohmann::json& payload, const std::string token)
{
    auto path = "/v2/groups/join_approval_strategy";
    nlohmann::json ret = co_await CallQBotApiAsync<method>(path, payload, token);
    co_return ret;
}

template<drogon::HttpMethod method> requires (method == drogon::Patch || method == drogon::Delete)
static drogon::Task<nlohmann::json> JoinApprovalStrategy(const nlohmann::json& payload, const std::string& strategyId, const std::string token)
{
    auto path = std::format("/v2/groups/join_approval_strategy/{}", strategyId);
    nlohmann::json ret = co_await CallQBotApiAsync<method>(path, payload, token);
    co_return ret;
}

static drogon::Task<nlohmann::json> GetSelfDetails(const std::string token)
{
    co_return co_await CallQBotApiAsync<drogon::Get>("/users/@me", {}, token);
}

template<drogon::HttpMethod method> requires (method == drogon::Get || method == drogon::Put)
static drogon::Task<nlohmann::json> Menu(const nlohmann::json& payload, const std::string token)
{
    auto path = "/v2/menu";
    nlohmann::json ret = co_await CallQBotApiAsync<method>(path, payload, token);
    co_return ret;
}

template<drogon::HttpMethod method> requires (method == drogon::Get || method == drogon::Post)
static drogon::Task<nlohmann::json> Panels(const nlohmann::json& payload, const std::string token)
{
    auto path = "/v2/panels";
    nlohmann::json ret = co_await CallQBotApiAsync<method>(path, payload, token);
    co_return ret;
}

template<drogon::HttpMethod method> requires (method == drogon::Get || method == drogon::Put || method == drogon::Delete)
static drogon::Task<nlohmann::json> Panels(const nlohmann::json& payload, const std::string& panelId, const std::string token)
{
    auto path = std::format("/v2/panels/{}", panelId);
    nlohmann::json ret = co_await CallQBotApiAsync<method>(path, payload, token);
    co_return ret;
}

static drogon::Task<nlohmann::json> UpdatePanelsTarget(const nlohmann::json& payload, const std::string& panelId, const std::string token)
{
    auto path = std::format("/v2/panels/{}/target", panelId);
    nlohmann::json ret = co_await CallQBotApiAsync<drogon::Put>(path, payload, token);
    co_return ret;
}

static void OnDispatchReceived(const nlohmann::json& data, const drogon::WebSocketConnectionPtr& connection)
{
    auto type = data["t"].get<std::string_view>();
    if (type == DispatchType::Ready.data)
    {
        auto payload = DispatchReady(data);
        SPDLOG_INFO(QBOT_TAG "SEND {}", payload.dump());
        connection->send(payload.dump());
        return;
    }
    if (type == DispatchType::Resumed.data)
    {
        auto payload = DispatchResumed(data);
        SPDLOG_INFO(QBOT_TAG "SEND {}", payload.dump());
        connection->send(payload.dump());
        return;
    }
    for (auto&& action : getInstance()->getDispatchMap().at(type))
    {
        action(data);
    }
}

static void OnReconnectReceived(const drogon::WebSocketConnectionPtr& connection)
{
    SPDLOG_WARN(QBOT_TAG "Need Resume");
    getInstance()->needResume().test_and_set();
}

static void SendIdentify(const drogon::WebSocketConnectionPtr& connection)
{
    auto payload = nlohmann::json{
        {"op", opcode::Identify},
        {"d", {
            {"token","QQBot " + getInstance()->getAccessToken()},
            {"intents", GROUP_AND_C2C_EVENT}
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
            {"token", "QQBot " + getInstance()->getAccessToken()},
            {"session_id", getInstance()->sessionId()},
            {"seq", getInstance()->seq().load()}
        }}
    };
    SPDLOG_INFO(QBOT_TAG "SEND {}", payload.dump());
    connection->send(payload.dump());
}


static void OnHelloReceived(const drogon::WebSocketConnectionPtr& connection)
{
    (getInstance()->needResume().test() ? SendResume : SendIdentify)(connection);
    getInstance()->needResume().clear();
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
            getInstance()->seq().store(data["s"].get<std::int64_t>());
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
    auto gateway = *client->getConnection()->getContext<std::string>();
    SPDLOG_INFO(QBOT_TAG "reconnect to {}", gateway);
    auto newClient = qbot::ConnectToWSServer(gateway, MessageHandler, ClosedHandler);
    drogon::app().getPlugin<qbot::Instance>()->setWSClient(newClient);
}

namespace qbot {
    void Instance::initAndStart(const Json::Value& config)
    {
        LOG_WARN << "init";
        m_sandbox = config.get("sandbox", false).asBool();
        m_appId = config.get("appId", "").asString();
        m_clientSecret = config.get("clientSecret", "").asString();
        m_apiClient = drogon::HttpClient::newHttpClient(m_sandbox ? QBotSandboxUrl : QBotUniversalUrl);
        drogon::app().registerBeginningAdvice(drogon::async_func([this]() -> drogon::Task<> {
            co_await getAccessTokenAsyncEveryExpiredTime(m_appId, m_clientSecret);
            auto gateway = co_await getGatewayAsync(m_accessToken);
            if (gateway.empty())
            {
                SPDLOG_ERROR("Empty Gateway! Please check the error message.");
                drogon::app().quit();
                co_return;
            }
            m_wsClient = ConnectToWSServer(gateway, MessageHandler, ClosedHandler);
        }));
    }

    void Instance::shutdown()
    {
        LOG_WARN << "down";
    }

    drogon::HttpClientPtr Instance::getApiClient()
    {
        return m_apiClient;
    }

    void Instance::setWSClient(const drogon::WebSocketClientPtr& client)
    {
        m_wsClient = client;
    }

    std::string Instance::getAccessToken()
    {
        std::shared_lock lock(m_tokenMutex);
        return m_accessToken;
    }

    void Instance::setAccessToken(const std::string token)
    {
        std::unique_lock lock(m_tokenMutex);
        m_accessToken = token;
    }

    std::string& Instance::sessionId()
    {
        return m_sessionId;
    }

    std::atomic_flag& Instance::needResume()
    {
        return m_needResume;
    }

    std::atomic_llong& Instance::seq()
    {
        return m_seq;
    }

    const DispatchMap& qbot::Instance::getDispatchMap()
    {
        return m_dispatchMap;
    }

    drogon::Task<nlohmann::json> Instance::sendC2CMessageAsync(const nlohmann::json& payload, const std::string& openId)
    {
        return SendC2CMessageAsync(payload, openId, getAccessToken());
    }

    drogon::Task<nlohmann::json> Instance::sendGroupMessageAsync(const nlohmann::json& payload, const std::string& openId)
    {
        return SendGroupMessageAsync(payload, openId, getAccessToken());
    }
}