# Guide: Adding New Actions to Librium Engine

This document outlines the standard procedure for adding a new JSON-based action to the Librium Engine.

## 1. Define the Action Class
Create a new class in `libs/Service/Actions/Actions.hpp` (or a separate file if it's large) that inherits from `IServiceAction`.

```cpp
// libs/Service/Actions/Actions.hpp
class CMyNewAction : public IServiceAction 
{
public:
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
    auto& db = service.GetDatabase();
    const auto& cfg = service.GetConfig();

    // 2. Parse params
    std::string someValue = params.value("my_param", "default");

    // 3. Do work (optionally use reporter)
    if (reporter) reporter->OnProgress(50, 100);

    // 4. Return result
    return {{"status", "ok"}, {"data", {{"result", "done"}}}};
}
```

## 3. Register the Action
Add the action to the `CAppService` constructor in `libs/Service/AppService.cpp`.

```cpp
CAppService::CAppService(Config::CAppConfig cfg) : m_config(std::move(cfg))
{
    RegisterAction("import", std::make_unique<CImportAction>());
    // ...
    RegisterAction("my-new-action", std::make_unique<CMyNewAction>());
}
```

## 4. Verification
1.  **Add a Scenario**: Create a new `.json` file in `tests/Scenarios/` to test your action.
2.  **Run Pipeline**: Execute `python scripts/run.py --preset x64-debug`.
3.  **Check Audit**: Verify the command and response in `Librium.log`.

## 5. Implementation Rules
-   **No direct output**: Never use `std::cout` or `printf`. Use `LOG_*` macros for logging and return data via JSON.
-   **Error Handling**: Wrap logic in `try-catch` (or let `CAppService::Dispatch` handle it). Always return `{"status": "error", "error": "msg"}` for predictable failures.
-   **Paths**: Always use `service.GetConfig()` to get base paths for the library or database.
