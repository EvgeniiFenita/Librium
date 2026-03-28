#include "SearchQueryParser.hpp"

namespace Librium::Db {

SSearchToken ParseSearchQuery(const std::string& input)
{
    SSearchToken token;
    if (input.empty())
    {
        token.value = input;
        return token;
    }

    if (input[0] == '=')
    {
        token.mode  = ESearchMode::Exact;
        token.value = input.substr(1);
    }
    else if (input[0] == '*')
    {
        token.mode  = ESearchMode::Contains;
        token.value = input.substr(1);
    }
    else
    {
        token.mode  = ESearchMode::Prefix;
        token.value = input;
    }
    return token;
}

namespace {

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

} // namespace

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
            sqlFragment = " AND " + column + " LIKE librium_upper(?) ESCAPE '\\' ";
            bindValue   = "%" + escaped + "%";
            break;
        case ESearchMode::Prefix:
        default:
            sqlFragment = " AND " + column + " LIKE librium_upper(?) ESCAPE '\\' ";
            bindValue   = escaped + "%";
            break;
    }
}

} // namespace Librium::Db
