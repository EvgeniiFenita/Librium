#pragma once

#include "QueryResult.hpp"
#include "Database/Database.hpp"

namespace Librium::Query {

class CBookQuery
{
public:
    [[nodiscard]] static SQueryResult Execute(Db::CDatabase& db, const SQueryParams& params);
    [[nodiscard]] static std::optional<SBookResult> GetBookById(Db::CDatabase& db, int64_t id);
};

} // namespace Librium::Query
