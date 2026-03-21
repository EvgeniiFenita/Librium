#pragma once

#include "Indexer/ProgressReporter.hpp"
#include "Service/Response.hpp"

namespace Librium::Apps
{

class CProtocolProgressReporter : public Librium::Indexer::IProgressReporter
{
public:
    explicit CProtocolProgressReporter(Librium::Service::IResponse& response) 
        : m_response(response) {}

    void OnProgress(size_t processed, size_t total) override
    {
        m_response.SetProgress(processed, total);
    }
private:
    Librium::Service::IResponse& m_response;
};

} // namespace Librium::Apps
