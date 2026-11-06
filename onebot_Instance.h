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
        std::string getCacheId(const std::uint64_t sceneId);
        drogon::Task<std::string> getOpenIdAsync(const std::uint64_t sceneId);
        struct Impl;
        std::unique_ptr<Impl> m_pImpl;
    };
}
