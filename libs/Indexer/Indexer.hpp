#pragma once

#include "Config/AppConfig.hpp"
#include "Database/IBookWriter.hpp"
#include "Database/Database.hpp"
#include "Fb2/Fb2Parser.hpp"
#include "Inpx/BookRecord.hpp"
#include "Utils/ThreadSafeQueue.hpp"
#include "ProgressReporter.hpp"

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Librium::Indexer {

enum class EImportMode
{
    Full,
    Upgrade
};

class CIndexer;

namespace Detail {
struct SPreScanResult;
SPreScanResult PreScanArchives(
    CIndexer& indexer,
    Db::IBookWriter& db,
    EImportMode mode,
    const Config::SAppConfig& cfg,
    Config::CBookFilter& filter);
std::jthread StartProducerThread(
    CIndexer& indexer,
    Config::CBookFilter& filter,
    EImportMode mode);
}

class CIndexer
{
public:
    explicit CIndexer(Config::SAppConfig cfg);

    [[nodiscard]] Db::SImportStats Run(Db::IBookWriter& db, EImportMode mode, const std::shared_ptr<IProgressReporter>& reporter = nullptr);
    void RequestStop()
    {
        m_stopRequested = true;
    }

    [[nodiscard]] std::vector<std::string> GetNewArchives(Db::IBookWriter& db, const std::string& inpxPath);

private:
    friend Detail::SPreScanResult Detail::PreScanArchives(
        CIndexer& indexer,
        Db::IBookWriter& db,
        EImportMode mode,
        const Config::SAppConfig& cfg,
        Config::CBookFilter& filter);
    friend std::jthread Detail::StartProducerThread(
        CIndexer& indexer,
        Config::CBookFilter& filter,
        EImportMode mode);

    Config::SAppConfig        m_cfg;
    std::atomic<bool>         m_stopRequested{false};
    std::unordered_set<std::string> m_skipArchives;
    std::atomic<size_t>       m_parsedCount{0};
    std::atomic<size_t>       m_filteredCount{0};
    std::atomic<size_t>       m_errorCount{0};

    struct SWorkItem
    {
        std::string                    archiveName;
        std::vector<Inpx::SBookRecord> records;
    };

    struct SResultItem
    {
        Inpx::SBookRecord record;
        Fb2::SFb2Data     fb2;
    };

    // Work queue holds one batch per archive; result queue holds individual books.
    static constexpr size_t k_workQueueSize   = 128;
    static constexpr size_t k_resultQueueSize = 2000;

    Utils::CThreadSafeQueue<SWorkItem>   m_workQueue{k_workQueueSize};
    Utils::CThreadSafeQueue<SResultItem> m_resultQueue{k_resultQueueSize};

    void WorkerThread(const std::string& archivesDir, bool parseFb2);
    Db::SImportStats WriterThread(
        Db::IBookWriter& db,
        size_t batchSize,
        const std::shared_ptr<IProgressReporter>& reporter,
        size_t totalBooks,
        const std::unordered_map<std::string, size_t>& archiveSizes);
};

} // namespace Librium::Indexer
