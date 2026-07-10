#pragma once
#include <variant>
#include <nlohmann/json.hpp>

namespace Onebot {
    namespace V11 {
        //struct EventType
        //{
        //    std::string_view post_type;
        //    std::string_view sub_type;

        //    bool operator==(const EventType& other) const
        //    {
        //        return post_type == other.post_type && sub_type == other.sub_type;
        //    }
        //};

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
                uint32_t msg_id; // 消息ID

                std::string message; //消息
                std::string raw_message; //原始文本消息（含有CQ码）

                struct Sender
                {
                    std::string openid;// QQ开放平台ID
                }sender;
            };

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

                std::string message; //信息
                std::string raw_message; //原始文本消息（含有CQ码）
                std::string group_name; // 群的名称

                struct Sender
                {
                    std::string openid; // QQ开放平台ID
                    std::string nickname; // 昵称

                    enum Role // 权限级别
                    {
                        OWNER,// 群主
                        ADMIN,// 管理员
                        MEMBER// 普通群成员
                    } role;
                }sender;
            };

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

            struct HeartbeatEvent
            {
                static constexpr std::string_view post_type = "meta_event";
                static constexpr std::string_view meta_event_type = "heartbeat";

                uint64_t time; // 事件产生的时间
                uint64_t self_id; // 机器人自身QQ

                nlohmann::json status; // 状态信息
                uint64_t interval; // 心跳周期(ms)
            };

            struct GroupUploadNotice
            {
                static constexpr std::string_view post_type = "notice";
                static constexpr std::string_view meta_event_type = "group_upload";

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
            };

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
            };

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
            };

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
                std::string openid; // QQ开放平台ID
            };

            struct FriendDelNotice
            {
                static constexpr std::string_view post_type = "notice";
                static constexpr std::string_view notice_type = "friend_del";

                uint64_t time; // 事件产生的时间
                uint64_t self_id; // 机器人自身QQ
                uint64_t user_id; // 用户 QQ 号
                std::string openid; // QQ开放平台ID
            };

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
    }
}