#pragma once

#include "ICommandChannel.hpp"

namespace Librium::Service {

class CStdIoChannel final : public ICommandChannel
{
public:
    bool ReadLine(std::string& line) override;
    void WriteLine(const std::string& line) override;
};

} // namespace Librium::Service
