#include <csignal>
#include <drogon/drogon.h>
#include <drogon/WebSocketClient.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

using namespace std::literals;

constexpr auto versionInfo = "Branch: " GIT_BRANCH "\nCommit: " GIT_VERSION "\nDate: " GIT_DATE;
constexpr auto LogPattern = "%m-%d %H:%M:%S.%e [%^%l%$] [thread:%t] [%s:%#] %v";
constexpr auto TargetLocaleName = "zh_CN.UTF-8";
constexpr auto C_LocaleName = "C";
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

// FNV-1a 32位编译期哈希算法
static constexpr uint32_t FNV1aHash(std::string_view sv)
{
    uint32_t hash = 2166136261UL; // FNV 偏移基数
    for (char c : sv)
    {
        hash ^= static_cast<uint8_t>(c);
        hash *= 16777619UL;       // FNV 质数
    }
    return hash;
}

template<size_t N>
struct FixedString
{
    char data[N]{};

    constexpr FixedString(const char(&str)[N])
    {
        std::copy_n(str, N, data);
    }

    constexpr FixedString() = default;

    constexpr size_t length() const { return N - 1; }
};

struct DispatchType
{
    // 登录成功
    static constexpr FixedString Ready = "READY";
    // 重连成功
    static constexpr FixedString Resumed = "RESUMED";
    // 用户单聊发消息给机器人
    static constexpr FixedString C2CMessageCreate = "C2C_MESSAGE_CREATE";
    // 用户添加使用机器人
    static constexpr FixedString FriendAdd = "FRIEND_ADD";
    // 用户删除机器人
    static constexpr FixedString FriendDel = "FRIEND_DEL";
    // 用户在机器人资料卡手动关闭"主动消息"推送
    static constexpr FixedString C2CMsgReject = "C2C_MSG_REJECT";
    // 用户在机器人资料卡手动开启"主动消息"推送开关
    static constexpr FixedString C2CMsgReceived = "C2C_MSG_RECEIVE";
    // 用户在群里@机器人时收到的消息
    static constexpr FixedString GroupAtMessageCreate = "GROUP_AT_MESSAGE_CREATE";
    // 机器人被添加到群聊
    static constexpr FixedString GroupAddRobot = "GROUP_ADD_ROBOT";
    // 机器人被移出群聊
    static constexpr FixedString GroupDelRobot = "GROUP_DEL_ROBOT";
    // 群管理员主动在机器人资料页操作关闭通知
    static constexpr FixedString GroupMsgReject = "GROUP_MSG_REJECT";
    // 群管理员主动在机器人资料页操作开启通知
    static constexpr FixedString GroupMsgReceive = "GROUP_MSG_RECEIVE";
    // 机器人收到了群聊消息
    static constexpr FixedString GroupMessageCreate = "GROUP_MESSAGE_CREATE";
    // 群用户添加
    static constexpr FixedString GroupMemberAdd = "GROUP_MEMBER_ADD";
    // 群用户移除
    static constexpr FixedString GroupMemberRemove = "GROUP_MEMBER_REMOVE";
};

typedef nlohmann::json(*DispatchAction)(const nlohmann::json& data);

template<FixedString T, DispatchAction F>
struct Dispatcher
{
    static constexpr std::string_view type = T.data;
    static constexpr DispatchAction action = F;
};

namespace drogon {
    template<> 
    HttpRequestPtr toRequest(nlohmann::json&& obj)
    {
        auto req = HttpRequest::newHttpRequest();
        req->setMethod(drogon::Post);
        req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        req->setBody(obj.dump());
        return req;
    }

    template<>
    HttpRequestPtr toRequest(const nlohmann::json& obj)
    {
        auto req = HttpRequest::newHttpRequest();
        req->setMethod(drogon::Post);
        req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        req->setBody(obj.dump());
        return req;
    }

    template<>
    nlohmann::json fromResponse(const HttpResponse& resp)
    {
        return nlohmann::json::parse(resp.body());
    }
}

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

static MessageCache& getGlobalEventIdCache()
{
    static auto cacheMap = MessageCache(drogon::app().getIOLoop(0));
    return cacheMap;
}

static drogon::Task<nlohmann::json> CallQBotApiAsync(const std::string& path, const nlohmann::json& data, const std::string& token)
{
    static auto client = drogon::HttpClient::newHttpClient("https://api.sgroup.qq.com");
    auto req = drogon::HttpRequest::newCustomHttpRequest(data);
    req->setPath(path);
    req->addHeader("Authorization", "QQBot " + token);
    auto& resp = co_await client->sendRequestCoro(req);
    SPDLOG_INFO(QBOT_TAG "{} {} {} {}", req->methodString(), path, req->body(), resp->body());
    co_return *resp;
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
    trantor::Logger::enableSpdLog();
    drogon::app()
        .loadConfigFile("./config.json")
        .setLogLocalTime(true)
#ifdef DEBUG
        .setLogLevel(trantor::Logger::kDebug)
#else
        .setLogLevel(trantor::Logger::kInfo)
#endif // DEBUG
        .registerPostHandlingAdvice(HttpLogger)
        .setThreadNum(0);
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

static drogon::Task<std::string> getAccessTokenAsync()
{
    static auto client = drogon::HttpClient::newHttpClient("https://bots.qq.com");
    auto& config = drogon::app().getCustomConfig();
    auto req = drogon::HttpRequest::newCustomHttpRequest(nlohmann::json{
        {"appId", config["appId"].asString()},
        {"clientSecret", config["clientSecret"].asString()}
    });
    req->setPath("/app/getAppAccessToken");
    auto resp = co_await client->sendRequestCoro(req);
    SPDLOG_INFO(QBOT_TAG "{} {} {}", req->methodString(), req->path(), resp->body());
    co_return nlohmann::json::parse(resp->body()).value("access_token", "");
}

static drogon::Task<std::string> getGatewayAysnc(const std::string token)
{
    auto client = drogon::HttpClient::newHttpClient("https://api.sgroup.qq.com");
    auto req = drogon::HttpRequest::newHttpJsonRequest({});
    req->setPath("/gateway");
    req->addHeader("Authorization", "QQBot " + token);
    auto resp = co_await client->sendRequestCoro(req);
    SPDLOG_INFO(QBOT_TAG "{} {} {}", req->methodString(), req->path(), resp->body());
    co_return nlohmann::json::parse(resp->body()).value("url", "");
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

static drogon::Task<nlohmann::json> SendC2CFileAysnc(const nlohmann::json& payload, const std::string& openId, const std::string token)
{
    auto path = std::format("/v2/users/{}/files", openId);
    co_return co_await CallQBotApiAsync(path, payload, token);
}

static drogon::Task<nlohmann::json> SendGroupFileAysnc(const nlohmann::json& payload, const std::string& openId, const std::string token)
{
    auto path = std::format("/v2/groups/{}/files", openId);
    co_return co_await CallQBotApiAsync(path, payload, token);
}

static drogon::Task<nlohmann::json> DeleteGroupMessageAysnc(const nlohmann::json& payload, const std::string& openId, const std::string token)
{
    auto path = std::format("/v2/groups/{}/messages/{}", openId, payload.get<std::string_view>());
    co_return co_await CallQBotApiAsync(path, {}, token);
}

static drogon::Task<nlohmann::json> DeleteC2CMessageAysnc(const nlohmann::json& payload, const std::string& openId, const std::string token)
{
    auto path = std::format("/v2/users/{}/messages/{}", openId, payload.get<std::string_view>());
    co_return co_await CallQBotApiAsync(path, {}, token);
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
        Dispatcher<DispatchType::GroupDelRobot, DispatchGroupDelRobot>
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

template<FixedString schema>
static drogon::WebSocketClientPtr ConnectToWSGateway(const std::string gateway, const WSMessageHandler& messageHandler, const WSClosedHandler& closedHandler)
{
    auto pos = gateway.find("/", schema.length());
    auto host = gateway.substr(0, pos);
    auto path = gateway.substr(pos);
    auto client = drogon::WebSocketClient::newWebSocketClient(host);
    client->setMessageHandler(messageHandler);
    client->setConnectionClosedHandler(closedHandler);
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setPath(path);
    client->connectToServer(req, [gateway](drogon::ReqResult, const drogon::HttpResponsePtr&, const drogon::WebSocketClientPtr& client){
        SPDLOG_INFO("{} is connected!", gateway);
        client->getConnection()->setContext(std::make_shared<std::string>(gateway));
    });
    return client;
}

static void QBotClosedHandler(const drogon::WebSocketClientPtr& client)
{
    auto& cacheMap = getGlobalClientCache();
    auto gateway = *client->getConnection()->getContext<std::string>();
    cacheMap.modify(gateway, [](drogon::WebSocketClientPtr& pClient) {
        auto gateway = *pClient->getConnection()->getContext<std::string>();
        SPDLOG_INFO(QBOT_TAG "reconnect to {}", gateway);
        pClient = ConnectToWSGateway<"wss://">(gateway, QBotMessageHandler, QBotClosedHandler);
    });
}

static drogon::Task<> Start()
{
    auto token = getGlobalAccessToken().assign(co_await getAccessTokenAsync());
    auto gateway = co_await getGatewayAysnc(token);
    if (gateway.empty())
    {
        SPDLOG_ERROR("Empty Gateway! Please check the error message.");
        drogon::app().quit();
        co_return;
    }
    auto client = ConnectToWSGateway<"wss://">(gateway, QBotMessageHandler, QBotClosedHandler);
    getGlobalClientCache().insert(gateway, client);
    drogon::app().getLoop()->runEvery(30s, drogon::async_func([]() -> drogon::Task<> {
        if (std::string token = co_await getAccessTokenAsync(); token != getGlobalAccessToken()) [[unlikely]]
        {
            co_await drogon::switchThreadCoro(drogon::app().getLoop());
            getGlobalAccessToken().assign(std::move(token));
        }
    }));
}

int main() 
{
    initEnv();
    drogon::app()
        .addListener("0.0.0.0", drogon::app().getCustomConfig().get("port", 8080).as<Json::Int>())
        .registerHandler("/", &AppVersionHandler, { drogon::Get }, "AppVersion")
        .registerBeginningAdvice(drogon::async_func(Start))
        .run();
    return 0;
}
