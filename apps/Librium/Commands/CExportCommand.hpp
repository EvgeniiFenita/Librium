#pragma once

#include "ICommand.hpp"
#include <cstdint>
#include <string>

namespace Librium::Apps {

class CExportCommand : public ICommand
{
public:
    void Setup(CLI::App& app) override;
    int Execute() override;

private:
    std::string m_dbPath;
    std::string m_archivesDir;
    int64_t     m_bookId{0};
    std::string m_outputPath;
    std::string m_configPath;
    std::string m_logLevel;
};

} // namespace Librium::Apps
