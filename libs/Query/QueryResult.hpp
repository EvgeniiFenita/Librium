#pragma once

#include "QueryParams.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace LibIndexer::Query {

struct AuthorInfo
{
    std::string lastName;
    std::string firstName;
    std::string middleName;
};

struct BookResult
{
    std::string              libId;
    std::string              title;
    std::vector<AuthorInfo>  authors;
    std::vector<std::string> genres;
    std::string              series;
    int                      seriesNumber{0};
    std::string              language;
    std::string              dateAdded;
    int                      rating{0};
    uint64_t                 fileSize{0};
    std::string              archiveName;
    std::string              annotation;
    std::string              publisher;
    std::string              isbn;
    std::string              publishYear;
};

struct QueryResult
{
    std::vector<BookResult> books;
    int64_t                 totalFound{0};
    QueryParams             params;
};

} // namespace LibIndexer::Query
