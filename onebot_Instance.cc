#include "onebot_Instance.h"
#include "qbot_Instance.h"
#include "qbot_tools.h"
#include "onebot_Event.h"
#include <drogon/drogon.h>
#include <spdlog/spdlog.h>
#include <concurrentqueue/moodycamel/concurrentqueue.h>

#define ONEBOT_TAG "\033[36mOneBot\033[0m "

namespace QDT = qbot::DispatchType;
using OnebotContext = std::tuple<std::string, std::string, trantor::TimerId>;
using ClientCache = drogon::CacheMap<std::string, drogon::WebSocketClientPtr>;
using IdCache = drogon::CacheMap<std::uint32_t, std::string>;
using IdQueue = moodycamel::ConcurrentQueue<std::uint32_t>;
using IdQueueCache = drogon::CacheMap<std::uint64_t, IdQueue>;
namespace OneEvent = onebot::Event;

struct onebot::Instance::Impl
{
    ClientCache clientCache{ drogon::app().getLoop() };
    IdCache eventIdCache{ drogon::app().getLoop() };
    IdCache messageIdCache{ drogon::app().getLoop() };
    IdQueueCache idQueueCache{ drogon::app().getLoop() };
    std::vector<std::string> urlArray{};

    Impl() = default;
    Impl(Impl&&) = default;
    Impl(Impl&) = delete;
    Impl(const Impl&) = delete;
    ~Impl() = default;

    void dispatch(const std::shared_ptr<Event::Variant>& data);
    template<qbot::IdBucketConstants constants>
    void cacheId(std::uint64_t sceneId, std::uint32_t id, const std::string& idStr, IdCache& idcahe);
    template<OneEvent::Concept T>
    void cacheId(T&& data);
    std::string getCacheId(const std::uint64_t sceneId);
};

onebot::Instance* getInstance()
{
    return drogon::app().getPlugin<onebot::Instance>();
}

template<typename T> requires (std::same_as<std::decay_t<T>, OneEvent::Variant> || OneEvent::Concept<T>)
static void Send(const drogon::WebSocketConnectionPtr& connection, T&& data)
{
    auto payload = std::string{};
    if constexpr (std::same_as<std::decay_t<T>, OneEvent::Variant>)
    {
        payload = std::visit([](auto&& x) {
            getInstance()->m_pImpl->cacheId(std::forward<decltype(x)>(x));
            return nlohmann::json(std::forward<decltype(x)>(x)).dump();
        }, std::forward<T>(data));
    }
    else if constexpr(OneEvent::Concept<T>)
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
    Send(client->getConnection(), OneEvent::LifecycleEvent{
        .sub_type{OneEvent::LifecycleEvent::CONNECT},
        .time{(std::uint64_t)std::time(nullptr)},
        .self_id{std::stoull(drogon::app().getPlugin<qbot::Instance>()->getAppId())}
    });
    auto timerId = client->getLoop()->runEvery(std::chrono::seconds(5), [url]() {
        auto connection = getInstance()->m_pImpl->clientCache[url]->getConnection();
        if (connection->disconnected())
        {
            return;
        }
        Send(connection, OneEvent::HeartbeatEvent{
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
    getInstance()->m_pImpl->clientCache.modify(url, [](drogon::WebSocketClientPtr& pClient) {
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
                getInstance()->m_pImpl->clientCache.insert(url, client);
            }
        }, headers);
        getInstance()->m_pImpl->urlArray.emplace_back(url);
    }
}

static void RegisterDispatchActions()
{
    auto qBot = drogon::app().getPlugin<qbot::Instance>();
    qBot->registerDispatchAction<QDT::C2CMessageCreate>(OneEvent::OnPrivateMsgReveived);
    qBot->registerDispatchAction<QDT::GroupMessageCreate>(OneEvent::OnGroupMsgReveived);
    qBot->registerDispatchAction<QDT::GroupAtMessageCreate>(OneEvent::OnGroupMsgReveived);
    qBot->registerDispatchAction<QDT::GroupAddRobot>(OneEvent::onGroupInviteMeRequestReceived);
    qBot->registerDispatchAction<QDT::GroupDelRobot>(OneEvent::onGroupMemberKickMeNoticeReceived);
    qBot->registerDispatchAction<QDT::FriendAdd>(OneEvent::onFriendAddNoticeReceived);
    qBot->registerDispatchAction<QDT::FriendDel>(OneEvent::onFriendDelNoticeReceived);
    qBot->registerDispatchAction<QDT::GroupMemberAdd>(OneEvent::onGroupMemberIncreaseNoticeReceived);
    qBot->registerDispatchAction<QDT::GroupMemberRemove>(OneEvent::onGroupMemberLeaveNoticeReceived);
    qBot->registerDispatchAction<QDT::GroupJoinRequest>(OneEvent::onGroupMemberJoinRequestReceived);
}

namespace onebot {

    Instance::Instance() = default;

    Instance::~Instance() = default;

    void Instance::initAndStart(const Json::Value& config)
    {
        SPDLOG_WARN(ONEBOT_TAG "init");
        m_pImpl = std::make_unique<Impl>();
        InitAndConnect(config);
        RegisterDispatchActions();
    }

    void Instance::shutdown()
    {
        SPDLOG_WARN(ONEBOT_TAG "down");
    }

    void Instance::dispatch(const std::shared_ptr<void>& eventVariant) const
    {
        auto data = std::static_pointer_cast<Event::Variant>(eventVariant);
        m_pImpl->dispatch(data);
    }

    std::string Instance::getCacheId(const std::uint64_t sceneId)
    {
        return m_pImpl->getCacheId(sceneId);
    }

    inline void Instance::Impl::dispatch(const std::shared_ptr<Event::Variant>& data)
    {
        for (auto&& url : urlArray)
        {
            auto client = clientCache[url];
            client->getLoop()->runInLoop([client, data] {
                if (auto connection = client->getConnection(); connection != nullptr && connection->connected())
                {
                    Send(connection, *data);
                }
            });
        }
    }

    template<qbot::IdBucketConstants constants>
    inline void Instance::Impl::cacheId(std::uint64_t sceneId, std::uint32_t id, const std::string& idStr, IdCache& idcahe)
    {
        if (!idQueueCache.find(sceneId))
        {
            idQueueCache.insert(sceneId, IdQueue{});
        }
        idQueueCache.modify(sceneId, [sceneId, id, this, &idStr, &idcahe](IdQueue& val) {
            auto bucket = std::array<std::uint32_t, constants.capacity‌>{ id };
            val.enqueue_bulk(bucket.begin(), constants.capacity‌);
            idcahe.insert(id, idStr, constants.timeout);
        });
    }

    template<Event::Concept T>
    inline void Instance::Impl::cacheId(T&& data)
    {
        using TX = std::decay_t<T>;
        if constexpr (std::same_as<TX, Event::PrivateMsg>)
        {
            cacheId<qbot::C2CIdBucketConstants>(data.user_id, data.message_id, data.inter_ext.message_openid, messageIdCache);
        }
        else if constexpr (std::same_as<TX, Event::GroupMsg>)
        {
            cacheId<qbot::GroupIdBucketConstants>(data.group_id, data.message_id, data.inter_ext.message_openid, messageIdCache);
        }
        else if constexpr (std::same_as<TX, Event::FriendAddNotice>)
        {
            cacheId<qbot::C2CIdBucketConstants>(data.user_id, data.inter_ext.event_id, data.inter_ext.event_openid, eventIdCache);
        }
        else if constexpr (std::same_as<TX, Event::GroupIncreaseNotice>)
        {
            cacheId<qbot::GroupIdBucketConstants>(data.group_id, data.inter_ext.event_id, data.inter_ext.event_openid, eventIdCache);
        }
    }

    inline std::string Instance::Impl::getCacheId(const std::uint64_t sceneId)
    {
        auto ret = std::string{};
        idQueueCache.modify(sceneId, [this,&ret](IdQueue& idQueue) {
            auto val = std::uint32_t{};
            if (!idQueue.try_dequeue(val))
            {
                return;
            }
            if (eventIdCache.findAndFetch(val, ret) || messageIdCache.findAndFetch(val, ret))
            {
                return;
            }
            SPDLOG_DEBUG(ONEBOT_TAG "Empty ID cache! Will cost active capacity!");
        });
        return ret;
    }
}
