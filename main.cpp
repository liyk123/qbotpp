#include <csignal>
#include <drogon/drogon.h>
#include <spdlog/spdlog.h>

using namespace std::literals;

constexpr auto versionInfo = "Branch: " GIT_BRANCH "\nCommit: " GIT_VERSION "\nDate: " GIT_DATE;
constexpr auto LogPattern = "%m-%d %H:%M:%S.%e [%^%L%$] [thread:%t] [%s:%#] %v";
constexpr auto TargetLocaleName = "zh_CN.UTF-8";
constexpr auto C_LocaleName = "C";

static void initEnv()
{
#ifdef _WIN32
    ::system("chcp 65001 && cls");
#endif
    std::setlocale(LC_ALL, TargetLocaleName);
    std::setlocale(LC_NUMERIC, C_LocaleName);
    std::locale::global(std::locale(TargetLocaleName));
    std::locale::global(std::locale(std::locale(), C_LocaleName, std::locale::numeric));
    spdlog::default_logger()->set_pattern(LogPattern);
    trantor::Logger::enableSpdLog(spdlog::default_logger());
    drogon::app().loadConfigFile("config.yml");
    spdlog::default_logger()->set_level(spdlog::level::level_enum{ trantor::Logger::logLevel() });
    std::signal(SIGTERM, [](int) {
        drogon::app().getLoop()->runInLoop([] { drogon::app().quit(); });
    });
}

static void AppVersionHandler(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& callback)
{
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setPassThrough(true);
    resp->setBody(versionInfo);
    resp->setContentTypeCode(drogon::ContentType::CT_TEXT_HTML);
    callback(resp);
};

int main() 
{
    initEnv();
    drogon::app()
        .registerHandler("/", &AppVersionHandler, { drogon::Get })
        .run();
    return 0;
}
