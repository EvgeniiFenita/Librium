#include "Indexer.hpp"

#include "Inpx/InpParser.hpp"
#include "Log/Logger.hpp"
#include "Zip/ZipReader.hpp"

#include <filesystem>
#include <unordered_map>
#include <fstream>

namespace fs = std::filesystem;

namespace Librium::Indexer {

using Config::Utf8ToPath;

CIndexer::CIndexer(Config::SAppConfig cfg)
    : m_cfg(std::move(cfg))
{
}
std::vector<std::string> CIndexer::GetNewArchives(Db::CDatabase& db, const std::string& inpxPath) 
{
    auto indexed = db.GetIndexedArchives();
    std::unordered_set<std::string> indexedSet(indexed.begin(), indexed.end());

    std::vector<std::string> allArchives;
    Zip::CZipReader::IterateEntryNames(Utf8ToPath(inpxPath), [&](const Zip::SZipEntry& entry) 
    {
        if (entry.name.size() >= 4 &&
            entry.name.substr(entry.name.size()-4) == ".inp") 
        {
            fs::path p(entry.name);
            auto u8stem = p.stem().u8string();
            allArchives.push_back(std::string(u8stem.begin(), u8stem.end()));
        }
        return true;
    });

    std::vector<std::string> newArchives;
    for (const auto& a : allArchives)
        if (!indexedSet.count(a))
            newArchives.push_back(a);

    LOG_INFO(
        "Archives in INPX: {}, already indexed: {}, new: {}",
        allArchives.size(), indexed.size(), newArchives.size());

    return newArchives;
}

void CIndexer::ProducerThread(const std::string& inpxPath, const Config::CBookFilter& filter) 
{
    try
    {
        Inpx::CInpParser parser;
        std::unordered_map<std::string, std::vector<Inpx::SBookRecord>> archiveGroups;
        
        LOG_INFO("Producer: pre-scanning INPX to group by archive...");
        parser.ParseStreaming(inpxPath, [&](Inpx::SBookRecord&& rec) 
        {
            if (m_stopRequested) return false;
            
            if (!m_skipArchives.empty() && m_skipArchives.count(rec.archiveName))
                return true;

            auto filterRes = filter.ShouldInclude(rec);
            if (!filterRes) { ++m_filteredCount; return true; }
            
            archiveGroups[rec.archiveName].push_back(std::move(rec));
            
            if (archiveGroups[rec.archiveName].size() >= 5000)
            {
                for (auto& r : archiveGroups[rec.archiveName])
                    m_workQueue.Push(SWorkItem{std::move(r)});
                archiveGroups[rec.archiveName].clear();
            }
            return true;
        });

        for (auto& group : archiveGroups)
            for (auto& r : group.second)
                m_workQueue.Push(SWorkItem{std::move(r)});
    }
    catch (const std::exception& e) 
    {
        LOG_ERROR("Producer error: {}", e.what());
    }
    m_workQueue.Close();
}

void CIndexer::WorkerThread(const std::string& archivesDir, bool parseFb2) 
{
    std::unordered_map<std::string, fs::path> archivePathCache;
    std::unique_ptr<Zip::CZipReader> currentZip;
    Fb2::CFb2Parser fb2Parser;

    while (!m_stopRequested) 
    {
        try
        {
            auto item = m_workQueue.Pop();
            if (!item) break;

            SResultItem result;
            result.record = std::move(item->record);

            if (parseFb2) 
            {
                fs::path archivePath;
                auto it = archivePathCache.find(result.record.archiveName);
                if (it != archivePathCache.end()) archivePath = it->second;
                else
                {
                    for (const auto& suffix : {std::string(".zip"), std::string("")}) 
                    {
                        fs::path p = Utf8ToPath(archivesDir) / Utf8ToPath(result.record.archiveName + suffix);
                        if (fs::exists(p)) { archivePath = p; break; }
                    }
                    archivePathCache[result.record.archiveName] = archivePath;
                }

                if (!archivePath.empty()) 
                {
                    try
                    {
                        if (!currentZip || currentZip->Path() != archivePath)
                        {
                            currentZip = std::make_unique<Zip::CZipReader>(archivePath);
                        }

                        auto data = currentZip->ReadEntry(result.record.FilePath());
                        result.fb2 = fb2Parser.Parse(data);
                    }
                    catch (const std::exception& e) 
                    {
                        LOG_DEBUG("FB2 read error [{}]: {}", result.record.FilePath(), e.what());
                        ++m_errorCount;
                    }
                }
            }

            ++m_parsedCount;
            m_resultQueue.Push(std::move(result));
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Worker thread error: {}", e.what());
            ++m_errorCount;
        }
    }
}

Db::SImportStats CIndexer::WriterThread(Db::CDatabase& db, size_t batchSize, IProgressReporter* reporter, size_t totalBooks) 
{
    Db::SImportStats stats;
    size_t inBatch = 0;
    size_t processed = 0; // Total books handled (inserted + skipped)
    
    double totalDbMs = 0;
    db.BeginTransaction();

    while (true) 
    {
        auto tStart = std::chrono::high_resolution_clock::now();
        auto item = m_resultQueue.Pop();
        if (!item) break;

        if (db.BookExists(item->record.libId, item->record.archiveName)) 
        {
            ++stats.booksSkipped;
        }
        else
        {
            try
            {
                int64_t id = db.InsertBook(item->record, item->fb2);
                if (id > 0) 
                {
                    ++stats.booksInserted;
                    if (item->fb2.IsOk()) ++stats.fb2Parsed;
                    else                  ++stats.fb2Errors;

                    if (!item->fb2.coverData.empty())
                    {
                        try
                        {
                            auto metaDir = Config::GetBookMetaDir(Config::Utf8ToPath(m_cfg.database.path), id);
                            fs::create_directories(metaDir);
                            auto coverPath = metaDir / ("cover" + item->fb2.coverExt);
                            
                            std::ofstream ofs(coverPath, std::ios::binary);
                            if (ofs)
                            {
                                ofs.write(reinterpret_cast<const char*>(item->fb2.coverData.data()), item->fb2.coverData.size());
                            }
                        }
                        catch (const std::exception& e)
                        {
                            LOG_ERROR("Failed to write cover for book {}: {}", id, e.what());
                        }
                    }
                }
                else ++stats.booksSkipped;
            }
            catch (const std::exception& e) 
            {
                LOG_ERROR("DB insert error: {}", e.what());
                ++stats.fb2Errors;
            }
        }

        auto tEnd = std::chrono::high_resolution_clock::now();
        totalDbMs += std::chrono::duration_cast<std::chrono::microseconds>(tEnd - tStart).count() / 1000.0;

        ++inBatch;
        processed = stats.booksInserted + stats.booksSkipped;
        
        if (inBatch >= batchSize) 
        {
            auto tCStart = std::chrono::high_resolution_clock::now();
            db.Commit();
            auto tCEnd = std::chrono::high_resolution_clock::now();
            
            double commitMs = std::chrono::duration_cast<std::chrono::microseconds>(tCEnd - tCStart).count() / 1000.0;

            LOG_INFO("Batch done. Total handled: {}/{}. Avg DB op: {:.2f}ms, Commit: {:.1f}ms. Queues: Work={}, Res={}", 
                processed, totalBooks, totalDbMs / inBatch, commitMs, m_workQueue.Size(), m_resultQueue.Size());
            
            totalDbMs = 0;
            db.BeginTransaction();
            inBatch = 0;
            
            if (reporter) reporter->OnProgress(processed, totalBooks);
        }
    }

    try 
    { 
        db.Commit(); 
    } 
    catch (const std::exception& e) 
    { 
        LOG_ERROR("Final commit failed during import: {}. Rolling back.", e.what());
        try { db.Rollback(); } catch (const std::exception& rollbackEx) { LOG_ERROR("Rollback failed: {}", rollbackEx.what()); }
    }
    
    processed = stats.booksInserted + stats.booksSkipped;
    if (reporter && processed > 0) reporter->OnProgress(processed, totalBooks);

    stats.booksFiltered = m_filteredCount.load();
    stats.fb2Errors    += m_errorCount.load();
    return stats;
}

namespace {

class CImportGuard
{
public:
    explicit CImportGuard(Db::CDatabase& db) : m_db(db) {}
    ~CImportGuard()
    {
        if (!m_finished)
        {
            LOG_WARN("Import interrupted! Attempting to restore database state...");
            try { m_db.CreateIndexes(); }
            catch (const std::exception& e) { LOG_ERROR("Failed to restore indexes: {}", e.what()); }

            try { m_db.Exec("PRAGMA synchronous = NORMAL"); }
            catch (const std::exception& e) { LOG_ERROR("Failed to restore synchronous mode: {}", e.what()); }
        }
    }

    void MarkFinished() { m_finished = true; }

private:
    Db::CDatabase& m_db;
    bool           m_finished{false};
};

} // namespace

Db::SImportStats CIndexer::Run(Db::CDatabase& db, EImportMode mode, IProgressReporter* reporter) 
{
    Config::CBookFilter filter(m_cfg.filters);
    
    std::unordered_map<std::string, std::vector<Inpx::SBookRecord>> preparedWork;
    size_t totalToProcess = 0;
    size_t totalPreSkipped = 0;

    LOG_INFO("Pre-scanning INPX to calculate exact book count and group by archive...");
    Inpx::CInpParser scanner;
    std::vector<std::string> archivesToMark;

    scanner.ParseStreaming(m_cfg.library.inpxPath, [&](Inpx::SBookRecord&& rec) 
    {
        if (m_stopRequested) return false;
        
        if (mode == EImportMode::Upgrade) 
        {
            if (m_skipArchives.empty())
            {
                for (const auto& a : db.GetIndexedArchives()) m_skipArchives.insert(a);
            }
            if (m_skipArchives.count(rec.archiveName)) 
            {
                ++totalPreSkipped;
                return true;
            }
        }

        auto filterRes = filter.ShouldInclude(rec);
        if (filterRes) 
        {
            if (preparedWork.find(rec.archiveName) == preparedWork.end())
                archivesToMark.push_back(rec.archiveName);

            preparedWork[rec.archiveName].push_back(std::move(rec));
            ++totalToProcess;
        }
        else
        {
            ++m_filteredCount;
        }
        return true;
    });

    if (totalToProcess == 0)
    {
        LOG_INFO("No new books to process in {} mode.", mode == EImportMode::Upgrade ? "upgrade" : "full");
        Db::SImportStats emptyStats;
        emptyStats.booksSkipped = totalPreSkipped;
        emptyStats.archivesProcessed = db.GetIndexedArchives().size();
        return emptyStats;
    }

    if (mode == EImportMode::Full)
    {
        auto metaDir = Config::Utf8ToPath(m_cfg.database.path).parent_path() / "meta";
        if (fs::exists(metaDir))
        {
            LOG_INFO("Full import mode: cleaning meta directory...");
            try { fs::remove_all(metaDir); }
            catch (const std::exception& e) { LOG_ERROR("Failed to clean meta dir: {}", e.what()); }
        }
    }

    CImportGuard guard(db);

    db.Exec("PRAGMA synchronous = OFF");
    db.DropIndexes();

    int workerCount = std::max(1, m_cfg.import.threadCount);
    LOG_INFO("Starting import: {} worker threads, mode={}, total to process={}", workerCount, mode == EImportMode::Upgrade ? "upgrade" : "full", totalToProcess);

    std::jthread producer([this, work = std::move(preparedWork)]() mutable
    {
        for (auto& group : work)
        {
            for (auto& r : group.second)
            {
                if (m_stopRequested) break;
                m_workQueue.Push(SWorkItem{std::move(r)});
            }
            if (m_stopRequested) break;
        }
        m_workQueue.Close();
    });

    std::vector<std::jthread> workers;
    for (int i = 0; i < workerCount; ++i)
        workers.emplace_back([this]() { WorkerThread(m_cfg.library.archivesDir, m_cfg.import.parseFb2); });

    std::jthread closer([&]() { for (auto& w : workers) w.join(); m_resultQueue.Close(); });

    Db::SImportStats stats = WriterThread(db, m_cfg.import.transactionBatchSize, reporter, totalToProcess);

    if (producer.joinable()) producer.join();
    if (closer.joinable()) closer.join();

    db.CreateIndexes();
    db.Exec("PRAGMA synchronous = NORMAL");

    guard.MarkFinished();

    // Mark processed archives as indexed
    for (const auto& arch : archivesToMark)
        db.MarkArchiveIndexed(arch);

    stats.archivesProcessed = db.GetIndexedArchives().size();
    stats.PrintSummary();
    return stats;
}

} // namespace Librium::Indexer
