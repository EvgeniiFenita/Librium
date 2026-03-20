#pragma once

#include <string>
#include <vector>
#include <cctype>

namespace Librium::Utils {

class CBase64
{
public:
    static std::string Encode(const std::string& input);
    static std::string Decode(const std::string& input);

private:
    static constexpr const char* BASE64_CHARS = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
};

} // namespace Librium::Utils
