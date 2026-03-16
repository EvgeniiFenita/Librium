#pragma once

#include "BookRecord.hpp"

#include <functional>
#include <string>
#include <vector>

namespace Librium::Inpx {

struct CInpParseStats
{
    size_t totalLines{0};
    size_t parsedOk{0};
    size_t skippedDeleted{0};
    size_t skippedInvalid{0};
};

class CInpParser
{
public:
    [[nodiscard]] std::vector<CBookRecord> Parse(const std::string& inpxPath);

    CInpParseStats ParseStreaming(
        const std::string& inpxPath,
        const std::function<bool(CBookRecord&&)>& onBook);

    [[nodiscard]] const CInpParseStats& LastStats() const
{ return m_stats; }

private:
    CInpParseStats m_stats;

    std::vector<CBookRecord> ParseInpData(
        const std::vector<uint8_t>& data,
        const std::string& archiveName,
        CInpParseStats& stats);

    CBookRecord ParseLine(const std::string& line, const std::string& archiveName);

    static CAuthor        ParseAuthor(const std::string& s);
    static std::vector<std::string> Split(const std::string& s, char delim);
    static std::string   Trim(const std::string& s);
};

} // namespace Librium::Inpx






