#pragma once
#include <drogon/plugins/Plugin.h>
#include "qbot_tools.h"
#include "onebot_Event.h"

namespace onebot
{
    class Instance : public drogon::Plugin<Instance>
    {
    public:
        Instance() {}
        void initAndStart(const Json::Value& config) override;
        void shutdown() override;
    public:
        qbot::ClientCache& clientCache();
        std::vector<std::string>& urlArray();
        void dispatch(std::shared_ptr<onebot::Event::Variant> data);
    private:
        qbot::ClientCache m_clientCache{drogon::app().getLoop()};
        std::vector<std::string> m_urlArray{};
    };
}
