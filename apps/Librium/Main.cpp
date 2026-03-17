#include "Version.hpp"
#include "Utils.hpp"
#include "Commands/CIndexCommand.hpp"
#include "Commands/CQueryCommand.hpp"
#include "Commands/CHelperCommands.hpp"

#include <CLI/CLI.hpp>
#include <iostream>
#include <vector>
#include <memory>

using namespace Librium::Apps;

int main(int argc, char* argv[]) 
{
    (void)argc;
    (void)argv;

    std::vector<std::string> args;
#ifdef _WIN32
    args = get_utf8_args();
#else
    for (int i = 0; i < argc; ++i) args.push_back(argv[i]);
#endif

    CLI::App app{"Librium - INPX library manager"};
    app.set_version_flag("--version,-v", std::string("Librium v") + VersionString);
    app.require_subcommand(1);

    std::vector<std::unique_ptr<ICommand>> commands;
    commands.push_back(std::make_unique<CImportCommand>());
    commands.push_back(std::make_unique<CUpgradeCommand>());
    commands.push_back(std::make_unique<CQueryCommand>());
    commands.push_back(std::make_unique<CStatsCommand>());
    commands.push_back(std::make_unique<CInitConfigCommand>());

    for (auto& cmd : commands) 
    {
        cmd->Setup(app);
    }

    std::vector<const char*> argv_utf8;
    for (const auto& a : args) argv_utf8.push_back(a.c_str());

    try 
    {
        app.parse((int)argv_utf8.size(), argv_utf8.data());
    } 
    catch (const CLI::ParseError &e) 
    {
        return app.exit(e);
    }

    if (app.get_subcommand("import")->parsed()) return commands[0]->Execute();
    if (app.get_subcommand("upgrade")->parsed()) return commands[1]->Execute();
    if (app.get_subcommand("query")->parsed()) return commands[2]->Execute();
    if (app.get_subcommand("stats")->parsed()) return commands[3]->Execute();
    if (app.get_subcommand("init-config")->parsed()) return commands[4]->Execute();

    return 0;
}
