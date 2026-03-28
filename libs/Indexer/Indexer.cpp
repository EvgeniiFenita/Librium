#include "Indexer.hpp"

#include "Inpx/InpParser.hpp"
#include "Log/Logger.hpp"
#include "Utils/StringUtils.hpp"
#include "Zip/ZipReader.hpp"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <utility>
#include <unordered_map>

namespace fs = std::filesystem;

namespace Librium::Indexer {

using Utils::CStringUtils;

namespace {

[[nodiscard]] std::string_view ImportModeToString(EImportMode mode)
{
    return mode == EImportMode::Upgrade ? "upgrade" : "full";
}

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

[[nodiscard]] fs::path ResolveArchivePath(const std::string& archivesDir, const std::string& archiveName)
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

template<typename TQueue, typename TValue>
bool TryPushQueueItem(TQueue& queue, TValue&& value, std::string_view queueName)
{
    if (queue.Push(std::forward<TValue>(value)))
    {
        return true;
    }

    LOG_DEBUG("{} queue is already closed; dropping pending item", queueName);
    return false;
}

} // namespace

CIndexer::CIndexer(Config::SAppConfig cfg)
    : m_cfg(std::move(cfg))
{
}
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
    for (const auto& a : allArchives)
        if (!indexedSet.count(a))
            newArchives.push_back(a);

    LOG_INFO(
        "Archives in INPX: {}, already indexed: {}, new: {}",
        allArchives.size(), indexed.size(), newArchives.size());

    return newArchives;
}

void CIndexer::WorkerThread(const std::string& archivesDir, bool parseFb2) 
{
    std::unordered_map<std::string, fs::path> archivePathCache;
    Fb2::CFb2Parser fb2Parser;

    while (!m_stopRequested) 
    {
        try
        {
            auto item = m_workQueue.Pop();
            if (!item) break;

            // Resolve archive path once per batch.
            fs::path archivePath;
            auto it = archivePathCache.find(item->archiveName);
            if (it != archivePathCache.end())
            {
                archivePath = it->second;
            }
            else
            {
                archivePath = ResolveArchivePath(archivesDir, item->archiveName);
                archivePathCache[item->archiveName] = archivePath;
            }

            // Open ZIP once for the entire archive batch.
            std::unique_ptr<Zip::CZipReader> zip;
            if (parseFb2 && !archivePath.empty())
            {
                try
                {
                    zip = std::make_unique<Zip::CZipReader>(archivePath);
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("Failed to open archive [{}]: {}", item->archiveName, e.what());
                }
            }

            for (auto& rec : item->records)
            {
                if (m_stopRequested) break;

                try
                {
                    SResultItem result;
                    result.record = std::move(rec);

                    if (zip)
                    {
                        try
                        {
                            auto filePath = result.record.FilePath();
                            auto data = zip->ReadEntry(filePath);
                            result.fb2 = fb2Parser.Parse(data, filePath);
                        }
                        catch (const std::exception& e)
                        {
                            LOG_DEBUG("FB2 read error [{}]: {}", result.record.FilePath(), e.what());
                            ++m_errorCount;
                        }
                    }

                    ++m_parsedCount;
                    if (!TryPushQueueItem(m_resultQueue, std::move(result), "result"))
                    {
                        break;
                    }
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("Worker record error: {}", e.what());
                    ++m_errorCount;
                }
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Worker thread error: {}", e.what());
            ++m_errorCount;
        }
    }
}

Db::SImportStats CIndexer::WriterThread(Db::IBookWriter& db, size_t batchSize, const std::shared_ptr<IProgressReporter>& reporter, size_t totalBooks,
    const std::unordered_map<std::string, size_t>& archiveSizes)
{
    Db::SImportStats stats;
    size_t inBatch = 0;
    size_t processed = 0;
    
    double totalDbMs = 0;

    // Per-archive completion tracking: counts how many result items
    // have been processed for each archive this run.
    std::unordered_map<std::string, size_t> archiveDone;
    // Archives fully processed since last commit, waiting to be marked.
    std::vector<std::string> pendingMark;

    db.BeginTransaction();

    while (true) 
    {
        auto tStart = std::chrono::high_resolution_clock::now();
        auto item = m_resultQueue.Pop();
        if (!item) break;

        const std::string& archName = item->record.archiveName;

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

                    try
                    {
                        WriteCoverFile(m_cfg, id, item->fb2);
                    }
                    catch (const std::exception& e)
                    {
                        LOG_ERROR("Failed to write cover for book {}: {}", id, e.what());
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

        // Track per-archive completion. When all expected records for an
        // archive have been processed, schedule it for immediate marking.
        auto it = archiveSizes.find(archName);
        if (it != archiveSizes.end())
        {
            if (++archiveDone[archName] >= it->second)
                pendingMark.push_back(archName);
        }

        auto tEnd = std::chrono::high_resolution_clock::now();
        totalDbMs += std::chrono::duration_cast<std::chrono::microseconds>(tEnd - tStart).count() / 1000.0;

        ++inBatch;
        processed = stats.booksInserted + stats.booksSkipped;
        
        if (inBatch >= batchSize) 
        {
            auto tCStart = std::chrono::high_resolution_clock::now();

            // Mark completed archives inside the same transaction as the
            // books they contain — both committed atomically, so a crash
            // between batches never leaves books committed without their
            // archive being marked.
            for (const auto& arch : pendingMark)
                db.MarkArchiveIndexed(arch);
            pendingMark.clear();

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
        for (const auto& arch : pendingMark)
            db.MarkArchiveIndexed(arch);
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
    explicit CImportGuard(Db::IBookWriter& db) : m_db(db) 
    {
        m_db.BeginBulkImport();
        m_db.DropIndexes();
    }

    ~CImportGuard()
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

    void MarkFinished() { m_finished = true; }

private:
    Db::IBookWriter& m_db;
    bool             m_finished{false};
};

} // namespace

Db::SImportStats CIndexer::Run(Db::IBookWriter& db, EImportMode mode, const std::shared_ptr<IProgressReporter>& reporter)
 
{
    Config::CBookFilter filter(m_cfg.filters);
    
    std::unordered_map<std::string, std::vector<Inpx::SBookRecord>> preparedWork;
    size_t totalToProcess = 0;
    size_t totalPreSkipped = 0;

    LOG_INFO("Pre-scanning INPX to calculate exact book count and group by archive...");
    Inpx::CInpParser scanner;

    [[maybe_unused]] const auto scanStats = scanner.ParseStreaming(m_cfg.library.inpxPath, [&](Inpx::SBookRecord&& rec) 
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
        LOG_INFO("No new books to process in {} mode.", ImportModeToString(mode));
        Db::SImportStats emptyStats;
        emptyStats.booksSkipped = totalPreSkipped;
        emptyStats.archivesProcessed = db.GetIndexedArchives().size();
        return emptyStats;
    }

    if (mode == EImportMode::Full)
    {
        auto metaDir = CStringUtils::Utf8ToPath(m_cfg.database.path).parent_path() / "meta";
        if (fs::exists(metaDir))
        {
            LOG_INFO("Full import mode: cleaning meta directory...");
            try { fs::remove_all(metaDir); }
            catch (const std::exception& e) { LOG_ERROR("Failed to clean meta dir: {}", e.what()); }
        }
    }

    CImportGuard guard(db);

    int workerCount = std::max(1, m_cfg.import.threadCount);
    LOG_INFO("Starting import: {} worker threads, mode={}, total to process={}", workerCount, ImportModeToString(mode), totalToProcess);

    // Build expected per-archive record counts so WriterThread can mark
    // each archive as indexed immediately after all its books are committed.
    std::unordered_map<std::string, size_t> archiveSizes;
    archiveSizes.reserve(preparedWork.size());
    for (const auto& [arch, records] : preparedWork)
        archiveSizes[arch] = records.size();

    std::jthread producer([this, work = std::move(preparedWork)]() mutable
    {
        try
        {
            for (auto& [archiveName, records] : work)
            {
                if (m_stopRequested) break;
                if (!TryPushQueueItem(m_workQueue, SWorkItem{archiveName, std::move(records)}, "work"))
                {
                    break;
                }
            }
            m_workQueue.Close();
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Producer thread failed: {}", e.what());
            m_workQueue.Close();
        }
    });

    std::vector<std::jthread> workers;
    for (int i = 0; i < workerCount; ++i)
        workers.emplace_back([this]()
        {
            try
            {
                WorkerThread(m_cfg.library.archivesDir, m_cfg.import.parseFb2);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Worker thread failed: {}", e.what());
            }
        });

    std::jthread closer([&]()
    {
        try
        {
            for (auto& w : workers) w.join();
            m_resultQueue.Close();
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Closer thread failed: {}", e.what());
        }
    });

    auto startTime = std::chrono::high_resolution_clock::now();
    Db::SImportStats stats = WriterThread(db, m_cfg.import.transactionBatchSize, reporter, totalToProcess, archiveSizes);

    if (producer.joinable()) producer.join();
    if (closer.joinable()) closer.join();

    auto endTime = std::chrono::high_resolution_clock::now();
    stats.totalTimeMs = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count());

    guard.MarkFinished();

    stats.archivesProcessed = db.GetIndexedArchives().size();
    stats.PrintSummary();
    return stats;
}

} // namespace Librium::Indexer
