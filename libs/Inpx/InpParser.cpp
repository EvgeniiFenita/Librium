#include "InpParser.hpp"

#include "Zip/ZipReader.hpp"
#include "Log/Logger.hpp"
#include "Utils/StringUtils.hpp"

#include <filesystem>
#include <string>

namespace Librium::Inpx {

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
    constexpr char SEP = '\x04';
    auto fields = Split(line, SEP);
    while (fields.size() < 15)
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

std::vector<SBookRecord> CInpParser::ParseInpData(const std::vector<uint8_t>& data, const std::string& archiveName, SInpParseStats& stats)
{
    std::vector<SBookRecord> books;
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
        ++stats.totalLines;

        try
        {
            auto rec = ParseLine(line, archiveName);
            if (rec.deleted)
            {
                ++stats.skippedDeleted;
                continue;
            }
            if (rec.title.empty() && rec.fileName.empty())
            {
                ++stats.skippedInvalid;
                continue;
            }
            ++stats.parsedOk;
            books.push_back(std::move(rec));
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Unexpected error parsing line in archive {}: {}", archiveName, e.what());
            ++stats.skippedInvalid;
        }
    }
    return books;
}

std::vector<SBookRecord> CInpParser::Parse(const std::string& inpxPath)
{
    std::vector<SBookRecord> all;
    m_stats = {};

    LOG_INFO("Parsing INPX file: {}", inpxPath);

    Zip::CZipReader::IterateEntries(Utils::CStringUtils::Utf8ToPath(inpxPath), [&](const std::string& name, std::vector<uint8_t> data)
    {
        if (name.size() >= 4 && name.substr(name.size() - 4) == ".inp")
        {
            std::filesystem::path p = Utils::CStringUtils::Utf8ToPath(name);
            auto u8stem = p.stem().u8string();
            std::string archive(u8stem.begin(), u8stem.end());
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
        if (name.size() < 4 || name.substr(name.size() - 4) != ".inp")
        {
            return true;
        }

        std::filesystem::path p = Utils::CStringUtils::Utf8ToPath(name);
        auto u8stem = p.stem().u8string();
        std::string archive(u8stem.begin(), u8stem.end());
        LOG_DEBUG("Parsing archive: {}", archive);

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
            ++m_stats.totalLines;
            try
            {
                auto rec = ParseLine(line, archive);
                if (rec.deleted)
                {
                    ++m_stats.skippedDeleted;
                    continue;
                }
                if (rec.title.empty() && rec.fileName.empty())
                {
                    ++m_stats.skippedInvalid;
                    continue;
                }
                ++m_stats.parsedOk;
                if (!onBook(std::move(rec)))
                {
                    stop = true;
                    break;
                }
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Unexpected error parsing line in archive {}: {}", archive, e.what());
                ++m_stats.skippedInvalid;
            }
        }
        return !stop;
    });

    LOG_INFO("Input file streaming finished. Total lines: {}, books queued for processing: {}, skipped: {} (deleted: {}, invalid: {})",
        m_stats.totalLines, m_stats.parsedOk, m_stats.skippedDeleted + m_stats.skippedInvalid,
        m_stats.skippedDeleted, m_stats.skippedInvalid);

    return m_stats;
}

size_t CInpParser::CountLines(const std::string& inpxPath)
{
    size_t total = 0;
    Zip::CZipReader::IterateEntries(Utils::CStringUtils::Utf8ToPath(inpxPath), [&](const std::string& name, std::vector<uint8_t> data)
    {
        if (name.size() >= 4 && name.substr(name.size() - 4) == ".inp")
        {
            for (auto c : data)
            {
                if (c == '\n') ++total;
            }
            // If the last line doesn't end with a newline, we might miss one, but it's just an estimate anyway.
        }
        return true;
    });
    return total;
}

} // namespace Librium::Inpx
