#include "SearchQueryParser.hpp"

namespace Librium::Db {

namespace {

[[nodiscard]] bool HasSearchOperatorPrefix(const std::string& input)
{
    return !input.empty() && (input[0] == '=' || input[0] == '*');
}

[[nodiscard]] ESearchMode DetectSearchMode(const std::string& input)
{
    if (input.empty())
    {
        return ESearchMode::Prefix;
    }

    if (input[0] == '=')
    {
        return ESearchMode::Exact;
    }

    if (input[0] == '*')
    {
        return ESearchMode::Contains;
    }

    return ESearchMode::Prefix;
}

[[nodiscard]] std::string ExtractSearchValue(const std::string& input)
{
    if (HasSearchOperatorPrefix(input))
    {
        return input.substr(1);
    }

    return input;
}

std::string EscapeLike(const std::string& input)
{
    std::string res;
    res.reserve(input.size());
    for (char c : input)
    {
        if (c == '%' || c == '_' || c == '\\')
        {
            res += '\\';
        }
        res += c;
    }
    return res;
}

void BuildLikeSearchSql(const std::string& column, std::string& sqlFragment)
{
    sqlFragment = " AND " + column + " LIKE librium_upper(?) ESCAPE '\\' ";
}

} // namespace

SSearchToken ParseSearchQuery(const std::string& input)
{
    SSearchToken token;
    token.mode = DetectSearchMode(input);

    if (input.empty())
    {
        token.value = input;
        return token;
    }

    token.value = ExtractSearchValue(input);
    return token;
}

void BuildSearchSql(const SSearchToken& token,
                    const std::string& column,
                    std::string&       sqlFragment,
                    std::string&       bindValue)
{
    // Important: The DB expects librium_upper(?) to be used for case-insensitive match on search columns
    // We add "AND column OP librium_upper(?) ESCAPE '\'" to prevent wildcard injection.
    const std::string escaped = EscapeLike(token.value);
    switch (token.mode)
    {
        case ESearchMode::Exact:
            sqlFragment = " AND " + column + " = librium_upper(?) ";
            bindValue   = token.value;
            break;
        case ESearchMode::Contains:
            BuildLikeSearchSql(column, sqlFragment);
            bindValue   = "%" + escaped + "%";
            break;
        case ESearchMode::Prefix:
        default:
            BuildLikeSearchSql(column, sqlFragment);
            bindValue   = escaped + "%";
            break;
    }
}

} // namespace Librium::Db
