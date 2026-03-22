#include "SqliteFunctions.hpp"
#include "Log/Logger.hpp"
#include <sqlite3.h>
#include <string>
#include <cctype>

namespace Librium::Db {

namespace {

// Unicode to uppercase mapping for Cyrillic (U+0430–U+044F → U+0410–U+042F)
// and a few special cases (ё→Ё, etc.)
std::string Utf8ToUpper(const std::string& input)
{
    std::string result;
    result.reserve(input.size());

    size_t i = 0;
    while (i < input.size())
    {
        unsigned char c = static_cast<unsigned char>(input[i]);

        if (c < 0x80)
        {
            // ASCII: use standard toupper
            result += static_cast<char>(std::toupper(c));
            ++i;
        }
        else if ((c & 0xE0) == 0xC0 && i + 1 < input.size())
        {
            // 2-byte UTF-8
            unsigned char c2 = static_cast<unsigned char>(input[i + 1]);
            uint32_t cp = ((c & 0x1F) << 6) | (c2 & 0x3F);

            // Cyrillic lowercase а-я (U+0430–U+044F) → А-Я (U+0410–U+042F)
            if (cp >= 0x0430 && cp <= 0x044F)
                cp -= 0x20;
            // ё (U+0451) → Ё (U+0401)
            else if (cp == 0x0451)
                cp = 0x0401;
            // Ukrainian і (U+0456) → І (U+0406)
            else if (cp == 0x0456)
                cp = 0x0406;
            // Ukrainian ї (U+0457) → Ї (U+0407)
            else if (cp == 0x0457)
                cp = 0x0407;
            // Ukrainian є (U+0454) → Є (U+0404)
            else if (cp == 0x0454)
                cp = 0x0404;
            // ґ (U+0491) → Ґ (U+0490)
            else if (cp == 0x0491)
                cp = 0x0490;

            // Encode back to UTF-8
            if (cp < 0x80)
            {
                result += static_cast<char>(cp);
            }
            else if (cp < 0x800)
            {
                result += static_cast<char>(0xC0 | (cp >> 6));
                result += static_cast<char>(0x80 | (cp & 0x3F));
            }
            else
            {
                result += static_cast<char>(0xE0 | (cp >> 12));
                result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                result += static_cast<char>(0x80 | (cp & 0x3F));
            }
            i += 2;
        }
        else if ((c & 0xF0) == 0xE0 && i + 2 < input.size())
        {
            // 3-byte UTF-8 — pass through unchanged
            result += input[i];
            result += input[i + 1];
            result += input[i + 2];
            i += 3;
        }
        else if ((c & 0xF8) == 0xF0 && i + 3 < input.size())
        {
            // 4-byte UTF-8 — pass through
            result += input[i];
            result += input[i + 1];
            result += input[i + 2];
            result += input[i + 3];
            i += 4;
        }
        else
        {
            result += input[i];
            ++i;
        }
    }
    return result;
}

void SqliteLibriumUpper(sqlite3_context* ctx, int argc, sqlite3_value** argv)
{
    if (argc != 1)
    {
        sqlite3_result_null(ctx);
        return;
    }

    const char* text = reinterpret_cast<const char*>(sqlite3_value_text(argv[0]));
    if (!text)
    {
        sqlite3_result_null(ctx);
        return;
    }

    std::string upper = Utf8ToUpper(std::string(text));
    sqlite3_result_text(ctx, upper.c_str(), static_cast<int>(upper.size()), SQLITE_TRANSIENT);
}

} // anonymous namespace

void RegisterSqliteFunctions(sqlite3* db)
{
    int rc = sqlite3_create_function(
        db,
        "librium_upper",   // function name in SQL
        1,                 // number of arguments
        SQLITE_UTF8 | SQLITE_DETERMINISTIC,
        nullptr,           // user data
        SqliteLibriumUpper,
        nullptr,           // xStep (aggregate)
        nullptr            // xFinal (aggregate)
    );

    if (rc != SQLITE_OK)
        LOG_ERROR("Failed to register librium_upper(): {}", sqlite3_errmsg(db));
    else
        LOG_DEBUG("Registered SQLite function librium_upper()");
}

} // namespace Librium::Db
