#include "AppVersion.h"

constexpr auto versionInfo = "Branch: " GIT_BRANCH "\nCommit: " GIT_VERSION "\nDate: " GIT_DATE;

void AppVersion::asyncHandleHttpRequest(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback)
{
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setPassThrough(true);
    resp->setBody(versionInfo);
    resp->setContentTypeCode(drogon::ContentType::CT_TEXT_HTML);
    callback(resp);
}
