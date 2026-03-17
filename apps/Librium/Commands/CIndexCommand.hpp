#pragma once

#include "ICommand.hpp"
#include "Config/AppConfig.hpp"
#include <string>

namespace Librium::Apps {

class CImportCommand : public ICommand
{
public:
    void Setup(CLI::App& app) override;
    int Execute() override;

protected:
    std::string m_configPath;
    int         m_threads = 0;
    bool        m_noFb2   = false;
    std::string m_logLevel;
    bool        m_upgrade = false;
};

class CUpgradeCommand : public CImportCommand
{
public:
    void Setup(CLI::App& app) override;
};

} // namespace Librium::Apps
