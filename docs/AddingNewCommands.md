# Adding New Commands to Librium

Librium uses the **Command Pattern** implemented via the `IServiceAction` interface. Commands are isolated from the network and serialization logic.

## 1. Create the Action Class

Add your new action class to `libs/Service/Actions/Actions.hpp` and implement it in `Actions.cpp`.

### Interface: `IServiceAction`
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
        // 1. Extract parameters
        std::string name = req.GetString("name", "default");
        int64_t count = req.GetInt("count", 0);

        // 2. Business Logic (using service.GetApi())
        // NOTE: Do NOT write SQL strings here. ALL SQL — including queries,
        //       transaction control (BEGIN TRANSACTION, COMMIT, ROLLBACK), and
        //       PRAGMAs — must be defined in SqlQueries.hpp as
        //       constexpr std::string_view constants first.
        try {
            // ... logic here ...
            
            // 3. Set result data
            // res.SetData(some_domain_struct);
        } catch (const std::exception& e) {
            res.SetError(e.what());
        }
    }
};
```

## 2. Parameter Extraction (`IRequest`)

The `IRequest` interface provides safe access to request parameters without knowing about JSON:
- `GetString(key, default)`
- `GetInt(key, default)`
- `GetBool(key, default)`
- `HasParam(key)`

## 3. Returning Data (`IResponse`)

The `IResponse` interface is used to send the result back. It is overloaded for various domain structures:
- `SetData(Db::SImportStats)`
- `SetData(Db::SQueryResult)`
- `SetData(SAppStats)` — stats result with fields: `totalBooks`, `totalAuthors`, `indexedArchives`. Serialized to JSON as `total_books`, `total_authors`, `indexed_archives`.
- `SetData(SBookDetails)`
- `SetDataExport(const std::filesystem::path& path, const std::string& filename)` — used by export-like commands to return a file path and suggested download filename. Serialized to JSON as `{ "file": "/absolute/path/to/file.fb2", "filename": "Author - Title.fb2" }`.
- `SetError(message)`
- `SetProgress(processed, total)` — used for intermediate updates.

## 4. Registration

Register your new action in the `CAppService` constructor (`libs/Service/AppService.cpp`):

```cpp
CAppService::CAppService(Config::SAppConfig cfg)
    : m_config(std::move(cfg))
{
    // ... existing actions ...
    RegisterAction(std::make_unique<CMyNewAction>());
}
```

## 5. Serialization (Protocol Layer)

If you introduced a **new domain structure** that needs to be returned, you must update the `Protocol` layer to know how to serialize it.

Update `CJsonResponse` in `libs/Protocol/JsonProtocol.cpp`:
1. Add a new `SetData` overload to `libs/Service/Response.hpp`.
2. Implement the serialization logic in `JsonProtocol.cpp` using `nlohmann::json`.

## 6. Integration Test

Create a new `.json` scenario in `tests/Scenarios/` to verify your command:

```json
{
  "description": "Testing my new action",
  "steps": [
    {
      "args": ["my-action", "--name", "test", "--count", "10"],
      "expected": {
        "status": "ok",
        "data": { ... }
      }
    }
  ]
}
```

---

## 7. Completion Checklist

Before considering a new command complete, verify ALL of the following items. Do not mark the task done until every box can be checked.

- [ ] Action class declared in `libs/Service/Actions/Actions.hpp`
- [ ] Action class implemented in `libs/Service/Actions/Actions.cpp`
- [ ] Action registered in `CAppService` constructor in `libs/Service/AppService.cpp`
- [ ] If a new domain struct is returned: `SetData` overload added to `libs/Service/Response.hpp`
- [ ] If a new domain struct is returned: serialization implemented in `libs/Protocol/JsonProtocol.cpp`
- [ ] `.json` scenario created in `tests/Scenarios/` covering at least one success case
- [ ] `python scripts/Run.py --preset x64-debug` passes without errors
