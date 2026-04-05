---
name: adding-commands
description: >
  Use when adding a new command (IServiceAction) to the Librium engine.
  Triggers on tasks like "add a new CLI command", "implement new engine action",
  "add support for X in the service layer", or "register a new action".
  Do NOT use for web route changes (use $web-interface instead).
---

# Adding a New Command to Librium

Librium uses the **Command Pattern** via `IServiceAction`. Commands are fully
isolated from network and serialization logic.

## Step 1 — Create the Action Class

Declare in `libs/Service/Actions/Actions.hpp`, implement in `Actions.cpp`.

```cpp
class CMyNewAction : public IServiceAction
{
public:
    std::string GetName() const override { return "my-action"; }

    void Execute(CAppService& service,
                 const IRequest& req,
                 IResponse& res,
                 std::shared_ptr<Indexer::IProgressReporter> reporter) override
    {
        // 1. Extract parameters via IRequest (never touch JSON here)
        std::string name = req.GetString("name", "default");
        int64_t count    = req.GetInt("count", 0);

        // 2. Business logic — NEVER write inline SQL strings here.
        //    All SQL must be constexpr std::string_view in SqlQueries.hpp first.
        try
        {
            // ... call service.GetApi() methods ...

            // 3. Return result
            res.SetData(someStruct);
        }
        catch (const std::exception& e)
        {
            res.SetError(e.what());
        }
    }
};
```

## Step 2 — IRequest Parameter Extraction

```cpp
req.GetString("key", "default")
req.GetInt("key", 0)
req.GetBool("key", false)
req.HasParam("key")
```

## Step 3 — IResponse Return Types

```cpp
res.SetData(Db::SImportStats)
res.SetData(Db::SQueryResult)
res.SetData(SAppStats)          // → JSON: total_books, total_authors, indexed_archives
res.SetData(SBookDetails)
res.SetDataExport(path, filename)  // → JSON: { "file": "...", "filename": "..." }
res.SetError(message)
res.SetProgress(processed, total)  // intermediate progress updates
```

## Step 4 — Register the Action

In `libs/Service/AppService.cpp`, constructor:

```cpp
RegisterAction(std::make_unique<CMyNewAction>());
```

## Step 5 — New Domain Struct? Update Protocol Layer

If you introduced a new return struct:
1. Add `SetData` overload to `libs/Service/Response.hpp`
2. Implement serialization in `libs/Protocol/JsonProtocol.cpp` using `nlohmann::json`

## Step 6 — Integration Test

Create `tests/Scenarios/<category>/MyAction.json`:

```json
{
  "description": "Testing my new action",
  "steps": [
    {
      "args": ["my-action", "--name", "test", "--count", "10"],
      "expected": {
        "status": "ok",
        "data": {}
      }
    }
  ]
}
```

## Completion Checklist

Do not mark done until every box is checked:

- [ ] Class declared in `libs/Service/Actions/Actions.hpp`
- [ ] Class implemented in `libs/Service/Actions/Actions.cpp`
- [ ] Registered in `CAppService` constructor (`AppService.cpp`)
- [ ] If new domain struct: `SetData` overload in `Response.hpp`
- [ ] If new domain struct: serialization in `JsonProtocol.cpp`
- [ ] Scenario `.json` in `tests/Scenarios/` (at least one success case)
- [ ] `python scripts/Run.py --preset x64-debug` passes
