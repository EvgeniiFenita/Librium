#pragma once

#include <filesystem>
#include <string>

namespace Librium::Utils {

class CStringUtils
{
public:
    [[nodiscard]] static std::string Utf16ToUtf8(const std::wstring& utf16);
    [[nodiscard]] static std::string Cp1251ToUtf8(const std::string& cp1251);
    [[nodiscard]] static bool IsUtf8(const std::string& str);
    [[nodiscard]] static std::string SanitizeFilename(const std::string& filename);

    [[nodiscard]] static std::filesystem::path Utf8ToPath(const std::string& utf8Str);
};

} // namespace Librium::Utils
