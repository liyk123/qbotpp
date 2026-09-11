#pragma once
#include <drogon/plugins/Plugin.h>
#include <drogon/CacheMap.h>
#include <drogon/WebSocketClient.h>

namespace qbot {
    struct SceneConstants;
}

namespace onebot 
{
    using ClientCache = drogon::CacheMap<std::string, drogon::WebSocketClientPtr>;

    class Instance : public drogon::Plugin<Instance>
    {
    public:
        Instance() {}
        void initAndStart(const Json::Value& config) override;
        void shutdown() override;
    public:
        ClientCache& clientCache();
        void insertUrl(std::string_view urlView);
        void dispatch(const std::shared_ptr<void> &data);
        void cacheEventId(std::uint64_t sceneId, std::uint32_t eventId, std::string_view eventIdStr, qbot::SceneConstants constants);
        void cacheMessageId(std::uint64_t sceneId, std::uint32_t messageId, std::string_view messageIdStr, qbot::SceneConstants constants);
    private:
        struct Impl;
        std::shared_ptr<Impl> m_impl;
    };
}
