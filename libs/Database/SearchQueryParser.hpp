#pragma once
#include <string>

namespace Librium::Db {

enum class ESearchMode
{
    Prefix,    // "Пуш"    → LIKE 'Пуш%'
    Exact,     // "=Пушкин" → = 'Пушкин'
    Contains,  // "*робот"  → LIKE '%робот%'
};

struct SSearchToken
{
    ESearchMode mode{ESearchMode::Prefix};
    std::string value;  // without prefix operator
};

// Parses a string like "=Exact", "*Contains", "Prefix"
// Returns mode and cleaned value.
SSearchToken ParseSearchQuery(const std::string& input);

// Builds SQL pattern for LIKE or value for =
// bindValue is what should be bound in prepared statement
// sqlFragment is the SQL fragment for WHERE ("= ?" or "LIKE ?")
void BuildSearchSql(const SSearchToken& token,
                    const std::string& column,
                    std::string&       sqlFragment,
                    std::string&       bindValue);

} // namespace Librium::Db
