#pragma once
#include <drogon/WebSocketClient.h>
#include <drogon/HttpTypes.h>
#include <nlohmann/json_fwd.hpp>
#include <variant>
#include <span>
#include <fixed_string.hpp>

namespace tools {
    using WSMessageHandler = std::function<void(std::string&&, const drogon::WebSocketClientPtr&, const drogon::WebSocketMessageType&)>;
    using WSClosedHandler = std::function<void(const drogon::WebSocketClientPtr&)>;
    drogon::WebSocketClientPtr ConnectToWSServer(const std::string url, const WSMessageHandler& messageHandler, const WSClosedHandler& closedHandler, const drogon::WebSocketRequestCallback& requestCallback, const std::span<std::pair<std::string, std::string>>& headers = {});

    template <drogon::HttpMethod method>
    using HttpMethodType = std::integral_constant<drogon::HttpMethod, method>;

    using HttpMethodVariant = std::variant<
        HttpMethodType<drogon::Get>,
        HttpMethodType<drogon::Post>,
        HttpMethodType<drogon::Delete>,
        HttpMethodType<drogon::Patch>,
        HttpMethodType<drogon::Put>
    >;
    
    using JsonMethod = std::pair<nlohmann::json, HttpMethodVariant>;
    
    drogon::Task<bool> isIntranet(const std::string_view url);

    drogon::Task<drogon::HttpResponsePtr> SendHttpRequestAsync(const std::string& url, const nlohmann::json& data, const HttpMethodVariant method);
}

namespace qbot {
    typedef nlohmann::json(*DispatchAction)(const nlohmann::json& data);

    namespace DispatchType {
        // 登录成功
        static constexpr fixstr::fixed_string Ready = "READY";
        // 重连成功
        static constexpr fixstr::fixed_string Resumed = "RESUMED";

        inline namespace GroupAndC2CEvent {
            // 用户单聊发消息给机器人
            static constexpr fixstr::fixed_string C2CMessageCreate = "C2C_MESSAGE_CREATE";
            // 用户添加使用机器人
            static constexpr fixstr::fixed_string FriendAdd = "FRIEND_ADD";
            // 用户删除机器人
            static constexpr fixstr::fixed_string FriendDel = "FRIEND_DEL";
            // 用户在机器人资料卡手动关闭"主动消息"推送
            static constexpr fixstr::fixed_string C2CMsgReject = "C2C_MSG_REJECT";
            // 用户在机器人资料卡手动开启"主动消息"推送开关
            static constexpr fixstr::fixed_string C2CMsgReceived = "C2C_MSG_RECEIVE";
            // 用户在群里@机器人时收到的消息
            static constexpr fixstr::fixed_string GroupAtMessageCreate = "GROUP_AT_MESSAGE_CREATE";
            // 机器人被添加到群聊
            static constexpr fixstr::fixed_string GroupAddRobot = "GROUP_ADD_ROBOT";
            // 机器人被移出群聊
            static constexpr fixstr::fixed_string GroupDelRobot = "GROUP_DEL_ROBOT";
            // 群管理员主动在机器人资料页操作关闭通知
            static constexpr fixstr::fixed_string GroupMsgReject = "GROUP_MSG_REJECT";
            // 群管理员主动在机器人资料页操作开启通知
            static constexpr fixstr::fixed_string GroupMsgReceive = "GROUP_MSG_RECEIVE";
            // 机器人收到了群聊消息
            static constexpr fixstr::fixed_string GroupMessageCreate = "GROUP_MESSAGE_CREATE";
        }

        inline namespace GroupMemberEvent {
            // 群用户添加
            static constexpr fixstr::fixed_string GroupMemberAdd = "GROUP_MEMBER_ADD";
            // 群用户移除
            static constexpr fixstr::fixed_string GroupMemberRemove = "GROUP_MEMBER_REMOVE";
            // 用户申请加群
            static constexpr fixstr::fixed_string GroupJoinRequest = "GROUP_JOIN_REQUEST";
        }

        inline namespace Interaction {
            // 互动事件创建
            static constexpr fixstr::fixed_string InteractionCreate = "INTERACTION_CREATE";
        };
    };

    struct IdBucketConstants
    {
        std::uint32_t timeout;
        std::uint32_t capacity‌;
    };

    constexpr auto GroupIdBucketConstants = IdBucketConstants{ 5 * 60,5 };
    constexpr auto C2CIdBucketConstants = IdBucketConstants{ 60 * 60,4 };
}

namespace drogon {
    template<>
    HttpRequestPtr toRequest(tools::JsonMethod&& obj);

    template<>
    HttpRequestPtr toRequest(const tools::JsonMethod& obj);

    template<>
    HttpRequestPtr toRequest(tools::JsonMethod& obj);

    template<>
    HttpRequestPtr toRequest(nlohmann::json&& obj);

    template<>
    HttpRequestPtr toRequest(const nlohmann::json& obj);

    template<>
    HttpRequestPtr toRequest(nlohmann::json& obj);

    template<>
    nlohmann::json fromResponse(const HttpResponse& resp);
}