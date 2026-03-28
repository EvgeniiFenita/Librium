#include "LibraryApi.hpp"
#include "Database/Database.hpp"
#include "Indexer/Indexer.hpp"
#include "Utils/StringUtils.hpp"
#include "Zip/ZipReader.hpp"
#include "Log/Logger.hpp"
#include <fstream>
#include <stdexcept>

namespace Librium::Service {

CLibraryApi::CLibraryApi(Config::SAppConfig cfg)
    : m_config(std::move(cfg))
{
}

CLibraryApi::~CLibraryApi() = default;

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

Db::SImportStats CLibraryApi::Import(Indexer::IProgressReporter* reporter)
{
    Indexer::CIndexer indexer(m_config);
    return indexer.Run(GetWriter(), Indexer::EImportMode::Full, reporter);
}

Db::SImportStats CLibraryApi::Upgrade(Indexer::IProgressReporter* reporter)
{
    Indexer::CIndexer indexer(m_config);
    return indexer.Run(GetWriter(), Indexer::EImportMode::Upgrade, reporter);
}

Db::SQueryResult CLibraryApi::SearchBooks(const Db::SQueryParams& params)
{
    return GetReader().ExecuteQuery(params);
}

std::filesystem::path CLibraryApi::ExportBook(int64_t id, const std::filesystem::path& outDir)
{
    auto bookInfo = GetReader().GetBookPath(id);
    if (!bookInfo) throw std::runtime_error("Book not found");

    std::string zipName = bookInfo->archiveName;
    if (zipName.size() < 4 || zipName.substr(zipName.size() - 4) != ".zip") zipName += ".zip";

    auto zipPath = Utils::CStringUtils::Utf8ToPath(m_config.library.archivesDir) / Utils::CStringUtils::Utf8ToPath(zipName);
    auto data = Zip::CZipReader::ReadEntry(zipPath, bookInfo->fileName);

    if (!std::filesystem::exists(outDir)) std::filesystem::create_directories(outDir);

    auto outPath = outDir / Utils::CStringUtils::Utf8ToPath(bookInfo->fileName).filename();
    std::ofstream ofs(outPath, std::ios::binary);
    if (!ofs) throw std::runtime_error("Failed to open output file for writing");
    ofs.write(reinterpret_cast<const char*>(data.data()), data.size());

    return outPath;
}

SAppStats CLibraryApi::GetStats()
{
    auto& reader = GetReader();
    return {reader.CountBooks(), reader.CountAuthors(), reader.CountIndexedArchives()};
}

std::optional<SBookDetails> CLibraryApi::GetBook(int64_t id)
{
    auto book = GetReader().GetBookById(id);
    if (!book) return std::nullopt;

    SBookDetails details;
    details.book = std::move(*book);

    try
    {
        auto metaDir = Config::CAppPaths::GetBookMetaDir(Utils::CStringUtils::Utf8ToPath(m_config.database.path), id);
        
        if (std::filesystem::exists(metaDir) && std::filesystem::is_directory(metaDir))
        {
            for (const auto& entry : std::filesystem::directory_iterator(metaDir))
            {
                if (entry.is_regular_file())
                {
                    auto filename = entry.path().filename().u8string();
                    if (filename.find(u8"cover.") == 0)
                    {
                        details.coverPath = entry.path().u8string();
                        break;
                    }
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        LOG_WARN("Failed to check cover for book {}: {}", id, e.what());
    }

    return details;
}

} // namespace Librium::Service
