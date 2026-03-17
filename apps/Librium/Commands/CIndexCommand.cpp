#include "CIndexCommand.hpp"
#include "Indexer/Indexer.hpp"
#include "Log/Logger.hpp"
#include "Utils.hpp"
#include "Version.hpp"
#include <iostream>
#include <csignal>

namespace Librium::Apps {

static Indexer::CIndexer* g_activeIndexer = nullptr;
static void OnSignal(int) 
{
    if (g_activeIndexer) g_activeIndexer->RequestStop();
}

void CImportCommand::Setup(CLI::App& app) 
{
    auto* sub = app.add_subcommand("import", "Full import from INPX");
    sub->add_option("--config", m_configPath, "Config JSON path")->required();
    sub->add_option("--threads", m_threads, "Override thread count");
    sub->add_flag("--no-fb2", m_noFb2, "Skip FB2 parsing");
    sub->add_option("--log-level", m_logLevel, "Log level");
    
    sub->callback([this]() { m_upgrade = false; });
}

void CUpgradeCommand::Setup(CLI::App& app) 
{
    auto* sub = app.add_subcommand("upgrade", "Add only new archives");
    sub->add_option("--config", m_configPath, "Config JSON path")->required();
    sub->add_option("--threads", m_threads, "Override thread count");
    sub->add_flag("--no-fb2", m_noFb2, "Skip FB2 parsing");
    sub->add_option("--log-level", m_logLevel, "Log level");
    
    sub->callback([this]() { m_upgrade = true; });
}

int CImportCommand::Execute() 
{
    Config::CAppConfig cfg;
    try 
    { 
        cfg = Config::CAppConfig::Load(m_configPath); 
    }
    catch (const std::exception& e) 
    { 
        std::cerr << "Config error: " << e.what() << "\n"; return 1; 
    }

    if (m_threads > 0) cfg.import.threadCount = m_threads;
    if (m_noFb2)       cfg.import.parseFb2 = false;
    if (m_upgrade)     cfg.import.mode = "upgrade";

    auto level = ParseLogLevel(m_logLevel, ParseLogLevel(cfg.logging.level));
    SetupLogging(level, cfg.logging.file);

    std::signal(SIGINT, OnSignal);

    LOG_INFO("Librium v{} — mode: {}", VersionString, cfg.import.mode);

    try
    {
        Indexer::CIndexer indexer(cfg);
        g_activeIndexer = &indexer;
        (void)indexer.Run();
        g_activeIndexer = nullptr;
    }
    catch (const std::exception& e) 
    {
        LOG_ERROR("Fatal: {}", e.what());
        return 1;
    }
    return 0;
}

} // namespace Librium::Apps
