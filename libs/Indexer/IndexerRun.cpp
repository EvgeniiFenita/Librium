#include "Indexer.hpp"
#include "IndexerInternal.hpp"

#include "Log/Logger.hpp"
#include "Utils/StringUtils.hpp"

#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

namespace Librium::Indexer {

using Utils::CStringUtils;

Db::SImportStats CIndexer::Run(Db::IBookWriter& db, EImportMode mode, const std::shared_ptr<IProgressReporter>& reporter)
{
    Config::CBookFilter filter(m_cfg.filters);

    m_skipArchives.clear();
    const auto preScan = Detail::PreScanArchives(*this, db, mode, m_cfg, filter);

    if (preScan.totalToProcess == 0)
    {
        LOG_INFO("No new books to process in {} mode.", Detail::ImportModeToString(mode));
        Db::SImportStats emptyStats;
        emptyStats.booksSkipped = preScan.totalPreSkipped;
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

    Detail::CImportGuard guard(db);

    int workerCount = std::max(1, m_cfg.import.threadCount);
    LOG_INFO("Starting import: {} worker threads, mode={}, total to process={}", workerCount, Detail::ImportModeToString(mode), preScan.totalToProcess);

    std::jthread producer = Detail::StartProducerThread(*this, filter, mode);

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
    Db::SImportStats stats = WriterThread(db, m_cfg.import.transactionBatchSize, reporter, preScan.totalToProcess, preScan.archiveSizes);

    if (producer.joinable()) producer.join();
    if (closer.joinable()) closer.join();

    auto endTime = std::chrono::high_resolution_clock::now();
    stats.totalTimeMs = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count());

    db.ClearImportCaches();
    guard.MarkFinished();

    stats.archivesProcessed = db.GetIndexedArchives().size();
    stats.PrintSummary();
    return stats;
}

} // namespace Librium::Indexer
