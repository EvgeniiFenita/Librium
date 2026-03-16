#pragma once

#include "BookRecord.hpp"

#include <functional>
#include <string>
#include <vector>

namespace LibIndexer::Inpx {

struct InpParseStats
{
    size_t totalLines{0};
    size_t parsedOk{0};
    size_t skippedDeleted{0};
    size_t skippedInvalid{0};
};

class CInpParser
{
public:
    [[nodiscard]] std::vector<BookRecord> Parse(const std::string& inpxPath);

    InpParseStats ParseStreaming(
        const std::string& inpxPath,
        const std::function<bool(BookRecord&&)>& onBook);

    [[nodiscard]] const InpParseStats& LastStats() const { return m_stats; }

private:
    InpParseStats m_stats;

    std::vector<BookRecord> ParseInpData(
        const std::vector<uint8_t>& data,
        const std::string& archiveName,
        InpParseStats& stats);

    BookRecord ParseLine(const std::string& line, const std::string& archiveName);

    static Author        ParseAuthor(const std::string& s);
    static std::vector<std::string> Split(const std::string& s, char delim);
    static std::string   Trim(const std::string& s);
};

} // namespace LibIndexer::Inpx
