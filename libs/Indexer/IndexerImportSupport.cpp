#include "IndexerInternal.hpp"
#include "Utils/StringUtils.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <utility>

namespace fs = std::filesystem;

namespace Librium::Indexer::Detail {

using Utils::CStringUtils;

fs::path ResolveArchivePath(const std::string& archivesDir, const std::string& archiveName)
{
    for (const std::string_view suffix : std::array<std::string_view, 2>{".zip", ""})
    {
        const fs::path archivePath = CStringUtils::Utf8ToPath(archivesDir) /
            CStringUtils::Utf8ToPath(archiveName + std::string(suffix));
        if (fs::exists(archivePath))
        {
            return archivePath;
        }
    }

    return {};
}

void WriteCoverFile(const Config::SAppConfig& cfg, int64_t bookId, const Fb2::SFb2Data& fb2)
{
    if (fb2.coverData.empty())
    {
        return;
    }

    const fs::path metaDir = Config::CAppPaths::GetBookMetaDir(CStringUtils::Utf8ToPath(cfg.database.path), bookId);
    fs::create_directories(metaDir);

    const fs::path coverPath = metaDir / ("cover" + fb2.coverExt);
    std::ofstream output(coverPath, std::ios::binary);
    if (!output)
    {
        return;
    }

    output.write(reinterpret_cast<const char*>(fb2.coverData.data()), fb2.coverData.size());
}

CImportGuard::CImportGuard(Db::IBookWriter& db) : m_db(db)
{
    m_db.BeginBulkImport();
    m_db.DropIndexes();
}

CImportGuard::~CImportGuard()
{
    if (!m_finished)
    {
        LOG_WARN("Import interrupted! Attempting to restore database state...");
    }

    try { m_db.CreateIndexes(); }
    catch (const std::exception& e) { LOG_ERROR("Failed to restore indexes: {}", e.what()); }

    try { m_db.EndBulkImport(); }
    catch (const std::exception& e) { LOG_ERROR("Failed to restore pragma: {}", e.what()); }
}

} // namespace Librium::Indexer::Detail
