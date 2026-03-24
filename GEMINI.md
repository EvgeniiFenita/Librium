# AI Assistant Context and Rules for Librium

## Document Hierarchy (resolve conflicts using this order)

When rules in different files appear to conflict, this order decides which file wins:

1. `GEMINI.md` — **highest priority**, overrides all other documents
2. `docs/Code_Style_Guidelines.md` — code formatting and naming rules
3. `docs/Adding_New_Commands.md` — feature implementation guide
4. `docs/CodeReviewInstructions.md` — audit checklist
5. `docs/Commit_Message_Guidelines.md` — git history standards
6. Other docs — reference only

---

## Project Identity
- **Project Name**: Librium (C++20 Indexing Library)
- **Architecture**: Modular. Persistent C++ Engine + Node.js Web Proxy.

## Workflow for New Tasks

Follow these steps **in order** before writing any code:

1. Read the relevant docs in `docs/` to understand the affected module.
2. Identify which `libs/` module is affected and check its layer boundaries.
3. Write code following all rules in this file.
4. Cross-check your output against `docs/CodeReviewInstructions.md` before finishing.
5. Run `python scripts/Run.py --preset x64-debug` to confirm nothing is broken.
6. Propose a commit message draft — **NEVER commit without explicit user approval**.

---

## Critical Naming Rules
- **File Naming**: ALWAYS use **PascalCase** for all project-specific files (`ThisIsFile.cpp`, `Run.py`). Files should NOT include the `C` or `I` prefix, even if the primary class/interface inside does.
- **CMake Targets**: ALWAYS use **PascalCase** for targets (`Database`, `Librium`).
- **Classes**: Prefix with `C` (`CDatabase`).
- **Interfaces**: Prefix with `I` (`IDatabase`).
- **Structs**: Prefix with `S` (`SBookRecord`).
- **Enums**: Prefix with `E` (`ELogLevel`). Enum values use **PascalCase** (`Success`, `Playing`).
- **Member Variables**: Prefix with `m_` (`m_dbPath`).
- **Function Parameters**: Use **camelCase** (`userId`, `queryText`). Never `user_id` or `UserID`.
- **Methods/Functions**: Use **PascalCase** (`GetUserById`, `ExecuteQuery`). Never snake_case.
- **Access Specifier Order**: Always declare `public` first, then `protected`, then `private`. No empty lines after specifiers.
- **Namespace Closing Comment**: Always close a namespace with a comment: `} // namespace Librium::Db`.

## Coding & Formatting Style
- **Braces**: ALWAYS use **Allman style** (opening brace on a NEW line) for classes, structs, functions, and control blocks. Exception: namespace opening brace stays on the **same line** as the declaration. Single-statement `if`/`else`/`for`/`while` bodies MAY omit braces.
- **Namespaces**: Use C++20 nested namespace syntax (`namespace Librium::Db { ... }`). Always close with a comment.
- **Unicode**: ALWAYS use `Librium::Config::Utf8ToPath()` when creating paths from strings. NEVER use `path.string()` on Windows; use `path.u8string()` if conversion is needed. **Exception**: `libs/Log/Logger.cpp` defines its own local `Utf8ToPath()` in an anonymous namespace to avoid a circular dependency — do NOT change this.
- **Thread Safety**: ALWAYS use `std::jthread` for thread lifecycle. Wrap thread entry functions in `try-catch` blocks to prevent `std::terminate`.
- **Error Handling**: NEVER use `catch (...)`. ALWAYS catch at minimum `const std::exception&` and log the error using `LOG_ERROR` before handling or skipping. Silent exception swallowing is a build-breaking violation.
- **RAII Compliance**: ALWAYS wrap third-party handles (sqlite3, sqlite3_stmt, zip_t, etc.) in RAII containers (like `std::unique_ptr` with custom deleters) IMMEDIATELY upon acquisition. Manual resource management (manual `close`/`finalize`) is strictly forbidden. Ownership must be established in the same scope as acquisition.
- **`override` / `final`**: ALWAYS use `override` when overriding a virtual method. Use `final` to prevent further overriding.
- **`nullptr`**: Use `nullptr` instead of `NULL` or `0` for pointer initialization.
- **`[[nodiscard]]`**: Mark functions with `[[nodiscard]]` when ignoring the return value is likely a bug.
- **No `using namespace` in Headers**: `using namespace` is forbidden in any header file.
- **`#include` What You Use**: Each file must include only the headers it actually uses. No unnecessary includes.
- **Const Correctness**: Use `const` whenever possible — on methods, parameters, and local variables.
- **Logging**: ALWAYS use `LOG_DEBUG`, `LOG_INFO`, `LOG_WARN`, `LOG_ERROR` macros. Never call `CLogger` methods directly. Never use `std::cout`/`std::cerr` in `libs/`. **The logger is silent by default** — it will not output to the console unless `AddConsoleOutput()` is explicitly called.

## Common LLM Mistakes — DO NOT DO THIS

These are the most frequent code generation errors. Verify your output against each one before finishing.

### Mistake 1 — snake_case for method names

Wrong:
```cpp
void get_user_by_id(int id);
```
Right:
```cpp
void GetUserById(int id);
```

### Mistake 2 — Raw SQLite handles or direct API calls outside CSqliteDatabase

Wrong (forbidden in Indexer.cpp, Service.cpp, Query.cpp, or any apps/ file):
```cpp
sqlite3_exec(m_db, query, nullptr, nullptr, nullptr);
sqlite3_finalize(stmt); // Manual resource management
```
Right:
```cpp
m_database->Execute(SqlQueries::kSelectBookById, params);
// In CSqliteDatabase: Use std::unique_ptr with custom deleters
```

### Mistake 3 — Creating a filesystem path directly from a UTF-8 string

Wrong:
```cpp
std::filesystem::path p = someUtf8String;
std::filesystem::path p(someUtf8String);
```
Right:
```cpp
auto p = Librium::Config::Utf8ToPath(someUtf8String);
```

### Mistake 4 — Inline SQL strings in business logic

Wrong:
```cpp
m_db->Execute("SELECT * FROM books WHERE id = ?");
```
Right — define the constant in SqlQueries.hpp, then reference it:
```cpp
// In SqlQueries.hpp:
constexpr std::string_view kSelectBookById = "SELECT * FROM books WHERE id = ?";

// In the caller:
m_db->Execute(SqlQueries::kSelectBookById, params);
```

### Mistake 5 — Using standard UPPER() instead of librium_upper()

Standard `UPPER()` in SQLite does not support Unicode (Cyrillic). 
Wrong:
```sql
WHERE title LIKE UPPER(?)
```
Right:
```sql
WHERE search_title LIKE librium_upper(?)
```

### Mistake 6 — Silent or catch-all exception handlers

Wrong:
```cpp
catch (...) { }
catch (const std::exception&) { /* silently ignored */ }
```
Right:
```cpp
catch (const std::exception& e)
{
    LOG_ERROR("Operation failed: {}", e.what());
    // then handle or re-throw
}
```

### Mistake 7 — K&R brace style instead of Allman

Wrong:
```cpp
class CUserService {
    void DoWork() {
    }
};
```
Right:
```cpp
class CUserService
{
    void DoWork()
    {
    }
};
```

### Mistake 8 — snake_case for parameter names

Wrong:
```cpp
void GetUserById(int user_id);
void Execute(std::string query_text);
```
Right:
```cpp
void GetUserById(int userId);
void Execute(std::string queryText);
```

---

## Build & Environment
- **Generator**: Ninja (preferred) or Visual Studio 18 2026.
- **Master Pipeline**: `python scripts/Run.py --preset <preset> [--real-library <path>] [--clean]`. ALWAYS use this script for building and testing. DO NOT use `Build.py` or `Test.py` directly.
- **Presets**: `x64-debug`, `x64-release`, `linux-debug`, `linux-release`.
- **Dependencies**: `vcpkg` in manifest mode. Requires `VCPKG_ROOT` env var.

## Python Scripting Style

Automation scripts in `scripts/` must follow these rules to maintain consistency with the main C++ codebase:

- **File Naming**: Use **PascalCase** (`Run.py`, `LibraryGenerator.py`).
- **Class Naming**: Prefix with `C` (`CLibraryGenerator`, `CSmokeTester`).
- **Method Naming**: Use **PascalCase** (`GenerateLibrary`, `RunTests`).
- **Variable Naming**: Use **snake_case** for local variables; **camelCase** for function parameters.
- **Path Handling**: Use `pathlib.Path` objects for all filesystem operations.
- **Bytecode Prevention**: Always include `sys.dont_write_bytecode = True` and `os.environ["PYTHONDONTWRITEBYTECODE"] = "1"` at the very top of entry-point scripts.
- **Shared Logic**: Common paths and UI utilities must be imported from `Core.py`.

## Testing Standards
- **Unified Runner**: ALWAYS verify changes using `python scripts/Run.py`.
- **Mandatory Script Usage**: ALL tests (Unit, Scenario, Web) MUST be executed ONLY via `python scripts/Run.py`. Direct execution of test binaries, `npm test`, `jest`, or individual Python scripts is strictly FORBIDDEN. This ensures all temporary artifacts and downloaded dependencies are isolated within the `out/` directory, preventing pollution of the `libs/`, `apps/`, `tests/`, or `web/` source folders.
- **Stages**: 
  1. **Unit**: Fast C++ tests via Catch2. Self-contained, no external scripts. Covers filters, encoding, Base64, concurrency, transactions, INPX (streaming & edge cases), ZIP (RAII & edge cases), FB2 (cover extraction & encoding edge cases), logger, search query parser, database `get-book` fields, string sanitization, config (utilities & edge cases).
  2. **Scenarios**: Data-driven behavioral tests using `LibraryGenerator` (synthetic data) and `ScenarioTester`. Communicates with `Librium.exe` via TCP. Covers 8 categories: search operators, query filters, parsing & stats, upgrade/re-import, export, utility (covers), protocol error resilience, data integrity.
  3. **Web**: Backend API tests using **Jest + Supertest** with a mock TCP server (no real engine needed). Run via `python scripts/Run.py test --stage web`. Artifacts and `node_modules` go to `out/artifacts/<preset>/web_test/` — the `web/` source directory must NEVER receive test dependencies. The fbc EPUB converter is **not** downloaded during this stage; tests validate the "converter absent" path only.
  4. **Smoke (Real)**: Optional full-scale import test on real `Lib.rus.ec` data (triggered by `--real-library`).
- **New Features**: Every new CLI command, search parameter, or SQLite function MUST have corresponding tests (Unit or Scenario).
- **Artifacts**: All temporary data must stay in `out/artifacts/<preset>/`. NEVER create files in `libs/`, `apps/`, `tests/`, or `web/`.

### Rule: Test Immutability

**NEVER rewrite tests** — neither unit tests (Catch2) nor integration tests (Python) nor web tests (Jest).

When refactoring or adapting tests to code changes:

1. **Minimal change** — make exactly as many edits as the task requires. If one line is broken, fix one line.
2. **Never delete** test cases, sections (`SECTION`/`SUBCASE`), assertions (`REQUIRE`, `CHECK`, `assert`), input data, or expected values.
3. **Never rename** test cases or sections unless explicitly asked.
4. **Never change assertion logic** — if there was `REQUIRE(a == b)`, it must stay as-is.
5. **Never change test input data** (fixtures, parameters, test strings/numbers/files) unless explicitly asked.
6. **Never "simplify" or "refactor"** tests as part of tasks not directly related to tests.

**Algorithm**: Before editing a test, ask yourself: *"What exactly broke due to changes in the production code?"* Fix only that. Leave everything else untouched.

**Example** — Task: replace `Foo foo(a, b)` with `Foo foo = Foo::create(a, b)`

✅ Correct: find all occurrences of `Foo foo(` in tests and replace with `Foo foo = Foo::create(` — and nothing else.

❌ Wrong: rewrite the entire test case, add new checks, remove sections, change input data.

## Reference Documentation
- **[Project Overview](docs/Project_Documentation.md)**: Architecture, directory layout, and CLI usage.
- **[Web Interface](docs/Web_Interface.md)**: Node.js proxy, LRU caching, and frontend details.
- **[Code Style & Unicode](docs/Code_Style_Guidelines.md)**: Strict naming, formatting, and Unicode path handling rules.
- **[Code Review Checklist](docs/CodeReviewInstructions.md)**: List of common pitfalls and quality standards.
- **[CLI Extensibility](docs/Adding_New_Commands.md)**: Guide for adding new subcommands to the application.
- **[Git Commit Guidelines](docs/Commit_Message_Guidelines.md)**: Rules for writing descriptive and high-signal commit messages.

## Safety & Precision Rules

### Commit Safety
- **No Automatic Commits**: NEVER stage or commit changes unless explicitly requested by the user. Automatic commits are strictly forbidden.

### Database Rules
- **Database Architecture**: ALL SQL strings must be defined as `constexpr std::string_view` constants in `SqlQueries.hpp`. Inline SQL strings anywhere else in the codebase are **forbidden**. Database schema creation must be handled by the `CDatabaseSchema` class. Correct pattern:
  - In `SqlQueries.hpp`: `constexpr std::string_view kSelectBookById = "SELECT * FROM books WHERE id = ?";`
  - In the caller: `m_db->Execute(SqlQueries::kSelectBookById, params);`
- **Database Layer Isolation**: Files in `libs/Indexer/`, `libs/Service/`, `libs/Query/`, and all files under `apps/` MUST NEVER include `<sqlite3.h>` directly. If you find yourself writing `sqlite3_` anywhere outside of `CSqliteDatabase` or `CSqliteStatement`, stop — you are in the wrong layer. Business logic consumers MUST interact with the database through role-specific interfaces: `IBookWriter` (for import/indexing) and `IBookReader` (for search/query). The only files permitted to call the raw SQLite C API are `CSqliteDatabase.cpp`, `CSqliteStatement.cpp`, and `SqliteFunctions.cpp`. `ISqlDatabase` and `ISqlStatement` are low-level interfaces internal to the Database module — do NOT use them outside of it.
- **Unicode Search**: All text searches MUST be case-insensitive for Cyrillic. This is achieved by using the `librium_upper()` custom SQLite function and searching against denormalized `search_title` or `search_name` columns. Never use `LIKE` on raw `title` or `name` columns for user queries.
- **SQLite Binding**: Use `SQLITE_TRANSIENT` (not `SQLITE_STATIC`) when binding temporary string objects, to ensure SQLite copies the data before the binding call returns.
- **SQLite Cursor Lifecycle**: Any `sqlite3_step()` returning `SQLITE_ROW` holds an open read cursor. `sqlite3_reset()` MUST be called immediately after reading the result — before the function returns. Failure causes `SQLITE_LOCKED` on subsequent DDL statements (`DROP INDEX`, `CREATE INDEX`) in the same connection. INSERT statements (`SQLITE_DONE`) do NOT hold cursors.

### C++ Correctness Rules
- **Destructor Exception Handling**: Destructors that call potentially-throwing operations (e.g., `ISqlStatement::Reset()`) MUST wrap the call in `try-catch`, log via `LOG_WARN`, and swallow the exception — never let exceptions escape a destructor.
- **Fatal Errors Must Rethrow**: In critical paths (schema creation, bulk import setup), always `LOG_ERROR` then re-throw — never silently continue. Applies to `CDatabaseSchema::Exec()` and `BeginBulkImport()`.
- **Scope Guards for State Restoration**: Use `CImportGuard` (or equivalent RAII guards) to restore system state (search indexes, SQLite PRAGMAs) after exceptions during bulk operations.

### IPC Architecture Rules
- **IPC Architecture**:
    - **Transport Layer**: Only `libs/Transport` (Asio). Must remain format-agnostic.
    - **Protocol Layer**: Only `libs/Protocol` (JSON/Base64). Must be the ONLY place for `nlohmann/json` dependency.
    - **Service Layer**: Clean business logic. Must ONLY use `IRequest` and `IResponse` interfaces for communication. Interacts with `Database` to fulfill requests.
- **Deliberate Exception — Config Layer**: `libs/Config/AppConfig.cpp` also uses `nlohmann/json` for reading and writing the application configuration file (`config.json`). This is a sanctioned exception to the "Protocol only" rule because the Config layer must be able to persist its own data format independently of the IPC Protocol layer. Do NOT add `nlohmann/json` to any other module.

### Threading & Performance Rules
- **Ownership**: Use `std::unique_ptr` for managing object ownership. Raw pointers are allowed only for non-owning access (observers).
- **Performance Integrity**: Bulk operations (like indexing) MUST use Archive-Aware scheduling to minimize disk thrashing. Utilize `BeginBulkImport()`/`EndBulkImport()` to toggle SQLite pragmas (`journal_mode=OFF`) for maximum throughput. Always utilize memory caches for frequently accessed database IDs (Authors, Genres).

### File System & Encoding Rules
- **FB2 Parsing**: Only text metadata (annotations, keywords, etc.) and cover images are supported. Covers must be extracted and saved to disk next to the database in a `meta/` folder, using the database row `id` as the subfolder name.
- **Encoding Robustness**: All text parsers MUST prioritize UTF-8 validation before attempting legacy encoding conversions (e.g., CP1251).

### Console Output Rules
- **Clean Console**: Every `.cpp` and `.hpp` file under `libs/` must NEVER use `std::cout` or `std::cerr` — not for production output, not for temporary debug output, not under any circumstances. Use `LOG_*` macros exclusively. In `apps/`, `std::cout` is permitted only for final user-facing CLI output, not for diagnostics.

### Scope Rules
- **Mass-refactoring Whitelist**: `libs/`, `apps/`, `tests/` — these are the only directories you are permitted to modify during refactoring.
- **No-Touch Zones**: NEVER modify anything in `vcpkg_installed/` or `out/` — these are fully auto-generated and will be overwritten by the build system. Do NOT edit `vcpkg.json` or `CMakePresets.json` unless the task explicitly requires a dependency or preset change. Touching these files without cause silently breaks the build for all platforms.

### Verification
- **Verification Workflow**: After any structural change, run `python scripts/Run.py --preset x64-debug`.

## Communication
- **Technical Content**: ALL code, comments, documentation, and git commit messages must be in **English**.
- **Chat Language**: ALWAYS respond to the user in **Russian** within the chat interface.
- **No Transliteration**: Strictly forbidden.
- **Maintenance**: Check documentation in `docs/` during research. Update relevant files if architecture or rules change.
