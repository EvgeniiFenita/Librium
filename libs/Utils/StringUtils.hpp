#pragma once

#include <string>
#include <filesystem>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Librium::Utils {

class CStringUtils
{
public:
    static std::string Utf16ToUtf8(const std::wstring& utf16) 
    {
        if (utf16.empty()) return {};
#ifdef _WIN32
        int size = WideCharToMultiByte(CP_UTF8, 0, utf16.data(), (int)utf16.size(), nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};
        std::string res(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, utf16.data(), (int)utf16.size(), res.data(), size, nullptr, nullptr);
        return res;
#else
        auto u8 = std::filesystem::path(utf16).u8string();
        return std::string(u8.begin(), u8.end());
#endif
    }

    static std::string Cp1251ToUtf8(const std::string& cp1251)
    {
        if (cp1251.empty()) return {};
#ifdef _WIN32
        int size = MultiByteToWideChar(1251, 0, cp1251.data(), (int)cp1251.size(), nullptr, 0);
        if (size <= 0) return {};
        std::wstring wstr(size, 0);
        MultiByteToWideChar(1251, 0, cp1251.data(), (int)cp1251.size(), wstr.data(), size);
        return Utf16ToUtf8(wstr);
#else
        return cp1251;
#endif
    }

    static bool IsUtf8(const std::string& str)
    {
        const unsigned char* bytes = (const unsigned char*)str.c_str();
        size_t len = str.length();
        size_t i = 0;
        while (i < len)
        {
            if (bytes[i] <= 0x7F) i++;
            else if (bytes[i] >= 0xC2 && bytes[i] <= 0xDF && i + 1 < len && bytes[i+1] >= 0x80 && bytes[i+1] <= 0xBF) i += 2;
            else if (bytes[i] == 0xE0 && i + 2 < len && bytes[i+1] >= 0xA0 && bytes[i+1] <= 0xBF && bytes[i+2] >= 0x80 && bytes[i+2] <= 0xBF) i += 3;
            else if (((bytes[i] >= 0xE1 && bytes[i] <= 0xEC) || bytes[i] == 0xEE || bytes[i] == 0xEF) && i + 2 < len && bytes[i+1] >= 0x80 && bytes[i+1] <= 0xBF && bytes[i+2] >= 0x80 && bytes[i+2] <= 0xBF) i += 3;
            else if (bytes[i] == 0xED && i + 2 < len && bytes[i+1] >= 0x80 && bytes[i+1] <= 0x9F && bytes[i+2] >= 0x80 && bytes[i+2] <= 0xBF) i += 3;
            else if (bytes[i] == 0xF0 && i + 3 < len && bytes[i+1] >= 0x90 && bytes[i+1] <= 0xBF && bytes[i+2] >= 0x80 && bytes[i+2] <= 0xBF && bytes[i+3] >= 0x80 && bytes[i+3] <= 0xBF) i += 4;
            else if (bytes[i] >= 0xF1 && bytes[i] <= 0xF3 && i + 3 < len && bytes[i+1] >= 0x80 && bytes[i+1] <= 0xBF && bytes[i+2] >= 0x80 && bytes[i+2] <= 0xBF && bytes[i+3] >= 0x80 && bytes[i+3] <= 0xBF) i += 4;
            else if (bytes[i] == 0xF4 && i + 3 < len && bytes[i+1] >= 0x80 && bytes[i+1] <= 0x8F && bytes[i+2] >= 0x80 && bytes[i+2] <= 0xBF && bytes[i+3] >= 0x80 && bytes[i+3] <= 0xBF) i += 4;
            else return false;
        }
        return true;
    }
};

} // namespace Librium::Utils
