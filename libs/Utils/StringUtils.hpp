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
    [[nodiscard]] static std::string PathToUtf8String(const std::filesystem::path& path);
    [[nodiscard]] static std::string PathFilenameToUtf8String(const std::filesystem::path& path);
    [[nodiscard]] static std::string PathStemToUtf8String(const std::filesystem::path& path);
};

} // namespace Librium::Utils
