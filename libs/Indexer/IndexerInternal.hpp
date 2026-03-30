#pragma once

#include "Indexer.hpp"

#include "Log/Logger.hpp"

#include <filesystem>
#include <string_view>
#include <utility>

namespace Librium::Indexer::Detail {

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

} // namespace Librium::Indexer::Detail
