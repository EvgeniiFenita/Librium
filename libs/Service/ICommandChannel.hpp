#pragma once

#include <string>

namespace Librium::Service {

class ICommandChannel
{
public:
    virtual ~ICommandChannel() = default;

    virtual bool ReadLine(std::string& line) = 0;
    virtual void WriteLine(const std::string& line) = 0;
};

} // namespace Librium::Service
