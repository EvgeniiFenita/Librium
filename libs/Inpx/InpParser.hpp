#pragma once

#include "BookRecord.hpp"

#include <functional>
#include <string>
#include <vector>

namespace Librium::Inpx {

struct SInpParseStats
{
    size_t totalLines{0};
    size_t parsedOk{0};
    size_t skippedDeleted{0};
    size_t skippedInvalid{0};
};

class CInpParser
{
public:
    [[nodiscard]] std::vector<SBookRecord> Parse(const std::string& inpxPath);

    SInpParseStats ParseStreaming(const std::string& inpxPath, const std::function<bool(SBookRecord&&)>& onBook);

    [[nodiscard]] static size_t CountLines(const std::string& inpxPath);

    [[nodiscard]] const SInpParseStats& LastStats() const
    {
        return m_stats;
    }

private:
    SInpParseStats m_stats;

    std::vector<SBookRecord> ParseInpData(const std::vector<uint8_t>& data, const std::string& archiveName, SInpParseStats& stats);

    SBookRecord ParseLine(const std::string& line, const std::string& archiveName);

    static SAuthor        ParseAuthor(const std::string& s);
    static std::vector<std::string> Split(const std::string& s, char delim);
    static std::string   Trim(const std::string& s);
};

} // namespace Librium::Inpx
