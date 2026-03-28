#pragma once

#include <string>
#include <vector>
#include <cctype>

namespace Librium::Utils {

class CBase64
{
public:
    [[nodiscard]] static std::string Encode(const std::string& input);
    [[nodiscard]] static std::string Decode(const std::string& input);
};

} // namespace Librium::Utils
