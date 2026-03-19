#pragma once

#include <cstdint>
#include <string>

namespace Librium::Query {

struct SQueryParams
{
    std::string title;
    std::string author;
    std::string series;
    std::string genre;
    std::string language;
    std::string libId;
    std::string archiveName;
    std::string yearFrom;
    std::string dateFrom;
    std::string dateTo;
    int         ratingMin{0};
    bool        withAnnotation{false};
    int64_t     limit{100};
    int64_t     offset{0};
};

} // namespace Librium::Query
