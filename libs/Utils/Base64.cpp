#include "Base64.hpp"

namespace Librium::Utils {

namespace {

constexpr std::string_view kBase64Chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

void EncodeBlock(const unsigned char input[3], unsigned char output[4])
{
    output[0] = (input[0] & 0xfc) >> 2;
    output[1] = ((input[0] & 0x03) << 4) + ((input[1] & 0xf0) >> 4);
    output[2] = ((input[1] & 0x0f) << 2) + ((input[2] & 0xc0) >> 6);
    output[3] = input[2] & 0x3f;
}

void DecodeBlock(const unsigned char input[4], unsigned char output[3])
{
    output[0] = (input[0] << 2) + ((input[1] & 0x30) >> 4);
    output[1] = ((input[1] & 0xf) << 4) + ((input[2] & 0x3c) >> 2);
    output[2] = ((input[2] & 0x3) << 6) + input[3];
}

void AppendEncodedChars(std::string& output, const unsigned char input[4], int count)
{
    for (int i = 0; i < count; ++i)
    {
        output += kBase64Chars[input[i]];
    }
}

void MapBase64CharsToIndices(unsigned char input[4], std::string_view base64Chars)
{
    for (int i = 0; i < 4; ++i)
    {
        input[i] = static_cast<unsigned char>(base64Chars.find(static_cast<char>(input[i])));
    }
}

} // namespace

std::string CBase64::Encode(const std::string& input)
{
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    const char* bytes_to_encode = input.c_str();
    size_t in_len = input.size();

    while (in_len--)
    {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3)
        {
            EncodeBlock(char_array_3, char_array_4);
            AppendEncodedChars(ret, char_array_4, 4);
            i = 0;
        }
    }

    if (i)
    {
        for (j = i; j < 3; j++) char_array_3[j] = '\0';
        EncodeBlock(char_array_3, char_array_4);
        AppendEncodedChars(ret, char_array_4, i + 1);
        while ((i++ < 3)) ret += '=';
    }

    return ret;
}

std::string CBase64::Decode(const std::string& input)
{
    auto is_base64 = [](unsigned char c)
    {
        return (std::isalnum(c) || (c == '+') || (c == '/'));
    };

    size_t in_len = input.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string ret;

    while (in_len-- && (input[in_] != '=') && is_base64(input[in_]))
    {
        char_array_4[i++] = input[in_]; in_++;
        if (i == 4)
        {
            MapBase64CharsToIndices(char_array_4, kBase64Chars);
            DecodeBlock(char_array_4, char_array_3);

            for (i = 0; (i < 3); i++) ret += static_cast<char>(char_array_3[i]);
            i = 0;
        }
    }

    if (i)
    {
        for (j = i; j < 4; j++) char_array_4[j] = 0;
        MapBase64CharsToIndices(char_array_4, kBase64Chars);
        DecodeBlock(char_array_4, char_array_3);

        for (j = 0; (j < i - 1); j++) ret += static_cast<char>(char_array_3[j]);
    }

    return ret;
}

} // namespace Librium::Utils
