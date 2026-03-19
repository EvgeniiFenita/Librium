#pragma once

#include "QueryParams.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace Librium::Query {

struct SAuthorInfo
{
    std::string lastName;
    std::string firstName;
    std::string middleName;
};

struct SBookResult
{
    int64_t                   id{0};
    std::string               libId;
    std::string               title;
    std::vector<SAuthorInfo>  authors;
    std::vector<std::string>  genres;
    std::string               series;
    int                       seriesNumber{0};
    std::string               fileName;
    std::string               fileExt;
    std::string               language;
    std::string               dateAdded;
    int                       rating{0};
    uint64_t                  fileSize{0};
    std::string               archiveName;
    std::string               keywords;
    std::string               annotation;
    std::string               publisher;
    std::string               isbn;
    std::string               publishYear;
};

struct SQueryResult
{
    std::vector<SBookResult> books;
    int64_t                  totalFound{0};
    SQueryParams             params;
};

} // namespace Librium::Query
