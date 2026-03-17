# Guide: Adding New Commands to Librium CLI

This document outlines the standard procedure for adding a new subcommand to the unified Librium CLI application.

## 1. Define the Command Class
Create a new header and source file in `apps/Librium/Commands/`.
Follow the **PascalCase** naming convention (e.g., `CMyNewCommand.hpp` and `CMyNewCommand.cpp`).

### Header Template (`CMyNewCommand.hpp`)
```cpp
#pragma once

#include "ICommand.hpp"
#include <string>

namespace Librium::Apps {

class CMyNewCommand : public ICommand
{
public:
    void Setup(CLI::App& app) override;
    int Execute() override;

private:
    std::string m_someOption;
    // Add other members...
};

} // namespace Librium::Apps
```

### Implementation Template (`CMyNewCommand.cpp`)
```cpp
#include "CMyNewCommand.hpp"
#include "Log/Logger.hpp"
#include "Utils.hpp"

namespace Librium::Apps {

void CMyNewCommand::Setup(CLI::App& app) 
{
    auto* sub = app.add_subcommand("my-command", "Brief description of the command");
    sub->add_option("--option", m_someOption, "Description of the option")->required();
}

int CMyNewCommand::Execute() 
{
    try {
        // Implementation logic...
        LOG_INFO("Command executed successfully");
        return 0;
    } catch (const std::exception& e) {
        LOG_ERROR("Command failed: {}", e.what());
        return 1;
    }
}

} // namespace Librium::Apps
```

## 2. Register the Command in Main
Update `apps/Librium/Main.cpp`:
1.  Include the new header: `#include "Commands/CMyNewCommand.hpp"`
2.  Add the command to the `commands` vector in `main()`:
    ```cpp
    commands.push_back(std::make_unique<CMyNewCommand>());
    ```
3.  Add the execution logic in the subcommand check section:
    ```cpp
    if (app.get_subcommand("my-command")->parsed()) return commands[N]->Execute();
    ```
    *Note: Ensure the index `N` matches the order in the vector.*

## 3. Update Build System
Update `apps/Librium/CMakeLists.txt`:
Add the new `.cpp` file to the `add_executable(Librium ...)` list.

## 4. Documentation
1.  Add usage examples to `docs/Project_Documentation.md`.
2.  Mention any new dependencies or configuration changes.

## 5. Testing
1.  **Unit Tests**: If the command uses new logic in `libs/`, add Catch2 tests in `tests/Unit/`.
2.  **Integration Tests**: Add a new test case to `tests/Integration/RunIntegrationTests.py` to verify the CLI behavior.

## 6. Verification
Run the following scripts to ensure everything is correct:
1.  `.\Build.ps1 -Preset x64-debug`
2.  `.\RunAllTests.ps1 -Preset x64-debug`
