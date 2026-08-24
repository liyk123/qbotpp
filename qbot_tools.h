#pragma once
#include <drogon/WebSocketClient.h>
#include <drogon/drogon.h>
#include <nlohmann/json.hpp>

namespace qbot {
    using WSMessageHandler = std::function<void(std::string&&, const drogon::WebSocketClientPtr&, const drogon::WebSocketMessageType&)>;
    using WSClosedHandler = std::function<void(const drogon::WebSocketClientPtr&)>;
    using ClientCache = drogon::CacheMap<std::string, drogon::WebSocketClientPtr>;
    using MessageCache = drogon::CacheMap<std::uint32_t, std::string>;

    enum class opcode : std::int32_t
    {
        // 服务端进行消息推送
        Dispatch = 0,
        // 客户端或服务端发送心跳
        Heartbeat = 1,
        // 客户端发送鉴权
        Identify = 2,
        // 客户端恢复连接
        Resume = 6,
        // 服务端通知客户端重新连接
        Reconnect = 7,
        // 当identify或resume的时候，如果参数有错，服务端会返回该消息
        Invalid = 9,
        // 当客户端与网关建立ws连接之后，网关下发的第一条消息
        Hello = 10,
        // 当发送心跳成功之后，就会收到该消息
        HeartbeatACK = 11,
        // 仅用于 http 回调模式的回包，代表机器人收到了平台推送的数据
        HTTPCallbackACK = 12,
        // 开放平台对机器人服务端进行验证
        CallbackAuth = 13
    };

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