#include "onebot_Instance.h"
#include "qbot_Instance.h"
#include "onebot11.h"
#include <spdlog/spdlog.h>
#include <xxhash.h>

#define ONEBOT_TAG "\033[36mOneBot\033[0m "

constexpr XXH64_hash_t OPID_HASH_SEED = 'opid';
constexpr XXH32_hash_t MGID_HASH_SEED = 'mgid';

onebot::Instance* getInstance()
{
    return drogon::app().getPlugin<onebot::Instance>();
}

static std::uint64_t to_timestamp(std::string t)
{
    std::chrono::sys_time<std::chrono::seconds> tp;
    std::istringstream(t) >> std::chrono::parse("%Y-%m-%dT%H:%M:%S%Ez", tp);
    return tp.time_since_epoch().count();
}

static void TextHandler(const std::string& msg, const drogon::WebSocketConnectionPtr& connection)
{
    
}

static void CloseHandler(const drogon::WebSocketConnectionPtr& connection)
{
    SPDLOG_WARN(ONEBOT_TAG "Close");
    connection->forceClose();
}

static void MessageHandler(std::string&& msg, const drogon::WebSocketClientPtr& client, const drogon::WebSocketMessageType& type)
{
    SPDLOG_INFO(ONEBOT_TAG "RECV {}", msg);
    auto connection = client->getConnection();
    switch (type)
    {
        [[likely]] case drogon::WebSocketMessageType::Text:
            TextHandler(msg, connection);
            break;
        [[unlikely]] case drogon::WebSocketMessageType::Close:
            CloseHandler(connection);
            break;
        default:
            break;
    }
}

static void ClosedHandler(const drogon::WebSocketClientPtr& client)
{
    auto&& [url, _] = *client->getConnection()->getContext<std::pair<std::string, std::string>>();
    getInstance()->clientCache().modify(url, [](drogon::WebSocketClientPtr& pClient) {
        auto&& [url, token] = *pClient->getConnection()->getContext<std::pair<std::string, std::string>>();
        SPDLOG_INFO(ONEBOT_TAG "reconnect to {}", url);
        pClient = qbot::ConnectToWSServer(url, MessageHandler, ClosedHandler, token);
    });
}

static nlohmann::json parseContent(std::string_view long_text_view)
{
    std::string text(long_text_view);
    auto result_array = nlohmann::json::array();

    // 匹配 AT 标签和 face 标签的主正则
    // 捕获组 1: AT 的 ID
    // 捕获组 2: face 的属性字符串
    std::regex tag_regex(R"(<\s*(?:@([0-9A-F_-]+)|([^>]+))\s*>)");

    auto begin = std::sregex_iterator(text.begin(), text.end(), tag_regex);
    auto end = std::sregex_iterator();

    size_t last_pos = 0; // 记录上一次匹配结束的位置，用来切分普通文本

    for (auto i = begin; i != end; ++i)
    {
        std::smatch match = *i;
        size_t match_pos = match.position();
        size_t match_len = match.length();

        // 1. 提取当前标签之前的普通文本 (text)
        if (match_pos > last_pos)
        {
            std::string text_segment = text.substr(last_pos, match_pos - last_pos);
            if (!text_segment.empty())
            {
                result_array.push_back({
                    {"type", "text"},
                    {"data", {{"text",text_segment}}}
                });
            }
        }

        // 2. 解析匹配到的标签
        if (match[1].matched)
        {
            // 类型 1: AT 标签
            result_array.push_back({
                {"type", "AT"},
                {"data", {{"id", match[1].str()}}}
            });
        }
        else if (match[2].matched)
        {
            // 类型 2: face 标签
            std::string attrs_str = match[2].str();
            auto attr_data = nlohmann::json::object();

            // 子正则：解析属性对
            std::regex attr_pair_regex(R"(([a-zA-Z0-9_]+)\s*=\s*(?:"([^"]*)|([^,>\s]+)))");
            auto attr_begin = std::sregex_iterator(attrs_str.begin(), attrs_str.end(), attr_pair_regex);
            auto attr_end = std::sregex_iterator();

            for (auto j = attr_begin; j != attr_end; ++j)
            {
                std::smatch attr_match = *j;
                std::string key = attr_match[1].str();
                std::string val = attr_match[2].matched ? attr_match[2].str() : attr_match[3].str();
                if (key == "faceId") {
                    attr_data["id"] = val;
                }
            }

            result_array.push_back({
                {"type", "face"},
                {"data", attr_data}
            });
        }

        // 更新位置指针
        last_pos = match_pos + match_len;
    }

    // 3. 提取末尾剩余的普通文本 (text)
    if (last_pos < text.size())
    {
        std::string trailing_text = text.substr(last_pos);
        if (!trailing_text.empty())
        {
            result_array.push_back({
                {"type", "text"},
                {"data", {{"text",trailing_text}}}
            });
        }
    }

    return result_array;
}

static nlohmann::json parseAttachments(const nlohmann::json& attachments, std::initializer_list<std::pair<std::string_view, std::string_view>> ranges)
{
    auto ret = nlohmann::json::array();
    for (auto&& it : attachments)
    {
        auto contentType = it["content_type"].get<std::string_view>();
        for (auto&& [type, cqType] : ranges)
        {
            if (contentType.starts_with(type))
            {
                ret.emplace_back(nlohmann::json{
                    {"type", cqType},
                    {"data", {{"file", it["url"]}}},
                    {"ext", {{"size", it["size"]}}, {"name", it["filename"]}}
                });
            }
        }
    }
    return ret;
}

static nlohmann::json parseMessage(const nlohmann::json& data)
{
    auto ret = nlohmann::json::array();
    if (data["d"].contains("attachments"))
    {
        auto attachmentsArray = parseAttachments(data["d"]["attachments"], {
            std::pair{"image", "image"}, 
            std::pair{"video", "video"}, 
            std::pair{"voice", "record"},
            std::pair{"file", "file"}
        });
        ret.insert(ret.end(), attachmentsArray.begin(), attachmentsArray.end());
    }
    auto contentArray = parseContent(data["d"]["content"]);
    ret.insert(ret.end(), contentArray.begin(), contentArray.end());
    return ret;
}

static std::string toRaw(const nlohmann::json& data)
{
    std::string ret;
    for (auto&& it : data)
    {
        if (it["type"] == "text")
        {
            ret += it["data"]["text"].get<std::string_view>();
            continue;
        }
        ret += "[CQ:" + it["type"].get<std::string>();
        for (auto&& [key, val] : it["data"].items())
        {
            ret += std::format(",{}={}", key, val.get<std::string_view>());
        }
        ret += "]";
    }
    return ret;
}

namespace onebot {
    namespace Event {
        static nlohmann::json OnPrivateMsgReveived(const nlohmann::json& data)
        {
            auto userId = data["d"]["author"]["id"].get<std::string_view>();
            auto msgId = data["d"]["id"].get<std::string_view>();
            auto msgArray = parseMessage(data);
            auto msg = PrivateMsg{
                .sub_type = PrivateMsg::FRIEND,
                .time = to_timestamp(data["d"]["timestamp"]),
                .user_id = ::XXH64(userId.data(), userId.size(), OPID_HASH_SEED),
                .self_id = std::stoull(drogon::app().getPlugin<qbot::Instance>()->getAppId()),
                .message_id = ::XXH32(msgId.data(), msgId.size(), MGID_HASH_SEED),
                .message = msgArray,
                .raw_message = toRaw(msgArray),
                .open_qq_ext = {
                    .user_openid{userId},
                    .message_openid{msgId}
                }
            };
            return {};
        }

        std::optional<GroupUploadNotice::File> getGroupUploadFile(nlohmann::json& msgArray)
        {
            for (auto&& it : msgArray)
            {
                if (it["type"] == "file")
                {
                    return GroupUploadNotice::File{
                        .name = it["ext"]["name"],
                        .size = it["ext"]["size"],
                        .url = it["data"]["file"]
                    };
                }
            }
            return std::nullopt;
        }

        static nlohmann::json OnGroupMsgReveived(const nlohmann::json& data)
        {
            auto groupId = data["d"]["group_id"].get<std::string_view>();
            auto userId = data["d"]["author"]["id"].get<std::string_view>();
            auto msgId = data["d"]["id"].get<std::string_view>();
            auto msgArray = parseMessage(data);

            if (auto file = getGroupUploadFile(msgArray))
            {
                auto notice = GroupUploadNotice{
                    .time = to_timestamp(data["d"]["timestamp"]),
                    .self_id = std::stoull(drogon::app().getPlugin<qbot::Instance>()->getAppId()),
                    .group_id = ::XXH64(groupId.data(), groupId.size(), OPID_HASH_SEED),
                    .user_id = ::XXH64(userId.data(), userId.size(), OPID_HASH_SEED),
                    .file = *file
                };
                return {};
            }

            auto msg = GroupMsg{
                .sub_type = GroupMsg::NORMAL,
                .time = to_timestamp(data["d"]["timestamp"]),
                .user_id = ::XXH64(userId.data(), userId.size(), OPID_HASH_SEED),
                .self_id = std::stoull(drogon::app().getPlugin<qbot::Instance>()->getAppId()),
                .group_id = ::XXH64(groupId.data(), groupId.size(), OPID_HASH_SEED),
                .message_id = ::XXH32(msgId.data(), msgId.size(), MGID_HASH_SEED),
                .message = msgArray,
                .raw_message = toRaw(msgArray),
                .sender = {
                    .nickname{data["d"]["author"]["username"]},
                    .role{data["d"]["author"]["member_role"]}
                },
                .open_qq_ext = {
                    .user_openid{userId},
                    .group_openid{groupId},
                    .message_openid{msgId}
                }
            };
            LOG_INFO << nlohmann::json(msg).dump();
            return {};
        }

        static nlohmann::json onGroupMemberIncreaseReceived(const nlohmann::json& data)
        {
            auto groupId = data["d"]["group_openid"].get<std::string_view>();
            auto userId = data["d"]["member_openid"].get<std::string_view>();
            auto selfId = std::stoull(drogon::app().getPlugin<qbot::Instance>()->getAppId());
            auto notice = GroupIncreaseNotice{
                .time = data["d"]["timestamp"],
                .self_id = selfId,
                .group_id = ::XXH64(groupId.data(), groupId.size(), OPID_HASH_SEED),
                .user_id = ::XXH64(userId.data(), userId.size(), OPID_HASH_SEED),
                .operator_id = selfId,
                .sub_type = GroupIncreaseNotice::SubType::APPROVE,
                .open_qq_ext = {
                    .group_openid{groupId},
                    .user_openid{userId}
                }
            };
            return {};
        }

        template<GroupDecreaseNotice::SubType type>
        static nlohmann::json onGroupMemberDecreaseReceived(const nlohmann::json& data)
        {
            constexpr bool isKickMe = type == GroupDecreaseNotice::KICK_ME;
            auto groupId = data["d"]["group_openid"].get<std::string_view>();
            auto userId = data["d"][isKickMe ? "op_member_openid" : "member_openid"].get<std::string_view>();
            auto selfId = std::stoull(drogon::app().getPlugin<qbot::Instance>()->getAppId());
            auto notice = GroupDecreaseNotice{
                .time = data["d"]["timestamp"],
                .self_id = selfId,
                .group_id = ::XXH64(groupId.data(), groupId.size(), OPID_HASH_SEED),
                .user_id = isKickMe ? selfId : ::XXH64(userId.data(), userId.size(), OPID_HASH_SEED),
                .operator_id = isKickMe ? ::XXH64(userId.data(), userId.size(), OPID_HASH_SEED) : selfId,
                .sub_type = type,
                .open_qq_ext = {
                    .group_openid{groupId},
                    .user_openid{userId}
                }
            };
            return {};
        }

        template<typename T> requires (std::is_same_v<T, FriendAddNotice> || std::is_same_v<T, FriendDelNotice>)
        static nlohmann::json onFriendNoticeReceived(const nlohmann::json& data)
        {
            auto userId = data["d"]["openid"].get<std::string_view>();
            auto notice = T{
                .time = data["d"]["timestamp"],
                .self_id = std::stoull(drogon::app().getPlugin<qbot::Instance>()->getAppId()),
                .user_id = ::XXH64(userId.data(), userId.size(), OPID_HASH_SEED),
                .open_qq_ext = {
                    .user_openid{userId}
                }
            };
            return {};
        }

        template<GroupJoinRequest::SubType type>
        static nlohmann::json onGroupJoinRequestReceived(const nlohmann::json& data)
        {
            constexpr bool isAdd = type == GroupJoinRequest::ADD;
            if constexpr (isAdd)
            {
                if (data["d"]["apply_source"] != "self_apply")
                {
                    return {};
                }
            }
            auto groupId = data["d"]["group_openid"].get<std::string_view>();
            auto userId = data["d"][isAdd ? "member_openid" : "op_member_openid"].get<std::string_view>();
            auto request = GroupJoinRequest{
                .sub_type = type,
                .time = isAdd ? to_timestamp(data["d"]["apply_at"]) : data["d"]["timestamp"].get<std::uint64_t>(),
                .self_id = std::stoull(drogon::app().getPlugin<qbot::Instance>()->getAppId()),
                .group_id = ::XXH64(groupId.data(), groupId.size(), OPID_HASH_SEED),
                .user_id = ::XXH64(userId.data(), userId.size(), OPID_HASH_SEED),
                .comment{isAdd ? data["d"]["verify_info"].value("verify_message", std::string{}) : std::string{}},
                .flag{isAdd ? data["d"]["join_request_id"].get<std::string>() : std::string{}},
                .open_qq_ext{
                    .group_openid{groupId},
                    .user_openid{userId}
                }
            };
            return {};
        }
    }
}



namespace onebot {
    void Instance::initAndStart(const Json::Value& config)
    {
        //for (auto&& val : config["ws_reverse"])
        //{
        //    auto url = val.get("url", "").asString();
        //    auto token = val.get("access_token", "").asString();
        //    auto client = qbot::ConnectToWSServer(url, MessageHandler, ClosedHandler, token);
        //    m_clientCache.insert(url, client);
        //}
        auto qBot = drogon::app().getPlugin<qbot::Instance>();
        qBot->registerDispatchAction<qbot::DispatchType::C2CMessageCreate>(Event::OnPrivateMsgReveived);
        qBot->registerDispatchAction<qbot::DispatchType::GroupMessageCreate>(Event::OnGroupMsgReveived);
        qBot->registerDispatchAction<qbot::DispatchType::GroupAtMessageCreate>(Event::OnGroupMsgReveived);
        qBot->registerDispatchAction<qbot::DispatchType::GroupAddRobot>(&Event::onGroupJoinRequestReceived<Event::GroupJoinRequest::INVITE>);
        qBot->registerDispatchAction<qbot::DispatchType::GroupDelRobot>(&Event::onGroupMemberDecreaseReceived<Event::GroupDecreaseNotice::KICK_ME>);
        qBot->registerDispatchAction<qbot::DispatchType::FriendAdd>(&Event::onFriendNoticeReceived<Event::FriendAddNotice>);
        qBot->registerDispatchAction<qbot::DispatchType::FriendDel>(&Event::onFriendNoticeReceived<Event::FriendDelNotice>);
        qBot->registerDispatchAction<qbot::DispatchType::GroupMemberAdd>(Event::onGroupMemberIncreaseReceived);
        qBot->registerDispatchAction<qbot::DispatchType::GroupMemberRemove>(&Event::onGroupMemberDecreaseReceived<Event::GroupDecreaseNotice::LEAVE>);
        qBot->registerDispatchAction<qbot::DispatchType::GroupJoinRequest>(&Event::onGroupJoinRequestReceived<Event::GroupJoinRequest::ADD>);
    }

    void Instance::shutdown()
    {
        
    }

    qbot::ClientCache& Instance::clientCache()
    {
        return m_clientCache;
    }
}