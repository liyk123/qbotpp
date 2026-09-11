#include "onebot_API.h"
#include "qbot_Instance.h"

static nlohmann::json ref_message(nlohmann::json data)
{
    return { "message_reference", {{"message_id",data["d"]["message_scene"]["ext"].at(data["d"].contains("msg_elements")).get<std::string_view>().substr(sizeof("msg_idx=") - 1)}} };
}

static qbot::Instance* getQBotInstance()
{
    return drogon::app().getPlugin<qbot::Instance>();
}

namespace onebot {
    namespace API {
        Result sendPrivateMsg(uint64_t user_id, const std::string& message, bool auto_escape)
        {
            return getQBotInstance()->sendC2CMessageAsync({}, {});
        }

        Result sendGroupMsg(uint64_t group_id, const std::string& message, bool auto_escape)
        {
            return getQBotInstance()->sendGroupMessageAsync({}, {});
        }

        Result sendMsg(std::string message_type, uint64_t user_id, uint64_t group_id, const std::string& message, bool auto_escape)
        {
            if (message_type == "private")
            {
                co_return co_await sendPrivateMsg(user_id, message, auto_escape);
            }
            else if (message_type == "group")
            {
                co_return co_await sendGroupMsg(group_id, message, auto_escape);
            }
            co_return {};
        }

        Result deleteMsg(uint32_t message_id)
        {
            co_return {};
        }

        Result getMsg(uint32_t message_id)
        {
            co_return {};
        }

        Result getForwardMsg(uint32_t id)
        {
            co_return {};
        }

        Result sendLike(uint64_t user_id, uint32_t times)
        {
            co_return {};
        }

        Result setGroupKick(uint64_t group_id, uint64_t user_id, bool reject_add_request)
        {
            co_return {};
        }

        Result setGroupBan(uint64_t group_id, uint64_t user_id, uint32_t duration)
        {
            co_return {};
        }

        Result setGroupAnonymousBan(uint64_t group_id, const std::string& anonymous, const std::string& flag, uint32_t duration)
        {
            co_return {};
        }

        Result setGroupWholeBan(uint64_t group_id, bool enable)
        {
            co_return {};
        }

        Result setGroupAdmin(uint64_t group_id, uint64_t user_id, bool enable)
        {
            co_return {};
        }

        Result setGroupAnonymous(uint64_t group_id, bool enable)
        {
            co_return {};
        }

        Result setGroupCard(uint64_t group_id, uint64_t user_id, const std::string& card)
        {
            co_return {};
        }

        Result setGroupName(uint64_t group_id, const std::string& name)
        {
            co_return {};
        }

        Result setGroupLeave(uint64_t group_id, bool is_dismiss)
        {
            co_return {};
        }

        Result setGroupSpecialTitle(uint64_t group_id, uint64_t user_id, const std::string& special_title, int32_t duration)
        {
            co_return {};
        }

        Result setFriendAddRequest(const std::string& flag, bool approve, const std::string& remark)
        {
            co_return {};
        }

        Result setGroupAddRequest(const std::string& flag, const std::string& sub_type, bool approve, const std::string& reason)
        {
            co_return {};
        }

        Result getLoginInfo()
        {
            co_return {};
        }

        Result getStrangerInfo(uint64_t user_id, bool no_cache)
        {
            co_return {};
        }

        Result getFriendList()
        {
            co_return {};
        }

        Result getGroupInfo(uint64_t group_id, bool no_cache)
        {
            co_return {};
        }

        Result getGroupList()
        {
            co_return {};
        }

        Result getGroupMemberInfo(uint64_t group_id, uint64_t user_id, bool no_cache)
        {
            co_return {};
        }

        Result getGroupMemberList(uint64_t group_id)
        {
            co_return {};
        }

        Result getGroupHonorInfo(uint64_t group_id, const std::string& type)
        {
            co_return {};
        }

        Result getCookies(const std::string& domain)
        {
            co_return {};
        }

        Result getCsrfToken()
        {
            co_return {};
        }

        Result getCredentials(const std::string& domain)
        {
            co_return {};
        }

        Result getRecord(const std::string& file, const std::string& out_format)
        {
            co_return {};
        }

        Result getImage(const std::string& file)
        {
            co_return {};
        }

        Result canSendImage()
        {
            co_return {};
        }

        Result canSendRecord()
        {
            co_return {};
        }

        Result getStatus()
        {
            co_return {};
        }

        Result getVersionInfo()
        {
            co_return {};
        }

        Result setRestart(int delay)
        {
            co_return {};
        }

        Result cleanCache()
        {
            co_return {};
        }
    }
}
