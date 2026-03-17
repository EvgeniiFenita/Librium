#pragma once

#include "ICommand.hpp"
#include <string>

namespace Librium::Apps {

class CStatsCommand : public ICommand
{
public:
    void Setup(CLI::App& app) override;
    int Execute() override;

private:
    std::string m_dbPath;
};

class CInitConfigCommand : public ICommand
{
public:
    void Setup(CLI::App& app) override;
    int Execute() override;

private:
    std::string m_outputPath = "config.json";
};

} // namespace Librium::Apps
