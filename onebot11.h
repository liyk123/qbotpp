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

        //struct GroupAdminNotice
        //{
        //    static constexpr EventType getType()
        //    {
        //        return { "notice", "group_admin" };
        //    }

        //    uint64_t time; // 事件产生的时间
        //    uint64_t self_id; // 机器人自身QQ
        //    uint64_t group_id; // 群QQ
        //    uint64_t user_id; // 管理员的QQ

        //    enum SUB_TYPE // 事件子类型
        //    {
        //        SET,   // 设置
        //        UNSET, // 取消设置
        //    } sub_type; 
        //};

        struct GroupDecreaseNotice
        {
            static constexpr std::string_view post_type = "notice";
            static constexpr std::string_view notice_type = "group_decrease";

            uint64_t time; // 事件产生的时间
            uint64_t self_id; // 机器人自身QQ
            uint64_t group_id; // 群QQ
            uint64_t user_id; // 用户QQ
            uint64_t operator_id; // 操作者QQ 如果是主动退群，和user_id一致

            enum SUB_TYPE // 事件子类型
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

        NLOHMANN_JSON_SERIALIZE_ENUM(GroupDecreaseNotice::SUB_TYPE, {
            {GroupDecreaseNotice::SUB_TYPE::LEAVE, "leave"},
            {GroupDecreaseNotice::SUB_TYPE::KICK, "kick"},
            {GroupDecreaseNotice::SUB_TYPE::KICK_ME, "kick_me"}
        })

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(GroupDecreaseNotice::OpenQQEXT, group_openid, user_openid)

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(GroupDecreaseNotice, post_type, notice_type, time, user_id, self_id, group_id, operator_id, sub_type, open_qq_ext)

        struct GroupInceaseNotice
        {
            static constexpr std::string_view post_type = "notice";
            static constexpr std::string_view notice_type = "group_increase";

            uint64_t time; // 事件产生的时间
            uint64_t self_id; // 机器人自身QQ
            uint64_t group_id; // 群QQ
            uint64_t user_id; // 用户QQ
            uint64_t operator_id; // 操作者QQ 如果是主动加群，和user_id一致

            enum SUB_TYPE // 事件子类型
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

        NLOHMANN_JSON_SERIALIZE_ENUM(GroupInceaseNotice::SUB_TYPE, {
            {GroupInceaseNotice::SUB_TYPE::APPROVE, "approve"},
            {GroupInceaseNotice::SUB_TYPE::INVITE, "invite"}
        })

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(GroupInceaseNotice::OpenQQEXT, group_openid, user_openid)

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(GroupInceaseNotice, post_type, notice_type, time, user_id, self_id, group_id, operator_id, sub_type, open_qq_ext)

        //struct GroupBanNotice
        //{
        //    static constexpr EventType getType()
        //    {
        //        return { "notice", "group_ban" };
        //    }

        //    uint64_t time; // 事件产生的时间
        //    uint64_t self_id; // 机器人自身QQ
        //    uint64_t group_id; // 群QQ
        //    uint64_t user_id; // 被禁言的人的QQ
        //    uint64_t operator_id; // 操作者QQ 如果是主动禁言，和user_id一致
        //    uint64_t duration; //禁言时长，单位秒

        //    enum SUB_TYPE // 事件子类型
        //    {
        //        BAN,      // 禁言
        //        LIFT_BAN, // 解除禁言
        //    } sub_type; 
        //};

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

        // 群消息撤回事件
        //struct GroupRecallNotice
        //{
        //    static constexpr EventType getType()
        //    {
        //        return { "notice", "group_recall" };
        //    }

        //    uint64_t time; // 事件产生的时间
        //    uint64_t self_id; // 机器人自身QQ
        //    uint64_t group_id; // 群QQ
        //    uint64_t message_id; // 消息ID
        //    uint64_t user_id; // 发送者QQ
        //    uint64_t operator_id; // 操作者QQ
        //};

        // 好友消息撤回事件
        //struct FriendRecallNotice
        //{
        //    static constexpr EventType getType()
        //    {
        //        return { "notice", "friend_recall" };
        //    }

        //    uint64_t time; // 事件产生的时间
        //    uint64_t self_id; // 机器人自身QQ
        //    uint64_t user_id; // 发送者QQ
        //    uint64_t message_id; // 消息ID
        //};

        // 群内通知事件，如戳一戳、群红包运气王、群成员荣誉变更
        //struct GroupNotifyNotice
        //{
        //    static constexpr EventType getType()
        //    {
        //        return { "notice", "group_notify" };
        //    }

        //    uint64_t time; // 事件产生的时间
        //    uint64_t self_id; // 机器人自身QQ
        //    uint64_t group_id; // 群QQ
        //    uint64_t user_id; // 发送者QQ,如戳一戳的发送者，红包的发送者，荣誉变更者
        //    enum SUB_TYPE
        //    {
        //        POKE, //戳一戳
        //        LUCKY_KING, //群红包运气王
        //        HONOR, //群成员荣誉变更
        //    } sub_type; // 事件子类型，分别表示戳一戳、群红包运气王、群成员荣誉变更
        //    std::optional<uint64_t> target_id = std::nullopt; // 如果是戳一戳，则为被戳的人的QQ，如果是群红包运气王，则为群红包的ID
        //    enum HonorType
        //    {
        //        TALKATIVE, // 龙王
        //        PERFORMER, // 群聊之火
        //        EMOTION,   // 快乐源泉
        //    };
        //    std::optional<HonorType> honor_type = std::nullopt; // 荣誉类型
        //};
    }

    namespace API {
        nlohmann::json ref_message(nlohmann::json data)
        {
            return { "message_reference", {{"message_id",data["d"]["message_scene"]["ext"].at(data["d"].contains("msg_elements")).get<std::string_view>().substr(sizeof("msg_idx=") - 1)}} };
        }
    }
}