#pragma once

#include "Config/AppConfig.hpp"
#include "Database/Database.hpp"
#include "Fb2/Fb2Parser.hpp"
#include "Inpx/BookRecord.hpp"
#include "Utils/ThreadSafeQueue.hpp"
#include "ProgressReporter.hpp"

#include <atomic>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace Librium::Indexer {

enum class EImportMode
{
    Full,
    Upgrade
};

class CIndexer
{
public:
    explicit CIndexer(Config::SAppConfig cfg);

    [[nodiscard]] Db::SImportStats Run(Db::CDatabase& db, EImportMode mode, IProgressReporter* reporter = nullptr);
    void RequestStop()
    {
        m_stopRequested = true;
    }

    [[nodiscard]] std::vector<std::string> GetNewArchives(Db::CDatabase& db, const std::string& inpxPath);

private:
    Config::SAppConfig        m_cfg;
    std::atomic<bool>         m_stopRequested{false};
    std::unordered_set<std::string> m_skipArchives;
    std::atomic<size_t>       m_parsedCount{0};
    std::atomic<size_t>       m_filteredCount{0};
    std::atomic<size_t>       m_errorCount{0};

    struct SWorkItem
    {
        Inpx::SBookRecord record;
    };

    struct SResultItem
    {
        Inpx::SBookRecord record;
        Fb2::SFb2Data     fb2;
    };

    static constexpr size_t k_workQueueSize   = 5000;
    static constexpr size_t k_resultQueueSize = 2000;

    Utils::CThreadSafeQueue<SWorkItem>   m_workQueue{k_workQueueSize};
    Utils::CThreadSafeQueue<SResultItem> m_resultQueue{k_resultQueueSize};

    void ProducerThread(const std::string& inpxPath, const Config::CBookFilter& filter);
    void WorkerThread(const std::string& archivesDir, bool parseFb2);
    Db::SImportStats WriterThread(Db::CDatabase& db, size_t batchSize, IProgressReporter* reporter, size_t totalBooks);
};

} // namespace Librium::Indexer
