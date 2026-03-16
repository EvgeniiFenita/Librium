#pragma once

#include "QueryResult.hpp"
#include "Database/Database.hpp"

namespace Librium::Query {

class CBookQuery
{
public:
    [[nodiscard]] static QueryResult Execute(
        Db::CDatabase&     db,
        const QueryParams& params);
};

} // namespace Librium::Query






