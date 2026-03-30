#include "Indexer.hpp"

#include "Log/Logger.hpp"
#include "Utils/StringUtils.hpp"
#include "Zip/ZipReader.hpp"

#include <filesystem>
#include <unordered_set>

namespace fs = std::filesystem;

namespace Librium::Indexer {

using Utils::CStringUtils;

namespace {

[[nodiscard]] bool IsInpEntryName(const std::string& entryName)
{
    return entryName.size() >= 4 && entryName.substr(entryName.size() - 4) == ".inp";
}

[[nodiscard]] std::string ArchiveNameFromInpEntry(const std::string& entryName)
{
    const fs::path archivePath = CStringUtils::Utf8ToPath(entryName);
    const auto archiveStem = archivePath.stem().u8string();
    return std::string(archiveStem.begin(), archiveStem.end());
}

} // namespace

std::vector<std::string> CIndexer::GetNewArchives(Db::IBookWriter& db, const std::string& inpxPath)
{
    auto indexed = db.GetIndexedArchives();
    std::unordered_set<std::string> indexedSet(indexed.begin(), indexed.end());

    std::vector<std::string> allArchives;
    Zip::CZipReader::IterateEntryNames(CStringUtils::Utf8ToPath(inpxPath), [&](const Zip::SZipEntry& entry)
    {
        if (IsInpEntryName(entry.name))
        {
            allArchives.push_back(ArchiveNameFromInpEntry(entry.name));
        }
        return true;
    });

    std::vector<std::string> newArchives;
    for (const auto& archiveName : allArchives)
    {
        if (!indexedSet.count(archiveName))
            newArchives.push_back(archiveName);
    }

    LOG_INFO(
        "Archives in INPX: {}, already indexed: {}, new: {}",
        allArchives.size(), indexed.size(), newArchives.size());

    return newArchives;
}

} // namespace Librium::Indexer
