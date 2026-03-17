#pragma once

#include "QueryResult.hpp"

#include <nlohmann/json.hpp>
#include <string>

namespace Librium::Query {

class CQuerySerializer
{
public:
    [[nodiscard]] static nlohmann::json ToJson(const SQueryResult& result);
    static void SaveToFile(const SQueryResult& result, const std::string& path);
};

} // namespace Librium::Query
