#pragma once
#include <drogon/plugins/Plugin.h>
#include <nlohmann/json_fwd.hpp>
#include <drogon/utils/coroutine.h>
#include <fixed_string.hpp>

namespace qbot {

    typedef nlohmann::json(*DispatchAction)(const nlohmann::json& data);

    class Instance : public drogon::Plugin<Instance>
    {
    public:
        Instance();
        virtual ~Instance();
        void initAndStart(const Json::Value& config) override;
        void shutdown() override;
    public:
        std::string getAppId() const;

        template<fixstr::fixed_string type>
        void registerDispatchAction(DispatchAction&& action);

        drogon::Task<nlohmann::json> sendC2CMessageAsync(const nlohmann::json& payload, const std::string& openId) const;

        drogon::Task<nlohmann::json> sendGroupMessageAsync(const nlohmann::json& payload, const std::string& openId) const;

        struct Impl;
        std::unique_ptr<Impl> m_pImpl;
    private:
        void export_functions();
    };
}
