#pragma once

#include "Request.hpp"
#include "Response.hpp"
#include "Indexer/ProgressReporter.hpp"
#include <string>

namespace Librium::Service {

class CAppService;

class IServiceAction
{
public:
    virtual ~IServiceAction() = default;
    virtual std::string GetName() const = 0;
    virtual void Execute(CAppService& service, const IRequest& req, IResponse& res, Indexer::IProgressReporter* reporter = nullptr) = 0;
};

} // namespace Librium::Service