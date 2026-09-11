#pragma once
#include <drogon/plugins/Plugin.h>
#include <drogon/CacheMap.h>
#include <drogon/WebSocketClient.h>
#include "onebot_Event.h"

namespace onebot 
{
    using ClientCache = drogon::CacheMap<std::string, drogon::WebSocketClientPtr>;
    using IdCache = drogon::CacheMap<std::uint32_t, std::string>;
    using IdMessageIdMap = drogon::CacheMap<std::uint64_t, std::deque<std::uint32_t>>;

    class Instance : public drogon::Plugin<Instance>
    {
    public:
        Instance() {}
        void initAndStart(const Json::Value& config) override;
        void shutdown() override;
    public:
        ClientCache& clientCache();
        std::vector<std::string>& urlArray();
        void dispatch(std::shared_ptr<onebot::Event::Variant> data);
        void cacheEventId(std::uint64_t sceneId, std::uint32_t eventId, std::string_view eventIdStr);
        void cacheMessageId(std::uint64_t sceneId, std::uint32_t messageId, std::string_view messageIdStr);
    private:
        std::unique_ptr<ClientCache> m_clientCache;
        std::unique_ptr<IdCache> m_eventIdCache;
        std::unique_ptr<IdCache> m_messageIdCache;
        std::unique_ptr<IdMessageIdMap> m_idMessageIdMap;
        std::vector<std::string> m_urlArray{};
    };
}
