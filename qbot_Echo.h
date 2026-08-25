#pragma once

#include <drogon/plugins/Plugin.h>
namespace qbot
{

class Echo : public drogon::Plugin<Echo>
{
  public:
    Echo() {}

    void initAndStart(const Json::Value &config) override;

    void shutdown() override;
};

}
