#include "Indexer.hpp"

#include "Inpx/InpParser.hpp"
#include "Log/Logger.hpp"
#include "Zip/ZipReader.hpp"
#include "Utils.hpp"

#include <filesystem>
#include <unordered_map>

using namespace Librium::Apps;
namespace fs = std::filesystem;

namespace Librium::Indexer {

CIndexer::CIndexer(Config::CAppConfig cfg)
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
            allArchives.push_back(p.stem().string());
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
        parser.ParseStreaming(inpxPath, [&](Inpx::SBookRecord&& rec) 
        {
            if (m_stopRequested) return false;

            if (!m_skipArchives.empty() && m_skipArchives.count(rec.archiveName))
                return true;

            if (!filter.ShouldInclude(rec)) 
            {
                ++m_filteredCount;
                return true;
            }
            m_workQueue.Push(SWorkItem{std::move(rec)});
            return true;
        });
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
                if (it != archivePathCache.end()) 
                {
                    archivePath = it->second;
                }
                else
                {
                    // Try archiveName + .zip, then archiveName as-is
                    for (const auto& suffix : {std::string(".zip"), std::string("")}) 
                    {
                        fs::path p = Utf8ToPath(archivesDir) / Utf8ToPath(result.record.archiveName + suffix);
                        if (fs::exists(p)) 
                        { archivePath = p; break; }
                    }
                    archivePathCache[result.record.archiveName] = archivePath;
                }

                if (!archivePath.empty()) 
                {
                    try
                    {
                        auto data = Zip::CZipReader::ReadEntry(
                            archivePath, result.record.FilePath());
                        result.fb2 = fb2Parser.Parse(data);
                    }
                    catch (const std::exception& e) 
                    {
                        LOG_DEBUG(
                            "FB2 read error [{}]: {}", result.record.FilePath(), e.what());
                        ++m_errorCount;
                    }
                }
            }

            ++m_parsedCount;
            size_t cnt = m_parsedCount.load();
            if (cnt % m_cfg.logging.progressInterval == 0)
                LOG_INFO(
                    "Processed {} books ({} filtered, {} errors)",
                    cnt, m_filteredCount.load(), m_errorCount.load());

            m_resultQueue.Push(std::move(result));
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Worker thread error: {}", e.what());
            ++m_errorCount;
        }
        catch (...)
        {
            LOG_ERROR("Unknown worker thread error");
            ++m_errorCount;
        }
    }
}

Db::SImportStats CIndexer::WriterThread(Db::CDatabase& db, size_t batchSize) 
{
    Db::SImportStats stats;
    size_t inBatch = 0;
    db.BeginTransaction();

    while (true) 
    {
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
                }
                else ++stats.booksSkipped;
            }
            catch (const std::exception& e) 
            {
                LOG_ERROR("DB insert error: {}", e.what());
                ++stats.fb2Errors;
            }
        }

        ++inBatch;
        if (inBatch >= batchSize) 
        {
            db.Commit();
            db.BeginTransaction();
            inBatch = 0;
        }
    }

    try { db.Commit(); } catch (...) 
    { db.Rollback(); }

    stats.booksFiltered = m_filteredCount.load();
    stats.fb2Errors    += m_errorCount.load();
    return stats;
}

Db::SImportStats CIndexer::Run() 
{
    LOG_INFO("Opening database: {}", m_cfg.database.path);
    Db::CDatabase db(m_cfg.database.path);

    Config::CBookFilter filter(m_cfg.filters);
    int numWorkers = std::max(1, m_cfg.import.threadCount);

    std::unordered_set<std::string> archivesInInpx;

    // Upgrade mode: populate skip set
    if (m_cfg.import.mode == "upgrade") 
    {
        auto newArchives = GetNewArchives(db, m_cfg.library.inpxPath);
        if (newArchives.empty()) 
        {
            LOG_INFO("Database is up to date.");
            return {};
        }
        // Skip archives that are ALREADY indexed
        for (const auto& a : db.GetIndexedArchives())
            m_skipArchives.insert(a);
    }

    LOG_INFO(
        "Starting import: {} worker threads, mode={}",
        numWorkers, m_cfg.import.mode);

    std::thread producer([this, &filter, &archivesInInpx]() 
    {
        try
        {
            Inpx::CInpParser parser;
            parser.ParseStreaming(m_cfg.library.inpxPath, [&](Inpx::SBookRecord&& rec) 
            {
                if (m_stopRequested) return false;

                archivesInInpx.insert(rec.archiveName);

                if (!m_skipArchives.empty() && m_skipArchives.count(rec.archiveName))
                    return true;

                if (!filter.ShouldInclude(rec)) 
                {
                    ++m_filteredCount;
                    return true;
                }
                m_workQueue.Push(SWorkItem{std::move(rec)});
                return true;
            });
        }
        catch (const std::exception& e) 
        {
            LOG_ERROR("Producer error: {}", e.what());
        }
        m_workQueue.Close();
    });

    std::vector<std::thread> workers;
    workers.reserve(numWorkers);
    for (int i = 0; i < numWorkers; ++i)
        workers.emplace_back([this]() 
        {
            WorkerThread(m_cfg.library.archivesDir, m_cfg.import.parseFb2);
        });

    std::thread closer([&]() 
    {
        for (auto& w : workers) w.join();
        m_resultQueue.Close();
    });

    Db::SImportStats stats = WriterThread(db, m_cfg.import.transactionBatchSize);

    producer.join();
    closer.join();

    stats.archivesProcessed = archivesInInpx.size();

    // Mark all archives as indexed
    for (const auto& archive : archivesInInpx)
        db.MarkArchiveIndexed(archive);

    stats.archivesProcessed = db.GetIndexedArchives().size();
    stats.PrintSummary();
    return stats;
}

} // namespace Librium::Indexer
