#include <csignal>
#include <drogon/drogon.h>
#include <spdlog/spdlog.h>

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

int main() 
{
    initEnv();
    drogon::app().run();
    return 0;
}
