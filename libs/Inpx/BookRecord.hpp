#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace LibIndexer::Inpx {

struct Author
{
    std::string lastName;
    std::string firstName;
    std::string middleName;

    [[nodiscard]] std::string FullName() const
    {
        std::string name = lastName;
        if (!firstName.empty())
        {
            name += ", " + firstName;
            if (!middleName.empty())
                name += " " + middleName;
        }
        return name;
    }

    [[nodiscard]] bool IsEmpty() const
    {
        return lastName.empty() && firstName.empty();
    }
};

struct BookRecord
{
    std::vector<Author>      authors;
    std::vector<std::string> genres;
    std::string              title;
    std::string              series;
    int                      seriesNumber{0};
    std::string              fileName;
    uint64_t                 fileSize{0};
    std::string              libId;
    bool                     deleted{false};
    std::string              fileExt{"fb2"};
    std::string              dateAdded;
    std::string              language;
    int                      rating{0};
    std::string              keywords;
    std::string              archiveName;

    // Filled by FB2 parser
    std::string              annotation;
    std::string              publisher;
    std::string              isbn;
    std::string              publishDate;
    std::vector<uint8_t>     coverData;
    std::string              coverMime;

    [[nodiscard]] std::string FilePath() const
    {
        return fileName + "." + fileExt;
    }
};

} // namespace LibIndexer::Inpx
