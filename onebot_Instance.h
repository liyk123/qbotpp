#pragma once
#include <drogon/plugins/Plugin.h>
#include "qbot_tools.h"

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
    private:
        qbot::ClientCache m_clientCache{drogon::app().getLoop()};
    };
}
