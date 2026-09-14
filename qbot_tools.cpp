#include "qbot_tools.h"
#include <nlohmann/json.hpp>

using namespace std::literals;

namespace tools {
    drogon::WebSocketClientPtr ConnectToWSServer(const std::string url, const WSMessageHandler& messageHandler, const WSClosedHandler& closedHandler, const drogon::WebSocketRequestCallback& requestCallback, const std::span<std::pair<std::string, std::string>>& headers)
    {
        auto pos = url.find("/", url.starts_with("ws://"sv) ? "ws://"sv.length() : "wss://"sv.length());
        auto host = url.substr(0, pos);
        auto path = url.substr(pos);
        auto client = drogon::WebSocketClient::newWebSocketClient(host);
        client->setMessageHandler(messageHandler);
        client->setConnectionClosedHandler(closedHandler);
        auto req = drogon::HttpRequest::newHttpRequest();
        req->setPath(path);
        for (auto&& [key, val] : headers)
        {
            req->addHeader(key, val);
        }
        client->connectToServer(req, requestCallback);
        return client;
    }
}

template<typename T> requires std::same_as<std::decay_t<T>, tools::JsonMethod>
drogon::HttpRequestPtr toRequestPtr(T&& obj)
{
    auto&& [data, method] = obj;
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    std::visit([&](auto&& x) {
        req->setMethod(x.value);
        if constexpr (x.value == drogon::Get)
        {
            for (auto&& [key, val] : data.items())
            {
                req->setParameter(key, val);
            }
        }
        else
        {
            req->setBody(data.dump());
        }
    }, method);
    return req;
}

template<typename T> requires std::same_as<std::decay_t<T>, nlohmann::json>
drogon::HttpRequestPtr toRequestPtr(T&& obj)
{
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->setBody(obj.dump());
    return req;
}

namespace drogon {
    template<>
    HttpRequestPtr toRequest(tools::JsonMethod&& obj)
    {
        return toRequestPtr(obj);
    }

    template<>
    HttpRequestPtr toRequest(const tools::JsonMethod& obj)
    {
        return toRequestPtr(obj);
    }

    template<>
    HttpRequestPtr toRequest(tools::JsonMethod& obj)
    {
        return toRequestPtr(obj);
    }

    template<>
    HttpRequestPtr toRequest(nlohmann::json&& obj)
    {
        return toRequestPtr(obj);
    }

    template<>
    HttpRequestPtr toRequest(const nlohmann::json& obj)
    {
        return toRequestPtr(obj);
    }

    template<>
    HttpRequestPtr toRequest(nlohmann::json& obj)
    {
        return toRequestPtr(obj);
    }

    template<>
    nlohmann::json fromResponse(const HttpResponse& resp)
    {
        return nlohmann::json::parse(resp.body());
    }
}
