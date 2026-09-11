#include "qbot_Echo.h"
#include "qbot_Instance.h"
#include <drogon/HttpAppFramework.h>

static nlohmann::json DispatchC2CMessageCreate(const nlohmann::json& data)
{
    drogon::app().getLoop()->queueInLoop(drogon::async_func([data]() -> drogon::Task<> {
        nlohmann::json payload{
            {"markdown", {{"content", data["d"]["content"]}}},
            {"msg_type", 2},
            {"msg_id", data["d"]["id"]}
        };
        auto& userOpenId = data["d"]["author"]["user_openid"];
        co_await drogon::app().getPlugin<qbot::Instance>()->sendC2CMessageAsync(payload, userOpenId);
    }));
    return {};
}

static nlohmann::json DispatchGroupMessageCreate(const nlohmann::json& data)
{
    drogon::app().getLoop()->queueInLoop(drogon::async_func([data]() -> drogon::Task<> {
        nlohmann::json payload{
            {"markdown", {{"content", data["d"]["content"]}}},
            {"msg_type", 2},
            {"msg_id", data["d"]["id"]}
        };
        auto& userOpenId = data["d"]["group_openid"];
        co_await drogon::app().getPlugin<qbot::Instance>()->sendGroupMessageAsync(payload, userOpenId);
    }));
    return {};
}

static nlohmann::json DispatchGroupAtMessageCreate(const nlohmann::json& data)
{
    drogon::app().getLoop()->queueInLoop(drogon::async_func([data]() -> drogon::Task<> {
        nlohmann::json payload{
            {"markdown", {{"content", data["d"]["content"]}}},
            {"msg_type", 2},
            {"msg_id", data["d"]["id"]}
        };
        auto& userOpenId = data["d"]["group_openid"];
        co_await drogon::app().getPlugin<qbot::Instance>()->sendGroupMessageAsync(payload, userOpenId);
    }));
    return {};
}

namespace qbot {

    void Echo::initAndStart(const Json::Value& config)
    {
        LOG_WARN << "init";
        auto instance = drogon::app().getPlugin<Instance>();
        instance->registerDispatchAction<DispatchType::C2CMessageCreate>(DispatchC2CMessageCreate);
        instance->registerDispatchAction<DispatchType::GroupAtMessageCreate>(DispatchGroupMessageCreate);
        instance->registerDispatchAction<DispatchType::GroupMessageCreate>(DispatchGroupMessageCreate);
    }

    void Echo::shutdown()
    {
        LOG_WARN << "down";
    }
}