# Guide: Adding New Actions to Librium Engine

This document outlines the standard procedure for adding a new JSON-based action to the Librium Engine.

## 1. Define the Action Class
Create a new class in `libs/Service/Actions/Actions.hpp` (or a separate file if it's large) that inherits from `IServiceAction`.

```cpp
// libs/Service/Actions/Actions.hpp
class CMyNewAction : public IServiceAction 
{
public:
    // 1. Define the command name (used for JSON dispatching)
    std::string GetName() const override { return "my-new-action"; }

    // 2. Declare the execution logic
    nlohmann::json Execute(
        CAppService& service, 
        const nlohmann::json& params, 
        Indexer::IProgressReporter* reporter = nullptr) override;
};
```

## 2. Implement the Logic
Add the implementation in `libs/Service/Actions/Actions.cpp`.

```cpp
// libs/Service/Actions/Actions.cpp
nlohmann::json CMyNewAction::Execute(CAppService& service, const nlohmann::json& params, Indexer::IProgressReporter* reporter)
{
    // 1. Get resources from service
    auto& api = service.GetApi();

    // 2. Parse params
    std::string someValue = params.value("my_param", "default");

    // 3. Do work (optionally use reporter)
    if (reporter) reporter->OnProgress(50, 100);

    // 4. Return result
    return {{"status", "ok"}, {"data", {{"result", "done"}}}};
}
```

## 3. Register the Action
Add the action to the `CAppService` constructor in `libs/Service/AppService.cpp`. Registration is now automatic via `GetName()`.

```cpp
CAppService::CAppService(Config::SAppConfig cfg) : m_config(std::move(cfg))
{
    RegisterAction(std::make_unique<CImportAction>());
    RegisterAction(std::make_unique<CUpgradeAction>());
    // ...
    RegisterAction(std::make_unique<CMyNewAction>());
}
```

## 4. Verification
1.  **Add a Scenario**: Create a new `.json` file in `tests/Scenarios/` to test your action.
2.  **Run Pipeline**: Execute `python scripts/run.py --preset x64-debug`.
3.  **Check Audit**: Verify the command and response in `Librium.log`.

## 5. Implementation Rules
-   **No direct IO**: Never use `std::cin`, `std::cout`, or `printf`. The engine application uses `ICommandChannel` for input/output. Use `LOG_*` macros for logging and return data via JSON.
-   **Error Handling**: Wrap logic in `try-catch` (or let `CAppService::Run` handle it). Always return `{"status": "error", "error": "msg"}` for predictable failures.
-   **Paths**: Always use `service.GetConfig()` or `service.GetApi()` to interact with project resources.
