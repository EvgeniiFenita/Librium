#include "Indexer.hpp"
#include "IndexerInternal.hpp"

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

} // namespace

CIndexer::CIndexer(Config::SAppConfig cfg)
    : m_cfg(std::move(cfg))
{
}

Db::SImportStats CIndexer::Run(Db::IBookWriter& db, EImportMode mode, const std::shared_ptr<IProgressReporter>& reporter)
 
{
    Config::CBookFilter filter(m_cfg.filters);

    m_skipArchives.clear();
    size_t totalToProcess = 0;
    size_t totalPreSkipped = 0;

    LOG_INFO("Pre-scanning INPX to calculate exact book count and group by archive...");
    Inpx::CInpParser scanner;
    std::unordered_map<std::string, size_t> archiveSizes;

    if (mode == EImportMode::Upgrade)
    {
        for (const auto& archiveName : db.GetIndexedArchives())
        {
            m_skipArchives.insert(archiveName);
        }
    }

    [[maybe_unused]] const auto scanStats = scanner.ParseStreaming(m_cfg.library.inpxPath, [&](Inpx::SBookRecord&& rec) 
    {
        if (m_stopRequested) return false;
        
        if (mode == EImportMode::Upgrade) 
        {
            if (m_skipArchives.count(rec.archiveName)) 
            {
                ++totalPreSkipped;
                return true;
            }
        }

        auto filterRes = filter.ShouldInclude(rec);
        if (filterRes) 
        {
            ++archiveSizes[rec.archiveName];
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

    Detail::CImportGuard guard(db);

    int workerCount = std::max(1, m_cfg.import.threadCount);
    LOG_INFO("Starting import: {} worker threads, mode={}, total to process={}", workerCount, ImportModeToString(mode), totalToProcess);

    std::jthread producer([this, &filter, mode]()
    {
        try
        {
            Inpx::CInpParser parser;
            (void)parser.ParseByArchive(m_cfg.library.inpxPath, [&](const std::string& archiveName, std::vector<Inpx::SBookRecord>&& records)
            {
                if (m_stopRequested)
                {
                    return false;
                }

                if (mode == EImportMode::Upgrade && m_skipArchives.count(archiveName))
                {
                    return true;
                }

                std::vector<Inpx::SBookRecord> filteredRecords;
                filteredRecords.reserve(records.size());
                for (auto& record : records)
                {
                    auto filterRes = filter.ShouldInclude(record);
                    if (filterRes)
                    {
                        filteredRecords.push_back(std::move(record));
                    }
                }

                if (filteredRecords.empty())
                {
                    return true;
                }

                return Detail::TryPushQueueItem(m_workQueue, SWorkItem{archiveName, std::move(filteredRecords)}, "work");
            });

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

    db.ClearImportCaches();
    guard.MarkFinished();

    stats.archivesProcessed = db.GetIndexedArchives().size();
    stats.PrintSummary();
    return stats;
}

} // namespace Librium::Indexer
