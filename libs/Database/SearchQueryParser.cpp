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

void BuildSearchSql(const SSearchToken& token,
                    const std::string& column,
                    std::string&       sqlFragment,
                    std::string&       bindValue)
{
    // Important: The DB expects librium_upper(?) to be used for case-insensitive match on search columns
    // We add "AND column OP librium_upper(?)"
    switch (token.mode)
    {
        case ESearchMode::Exact:
            sqlFragment = " AND " + column + " = librium_upper(?) ";
            bindValue   = token.value;
            break;
        case ESearchMode::Contains:
            sqlFragment = " AND " + column + " LIKE librium_upper(?) ";
            bindValue   = "%" + token.value + "%";
            break;
        case ESearchMode::Prefix:
        default:
            sqlFragment = " AND " + column + " LIKE librium_upper(?) ";
            bindValue   = token.value + "%";
            break;
    }
}

} // namespace Librium::Db
