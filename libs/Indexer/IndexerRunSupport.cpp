#include "IndexerInternal.hpp"

#include "Inpx/InpParser.hpp"

namespace Librium::Indexer::Detail {

std::string_view ImportModeToString(EImportMode mode)
{
    return mode == EImportMode::Upgrade ? "upgrade" : "full";
}

SPreScanResult PreScanArchives(
    CIndexer& indexer,
    Db::IBookWriter& db,
    EImportMode mode,
    const Config::SAppConfig& cfg,
    Config::CBookFilter& filter)
{
    SPreScanResult result;

    LOG_INFO("Pre-scanning INPX to calculate exact book count and group by archive...");
    Inpx::CInpParser scanner;

    if (mode == EImportMode::Upgrade)
    {
        for (const auto& archiveName : db.GetIndexedArchives())
        {
            indexer.m_skipArchives.insert(archiveName);
        }
    }

    (void)scanner.ParseStreaming(cfg.library.inpxPath, [&](Inpx::SBookRecord&& rec)
    {
        if (indexer.m_stopRequested) return false;

        if (mode == EImportMode::Upgrade)
        {
            if (indexer.m_skipArchives.count(rec.archiveName))
            {
                ++result.totalPreSkipped;
                return true;
            }
        }

        auto filterRes = filter.ShouldInclude(rec);
        if (filterRes)
        {
            ++result.archiveSizes[rec.archiveName];
            ++result.totalToProcess;
        }
        else
        {
            ++indexer.m_filteredCount;
        }
        return true;
    });

    return result;
}

std::jthread StartProducerThread(CIndexer& indexer, Config::CBookFilter& filter, EImportMode mode)
{
    return std::jthread([&indexer, &filter, mode]()
    {
        try
        {
            Inpx::CInpParser parser;
            (void)parser.ParseByArchive(indexer.m_cfg.library.inpxPath, [&](const std::string& archiveName, std::vector<Inpx::SBookRecord>&& records)
            {
                if (indexer.m_stopRequested)
                {
                    return false;
                }

                if (mode == EImportMode::Upgrade && indexer.m_skipArchives.count(archiveName))
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

                return TryPushQueueItem(indexer.m_workQueue, CIndexer::SWorkItem{archiveName, std::move(filteredRecords)}, "work");
            });

            indexer.m_workQueue.Close();
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Producer thread failed: {}", e.what());
            indexer.m_workQueue.Close();
        }
    });
}

} // namespace Librium::Indexer::Detail
