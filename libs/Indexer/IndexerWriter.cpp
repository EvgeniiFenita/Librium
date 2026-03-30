#include "Indexer.hpp"
#include "IndexerInternal.hpp"

#include "Log/Logger.hpp"

#include <chrono>
#include <unordered_map>

namespace Librium::Indexer {

Db::SImportStats CIndexer::WriterThread(Db::IBookWriter& db, size_t batchSize, const std::shared_ptr<IProgressReporter>& reporter, size_t totalBooks,
    const std::unordered_map<std::string, size_t>& archiveSizes)
{
    Db::SImportStats stats;
    size_t inBatch = 0;
    size_t processed = 0;

    double totalDbMs = 0;

    std::unordered_map<std::string, size_t> archiveDone;
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
                        Detail::WriteCoverFile(m_cfg, id, item->fb2);
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
    stats.fb2Errors += m_errorCount.load();
    return stats;
}

} // namespace Librium::Indexer
