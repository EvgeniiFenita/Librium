#pragma once

#include <string>
#include <filesystem>

namespace Librium::Utils {

class CStringUtils
{
public:
    static std::string Utf16ToUtf8(const std::wstring& utf16) 
    {
        if (utf16.empty()) return {};
        auto u8 = std::filesystem::path(utf16).u8string();
        return std::string(u8.begin(), u8.end());
    }
};

} // namespace Librium::Utils
