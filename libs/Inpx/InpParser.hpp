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

    [[nodiscard]] SInpParseStats ParseStreaming(const std::string& inpxPath, const std::function<bool(SBookRecord&&)>& onBook);
    [[nodiscard]] SInpParseStats ParseByArchive(
        const std::string& inpxPath,
        const std::function<bool(const std::string&, std::vector<SBookRecord>&&)>& onArchive);

    [[nodiscard]] static size_t CountLines(const std::string& inpxPath);

    [[nodiscard]] const SInpParseStats& LastStats() const
    {
        return m_stats;
    }

private:
    SInpParseStats m_stats;

    std::vector<SBookRecord> ParseInpData(const std::vector<uint8_t>& data, const std::string& archiveName, SInpParseStats& stats);
    bool ProcessBookLine(
        std::string_view line,
        const std::string& archiveName,
        SInpParseStats& stats,
        const std::function<bool(SBookRecord&&)>& onBook);

    SBookRecord ParseLine(std::string_view line, const std::string& archiveName);

    static SAuthor        ParseAuthor(std::string_view s);
    static std::vector<std::string_view> Split(std::string_view s, char delim);
    static std::string_view   Trim(std::string_view s);
};

} // namespace Librium::Inpx
