#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <format>

namespace Librium::Db {

struct SQueryParams
{
    std::string title;
    std::string author;
    std::string series;
    std::string genre;
    std::string language;
    std::string libId;
    std::string archiveName;
    std::string dateFrom;
    std::string dateTo;
    int         ratingMin{0};
    int         ratingMax{0};
    bool        withAnnotation{false};
    int64_t     limit{100};
    int64_t     offset{0};

    [[nodiscard]] std::string ToString() const
    {
        return std::format(
            "title='{}', author='{}', series='{}', genre='{}', lang='{}', "
            "libId='{}', archive='{}', date={}-{}, rating={}-{}, "
            "anno={}, limit={}, offset={}",
            title, author, series, genre, language,
            libId, archiveName, dateFrom, dateTo, ratingMin, ratingMax,
            withAnnotation, limit, offset
        );
    }
};

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

struct SBookPath
{
    std::string archiveName;
    std::string fileName;
};

} // namespace Librium::Db
