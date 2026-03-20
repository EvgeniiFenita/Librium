#include "StdIoChannel.hpp"
#include <iostream>

namespace Librium::Service {

bool CStdIoChannel::ReadLine(std::string& line)
{
    return static_cast<bool>(std::getline(std::cin, line));
}

void CStdIoChannel::WriteLine(const std::string& line)
{
    std::cout << line << std::endl;
}

} // namespace Librium::Service
