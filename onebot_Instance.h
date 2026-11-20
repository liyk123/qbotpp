#pragma once
#include <drogon/plugins/Plugin.h>
#include <drogon/utils/coroutine.h>

namespace onebot 
{
    class Instance : public drogon::Plugin<Instance>
    {
    public:
        Instance();
        virtual ~Instance();
        void initAndStart(const Json::Value& config) override;
        void shutdown() override;
    public:
        void dispatch(const std::shared_ptr<void> &data) const;
        std::string tryDequeueCacheId(const std::uint64_t sceneId);
        drogon::Task<std::string> getOpenIdAsync(const std::uint64_t sceneId);
        std::string findAndFetchMessageId(const std::uint32_t messageId);
        void mapMessageIdGroupId(const std::uint32_t messageId, const std::string& groupId);
        void mapMessageIdUserId(const std::uint32_t messageId, const std::string& userId);
        struct Impl;
        std::unique_ptr<Impl> m_pImpl;
    };
}
