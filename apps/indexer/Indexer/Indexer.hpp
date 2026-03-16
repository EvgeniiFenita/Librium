#pragma once

#include "Config/AppConfig.hpp"
#include "Database/Database.hpp"
#include "Fb2/Fb2Parser.hpp"
#include "Inpx/BookRecord.hpp"
#include "ThreadSafeQueue.hpp"

#include <atomic>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace LibIndexer::Indexer {

class CIndexer
{
public:
    explicit CIndexer(Config::CAppConfig cfg);

    [[nodiscard]] Db::ImportStats Run();
    void RequestStop() { m_stopRequested = true; }

    [[nodiscard]] std::vector<std::string> GetNewArchives(
        Db::CDatabase& db, const std::string& inpxPath);

private:
    Config::CAppConfig        m_cfg;
    std::atomic<bool>         m_stopRequested{false};
    std::unordered_set<std::string> m_skipArchives;
    std::atomic<size_t>       m_parsedCount{0};
    std::atomic<size_t>       m_filteredCount{0};
    std::atomic<size_t>       m_errorCount{0};

    struct WorkItem   { Inpx::BookRecord record; };
    struct ResultItem { Inpx::BookRecord record; Fb2::Fb2Data fb2; };

    static constexpr size_t k_workQueueSize   = 5000;
    static constexpr size_t k_resultQueueSize = 2000;

    CThreadSafeQueue<WorkItem>   m_workQueue{k_workQueueSize};
    CThreadSafeQueue<ResultItem> m_resultQueue{k_resultQueueSize};

    void ProducerThread(const std::string& inpxPath,
                        const Config::CBookFilter& filter);

    void WorkerThread(const std::string& archivesDir, bool parseFb2);

    Db::ImportStats WriterThread(Db::CDatabase& db, size_t batchSize);
};

} // namespace LibIndexer::Indexer
