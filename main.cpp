#include <csignal>
#include <drogon/drogon.h>
#include <drogon/WebSocketClient.h>
#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include "qbot_tools.h"

using namespace std::literals;

constexpr auto versionInfo = "Branch: " GIT_BRANCH "\nCommit: " GIT_VERSION "\nDate: " GIT_DATE;
constexpr auto LogPattern = "%m-%d %H:%M:%S.%e [%^%L%$] [thread:%t] [%s:%#] %v";
constexpr auto TargetLocaleName = "zh_CN.UTF-8";
constexpr auto C_LocaleName = "C";
constexpr auto QBotUniversalUrl = "https://api.bot.qq.com";
constexpr auto QBotSandboxUrl = "https://sandbox.api.sgroup.qq.com";
#define QBOT_TAG "\033[36mQBot\033[0m "

using ClientCache = drogon::CacheMap<std::string, drogon::WebSocketClientPtr>;
using MessageCache = drogon::CacheMap<std::uint32_t, std::string>;
using WSAsyncMessageHandler = std::function<drogon::Task<void>(std::string&&, const drogon::WebSocketClientPtr&, const drogon::WebSocketMessageType&)>;
using WSMessageHandler = std::function<void(std::string&&, const drogon::WebSocketClientPtr&, const drogon::WebSocketMessageType&)>;
using WSAsyncClosedHandler = std::function<drogon::Task<void>(const drogon::WebSocketClientPtr&)>;
using WSClosedHandler = std::function<void(const drogon::WebSocketClientPtr&)>;

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

constexpr auto GROUP_AND_C2C_EVENT = 1U << 25 | 1U << 24;

alignas(std::hardware_destructive_interference_size) std::atomic<std::uint64_t> g_seq = 0;

alignas(std::hardware_destructive_interference_size) std::atomic_flag g_needResume{};

using qbot::DispatchType;
using qbot::Dispatcher;
using qbot::HttpMethodType;
using qbot::JsonMethod;
using qbot::ConnectToWSServer;

static std::string& getGlobalAccessToken()
{
    static std::string accessToken;
    return accessToken;
}

static ClientCache& getGlobalClientCache()
{
    static auto cacheMap = ClientCache(drogon::app().getLoop());
    return cacheMap;
}

static std::string& getGlobalSessionId()
{
    static std::string sessionId;
    return sessionId;
}

static drogon::HttpClientPtr getGlobalQQBotApiClient()
{
    static auto sandbox = drogon::app().getCustomConfig().get("sandbox", false).asBool();
    static auto client = drogon::HttpClient::newHttpClient(sandbox ? QBotSandboxUrl : QBotUniversalUrl);
    return client;
}

static MessageCache& getGlobalEventIdCache()
{
    static auto cacheMap = MessageCache(drogon::app().getIOLoop(0));
    return cacheMap;
}

template<drogon::HttpMethod method = drogon::Post>
static drogon::Task<nlohmann::json> CallQBotApiAsync(const std::string& path, const nlohmann::json& data, const std::string& token)
{
    auto client = getGlobalQQBotApiClient();
    auto req = drogon::HttpRequest::newCustomHttpRequest(JsonMethod{ data, HttpMethodType<method>{} });
    req->setPath(path);
    req->addHeader("Authorization", "QQBot " + token);
    drogon::HttpResponsePtr resp = co_await client->sendRequestCoro(req);
    SPDLOG_INFO(QBOT_TAG "{} {} {} {}", req->methodString(), path, req->body(), resp->body());
    co_return resp->as<nlohmann::json>();
}

static void HttpLogger(const drogon::HttpRequestPtr& req, const drogon::HttpResponsePtr& resp)
{
    SPDLOG_INFO("{} {} {} {}", req->methodString(), req->path(), nlohmann::json(req->parameters()).dump(), resp->body());
}

static void initEnv()
{
#ifdef _WIN32
    ::system("chcp 65001 && cls");
#endif
    std::setlocale(LC_ALL, TargetLocaleName);
    std::setlocale(LC_NUMERIC, C_LocaleName);
    std::locale::global(std::locale(TargetLocaleName));
    std::locale::global(std::locale(std::locale(), C_LocaleName, std::locale::numeric));
    spdlog::default_logger()->set_pattern(LogPattern);
    trantor::Logger::enableSpdLog(spdlog::default_logger());
    drogon::app()
        .setThreadNum(0)
        .registerPostHandlingAdvice(HttpLogger)
        .loadConfigFile("config.yml");
    spdlog::default_logger()->set_level(spdlog::level::level_enum{ trantor::Logger::logLevel() });
    std::signal(SIGTERM, [](int) {
        drogon::app().getLoop()->runInLoop([] { drogon::app().quit(); }); 
    });
}

static void AppVersionHandler(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback)
{
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setPassThrough(true);
    resp->setBody(versionInfo);
    resp->setContentTypeCode(drogon::ContentType::CT_TEXT_HTML);
    callback(resp);
};

static drogon::Task<std::pair<std::string, std::uint64_t>> getAccessTokenAsync()
{
    static auto client = drogon::HttpClient::newHttpClient(QBotUniversalUrl);
    auto& config = drogon::app().getCustomConfig();
    auto req = drogon::HttpRequest::newCustomHttpRequest(nlohmann::json{
        {"appId", config["appId"].asString()},
        {"clientSecret", config["clientSecret"].asString()}
    });
    req->setPath("/app/getAppAccessToken");
    auto resp = co_await client->sendRequestCoro(req);
    SPDLOG_INFO(QBOT_TAG "{} {} {}", req->methodString(), req->path(), resp->body());
    auto data = resp->as<nlohmann::json>();
    co_return{ data.value("access_token", ""),std::stoull(data.value("expires_in","30"))};
}

static drogon::Task<std::string> getGatewayAsync(const std::string token)
{
    auto client = getGlobalQQBotApiClient();
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
        {"d", g_seq.load()}
    };
    SPDLOG_INFO(QBOT_TAG "SEND {}", payload.dump());
    connection->send(payload.dump());
}

static nlohmann::json DispatchReady(const nlohmann::json& data)
{
    getGlobalSessionId().assign(data["d"]["session_id"].get<std::string>());
    return {
        {"op", opcode::Heartbeat},
        {"d", nullptr}
    };
}

static nlohmann::json DispatchResumed(const nlohmann::json& data)
{
    return {
        {"op", opcode::Heartbeat},
        {"d", g_seq.load()}
    };
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
static drogon::Task<nlohmann::json> Panels(const nlohmann::json& payload,const std::string& panelId, const std::string token)
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

static nlohmann::json DispatchC2CMessageCreate(const nlohmann::json& data)
{
    drogon::app().getLoop()->queueInLoop(drogon::async_func([data]() -> drogon::Task<> {
        nlohmann::json payload{
            {"markdown", {{"content", data["d"]["content"]}}},
            {"msg_type", 2},
            {"msg_id", data["d"]["id"]}
        };
        auto& userOpenId = data["d"]["author"]["user_openid"];
        co_await SendC2CMessageAsync(payload, userOpenId, getGlobalAccessToken());
    }));
    return {};
}

static nlohmann::json DispatchGroupMessageCreate(const nlohmann::json& data)
{
    drogon::app().getLoop()->queueInLoop(drogon::async_func([data]() -> drogon::Task<> {
        nlohmann::json payload{
            {"markdown", {{"content", data["d"]["content"]}}},
            {"msg_type", 2},
            {"msg_id", data["d"]["id"]}
        };
        auto& userOpenId = data["d"]["group_openid"];
        co_await SendGroupMessageAsync(payload, userOpenId, getGlobalAccessToken());
    }));
    return {};
}

static nlohmann::json DispatchGroupAtMessageCreate(const nlohmann::json& data)
{
    drogon::app().getLoop()->queueInLoop(drogon::async_func([data]() -> drogon::Task<> {
        nlohmann::json payload{
            {"markdown", {{"content", data["d"]["content"]}}},
            {"msg_type", 2},
            {"msg_id", data["d"]["id"]}
        };
        auto& userOpenId = data["d"]["group_openid"];
        co_await SendGroupMessageAsync(payload, userOpenId, getGlobalAccessToken());
    }));
    return {};
}

static nlohmann::json DispatchFriendAdd(const nlohmann::json& data)
{
    return {};
}

static nlohmann::json DispatchFriendDel(const nlohmann::json& data)
{
    return {};
}

static nlohmann::json DispatchGroupAddRobot(const nlohmann::json& data)
{
    return {};
}

static nlohmann::json DispatchGroupDelRobot(const nlohmann::json& data)
{
    return {};
}

static nlohmann::json DispatchGroupJoinRequest(const nlohmann::json& data)
{
    return {};
}

template<typename... Ds>
nlohmann::json _dispacher_construct(const std::string_view type, const nlohmann::json& data)
{
    auto payload = nlohmann::json{};
    ([&] { return type == Ds::type ? (payload = Ds::action(data), true) : false; }() || ...);
    return payload;
}

static void OnDispatchReceived(const nlohmann::json& data, const drogon::WebSocketConnectionPtr& connection)
{
    auto type = data["t"].get<std::string_view>();
    auto payload = _dispacher_construct<
        Dispatcher<DispatchType::Ready, DispatchReady>,
        Dispatcher<DispatchType::Resumed, DispatchResumed>,
        Dispatcher<DispatchType::C2CMessageCreate, DispatchC2CMessageCreate>,
        Dispatcher<DispatchType::GroupMessageCreate, DispatchGroupMessageCreate>,
        Dispatcher<DispatchType::GroupAtMessageCreate, DispatchGroupAtMessageCreate>,
        Dispatcher<DispatchType::FriendAdd, DispatchFriendAdd>,
        Dispatcher<DispatchType::FriendDel, DispatchFriendDel>,
        Dispatcher<DispatchType::GroupAddRobot, DispatchGroupAddRobot>,
        Dispatcher<DispatchType::GroupDelRobot, DispatchGroupDelRobot>,
        Dispatcher<DispatchType::GroupJoinRequest, DispatchGroupJoinRequest>
    >(type, data);
    if (payload.is_null())
    {
        return;
    }
    if (type == DispatchType::Ready.data || type == DispatchType::Resumed.data)
    {
        SPDLOG_INFO(QBOT_TAG "SEND {}", payload.dump());
        connection->send(payload.dump());
        return;
    }
}

static void OnReconnectReceived(const drogon::WebSocketConnectionPtr& connection)
{
    SPDLOG_WARN(QBOT_TAG "Need Resume");
    g_needResume.test_and_set();
}

static void SendIdentify(const drogon::WebSocketConnectionPtr& connection)
{
    auto payload = nlohmann::json{
        {"op", opcode::Identify},
        {"d", {
            {"token","QQBot " + getGlobalAccessToken()},
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
            {"token", "QQBot " + getGlobalAccessToken()},
            {"session_id", getGlobalSessionId()},
            {"seq", g_seq.load()}
        }}
    };
    SPDLOG_INFO(QBOT_TAG "SEND {}", payload.dump());
    connection->send(payload.dump());
}


static void OnHelloReceived(const drogon::WebSocketConnectionPtr& connection)
{
    (g_needResume.test() ? SendResume : SendIdentify)(connection);
    g_needResume.clear();
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
            g_seq = data["s"];
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

static void QBotMessageHandler(std::string&& msg, const drogon::WebSocketClientPtr& client, const drogon::WebSocketMessageType& type) 
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

static void QBotClosedHandler(const drogon::WebSocketClientPtr& client)
{
    auto& cacheMap = getGlobalClientCache();
    auto gateway = *client->getConnection()->getContext<std::string>();
    cacheMap.modify(gateway, [](drogon::WebSocketClientPtr& pClient) {
        auto gateway = *pClient->getConnection()->getContext<std::string>();
        SPDLOG_INFO(QBOT_TAG "reconnect to {}", gateway);
        pClient = ConnectToWSServer(gateway, QBotMessageHandler, QBotClosedHandler);
    });
}

static drogon::Task<> getAccessTokenAsyncEveryExpiredTime()
{
    auto [token, expiredTime] = co_await getAccessTokenAsync();
    if (token != getGlobalAccessToken()) [[unlikely]]
    {
        co_await drogon::switchThreadCoro(drogon::app().getLoop());
        getGlobalAccessToken().assign(std::move(token));
    }
    auto time = std::chrono::seconds{ expiredTime > 30 ? expiredTime - 30 : expiredTime };
    drogon::app().getLoop()->runAfter(time, drogon::async_func(getAccessTokenAsyncEveryExpiredTime));
}

static drogon::Task<> Start()
{
    co_await getAccessTokenAsyncEveryExpiredTime();
    auto gateway = co_await getGatewayAsync(getGlobalAccessToken());
    if (gateway.empty())
    {
        SPDLOG_ERROR("Empty Gateway! Please check the error message.");
        drogon::app().quit();
        co_return;
    }
    auto client = ConnectToWSServer(gateway, QBotMessageHandler, QBotClosedHandler);
    getGlobalClientCache().insert(gateway, client);
}

int main() 
{
    initEnv();
    drogon::app()
        .registerHandler("/", &AppVersionHandler, { drogon::Get })
        .registerBeginningAdvice(drogon::async_func(Start))
        .run();
    return 0;
}
