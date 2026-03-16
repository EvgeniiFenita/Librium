#pragma once

#include "QueryResult.hpp"

#include <nlohmann/json.hpp>
#include <string>

namespace Librium::Query {

class CQuerySerializer
{
public:
    [[nodiscard]] static nlohmann::json ToJson(const QueryResult& result);
    static void SaveToFile(const QueryResult& result, const std::string& path);
};

} // namespace Librium::Query






