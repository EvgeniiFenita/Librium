#include "InpParser.hpp"

#include "Zip/ZipReader.hpp"

#include <filesystem>
#include <sstream>

namespace Librium::Inpx {

std::string CInpParser::Trim(const std::string& s) 
{
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::vector<std::string> CInpParser::Split(const std::string& s, char delim) 
{
    std::vector<std::string> result;
    std::istringstream ss(s);
    std::string token;
    while (std::getline(ss, token, delim))
        result.push_back(token);
    return result;
}

CAuthor CInpParser::ParseAuthor(const std::string& s) 
{
    CAuthor a;
    if (s.empty()) return a;
    auto parts = Split(s, ',');
    if (parts.size() > 0) a.lastName   = Trim(parts[0]);
    if (parts.size() > 1) a.firstName  = Trim(parts[1]);
    if (parts.size() > 2) a.middleName = Trim(parts[2]);
    return a;
}

CBookRecord CInpParser::ParseLine(const std::string& line,
                                  const std::string& archiveName) 
{
    constexpr char SEP = '\x04';
    auto fields = Split(line, SEP);
    while (fields.size() < 15)
        fields.emplace_back();

    CBookRecord rec;
    rec.archiveName = archiveName;

    // Authors — split by ':', discard empty tokens
    for (auto& aStr : Split(fields[0], ':')) 
{
        auto t = Trim(aStr);
        if (t.empty()) continue;
        auto a = ParseAuthor(t);
        if (!a.IsEmpty())
            rec.authors.push_back(std::move(a));
    }

    // Genres — split by ':', discard empty tokens
    for (auto& g : Split(fields[1], ':')) 
{
        auto gn = Trim(g);
        if (!gn.empty())
            rec.genres.push_back(gn);
    }

    rec.title        = Trim(fields[2]);
    rec.series       = Trim(fields[3]);
    try { if (!Trim(fields[4]).empty()) rec.seriesNumber = std::stoi(Trim(fields[4])); }
    catch (...) 
{}

    rec.fileName = Trim(fields[5]);
    try { if (!Trim(fields[6]).empty()) rec.fileSize = std::stoull(Trim(fields[6])); }
    catch (...) 
{}

    rec.libId    = Trim(fields[7]);
    rec.deleted  = (Trim(fields[8]) == "1");
    rec.fileExt  = Trim(fields[9]);
    if (rec.fileExt.empty()) rec.fileExt = "fb2";

    rec.dateAdded = Trim(fields[10]);
    rec.language  = Trim(fields[11]);

    try { if (!Trim(fields[12]).empty()) rec.rating = std::stoi(Trim(fields[12])); }
    catch (...) 
{}

    rec.keywords = Trim(fields[13]);
    // fields[14] always empty — ignored

    return rec;
}

std::vector<CBookRecord> CInpParser::ParseInpData(
    const std::vector<uint8_t>& data,
    const std::string& archiveName,
    CInpParseStats& stats) 
{
    std::vector<CBookRecord> books;
    std::string text(reinterpret_cast<const char*>(data.data()), data.size());
    std::istringstream ss(text);
    std::string line;

    while (std::getline(ss, line)) 
{
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty()) continue;
        ++stats.totalLines;

        try
        {
            auto rec = ParseLine(line, archiveName);
            if (rec.deleted) 
{ ++stats.skippedDeleted; continue; }
            if (rec.title.empty() && rec.fileName.empty()) 
{ ++stats.skippedInvalid; continue; }
            ++stats.parsedOk;
            books.push_back(std::move(rec));
        }
        catch (...) 
{ ++stats.skippedInvalid; }
    }
    return books;
}

std::vector<CBookRecord> CInpParser::Parse(const std::string& inpxPath) 
{
    std::vector<CBookRecord> all;
    m_stats = {};

    Zip::CZipReader::IterateEntries(inpxPath,
        [&](const std::string& name, std::vector<uint8_t> data) 
{
            if (name.size() >= 4 && name.substr(name.size() - 4) == ".inp") 
{
                std::filesystem::path p(name);
                auto archive = p.stem().string();
                auto books   = ParseInpData(data, archive, m_stats);
                all.insert(all.end(),
                    std::make_move_iterator(books.begin()),
                    std::make_move_iterator(books.end()));
            }
            return true;
        });

    return all;
}

CInpParseStats CInpParser::ParseStreaming(
    const std::string& inpxPath,
    const std::function<bool(CBookRecord&&)>& onBook) 
{
    m_stats = {};
    bool stop = false;

    Zip::CZipReader::IterateEntries(inpxPath,
        [&](const std::string& name, std::vector<uint8_t> data) 
{
            if (stop) return false;
            if (name.size() < 4 || name.substr(name.size() - 4) != ".inp")
                return true;

            std::filesystem::path p(name);
            auto archive = p.stem().string();

            std::string text(reinterpret_cast<const char*>(data.data()), data.size());
            std::istringstream ss(text);
            std::string line;

            while (std::getline(ss, line)) 
{
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.empty()) continue;
                ++m_stats.totalLines;
                try
                {
                    auto rec = ParseLine(line, archive);
                    if (rec.deleted) 
{ ++m_stats.skippedDeleted; continue; }
                    if (rec.title.empty() && rec.fileName.empty()) 
{ ++m_stats.skippedInvalid; continue; }
                    ++m_stats.parsedOk;
                    if (!onBook(std::move(rec))) 
{ stop = true; break; }
                }
                catch (...) 
{ ++m_stats.skippedInvalid; }
            }
            return !stop;
        });

    return m_stats;
}

} // namespace Librium::Inpx






