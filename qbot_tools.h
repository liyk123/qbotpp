#pragma once
#include <drogon/WebSocketClient.h>
#include <drogon/drogon.h>
#include <nlohmann/json.hpp>

namespace qbot {
    using WSAsyncMessageHandler = std::function<drogon::Task<void>(std::string&&, const drogon::WebSocketClientPtr&, const drogon::WebSocketMessageType&)>;
    using WSMessageHandler = std::function<void(std::string&&, const drogon::WebSocketClientPtr&, const drogon::WebSocketMessageType&)>;
    using WSAsyncClosedHandler = std::function<drogon::Task<void>(const drogon::WebSocketClientPtr&)>;
    using WSClosedHandler = std::function<void(const drogon::WebSocketClientPtr&)>;

    template<size_t N>
    struct FixedString
    {
        char data[N]{};

        constexpr FixedString(const char(&str)[N])
        {
            std::copy_n(str, N, data);
        }

        constexpr FixedString() = default;

        constexpr size_t length() const { return N - 1; }
    };

    typedef nlohmann::json(*DispatchAction)(const nlohmann::json& data);

    struct DispatchType
    {
        // 登录成功
        static constexpr FixedString Ready = "READY";
        // 重连成功
        static constexpr FixedString Resumed = "RESUMED";
        // 用户单聊发消息给机器人
        static constexpr FixedString C2CMessageCreate = "C2C_MESSAGE_CREATE";
        // 用户添加使用机器人
        static constexpr FixedString FriendAdd = "FRIEND_ADD";
        // 用户删除机器人
        static constexpr FixedString FriendDel = "FRIEND_DEL";
        // 用户在机器人资料卡手动关闭"主动消息"推送
        static constexpr FixedString C2CMsgReject = "C2C_MSG_REJECT";
        // 用户在机器人资料卡手动开启"主动消息"推送开关
        static constexpr FixedString C2CMsgReceived = "C2C_MSG_RECEIVE";
        // 用户在群里@机器人时收到的消息
        static constexpr FixedString GroupAtMessageCreate = "GROUP_AT_MESSAGE_CREATE";
        // 机器人被添加到群聊
        static constexpr FixedString GroupAddRobot = "GROUP_ADD_ROBOT";
        // 机器人被移出群聊
        static constexpr FixedString GroupDelRobot = "GROUP_DEL_ROBOT";
        // 群管理员主动在机器人资料页操作关闭通知
        static constexpr FixedString GroupMsgReject = "GROUP_MSG_REJECT";
        // 群管理员主动在机器人资料页操作开启通知
        static constexpr FixedString GroupMsgReceive = "GROUP_MSG_RECEIVE";
        // 机器人收到了群聊消息
        static constexpr FixedString GroupMessageCreate = "GROUP_MESSAGE_CREATE";
        // 群用户添加
        static constexpr FixedString GroupMemberAdd = "GROUP_MEMBER_ADD";
        // 群用户移除
        static constexpr FixedString GroupMemberRemove = "GROUP_MEMBER_REMOVE";
        // 用户申请加群
        static constexpr FixedString GroupJoinRequest = "GROUP_JOIN_REQUEST";
    };

    template<FixedString T, DispatchAction F>
    struct Dispatcher
    {
        static constexpr std::string_view type = T.data;
        static constexpr DispatchAction action = F;
    };

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

    drogon::WebSocketClientPtr ConnectToWSServer(const std::string url, const WSMessageHandler& messageHandler, const WSClosedHandler& closedHandler);
}

namespace drogon {
    template<>
    HttpRequestPtr toRequest(qbot::JsonMethod&& obj);

    template<>
    HttpRequestPtr toRequest(const qbot::JsonMethod& obj);

    template<>
    HttpRequestPtr toRequest(qbot::JsonMethod& obj);

    template<>
    HttpRequestPtr toRequest(nlohmann::json&& obj);

    template<>
    HttpRequestPtr toRequest(const nlohmann::json& obj);

    template<>
    HttpRequestPtr toRequest(nlohmann::json& obj);

    template<>
    nlohmann::json fromResponse(const HttpResponse& resp);
}