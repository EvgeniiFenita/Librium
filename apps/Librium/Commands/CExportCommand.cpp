#include "CExportCommand.hpp"
#include "Config/AppConfig.hpp"
#include "Database/Database.hpp"
#include "Log/Logger.hpp"
#include "Zip/ZipReader.hpp"
#include "Utils.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace Librium::Apps {

void CExportCommand::Setup(CLI::App& app) 
{
    auto* sub = app.add_subcommand("export", "Export book from archive");
    sub->add_option("--config",   m_configPath,   "Config JSON path");
    sub->add_option("--db",       m_dbPath,       "Path to library.db");
    sub->add_option("--archives", m_archivesDir,  "Archives directory");
    sub->add_option("--id",       m_bookId,       "Book database ID")->required();
    sub->add_option("--out",      m_outputPath,   "Output directory")->required();
    sub->add_option("--log-level", m_logLevel,    "debug/info/warn/error");
}

int CExportCommand::Execute() 
{
    Config::CAppConfig cfg;
    if (!m_configPath.empty())
    {
        try { cfg = Config::CAppConfig::Load(m_configPath); }
        catch (const std::exception& e) 
        { 
            std::cerr << "Config error: " << e.what() << "\n"; 
            return 1; 
        }
    }

    if (m_dbPath.empty()) m_dbPath = cfg.database.path;
    if (m_archivesDir.empty()) m_archivesDir = cfg.library.archivesDir;

    if (!m_logLevel.empty()) 
    {
        SetupLogging(ParseLogLevel(m_logLevel));
    }
    else if (!cfg.logging.level.empty())
    {
        SetupLogging(ParseLogLevel(cfg.logging.level), cfg.logging.file);
    }

    if (m_dbPath.empty())
    {
        std::cerr << "Error: Database path not specified (use --db or --config)\n";
        return 1;
    }

    if (m_archivesDir.empty())
    {
        std::cerr << "Error: Archives directory not specified (use --archives or --config)\n";
        return 1;
    }

    try
    {
        Db::CDatabase db(m_dbPath);
        auto bookInfo = db.GetBookPath(m_bookId);
        
        if (!bookInfo)
        {
            LOG_ERROR("Book with ID {} not found in database", m_bookId);
            return 1;
        }

        std::string zipName = bookInfo->archiveName;
        if (zipName.size() < 4 || zipName.substr(zipName.size() - 4) != ".zip")
        {
            zipName += ".zip";
        }

        std::filesystem::path zipPath = Utf8ToPath(m_archivesDir) / Utf8ToPath(zipName);
        if (!std::filesystem::exists(zipPath))
        {
            LOG_ERROR("Archive not found: {}", zipPath.string());
            return 1;
        }

        LOG_INFO("Extracting {} from {}...", bookInfo->fileName, bookInfo->archiveName);

        auto data = Zip::CZipReader::ReadEntry(zipPath, bookInfo->fileName);
        
        std::filesystem::path outDir = Utf8ToPath(m_outputPath);
        if (!std::filesystem::exists(outDir))
        {
            std::filesystem::create_directories(outDir);
        }

        std::filesystem::path outPath = outDir / Utf8ToPath(bookInfo->fileName);
        std::ofstream ofs(outPath, std::ios::binary);
        if (!ofs)
        {
            LOG_ERROR("Failed to open output file for writing: {}", outPath.string());
            return 1;
        }

        ofs.write(reinterpret_cast<const char*>(data.data()), data.size());
        LOG_INFO("Successfully exported to {}", outPath.string());
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Export failed: {}", e.what());
        return 1;
    }

    return 0;
}

} // namespace Librium::Apps
