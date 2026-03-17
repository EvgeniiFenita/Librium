#pragma once

#include <CLI/CLI.hpp>

namespace Librium::Apps {

class ICommand
{
public:
    virtual ~ICommand() = default;
    virtual void Setup(CLI::App& app) = 0;
    virtual int Execute() = 0;
};

} // namespace Librium::Apps
