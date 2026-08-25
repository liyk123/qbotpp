#pragma once

#include <drogon/HttpSimpleController.h>

using namespace drogon;

class AppVersion : public drogon::HttpSimpleController<AppVersion>
{
  public:
    void asyncHandleHttpRequest(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) override;
    PATH_LIST_BEGIN
        PATH_ADD("/", HttpMethod::Get);
    PATH_LIST_END
};
