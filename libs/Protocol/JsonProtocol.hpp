#pragma once

#include <string>
#include <functional>
#include <memory>

namespace Librium::Service { 
    class CAppService; 
    class IResponse;
}
namespace Librium::Indexer { class IProgressReporter; }

namespace Librium::Protocol {

class CJsonProtocol
{
public:
    using SendCallback = std::function<void(const std::string&)>;
    using ReporterFactory = std::function<std::unique_ptr<Librium::Indexer::IProgressReporter>(Service::IResponse&)>;

    static std::string Process(
        const std::string& base64Request, 
        Service::CAppService& service, 
        SendCallback sendCallback,
        ReporterFactory reporterFactory = nullptr);

    static std::string BuildErrorResponse(const std::string& errorMsg);
};

} // namespace Librium::Protocol
