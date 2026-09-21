#include "qbot_tools.h"
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpClient.h>
#include <nlohmann/json.hpp>

using namespace std::literals;

// DNS 解析 Awaiter
struct DnsResolveAwaiter
{
    std::string hostname;
    trantor::InetAddress result_address;

    // 总是先挂起协程，等待 DNS 回调完成
    bool await_ready() const noexcept { return false; }

    // 在这里调用原生的带有回调的 resolve 函数
    void await_suspend(std::coroutine_handle<> handle) noexcept
    {
        auto& resolver = drogon::app().getResolver();
        if (!resolver)
        {
            // 如果解析器未就绪，直接恢复协程（防止永久挂起）
            handle.resume();
            return;
        }

        resolver->resolve(hostname, [this, handle](const trantor::InetAddress& addr) mutable {
            // 保存回调返回的结果
            this->result_address = addr;
            // 回调执行完成，恢复协程的运行
            handle.resume();
        });
    }

    // 协程恢复时，返回最终获得的 InetAddress 对象
    trantor::InetAddress await_resume() noexcept
    {
        return std::move(result_address);
    }
};

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

    drogon::Task<bool> isIntranet(const std::string_view url)
    {
        auto sechmeLen = url.starts_with("http://"sv) ? "http://"sv.length() : "https://"sv.length();
        auto posEnd1 = url.find("/", sechmeLen);
        auto posEnd2 = url.find(":", sechmeLen);
        auto pos = posEnd1 > posEnd2 ? posEnd2 : posEnd1;
        auto host = url.substr(sechmeLen, pos - sechmeLen);
        auto addr = co_await DnsResolveAwaiter{ std::string(host) };
        bool ret = addr.isIntranetIp() ? true : (addr.isIpV6() ? addr.toIp() == "[::]" : addr.toIp() == "0.0.0.0");
        co_return ret;
    }

    drogon::Task<drogon::HttpResponsePtr> SendHttpRequestAsync(const std::string& url, const nlohmann::json& data, const HttpMethodVariant method)
    {
        auto pos = url.find("/", url.starts_with("http://"sv) ? "http://"sv.length() : "https://"sv.length());
        auto host = url.substr(0, pos);
        auto path = url.substr(pos);
        auto client = drogon::HttpClient::newHttpClient(host);
        auto req = drogon::HttpRequest::newCustomHttpRequest(JsonMethod{ data, method });
        req->setPath(path);
        auto resp = co_await client->sendRequestCoro(req);
        LOG_INFO << std::format("{} {} {} {}", req->methodString(), url, req->body(), resp->body());
        co_return resp;
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
