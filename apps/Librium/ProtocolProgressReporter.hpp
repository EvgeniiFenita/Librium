#pragma once

#include "Indexer/IProgressReporter.hpp"
#include "Utils/Base64.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>

namespace Librium::Apps
{

class CProtocolProgressReporter : public Librium::Indexer::IProgressReporter
{
public:
    void OnProgress(size_t processed, size_t total) override
    {
        nlohmann::json progress_msg = {
            {"status", "progress"},
            {"data", {
                {"processed", processed},
                {"total", total}
            }}
        };
        std::cout << Librium::Utils::CBase64::Encode(progress_msg.dump()) << std::endl;
    }
};

} // namespace Librium::Apps
