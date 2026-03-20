#include "LibraryApi.hpp"
#include "Database/Database.hpp"
#include "Indexer/Indexer.hpp"
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

Db::CDatabase& CLibraryApi::GetDatabase()
{
    if (!m_db)
    {
        m_db = std::make_unique<Db::CDatabase>(m_config.database.path);
    }
    return *m_db;
}

Db::SImportStats CLibraryApi::Import(Indexer::IProgressReporter* reporter)
{
    m_config.import.mode = "full";
    Indexer::CIndexer indexer(m_config);
    return indexer.Run(reporter);
}

Db::SImportStats CLibraryApi::Upgrade(Indexer::IProgressReporter* reporter)
{
    m_config.import.mode = "upgrade";
    Indexer::CIndexer indexer(m_config);
    return indexer.Run(reporter);
}

Query::SQueryResult CLibraryApi::SearchBooks(const Query::SQueryParams& params)
{
    return Query::CBookQuery::Execute(GetDatabase(), params);
}

std::filesystem::path CLibraryApi::ExportBook(int64_t id, const std::filesystem::path& outDir)
{
    auto& db = GetDatabase();
    auto bookInfo = db.GetBookPath(id);
    if (!bookInfo) throw std::runtime_error("Book not found");

    std::string zipName = bookInfo->archiveName;
    if (zipName.size() < 4 || zipName.substr(zipName.size() - 4) != ".zip") zipName += ".zip";

    auto zipPath = Config::Utf8ToPath(m_config.library.archivesDir) / Config::Utf8ToPath(zipName);
    auto data = Zip::CZipReader::ReadEntry(zipPath, bookInfo->fileName);

    if (!std::filesystem::exists(outDir)) std::filesystem::create_directories(outDir);

    auto outPath = outDir / Config::Utf8ToPath(bookInfo->fileName);
    std::ofstream ofs(outPath, std::ios::binary);
    if (!ofs) throw std::runtime_error("Failed to open output file for writing");
    ofs.write(reinterpret_cast<const char*>(data.data()), data.size());

    return outPath;
}

SAppStats CLibraryApi::GetStats()
{
    auto& db = GetDatabase();
    return {db.CountBooks(), db.CountAuthors()};
}

std::optional<SBookDetails> CLibraryApi::GetBook(int64_t id)
{
    auto& db = GetDatabase();
    auto book = Query::CBookQuery::GetBookById(db, id);
    if (!book) return std::nullopt;

    SBookDetails details;
    details.book = std::move(*book);

    try
    {
        auto metaDir = Config::GetBookMetaDir(Config::Utf8ToPath(m_config.database.path), id);
        
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
