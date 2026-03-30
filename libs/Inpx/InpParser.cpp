#include "InpParser.hpp"

#include "Zip/ZipReader.hpp"
#include "Log/Logger.hpp"
#include "Utils/StringUtils.hpp"

#include <filesystem>
#include <functional>
#include <string>

namespace Librium::Inpx {

namespace {

constexpr char kInpFieldSeparator = '\x04';
constexpr size_t kInpFieldCount = 15;

std::string ArchiveNameFromEntry(const std::string& entryName)
{
    std::filesystem::path path = Utils::CStringUtils::Utf8ToPath(entryName);
    auto u8stem = path.stem().u8string();
    return std::string(u8stem.begin(), u8stem.end());
}

bool IsInpEntry(const std::string& entryName)
{
    return entryName.size() >= 4 && entryName.substr(entryName.size() - 4) == ".inp";
}

template<typename TOnLine>
void ForEachInpLine(const std::vector<uint8_t>& data, TOnLine&& onLine)
{
    std::string_view text(reinterpret_cast<const char*>(data.data()), data.size());

    size_t start = 0;
    size_t end = text.find('\n', start);

    while (start < text.size())
    {
        std::string_view line = (end == std::string_view::npos)
            ? text.substr(start)
            : text.substr(start, end - start);

        start = (end == std::string_view::npos) ? text.size() : end + 1;
        end = text.find('\n', start);

        if (!line.empty() && line.back() == '\r')
        {
            line.remove_suffix(1);
        }
        if (line.empty())
        {
            continue;
        }

        if (!onLine(line))
        {
            break;
        }
    }
}

} // namespace

std::string_view CInpParser::Trim(std::string_view s)
{
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos)
    {
        return {};
    }
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::vector<std::string_view> CInpParser::Split(std::string_view s, char delim)
{
    std::vector<std::string_view> result;
    size_t start = 0;
    size_t end = s.find(delim);

    while (end != std::string_view::npos)
    {
        result.push_back(s.substr(start, end - start));
        start = end + 1;
        end = s.find(delim, start);
    }
    result.push_back(s.substr(start));

    return result;
}

SAuthor CInpParser::ParseAuthor(std::string_view s)
{
    SAuthor a;
    if (s.empty())
    {
        return a;
    }
    auto parts = Split(s, ',');
    if (parts.size() > 0)
    {
        a.lastName = std::string(Trim(parts[0]));
    }
    if (parts.size() > 1)
    {
        a.firstName = std::string(Trim(parts[1]));
    }
    if (parts.size() > 2)
    {
        a.middleName = std::string(Trim(parts[2]));
    }
    return a;
}

SBookRecord CInpParser::ParseLine(std::string_view line, const std::string& archiveName)
{
    auto fields = Split(line, kInpFieldSeparator);
    while (fields.size() < kInpFieldCount)
    {
        fields.emplace_back();
    }

    SBookRecord rec;
    rec.archiveName = archiveName;

    // Authors — split by ':', discard empty tokens
    for (auto& aStr : Split(fields[0], ':'))
    {
        auto t = Trim(aStr);
        if (t.empty())
        {
            continue;
        }
        auto a = ParseAuthor(t);
        if (!a.IsEmpty())
        {
            rec.authors.push_back(std::move(a));
        }
    }

    // Genres — split by ':', discard empty tokens
    for (auto& g : Split(fields[1], ':'))
    {
        auto gn = Trim(g);
        if (!gn.empty())
        {
            rec.genres.emplace_back(std::string(gn));
        }
    }

    rec.title = std::string(Trim(fields[2]));
    rec.series = std::string(Trim(fields[3]));
    try
    {
        auto seriesNumStr = Trim(fields[4]);
        if (!seriesNumStr.empty())
        {
            rec.seriesNumber = std::stoi(std::string(seriesNumStr));
        }
    }
    catch (const std::exception& e)
    {
        LOG_DEBUG("Failed to parse series number '{}' in archive {}: {}", fields[4], archiveName, e.what());
    }

    rec.fileName = std::string(Trim(fields[5]));
    try
    {
        auto fileSizeStr = Trim(fields[6]);
        if (!fileSizeStr.empty())
        {
            rec.fileSize = std::stoull(std::string(fileSizeStr));
        }
    }
    catch (const std::exception& e)
    {
        LOG_DEBUG("Failed to parse file size '{}' in archive {}: {}", fields[6], archiveName, e.what());
    }

    rec.libId = std::string(Trim(fields[7]));
    rec.deleted = (Trim(fields[8]) == "1");
    rec.fileExt = std::string(Trim(fields[9]));
    if (rec.fileExt.empty())
    {
        rec.fileExt = "fb2";
    }

    rec.dateAdded = std::string(Trim(fields[10]));
    rec.language = std::string(Trim(fields[11]));

    try
    {
        auto ratingStr = Trim(fields[12]);
        if (!ratingStr.empty())
        {
            rec.rating = std::stoi(std::string(ratingStr));
        }
    }
    catch (const std::exception& e)
    {
        LOG_DEBUG("Failed to parse rating '{}' in archive {}: {}", fields[12], archiveName, e.what());
    }

    rec.keywords = std::string(Trim(fields[13]));
    // fields[14] always empty — ignored

    return rec;
}

bool CInpParser::ProcessBookLine(
    std::string_view line,
    const std::string& archiveName,
    SInpParseStats& stats,
    const std::function<bool(SBookRecord&&)>& onBook)
{
    ++stats.totalLines;

    try
    {
        auto rec = ParseLine(line, archiveName);
        if (rec.deleted)
        {
            ++stats.skippedDeleted;
            return true;
        }
        if (rec.title.empty() && rec.fileName.empty())
        {
            ++stats.skippedInvalid;
            return true;
        }

        ++stats.parsedOk;
        return onBook(std::move(rec));
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Unexpected error parsing line in archive {}: {}", archiveName, e.what());
        ++stats.skippedInvalid;
        return true;
    }
}

std::vector<SBookRecord> CInpParser::ParseInpData(const std::vector<uint8_t>& data, const std::string& archiveName, SInpParseStats& stats)
{
    std::vector<SBookRecord> books;
    ForEachInpLine(data, [&](std::string_view line)
    {
        return ProcessBookLine(line, archiveName, stats, [&](SBookRecord&& rec)
        {
            books.push_back(std::move(rec));
            return true;
        });
    });
    return books;
}

std::vector<SBookRecord> CInpParser::Parse(const std::string& inpxPath)
{
    std::vector<SBookRecord> all;
    m_stats = {};

    LOG_INFO("Parsing INPX file: {}", inpxPath);

    Zip::CZipReader::IterateEntries(Utils::CStringUtils::Utf8ToPath(inpxPath), [&](const std::string& name, std::vector<uint8_t> data)
    {
        if (IsInpEntry(name))
        {
            std::string archive = ArchiveNameFromEntry(name);
            LOG_DEBUG("Parsing archive: {}", archive);
            auto books = ParseInpData(data, archive, m_stats);
            all.insert(all.end(),
                std::make_move_iterator(books.begin()),
                std::make_move_iterator(books.end()));
        }
        return true;
    });

    LOG_INFO("Parsing complete. Total lines: {}, parsed OK: {}, skipped: {} (deleted: {}, invalid: {})",
        m_stats.totalLines, m_stats.parsedOk, m_stats.skippedDeleted + m_stats.skippedInvalid,
        m_stats.skippedDeleted, m_stats.skippedInvalid);

    return all;
}

SInpParseStats CInpParser::ParseStreaming(const std::string& inpxPath, const std::function<bool(SBookRecord&&)>& onBook)
{
    m_stats = {};
    bool stop = false;

    LOG_INFO("Streaming INPX file: {}", inpxPath);

    Zip::CZipReader::IterateEntries(Utils::CStringUtils::Utf8ToPath(inpxPath), [&](const std::string& name, std::vector<uint8_t> data)
    {
        if (stop)
        {
            return false;
        }
        if (!IsInpEntry(name))
        {
            return true;
        }

        std::string archive = ArchiveNameFromEntry(name);
        LOG_DEBUG("Parsing archive: {}", archive);

        ForEachInpLine(data, [&](std::string_view line)
        {
            if (stop)
            {
                return false;
            }

            return ProcessBookLine(line, archive, m_stats, [&](SBookRecord&& rec)
            {
                if (!onBook(std::move(rec)))
                {
                    stop = true;
                    return false;
                }
                return true;
            });
        });
        return !stop;
    });

    LOG_INFO("Input file streaming finished. Total lines: {}, books queued for processing: {}, skipped: {} (deleted: {}, invalid: {})",
        m_stats.totalLines, m_stats.parsedOk, m_stats.skippedDeleted + m_stats.skippedInvalid,
        m_stats.skippedDeleted, m_stats.skippedInvalid);

    return m_stats;
}

SInpParseStats CInpParser::ParseByArchive(
    const std::string& inpxPath,
    const std::function<bool(const std::string&, std::vector<SBookRecord>&&)>& onArchive)
{
    m_stats = {};
    bool stop = false;

    LOG_INFO("Streaming INPX file by archive: {}", inpxPath);

    Zip::CZipReader::IterateEntries(Utils::CStringUtils::Utf8ToPath(inpxPath), [&](const std::string& name, std::vector<uint8_t> data)
    {
        if (stop)
        {
            return false;
        }
        if (!IsInpEntry(name))
        {
            return true;
        }

        std::string archive = ArchiveNameFromEntry(name);
        LOG_DEBUG("Parsing archive batch: {}", archive);

        auto books = ParseInpData(data, archive, m_stats);
        if (!onArchive(archive, std::move(books)))
        {
            stop = true;
            return false;
        }

        return true;
    });

    LOG_INFO("Archive streaming finished. Total lines: {}, books queued for processing: {}, skipped: {} (deleted: {}, invalid: {})",
        m_stats.totalLines, m_stats.parsedOk, m_stats.skippedDeleted + m_stats.skippedInvalid,
        m_stats.skippedDeleted, m_stats.skippedInvalid);

    return m_stats;
}

size_t CInpParser::CountLines(const std::string& inpxPath)
{
    size_t total = 0;
    Zip::CZipReader::IterateEntries(Utils::CStringUtils::Utf8ToPath(inpxPath), [&](const std::string& name, std::vector<uint8_t> data)
    {
        if (IsInpEntry(name))
        {
            ForEachInpLine(data, [&](std::string_view)
            {
                ++total;
                return true;
            });
        }
        return true;
    });
    return total;
}

} // namespace Librium::Inpx
