#include "onebot_Instance.h"
#include "qbot_Instance.h"
#include "onebot_Event.h"
#include <spdlog/spdlog.h>
#include <drogon/HttpAppFramework.h>

#define ONEBOT_TAG "\033[36mOneBot\033[0m "

using qbot::DispatchType;
using OnebotContext = std::tuple<std::string, std::string, trantor::TimerId>;
using IdCache = drogon::CacheMap<std::uint32_t, std::pair<std::string, std::uint32_t>>;
using IdDequeCache = drogon::CacheMap<std::uint64_t, std::deque<std::uint32_t>>;

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
        pClient = tools::ConnectToWSServer(url, MessageHandler, ClosedHandler, [url, token](drogon::ReqResult r, const drogon::HttpResponsePtr& resp, const drogon::WebSocketClientPtr& client) {
            RequestCallback(url, token, r, resp, client);
        }, headers);
    });
}

namespace onebot {

    struct Instance::Impl
    {
        Impl() = default;
        ~Impl() = default;
        ClientCache clientCache{ drogon::app().getLoop() };
        IdCache eventIdCache{ drogon::app().getLoop() };
        IdCache messageIdCache{ drogon::app().getLoop() };
        IdDequeCache idDequeCache{ drogon::app().getLoop() };
        std::vector<std::string> urlArray{};
    };

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
            tools::ConnectToWSServer(url, MessageHandler, ClosedHandler, [url, token](drogon::ReqResult r, const drogon::HttpResponsePtr& resp, const drogon::WebSocketClientPtr& client) {
                RequestCallback(url, token, r, resp, client);
                if (r == drogon::ReqResult::Ok)
                {
                    getInstance()->clientCache().insert(url, client);
                }
            }, headers);
            getInstance()->insertUrl(url);
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
        m_impl = std::make_shared<Impl>();
        InitAndConnect(config);
        RegisterDispatchActions();
    }

    void Instance::shutdown()
    {
        SPDLOG_WARN(ONEBOT_TAG "down");
    }

    ClientCache& Instance::clientCache()
    {
        return m_impl->clientCache;
    }

    void Instance::insertUrl(std::string_view urlView)
    {
        m_impl->urlArray.emplace_back(urlView);
    }

    void Instance::dispatch(const std::shared_ptr<void>& eventVariant)
    {
        auto pVariant = std::static_pointer_cast<Event::Variant>(eventVariant);
        for (auto&& url : m_impl->urlArray)
        {
            auto client = (m_impl->clientCache)[url];
            client->getLoop()->runInLoop([client, pVariant] {
                if (auto connection = client->getConnection(); connection != nullptr && connection->connected())
                {
                    Send(connection, *pVariant);
                }
            });
        }
    }

    void Instance::cacheEventId(std::uint64_t sceneId, std::uint32_t eventId, std::string_view eventIdStr, qbot::SceneConstants constants)
    {
        if (!m_impl->idDequeCache.find(sceneId))
        {
            m_impl->idDequeCache.insert(sceneId, {});
        }
        m_impl->idDequeCache.modify(sceneId, [sceneId, eventId, this, idStr = std::string(eventIdStr), constants](std::deque<std::uint32_t>& val) {
            val.push_back(eventId);
            m_impl->eventIdCache.insert(eventId, { idStr, constants.times }, constants.timeout, [sceneId, this] {
                m_impl->idDequeCache.modify(sceneId, [](std::deque<std::uint32_t>& val) {
                    val.pop_front();
                });
            });
        });
    }

    void Instance::cacheMessageId(std::uint64_t sceneId, std::uint32_t messageId, std::string_view messageIdStr, qbot::SceneConstants constants)
    {
        if (!m_impl->idDequeCache.find(sceneId))
        {
            m_impl->idDequeCache.insert(sceneId, {});
        }
        m_impl->idDequeCache.modify(sceneId, [sceneId, messageId, this, idStr = std::string(messageIdStr), constants](std::deque<std::uint32_t>& val) {
            val.push_back(messageId);
            m_impl->messageIdCache.insert(messageId, { idStr, constants.times }, constants.timeout, [sceneId, this] {
                m_impl->idDequeCache.modify(sceneId, [](std::deque<std::uint32_t>& val) {
                    val.pop_front();
                });
            });
        });
    }
}
