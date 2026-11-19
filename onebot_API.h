#pragma once
#include <nlohmann/json_fwd.hpp>
#include <drogon/utils/coroutine.h>

namespace onebot {
    namespace API {
        using Result = drogon::Task<nlohmann::json>;

        /**
         * @brief 发送私聊消息 (`send_private_msg`)
         * @param user_id 对方 QQ 号
         * @param message 要发送的内容
         * @param auto_escape 消息内容是否作为纯文本发送（即不解析 CQ 码），只在 `message` 字段是字符串时有效
         * @return Result 响应数据包含以下字段：
         *
         * - `message_id` (`int32`): 消息 ID
         */
        Result sendPrivateMsg(uint64_t user_id, const std::string& message, bool auto_escape = false);

        /**
         * @brief 发送群消息 (`send_group_msg`)
         * @param group_id 群号
         * @param message 要发送的内容
         * @param auto_escape 消息内容是否作为纯文本发送（即不解析 CQ 码），只在 `message` 字段是字符串时有效
         * @return Result 响应数据包含以下字段：
         *
         * - `message_id` (`int32`): 消息 ID
         */
        Result sendGroupMsg(uint64_t group_id, const std::string& message, bool auto_escape = false);

        /**
         * @brief 发送消息 (`send_msg`)
         * @param message_type 消息类型，支持 `private`、`group`，分别对应私聊、群组；如不传入，则根据传入的 `*_id` 参数判断
         * @param user_id 对方 QQ 号（消息类型为 `private` 时需要）
         * @param group_id 群号（消息类型为 `group` 时需要）
         * @param message 要发送的内容
         * @param auto_escape 消息内容是否作为纯文本发送（即不解析 CQ 码），只在 `message` 字段是字符串时有效
         * @return Result 响应数据包含以下字段：
         *
         * - `message_id` (`int32`): 消息 ID
         */
        Result sendMsg(std::string message_type, uint64_t user_id, uint64_t group_id, const std::string& message, bool auto_escape = false);

        /**
         * @brief 撤回消息 (`delete_msg`)
         * @param message_id 消息 ID
         * @return Result 无响应数据
         */
        Result deleteMsg(uint32_t message_id);

        /**
         * @brief 获取消息 (`get_msg`)
         * @param message_id 消息 ID
         * @return Result 响应数据包含以下字段：
         *
         * - `time` (`int32`): 发送时间
         * - `message_type` (`string`): 消息类型，同消息事件
         * - `message_id` (`int32`): 消息 ID
         * - `real_id` (`int32`): 消息真实 ID
         * - `sender` (`object`): 发送人信息，同消息事件
         * - `message` (`message`): 消息内容
         */
        Result getMsg(uint32_t message_id);

        /**
         * @brief 获取合并转发消息 (`get_forward_msg`)
         * @param id 合并转发 ID
         * @return Result 响应数据包含以下字段：
         *
         * - `message` (`message`): 消息内容，使用消息的数组格式表示，数组中的消息段全部为 `node` 消息段
         */
        Result getForwardMsg(uint32_t id);

        /**
         * @brief 发送好友赞 (`send_like`)
         * @param user_id 对方 QQ 号
         * @param times 赞的次数，每个好友每天最多 10 次
         * @return Result 无响应数据
         */
        Result sendLike(uint64_t user_id, uint32_t times = 1);

        /**
         * @brief 群组踢人 (`set_group_kick`)
         * @param group_id 群号
         * @param user_id 要踢的 QQ 号
         * @param reject_add_request 拒绝此人的加群请求
         * @return Result 无响应数据
         */
        Result setGroupKick(uint64_t group_id, uint64_t user_id, bool reject_add_request = false);

        /**
         * @brief 群组单人禁言 (`set_group_ban`)
         * @param group_id 群号
         * @param user_id 要禁言的 QQ 号
         * @param duration 禁言时长，单位秒，`0` 表示取消禁言
         * @return Result 无响应数据
         */
        Result setGroupBan(uint64_t group_id, uint64_t user_id, uint32_t duration = 30 * 60);

        /**
         * @brief 群组匿名用户禁言 (`set_group_anonymous_ban`)
         * @details `anonymous` 和 `flag` 两者任选其一传入即可，若都传入，则使用 `anonymous`。
         * @param group_id 群号
         * @param anonymous 可选，要禁言的匿名用户对象（群消息上报的 `anonymous` 字段）
         * @param flag 可选，要禁言的匿名用户的 `flag`（需从群消息上报的数据中获得）
         * @param duration 禁言时长，单位秒，无法取消匿名用户禁言
         * @return Result 无响应数据
         */
        Result setGroupAnonymousBan(uint64_t group_id, const std::string& anonymous, const std::string& flag, uint32_t duration = 30 * 60);

        /**
         * @brief 群组全员禁言 (`set_group_whole_ban`)
         * @param group_id 群号
         * @param enable 是否禁言
         * @return Result 无响应数据
         */
        Result setGroupWholeBan(uint64_t group_id, bool enable = true);

        /**
         * @brief 群组设置管理员 (`set_group_admin`)
         * @param group_id 群号
         * @param user_id 要设置的管理员 QQ 号
         * @param enable 是否设置为管理员
         * @return Result 无响应数据
         */
        Result setGroupAdmin(uint64_t group_id, uint64_t user_id, bool enable = true);

        /**
         * @brief 群组匿名设置 (`set_group_anonymous`)
         * @param group_id 群号
         * @param enable 是否允许匿名聊天
         * @return Result 无响应数据
         */
        Result setGroupAnonymous(uint64_t group_id, bool enable = true);

        /**
         * @brief 群名片设置 / 群备注 (`set_group_card`)
         * @param group_id 群号
         * @param user_id 要设置的群名片 QQ 号
         * @param card 要设置的群名片内容
         * @return Result 无响应数据
         */
        Result setGroupCard(uint64_t group_id, uint64_t user_id, const std::string& card);

        /**
         * @brief 设置群名 (`set_group_name`)
         * @param group_id 群号
         * @param name 要设置的群名
         * @return Result 无响应数据
         */
        Result setGroupName(uint64_t group_id, const std::string& name);

        /**
         * @brief 退出群组 (`set_group_leave`)
         * @param group_id 群号
         * @param is_dismiss 是否解散该群组
         * @return Result 无响应数据
         */
        Result setGroupLeave(uint64_t group_id, bool is_dismiss = false);

        /**
         * @brief 设置群组专属头衔 (`set_group_special_title`)
         * @param group_id 群号
         * @param user_id 要设置的 QQ 号
         * @param special_title 专属头衔，不填或空字符串表示删除专属头衔
         * @param duration 专属头衔有效期，单位秒，`-1` 表示永久（不过此项似乎没有效果，可能是只有某些特殊的时间长度有效，有待测试）
         * @return Result 无响应数据
         */
        Result setGroupSpecialTitle(uint64_t group_id, uint64_t user_id, const std::string& special_title, int32_t duration = -1);

        /**
         * @brief 处理加好友请求 (`set_friend_add_request`)
         * @param flag 加好友请求的 `flag`（需从上报的数据中获得）
         * @param approve 是否同意请求
         * @param remark 添加后的好友备注（仅在同意时有效）
         * @return Result 无响应数据
         */
        Result setFriendAddRequest(const std::string& flag, bool approve = true, const std::string& remark = "");

        /**
         * @brief 处理加群请求／邀请 (`set_group_add_request`)
         * @param flag 加群请求的 `flag`（需从上报的数据中获得）
         * @param sub_type `add` 或 `invite`，请求类型（需要和上报消息中的 `sub_type` 字段相符）
         * @param approve 是否同意请求／邀请
         * @param reason 拒绝理由（仅在拒绝时有效）
         * @return Result 无响应数据
         */
        Result setGroupAddRequest(const std::string& flag, const std::string& sub_type, bool approve = true, const std::string& reason = "");

        /**
         * @brief 获取登录号信息 (`get_login_info`)
         * @return Result 响应数据包含以下字段：
         *
         * - `user_id` (`int64`): QQ 号
         * - `nickname` (`string`): QQ 昵称
         */
        Result getLoginInfo();

        /**
         * @brief 获取陌生人信息 (`get_stranger_info`)
         * @param user_id QQ 号
         * @param no_cache 是否不使用缓存（使用缓存可能更新不及时，但响应更快）
         * @return Result 响应数据包含以下字段：
         *
         * - `user_id` (`int64`): QQ 号
         * - `nickname` (`string`): 昵称
         * - `sex` (`string`): 性别，`male`、`female` 或 `unknown`
         * - `age` (`int32`): 年龄
         */
        Result getStrangerInfo(uint64_t user_id, bool no_cache = false);

        /**
         * @brief 获取好友列表 (`get_friend_list`)
         * @return Result 响应数据为 JSON 数组，每个元素包含以下字段：
         *
         * - `user_id` (`int64`): QQ 号
         * - `nickname` (`string`): 昵称
         * - `remark` (`string`): 备注名
         */
        Result getFriendList();

        /**
         * @brief 获取群信息 (`get_group_info`)
         * @param group_id 群号
         * @param no_cache 是否不使用缓存（使用缓存可能更新不及时，但响应更快）
         * @return Result 响应数据包含以下字段：
         *
         * - `group_id` (`int64`): 群号
         * - `group_name` (`string`): 群名称
         * - `member_count` (`int32`): 成员数
         * - `max_member_count` (`int32`): 最大成员数（群容量）
         */
        Result getGroupInfo(uint64_t group_id, bool no_cache = false);

        /**
         * @brief 获取群列表 (`get_group_list`)
         * @return Result 响应数据为 JSON 数组，每个元素结构与 `getGroupInfo()` 相同
         */
        Result getGroupList();

        /**
         * @brief 获取群成员信息 (`get_group_member_info`)
         * @param group_id 群号
         * @param user_id QQ 号
         * @param no_cache 是否不使用缓存（使用缓存可能更新不及时，但响应更快）
         * @return Result 响应数据包含以下字段：
         *
         * - `group_id` (`int64`): 群号
         * - `user_id` (`int64`): QQ 号
         * - `nickname` (`string`): 昵称
         * - `card` (`string`): 群名片／备注
         * - `sex` (`string`): 性别，`male`、`female` 或 `unknown`
         * - `age` (`int32`): 年龄
         * - `area` (`string`): 地区
         * - `join_time` (`int32`): 加群时间戳
         * - `last_sent_time` (`int32`): 最后发言时间戳
         * - `level` (`string`): 成员等级
         * - `role` (`string`): 角色，`owner`、`admin` 或 `member`
         * - `unfriendly` (`boolean`): 是否不良记录成员
         * - `title` (`string`): 专属头衔
         * - `title_expire_time` (`int32`): 专属头衔过期时间戳
         * - `card_changeable` (`boolean`): 是否允许修改群名片
         */
        Result getGroupMemberInfo(uint64_t group_id, uint64_t user_id, bool no_cache = false);

        /**
         * @brief 获取群成员列表 (`get_group_member_list`)
         * @note 对于同一个群组的同一个成员，获取列表时和获取单独的成员信息时，某些字段可能有所不同（例如 `area`、`title` 等字段在获取列表时无法获得），具体应以单独的成员信息为准。
         * @param group_id 群号
         * @return Result 响应数据为 JSON 数组，每个元素结构与 `getGroupMemberInfo()` 相同
         */
        Result getGroupMemberList(uint64_t group_id);

        /**
         * @brief 获取群荣誉信息 (`get_group_honor_info`)
         * @param group_id 群号
         * @param type 要获取的群荣誉类型，可传入 `talkative`、`performer`、`legend`、`strong_newbie`、`emotion` 以分别获取单个类型的群荣誉数据，或传入 `all` 获取所有数据
         * @return Result 响应数据包含以下字段：
         *
         * - `group_id` (`int64`): 群号
         * - `current_talkative` (`object`): 当前龙王（仅 `type` 为 `talkative` 或 `all` 时有数据），含 `user_id` (`int64`)、`nickname` (`string`)、`avatar` (`string`)、`day_count` (`int32`)
         * - `talkative_list` (`array`): 历史龙王（仅 `type` 为 `talkative` 或 `all` 时有数据），元素含 `user_id` (`int64`)、`nickname` (`string`)、`avatar` (`string`)、`description` (`string`)
         * - `performer_list` (`array`): 群聊之火（仅 `type` 为 `performer` 或 `all` 时有数据），元素结构同上
         * - `legend_list` (`array`): 群聊炽焰（仅 `type` 为 `legend` 或 `all` 时有数据），元素结构同上
         * - `strong_newbie_list` (`array`): 冒尖小春笋（仅 `type` 为 `strong_newbie` 或 `all` 时有数据），元素结构同上
         * - `emotion_list` (`array`): 快乐之源（仅 `type` 为 `emotion` 或 `all` 时有数据），元素结构同上
         */
        Result getGroupHonorInfo(uint64_t group_id, const std::string& type);

        /**
         * @brief 获取 Cookies (`get_cookies`)
         * @param domain 需要获取 cookies 的域名
         * @return Result 响应数据包含以下字段：
         *
         * - `cookies` (`string`): Cookies
         */
        Result getCookies(const std::string& domain = "");

        /**
         * @brief 获取 CSRF Token (`get_csrf_token`)
         * @return Result 响应数据包含以下字段：
         *
         * - `token` (`int32`): CSRF Token
         */
        Result getCsrfToken();

        /**
         * @brief 获取 QQ 相关接口凭证 (`get_credentials`)
         * @details 相当于 `getCookies()` 和 `getCsrfToken()` 两个接口的合并。
         * @param domain 需要获取 cookies 的域名
         * @return Result 响应数据包含以下字段：
         *
         * - `cookies` (`string`): Cookies
         * - `csrf_token` (`int32`): CSRF Token
         */
        Result getCredentials(const std::string& domain = "");

        /**
         * @brief 获取语音 (`get_record`)
         * @note 要使用此接口，通常需要安装 `ffmpeg`，请参考 OneBot 实现的相关说明。
         * @param file 收到的语音文件名（消息段的 `file` 参数），如 `0B38145AA44505000B38145AA4450500.silk`
         * @param out_format 要转换到的格式，目前支持 `mp3`、`amr`、`wma`、`m4a`、`spx`、`ogg`、`wav`、`flac`
         * @return Result 响应数据包含以下字段：
         *
         * - `file` (`string`): 转换后的语音文件路径，如 `/home/somebody/cqhttp/data/record/0B38145AA44505000B38145AA4450500.mp3`
         */
        Result getRecord(const std::string& file, const std::string& out_format);

        /**
         * @brief 获取图片 (`get_image`)
         * @param file 收到的图片文件名（消息段的 `file` 参数），如 `6B4DE3DFD1BD271E3297859D41C530F5.jpg`
         * @return Result 响应数据包含以下字段：
         *
         * - `file` (`string`): 下载后的图片文件路径，如 `/home/somebody/cqhttp/data/image/6B4DE3DFD1BD271E3297859D41C530F5.jpg`
         */
        Result getImage(const std::string& file);

        /**
         * @brief 检查是否可以发送图片 (`can_send_image`)
         * @return Result 响应数据包含以下字段：
         *
         * - `yes` (`boolean`): 是或否
         */
        Result canSendImage();

        /**
         * @brief 检查是否可以发送语音 (`can_send_record`)
         * @return Result 响应数据包含以下字段：
         *
         * - `yes` (`boolean`): 是或否
         */
        Result canSendRecord();

        /**
         * @brief 获取运行状态 (`get_status`)
         * @note 通常情况下建议只使用 `online` 和 `good` 这两个字段来判断运行状态，因为根据 OneBot 实现的不同，其它字段可能完全不同。
         * @return Result 响应数据包含以下字段：
         *
         * - `online` (`boolean`): 当前 QQ 在线，`null` 表示无法查询到在线状态
         * - `good` (`boolean`): 状态符合预期，意味着各模块正常运行、功能正常，且 QQ 在线
         * - `……`: OneBot 实现自行添加的其它内容
         */
        Result getStatus();

        /**
         * @brief 获取版本信息 (`get_version_info`)
         * @return Result 响应数据包含以下字段：
         *
         * - `app_name` (`string`): 应用标识，如 `mirai-native`
         * - `app_version` (`string`): 应用版本，如 `1.2.3`
         * - `protocol_version` (`string`): OneBot 标准版本，如 `v11`
         * - `……`: OneBot 实现自行添加的其它内容
         */
        Result getVersionInfo();

        /**
         * @brief 重启 OneBot 实现 (`set_restart`)
         * @details 由于重启 OneBot 实现同时需要重启 API 服务，这意味着当前的 API 请求会被中断，因此需要异步地重启，接口返回的 `status` 是 `async`。
         * @param delay 要延迟的毫秒数，如果默认情况下无法重启，可以尝试设置延迟为 `2000` 左右
         * @return Result 无响应数据
         */
        Result setRestart(int delay = 0);

        /**
         * @brief 清理缓存 (`clean_cache`)
         * @details 用于清理积攒了太多的缓存文件。
         * @return Result 无响应数据
         */
        Result cleanCache();
    }
}