#include "onebot_API.h"
#include "qbot_Instance.h"
#include "qbot_types.h"
#include "qbot_tools.h"
#include <drogon/drogon.h>
#include <nlohmann/json.hpp>

using namespace std::literals;

static nlohmann::json ref_message(nlohmann::json data)
{
    return { "message_reference", {{"message_id",data["d"]["message_scene"]["ext"].at(data["d"].contains("msg_elements")).get<std::string_view>().substr(sizeof("msg_idx=") - 1)}} };
}

static qbot::Instance* getQBotInstance()
{
    return drogon::app().getPlugin<qbot::Instance>();
}

// 解码 CQ 码中的转义字符
static std::string unescape_cq(std::string_view str)
{
    std::string res;
    res.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i)
    {
        if (str[i] == '&' && i + 4 < str.size() && str.substr(i, 5) == "&#44;")
        {
            res += ',';
            i += 4;
        }
        else if (str[i] == '&' && i + 4 < str.size() && str.substr(i, 5) == "&amp;")
        {
            res += '&';
            i += 4;
        }
        else if (str[i] == '[' && i + 4 < str.size() && str.substr(i, 5) == "&#91;")
        {
            res += '[';
            i += 4;
        }
        else if (str[i] == ']' && i + 4 < str.size() && str.substr(i, 5) == "&#93;")
        {
            res += ']';
            i += 4;
        }
        else
        {
            res += str[i];
        }
    }
    return res;
}

// 解析混合消息为 nlohmann::json 数组
static nlohmann::json parse_cqcode(std::string_view message)
{
    auto result = nlohmann::json::array();

    // 匹配 [CQ:type,key=value,key=value] 的正则表达式
    // C++20 的 std::regex 暂不支持 std::string_view 直接匹配，这里转为 std::match_results 内部迭代
    std::regex cq_regex(R"(\[CQ:([a-zA-Z0-9_\-]+)((?:,[a-zA-Z0-9_\-]+=[^,\]]*)*)\])");
    std::regex param_regex(R"(([a-zA-Z0-9_\-]+)=([^,\]]*))");

    auto s_begin = message.begin();
    auto s_end = message.end();

    std::cmatch match;
    size_t last_pos = 0;

    // 循环匹配所有 CQ 码
    while (std::regex_search(message.data() + last_pos, message.data() + message.size(), match, cq_regex))
    {
        size_t match_pos = last_pos + match.position();

        // 1. 处理 CQ 码之前的纯文本段
        if (match_pos > last_pos)
        {
            std::string_view text_segment = message.substr(last_pos, match_pos - last_pos);
            if (!text_segment.empty())
            {
                result.push_back({
                    {"type", "text"},
                    {"data", {{"text", unescape_cq(text_segment)}}}
                    });
            }
        }

        // 2. 处理提取到的 CQ 码
        std::string cq_type = match[1].str();
        std::string params_str = match[2].str();
        auto data_obj = nlohmann::json::object();

        // 解析 CQ 码内部的参数对
        auto params_begin = std::sregex_iterator(params_str.begin(), params_str.end(), param_regex);
        auto params_end = std::sregex_iterator();

        for (std::sregex_iterator i = params_begin; i != params_end; ++i)
        {
            std::smatch param_match = *i;
            std::string key = param_match[1].str();
            std::string value = param_match[2].str();
            data_obj[key] = unescape_cq(value);
        }

        result.push_back({
            {"type", cq_type},
            {"data", data_obj}
        });

        // 更新下一次搜索的起始位置
        last_pos = match_pos + match.length();
    }

    // 3. 处理尾部的纯文本段
    if (last_pos < message.size())
    {
        std::string_view trailing_text = message.substr(last_pos);
        if (!trailing_text.empty())
        {
            result.push_back({
                {"type", "text"},
                {"data", {{"text", unescape_cq(trailing_text)}}}
                });
        }
    }

    return result;
}

template<qbot::SceneType scene>
static drogon::Task<nlohmann::json> getFileInfo(std::string_view url, qbot::FileType type, std::string_view sceneId)
{
    std::string data;
    if (url.starts_with("file://"))
    {
#ifdef _WIN32
        std::filesystem::path path = url.substr("file:///"sv.length());
#else
        std::filesystem::path path = url.substr("file://"sv.length());
#endif // _WIN32
        std::stringstream ss;
        auto ifile = std::ifstream(path, std::ios::binary);
        if (!ifile.is_open())
        {
            co_return {};
        }
        ifile >> ss.rdbuf();
        data.assign(ss.str());
    }
    else if (co_await tools::isIntranet(url))
    {
        auto resp = co_await tools::SendHttpRequestAsync(url, {}, tools::HttpMethodType<drogon::Get>());
        if (resp == nullptr || resp->statusCode() != drogon::k200OK)
        {
            co_return{};
        }
        data.assign(resp->body());
    }
    else if (url.starts_with("base64://"))
    {
        data.assign(drogon::utils::base64Decode(url.substr("base64://"sv.length())));
    }

    if (!data.empty())
    {
        auto name = url.substr(url.find_last_of("/") + 1);
        if constexpr (scene == qbot::c2c)
        {
            co_return co_await getQBotInstance()->uploadC2CBufferFileAsync(data, name, type, sceneId);
        }
        if constexpr (scene == qbot::group)
        {
            co_return co_await getQBotInstance()->uploadGroupBufferFileAsync(data, name, type, sceneId);
        }
    }

    if (url.starts_with("http"))
    {
        if constexpr (scene == qbot::c2c)
        {
            co_return co_await getQBotInstance()->uploadC2CUrlFileAsync(url, type, sceneId);
        }
        if constexpr (scene == qbot::group)
        {
            co_return co_await getQBotInstance()->uploadGroupUrlFileAsync(url, type, sceneId);
        }
    }
    co_return{};
}

static drogon::Task<nlohmann::json> parseMessage(std::string_view message)
{
    auto segments = parse_cqcode(message);
    auto ret = nlohmann::json{};
    if (segments.size() == 1)
    {
        auto&& segment = *segments.begin();
        auto type = segment["type"].get<std::string_view>();
        auto&& data = segment["data"];
        if (type == "text")
        {
            ret["msg_type"] = 0;
            ret["content"] = data["text"];
        }
        else if (type == "image")
        {
            ret["msg_type"] = 7;
            auto url = data["file"].get<std::string_view>();
            ret["media"]["file_info"] = {};
        }
    }
    else
    {
        auto content = std::string{};
        for (auto&& segment : segments)
        {
            auto type = segment["type"].get<std::string_view>();
            if (type == "text")
            {
                content += segment["data"]["text"];
            }
        }
    }
    
    co_return {};
}

namespace onebot {
    namespace API {
        Result sendPrivateMsg(uint64_t user_id, const std::string& message, bool auto_escape)
        {
            auto payload = co_await parseMessage(message);
            co_return co_await getQBotInstance()->sendC2CMessageAsync(payload, {});
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
