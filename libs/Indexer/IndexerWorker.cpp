#include "Indexer.hpp"
#include "IndexerInternal.hpp"

#include "Fb2/Fb2Parser.hpp"
#include "Log/Logger.hpp"
#include "Zip/ZipReader.hpp"

#include <filesystem>
#include <unordered_map>

namespace fs = std::filesystem;

namespace Librium::Indexer {

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

            fs::path archivePath;
            auto it = archivePathCache.find(item->archiveName);
            if (it != archivePathCache.end())
            {
                archivePath = it->second;
            }
            else
            {
                archivePath = Detail::ResolveArchivePath(archivesDir, item->archiveName);
                archivePathCache[item->archiveName] = archivePath;
            }

            std::unique_ptr<Zip::CZipReader> zip;
            if (parseFb2 && !archivePath.empty())
            {
                try
                {
                    zip = std::make_unique<Zip::CZipReader>(archivePath);
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("Failed to open archive [{}]: {}", item->archiveName, e.what());
                }
            }

            for (auto& rec : item->records)
            {
                if (m_stopRequested) break;

                try
                {
                    SResultItem result;
                    result.record = std::move(rec);

                    if (zip)
                    {
                        try
                        {
                            auto filePath = result.record.FilePath();
                            auto data = zip->ReadEntry(filePath);
                            result.fb2 = fb2Parser.Parse(data, filePath);
                        }
                        catch (const std::exception& e)
                        {
                            LOG_DEBUG("FB2 read error [{}]: {}", result.record.FilePath(), e.what());
                            ++m_errorCount;
                        }
                    }

                    ++m_parsedCount;
                    if (!Detail::TryPushQueueItem(m_resultQueue, std::move(result), "result"))
                    {
                        break;
                    }
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("Worker record error: {}", e.what());
                    ++m_errorCount;
                }
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Worker thread error: {}", e.what());
            ++m_errorCount;
        }
    }
}

} // namespace Librium::Indexer
