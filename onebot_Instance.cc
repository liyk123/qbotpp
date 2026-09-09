#include "onebot_Instance.h"
#include "qbot_Instance.h"
#include <spdlog/spdlog.h>
#include <xxhash.h>

#define ONEBOT_TAG "\033[36mOneBot\033[0m "

using qbot::DispatchType;
using OnebotContext = std::tuple<std::string, std::string, trantor::TimerId>;

constexpr XXH64_hash_t OPID_HASH_SEED = 'opid';
constexpr XXH32_hash_t MGID_HASH_SEED = 'mgid';

onebot::Instance* getInstance()
{
    return drogon::app().getPlugin<onebot::Instance>();
}

template<typename T> requires (std::same_as<std::decay_t<T>, onebot::Event::Variant> || onebot::Event::Concept<T>)
static void Send(const drogon::WebSocketConnectionPtr& connection, T&& data)
{
    auto payload = std::string{};
    if constexpr (std::same_as<std::decay_t<T>, onebot::Event::Variant>)
    {
        payload = std::visit([](auto&& x) {
            return nlohmann::json(x).dump();
        }, std::forward<T>(data));
    }
    else if constexpr(onebot::Event::Concept<T>)
    {
        payload = nlohmann::json(std::forward<T>(data)).dump();
    }
    SPDLOG_INFO(ONEBOT_TAG "SEND {}", payload);
    connection->send(payload);
}

static void TextHandler(const std::string& msg, const drogon::WebSocketConnectionPtr& connection)
{
    auto data = nlohmann::json::parse(msg);
}

static void CloseHandler(const drogon::WebSocketConnectionPtr& connection)
{
    SPDLOG_WARN(ONEBOT_TAG "Close");
    connection->forceClose();
}

static void MessageHandler(std::string&& msg, const drogon::WebSocketClientPtr& client, const drogon::WebSocketMessageType& type)
{
    SPDLOG_INFO(ONEBOT_TAG "RECV {}", msg);
    auto connection = client->getConnection();
    switch (type)
    {
        [[likely]] case drogon::WebSocketMessageType::Text:
            TextHandler(msg, connection);
            break;
        [[unlikely]] case drogon::WebSocketMessageType::Close:
            CloseHandler(connection);
            break;
        default:
            break;
    }
}

static void RequestCallback(const std::string url,const std::string token, const drogon::ReqResult r, const drogon::HttpResponsePtr& resp, const drogon::WebSocketClientPtr& client)
{
    if (r != drogon::ReqResult::Ok)
    {
        SPDLOG_ERROR(ONEBOT_TAG "{} {} {}", url, (int)r, resp->body());
        return;
    }
    SPDLOG_INFO("{} is connected!", url);
    Send(client->getConnection(), onebot::Event::LifecycleEvent{
        .sub_type{onebot::Event::LifecycleEvent::CONNECT},
        .time{(std::uint64_t)std::time(nullptr)},
        .self_id{std::stoull(drogon::app().getPlugin<qbot::Instance>()->getAppId())}
    });
    auto timerId = client->getLoop()->runEvery(std::chrono::seconds(5), [url]() {
        auto connection = getInstance()->clientCache()[url]->getConnection();
        if (connection->disconnected())
        {
            return;
        }
        Send(connection, onebot::Event::HeartbeatEvent{
            .time{(std::uint64_t)std::time(nullptr)},
            .self_id{std::stoull(drogon::app().getPlugin<qbot::Instance>()->getAppId())},
            .status{
                .online{true},
                .good{true}
            },
            .interval{5000}
        });
    });
    client->getConnection()->setContext(std::make_shared<OnebotContext>(url, token, timerId));
}

static void ClosedHandler(const drogon::WebSocketClientPtr& client)
{
    auto&& [url, _, timerId] = *client->getConnection()->getContext<OnebotContext>();
    client->getLoop()->invalidateTimer(timerId);
    getInstance()->clientCache().modify(url, [](drogon::WebSocketClientPtr& pClient) {
        auto&& [url, token, _] = *pClient->getConnection()->getContext<OnebotContext>();
        SPDLOG_INFO(ONEBOT_TAG "reconnect to {}", url);
        auto headers = std::vector<std::pair<std::string, std::string>>{};
        if (!token.empty())
        {
            headers.emplace_back("Authorization", "Bearer " + token);
        }
        pClient = qbot::ConnectToWSServer(url, MessageHandler, ClosedHandler, [url, token](drogon::ReqResult r, const drogon::HttpResponsePtr& resp, const drogon::WebSocketClientPtr& client) {
            RequestCallback(url, token, r, resp, client);
        }, headers);
    });
}
namespace onebot {
    static void InitAndConnect(const Json::Value& config)
    {
        for (auto&& val : config["ws_reverse"])
        {
            auto url = val.get("url", "").asString();
            if (url.empty())
            {
                continue;
            }
            auto token = val.get("access_token", "").asString();
            auto headers = std::vector<std::pair<std::string, std::string>>{};
            if (!token.empty())
            {
                headers.emplace_back("Authorization", "Bearer " + token);
            }
            qbot::ConnectToWSServer(url, MessageHandler, ClosedHandler, [url, token](drogon::ReqResult r, const drogon::HttpResponsePtr& resp, const drogon::WebSocketClientPtr& client) {
                RequestCallback(url, token, r, resp, client);
                if (r == drogon::ReqResult::Ok)
                {
                    getInstance()->clientCache().insert(url, client);
                }
            }, headers);
            getInstance()->urlArray().emplace_back(url);
        }
    }

    static void RegisterDispatchActions()
    {
        auto qBot = drogon::app().getPlugin<qbot::Instance>();
        qBot->registerDispatchAction<DispatchType::C2CMessageCreate>(Event::OnPrivateMsgReveived);
        qBot->registerDispatchAction<DispatchType::GroupMessageCreate>(Event::OnGroupMsgReveived);
        qBot->registerDispatchAction<DispatchType::GroupAtMessageCreate>(Event::OnGroupMsgReveived);
        qBot->registerDispatchAction<DispatchType::GroupAddRobot>(Event::onGroupInviteMeRequestReceived);
        qBot->registerDispatchAction<DispatchType::GroupDelRobot>(Event::onGroupMemberKickMeNoticeReceived);
        qBot->registerDispatchAction<DispatchType::FriendAdd>(Event::onFriendAddNoticeReceived);
        qBot->registerDispatchAction<DispatchType::FriendDel>(Event::onFriendDelNoticeReceived);
        qBot->registerDispatchAction<DispatchType::GroupMemberAdd>(Event::onGroupMemberIncreaseNoticeReceived);
        qBot->registerDispatchAction<DispatchType::GroupMemberRemove>(Event::onGroupMemberLeaveNoticeReceived);
        qBot->registerDispatchAction<DispatchType::GroupJoinRequest>(Event::onGroupMemberJoinRequestReceived);
    }

    void Instance::initAndStart(const Json::Value& config)
    {
        SPDLOG_WARN(ONEBOT_TAG "init");
        InitAndConnect(config);
        RegisterDispatchActions();
    }

    void Instance::shutdown()
    {
        SPDLOG_WARN(ONEBOT_TAG "down");
    }

    qbot::ClientCache& Instance::clientCache()
    {
        return m_clientCache;
    }

    std::vector<std::string>& Instance::urlArray()
    {
        return m_urlArray;
    }

    void Instance::dispatch(std::shared_ptr<onebot::Event::Variant> data)
    {
        for (auto&& url : m_urlArray)
        {
            auto client = m_clientCache[url];
            client->getLoop()->runInLoop([client, data] {
                if (auto connection = client->getConnection(); connection != nullptr && connection->connected())
                {
                    Send(connection, *data);
                }
            });
        }
    }
}