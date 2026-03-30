#include "LibraryApi.hpp"
#include "Database/Database.hpp"
#include "Indexer/Indexer.hpp"
#include "Utils/StringUtils.hpp"
#include "Zip/ZipReader.hpp"
#include "Log/Logger.hpp"

#include <fstream>
#include <stdexcept>

namespace Librium::Service {

namespace {

std::filesystem::path BuildArchiveZipPath(
    const Config::SAppConfig& config,
    const Db::SBookPath& bookPath)
{
    std::string zipName = bookPath.archiveName;
    if (zipName.size() < 4 || zipName.substr(zipName.size() - 4) != ".zip")
    {
        zipName += ".zip";
    }

    return Utils::CStringUtils::Utf8ToPath(config.library.archivesDir) /
           Utils::CStringUtils::Utf8ToPath(zipName);
}

std::filesystem::path BuildExportPath(
    const std::filesystem::path& outDir,
    const Db::SBookPath& bookPath)
{
    return outDir / Utils::CStringUtils::Utf8ToPath(bookPath.fileName).filename();
}

void EnsureOutputDirectory(const std::filesystem::path& outDir)
{
    if (!std::filesystem::exists(outDir))
    {
        std::filesystem::create_directories(outDir);
    }
}

std::optional<std::u8string> FindCoverPath(
    const std::filesystem::path& databasePath,
    int64_t id)
{
    const auto metaDir = Config::CAppPaths::GetBookMetaDir(databasePath, id);
    if (!std::filesystem::exists(metaDir) || !std::filesystem::is_directory(metaDir))
    {
        return std::nullopt;
    }

    for (const auto& entry : std::filesystem::directory_iterator(metaDir))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        const auto filename = Utils::CStringUtils::PathFilenameToUtf8String(entry.path());
        if (filename.find("cover.") == 0)
        {
            return Utils::CStringUtils::PathToUtf8U8String(entry.path());
        }
    }

    return std::nullopt;
}

SAppStats BuildAppStats(Db::IBookReader& reader)
{
    return {
        reader.CountBooks(),
        reader.CountAuthors(),
        reader.CountIndexedArchives()
    };
}

} // namespace

CLibraryApi::CLibraryApi(Config::SAppConfig cfg)
    : m_config(std::move(cfg))
{
    LOG_INFO("CLibraryApi::CLibraryApi(dbPath='{}')", m_config.database.path);
}

CLibraryApi::~CLibraryApi()
{
    LOG_INFO("CLibraryApi::~CLibraryApi");
}

void CLibraryApi::EnsureDatabase()
{
    if (!m_db)
    {
        m_db = std::make_unique<Db::CDatabase>(m_config.database.path, m_config.import);
    }
}

Db::IBookReader& CLibraryApi::GetReader()
{
    EnsureDatabase();
    return *m_db;
}

Db::IBookWriter& CLibraryApi::GetWriter()
{
    EnsureDatabase();
    return *m_db;
}

Db::SImportStats CLibraryApi::Import(const std::shared_ptr<Indexer::IProgressReporter>& reporter)
{
    LOG_INFO("CLibraryApi::Import");
    Indexer::CIndexer indexer(m_config);
    return indexer.Run(GetWriter(), Indexer::EImportMode::Full, reporter);
}

Db::SImportStats CLibraryApi::Upgrade(const std::shared_ptr<Indexer::IProgressReporter>& reporter)
{
    LOG_INFO("CLibraryApi::Upgrade");
    Indexer::CIndexer indexer(m_config);
    return indexer.Run(GetWriter(), Indexer::EImportMode::Upgrade, reporter);
}

Db::SQueryResult CLibraryApi::SearchBooks(const Db::SQueryParams& params)
{
    LOG_INFO("CLibraryApi::SearchBooks({})", params.ToString());
    return GetReader().ExecuteQuery(params);
}

std::filesystem::path CLibraryApi::ExportBook(int64_t id, const std::filesystem::path& outDir)
{
    LOG_INFO("CLibraryApi::ExportBook(id={}, outDir='{}')", id, Utils::CStringUtils::PathToUtf8String(outDir));
    auto bookInfo = GetReader().GetBookPath(id);
    if (!bookInfo) throw std::runtime_error("Book not found");

    const auto zipPath = BuildArchiveZipPath(m_config, *bookInfo);
    auto data = Zip::CZipReader::ReadEntry(zipPath, bookInfo->fileName);

    EnsureOutputDirectory(outDir);

    auto outPath = BuildExportPath(outDir, *bookInfo);
    std::ofstream ofs(outPath, std::ios::binary);
    if (!ofs) throw std::runtime_error("Failed to open output file for writing");
    ofs.write(reinterpret_cast<const char*>(data.data()), data.size());

    return outPath;
}

SAppStats CLibraryApi::GetStats()
{
    LOG_INFO("CLibraryApi::GetStats");
    return BuildAppStats(GetReader());
}

std::optional<SBookDetails> CLibraryApi::GetBook(int64_t id)
{
    LOG_INFO("CLibraryApi::GetBook(id={})", id);
    auto book = GetReader().GetBookById(id);
    if (!book) return std::nullopt;

    SBookDetails details;
    details.book = std::move(*book);

    try
    {
        auto coverPath = FindCoverPath(Utils::CStringUtils::Utf8ToPath(m_config.database.path), id);
        if (coverPath)
        {
            details.coverPath = std::move(*coverPath);
        }
    }
    catch (const std::exception& e)
    {
        LOG_WARN("Failed to check cover for book {}: {}", id, e.what());
    }

    return details;
}

} // namespace Librium::Service
