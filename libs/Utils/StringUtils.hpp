#pragma once

#include <string>

namespace Librium::Utils {

class CStringUtils
{
public:
    static std::string Utf16ToUtf8(const std::wstring& utf16);
    static std::string Cp1251ToUtf8(const std::string& cp1251);
    static bool IsUtf8(const std::string& str);
};

} // namespace Librium::Utils
