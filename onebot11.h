#pragma once
#include <nlohmann/json.hpp>

namespace onebot {
    namespace Event {
        struct PrivateMsg
        {
            static constexpr std::string_view post_type = "message";
            static constexpr std::string_view message_type = "private";

            enum SubType //消息子类型
            {
                FRIEND, // 好友
                GROUP,  // 群私聊
                OTHER   // 其他
            } sub_type;

            uint64_t time; // 消息发送时间
            uint64_t user_id; // 发送消息的人的QQ
            uint64_t self_id; // 机器人自身QQ
            uint32_t message_id; // 消息ID

            nlohmann::json message; //消息
            std::string raw_message; //原始文本消息（含有CQ码）

            struct OpenQQEXT
            {
                std::string user_openid;
                std::string message_openid;
            } open_qq_ext;
        };

        NLOHMANN_JSON_SERIALIZE_ENUM(PrivateMsg::SubType, {
            {PrivateMsg::SubType::FRIEND, "friend"},
            {PrivateMsg::SubType::GROUP, "group"},
            {PrivateMsg::SubType::OTHER, "other"},
        })

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(PrivateMsg::OpenQQEXT, user_openid, message_openid)
        
        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(PrivateMsg, post_type, sub_type, message_type, time, user_id, self_id, message_id, message, raw_message, open_qq_ext)

        struct GroupMsg
        {
            static constexpr std::string_view post_type = "message";
            static constexpr std::string_view message_type = "group";

            enum SubType //消息子类型
            {
                NORMAL,     // 正常消息
                ANONYMOUS,  // 系统消息
                NOTICE,     // 通知消息，如 管理员已禁止群内匿名聊天
            } sub_type;

            uint64_t time; // 消息发送时间
            uint64_t user_id; // 发送消息的人的QQ
            uint64_t self_id; // 机器人自身QQ
            uint64_t group_id; // 群QQ
            uint32_t message_id; // 消息ID

            nlohmann::json message; //信息
            std::string raw_message; //原始文本消息（含有CQ码）
            std::string group_name; // 群的名称

            struct Sender
            {
                std::string nickname; // 昵称

                enum Role // 权限级别
                {
                    OWNER,// 群主
                    ADMIN,// 管理员
                    MEMBER// 普通群成员
                } role;
            }sender;

            struct OpenQQEXT
            {
                std::string user_openid;
                std::string group_openid;
                std::string message_openid;
            } open_qq_ext;
        };

        NLOHMANN_JSON_SERIALIZE_ENUM(GroupMsg::SubType, {
            {GroupMsg::SubType::NORMAL, "normal"},
            {GroupMsg::SubType::ANONYMOUS, "anonymous"},
            {GroupMsg::SubType::NOTICE, "notice"},
        })

        NLOHMANN_JSON_SERIALIZE_ENUM(GroupMsg::Sender::Role, {
            {GroupMsg::Sender::Role::ADMIN, "admin"},
            {GroupMsg::Sender::Role::MEMBER, "member"},
            {GroupMsg::Sender::Role::OWNER, "owner"}
        })

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(GroupMsg::OpenQQEXT, user_openid, group_openid, message_openid)
        
        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(GroupMsg::Sender, nickname, role)

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(GroupMsg, post_type, sub_type, message_type, time, user_id, self_id, group_id, message_id, message, raw_message, sub_type, sender, open_qq_ext)

        struct LifecycleEvent
        {
            static constexpr std::string_view post_type = "meta_event";
            static constexpr std::string_view meta_event_type = "lifecycle";

            enum SubType // 生命周期子类型
            {
                ENABLE, // HTTP POST启用
                DISBALE,// HTTP POST停用
                CONNECT // Websocket已连接
            } sub_type;

            uint64_t time; // 事件产生的时间
            uint64_t self_id; // 机器人自身QQ
        };

        NLOHMANN_JSON_SERIALIZE_ENUM(LifecycleEvent::SubType, {
            {LifecycleEvent::SubType::ENABLE, "enable"},
            {LifecycleEvent::SubType::DISBALE, "disable"},
            {LifecycleEvent::SubType::CONNECT, "connect"}
        })

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(LifecycleEvent, post_type, meta_event_type, sub_type, time, self_id)

        struct HeartbeatEvent
        {
            static constexpr std::string_view post_type = "meta_event";
            static constexpr std::string_view meta_event_type = "heartbeat";

            uint64_t time; // 事件产生的时间
            uint64_t self_id; // 机器人自身QQ

            struct Status {
                bool online; // 在线
                bool good; // 状态符合预期，意味着各模块正常运行、功能正常，且在线
            }status; // 状态信息

            uint64_t interval; // 心跳周期(ms)
        };

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(HeartbeatEvent::Status, online, good)

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(HeartbeatEvent, post_type, meta_event_type, time, self_id)

        struct GroupUploadNotice
        {
            static constexpr std::string_view post_type = "notice";
            static constexpr std::string_view notice_type = "group_upload";

            uint64_t time; // 事件产生的时间
            uint64_t self_id; // 机器人自身QQ
            uint64_t group_id; // 群QQ
            uint64_t user_id; // 上传文件的人的QQ

            struct File // 上传的文件信息
            {
                std::string name; // 文件名
                uint64_t size;  // 文件大小(byte)
                std::string url; // 文件地址
            } file;

            struct OpenQQEXT
            {
                std::string group_openid;
                std::string user_openid;
            } open_qq_ext;
        };

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(GroupUploadNotice::File, name, size, url)

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(GroupUploadNotice::OpenQQEXT, group_openid, user_openid)

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(GroupUploadNotice, post_type, notice_type, time, user_id, self_id, group_id, file, open_qq_ext)

        struct GroupDecreaseNotice
        {
            static constexpr std::string_view post_type = "notice";
            static constexpr std::string_view notice_type = "group_decrease";

            uint64_t time; // 事件产生的时间
            uint64_t self_id; // 机器人自身QQ
            uint64_t group_id; // 群QQ
            uint64_t user_id; // 用户QQ
            uint64_t operator_id; // 操作者QQ 如果是主动退群，和user_id一致

            enum SubType // 事件子类型
            {
                LEAVE,      // 退出
                KICK,       // 被踢出
                KICK_ME,    // 机器人被踢出
            } sub_type;

            struct OpenQQEXT
            {
                std::string group_openid;
                std::string user_openid;
            } open_qq_ext;
        };

        NLOHMANN_JSON_SERIALIZE_ENUM(GroupDecreaseNotice::SubType, {
            {GroupDecreaseNotice::SubType::LEAVE, "leave"},
            {GroupDecreaseNotice::SubType::KICK, "kick"},
            {GroupDecreaseNotice::SubType::KICK_ME, "kick_me"}
        })

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(GroupDecreaseNotice::OpenQQEXT, group_openid, user_openid)

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(GroupDecreaseNotice, post_type, notice_type, time, user_id, self_id, group_id, operator_id, sub_type, open_qq_ext)

        struct GroupIncreaseNotice
        {
            static constexpr std::string_view post_type = "notice";
            static constexpr std::string_view notice_type = "group_increase";

            uint64_t time; // 事件产生的时间
            uint64_t self_id; // 机器人自身QQ
            uint64_t group_id; // 群QQ
            uint64_t user_id; // 用户QQ
            uint64_t operator_id; // 操作者QQ 如果是主动加群，和user_id一致

            enum SubType // 事件子类型
            {
                APPROVE, // 同意入群
                INVITE,  // 邀请入群
            } sub_type;
            
            struct OpenQQEXT
            {
                std::string group_openid;
                std::string user_openid;
            } open_qq_ext;
        };

        NLOHMANN_JSON_SERIALIZE_ENUM(GroupIncreaseNotice::SubType, {
            {GroupIncreaseNotice::SubType::APPROVE, "approve"},
            {GroupIncreaseNotice::SubType::INVITE, "invite"}
        })

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(GroupIncreaseNotice::OpenQQEXT, group_openid, user_openid)

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(GroupIncreaseNotice, post_type, notice_type, time, user_id, self_id, group_id, operator_id, sub_type, open_qq_ext)

        struct FriendAddNotice
        {
            static constexpr std::string_view post_type = "notice";
            static constexpr std::string_view notice_type = "friend_add";

            uint64_t time; // 事件产生的时间
            uint64_t self_id; // 机器人自身QQ
            uint64_t user_id; // 新添加好友 QQ 号

            struct OpenQQEXT
            {
                std::string user_openid;
            } open_qq_ext;
        };

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(FriendAddNotice::OpenQQEXT, user_openid)

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(FriendAddNotice, post_type, notice_type, time, user_id, self_id, open_qq_ext)

        struct FriendDelNotice
        {
            static constexpr std::string_view post_type = "notice";
            static constexpr std::string_view notice_type = "friend_del";

            uint64_t time; // 事件产生的时间
            uint64_t self_id; // 机器人自身QQ
            uint64_t user_id; // 用户 QQ 号

            struct OpenQQEXT
            {
                std::string user_openid;
            } open_qq_ext;
        };

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(FriendDelNotice::OpenQQEXT, user_openid)

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(FriendDelNotice, post_type, notice_type, time, user_id, self_id, open_qq_ext)

        struct GroupJoinRequest
        {
            static constexpr std::string_view post_type = "request";
            static constexpr std::string_view request_type = "group";

            enum SubType // 事件子类型
            {
                ADD, // 加群请求
                INVITE // 邀请登录号入群
            }sub_type;

            uint64_t time; // 事件发生的时间戳
            uint64_t self_id; // 收到事件的机器人 QQ 号
            uint64_t group_id; // 群号
            uint64_t user_id; // 发送请求的 QQ 号
            std::string comment; // 验证信息
            std::string flag; // 请求 flag，在调用处理请求的 API 时需要传入

            struct OpenQQEXT
            {
                std::string group_openid;
                std::string user_openid;
            } open_qq_ext;
        };

        NLOHMANN_JSON_SERIALIZE_ENUM(GroupJoinRequest::SubType, {
            {GroupJoinRequest::SubType::ADD, "add"},
            {GroupJoinRequest::SubType::INVITE, "invite"}
        })

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(GroupJoinRequest::OpenQQEXT, group_openid, user_openid)

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(GroupJoinRequest, sub_type, time, self_id, group_id, user_id, comment, flag, open_qq_ext)
    }

    namespace API {
        nlohmann::json ref_message(nlohmann::json data)
        {
            return { "message_reference", {{"message_id",data["d"]["message_scene"]["ext"].at(data["d"].contains("msg_elements")).get<std::string_view>().substr(sizeof("msg_idx=") - 1)}} };
        }
    }
}