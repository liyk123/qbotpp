#pragma once
#include <drogon/plugins/Plugin.h>
#include <drogon/HttpClient.h>
#include <drogon/WebSocketClient.h>
#include <shared_mutex>

namespace qbot {

    class Instance : public drogon::Plugin<Instance>
    {
    public:
        Instance() = default;
        void initAndStart(const Json::Value& config) override;
        void shutdown() override;
    public:
        drogon::HttpClientPtr getApiClient();
        void setWSClient(const drogon::WebSocketClientPtr& client);
        std::string getAccessToken();
        void setAccessToken(const std::string token);
        std::string& sessionId();
        std::atomic_flag& needResume();
        std::atomic_llong& seq();
    private:
        std::shared_mutex m_tokenMutex{};
        std::string m_accessToken{};
        drogon::HttpClientPtr m_apiClient{};
        drogon::WebSocketClientPtr m_wsClient{};
        bool m_sandbox{};
        std::string m_appId{};
        std::string m_clientSecret{};
        std::string m_sessionId{};
        std::atomic_flag m_needResume{};
        std::atomic_llong m_seq{0};
    };

}
