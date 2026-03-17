#pragma once

#include "ICommand.hpp"
#include "Query/QueryParams.hpp"
#include <string>

namespace Librium::Apps {

class CQueryCommand : public ICommand
{
public:
    void Setup(CLI::App& app) override;
    int Execute() override;

private:
    std::string m_dbPath;
    std::string m_outputPath;
    std::string m_logLevel;
    Query::SQueryParams m_params;
};

} // namespace Librium::Apps
