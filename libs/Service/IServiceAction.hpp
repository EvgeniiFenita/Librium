#pragma once

#include <nlohmann/json.hpp>
#include "Indexer/IProgressReporter.hpp"

namespace Librium::Service {

class CAppService;

class IServiceAction
{
public:
    virtual ~IServiceAction() = default;
    virtual nlohmann::json Execute(CAppService& service, const nlohmann::json& params, Indexer::IProgressReporter* reporter = nullptr) = 0;
};

} // namespace Librium::Service
