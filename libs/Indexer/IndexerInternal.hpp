#pragma once

#include "Indexer.hpp"

#include "Log/Logger.hpp"

#include <filesystem>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace Librium::Indexer::Detail {

[[nodiscard]] std::string_view ImportModeToString(EImportMode mode);

[[nodiscard]] std::filesystem::path ResolveArchivePath(const std::string& archivesDir, const std::string& archiveName);

void WriteCoverFile(const Config::SAppConfig& cfg, int64_t bookId, const Fb2::SFb2Data& fb2);

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

class CImportGuard
{
public:
    explicit CImportGuard(Db::IBookWriter& db);
    ~CImportGuard();

    void MarkFinished()
    {
        m_finished = true;
    }

private:
    Db::IBookWriter& m_db;
    bool             m_finished{false};
};

struct SPreScanResult
{
    size_t totalToProcess{0};
    size_t totalPreSkipped{0};
    std::unordered_map<std::string, size_t> archiveSizes;
};

[[nodiscard]] SPreScanResult PreScanArchives(
    CIndexer& indexer,
    Db::IBookWriter& db,
    EImportMode mode,
    const Config::SAppConfig& cfg,
    Config::CBookFilter& filter);

std::jthread StartProducerThread(
    CIndexer& indexer,
    Config::CBookFilter& filter,
    EImportMode mode);

} // namespace Librium::Indexer::Detail
