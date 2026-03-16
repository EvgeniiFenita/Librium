#pragma once

#include "QueryResult.hpp"
#include "Db/Database.hpp"

namespace LibIndexer::Query {

class CBookQuery
{
public:
    [[nodiscard]] static QueryResult Execute(
        Db::CDatabase&     db,
        const QueryParams& params);
};

} // namespace LibIndexer::Query
