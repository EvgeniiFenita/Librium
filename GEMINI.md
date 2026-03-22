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
- **Architecture**: Modular. Standalone static libraries in `libs/`.

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
- **Enums**: Prefix with `E` (`ELogLevel`).
- **Member Variables**: Prefix with `m_` (`m_dbPath`).

## Coding & Formatting Style
- **Braces**: ALWAYS use **Allman style** (opening brace on a NEW line).
- **Namespaces**: Use C++20 nested namespace syntax (`namespace Librium::Db { ... }`).
- **Unicode**: ALWAYS use `Librium::Config::Utf8ToPath()` when creating paths from strings. NEVER use `path.string()` on Windows; use `path.u8string()` if conversion is needed.
- **Thread Safety**: ALWAYS use `std::jthread` for thread lifecycle. Wrap thread entry functions in `try-catch` blocks to prevent `std::terminate`.
- **Error Handling**: NEVER use `catch (...)`. ALWAYS catch at minimum `const std::exception&` and log the error using `LOG_ERROR` before handling or skipping. Silent exception swallowing is a build-breaking violation.
- **RAII Compliance**: ALWAYS wrap third-party handles (sqlite3, sqlite3_stmt, zip_t, etc.) in RAII containers (like `std::unique_ptr` with custom deleters) IMMEDIATELY upon acquisition. Manual resource management (manual `close`/`finalize`) is strictly forbidden. Ownership must be established in the same scope as acquisition.

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

---

## Build & Environment
- **Generator**: Ninja (preferred) or Visual Studio 18 2026.
- **Master Pipeline**: `python scripts/Run.py --preset <preset> [--real-library <path>]`. ALWAYS use this script for building and testing. DO NOT use `Build.py` or `Test.py` directly.
- **Presets**: `x64-debug`, `x64-release`, `linux-debug`, `linux-release`.
- **Dependencies**: `vcpkg` in manifest mode. Requires `VCPKG_ROOT` env var.

## Testing Standards
- **Unified Runner**: ALWAYS verify changes using `python scripts/Run.py`.
- **Stages**: 
  1. **Unit**: Fast C++ tests via Catch2.
  2. **Scenarios**: Data-driven behavioral tests using `LibraryGenerator` (synthetic data) and `ScenarioTester`.
  3. **Smoke (Real)**: Optional full-scale import test on real `Lib.rus.ec` data (triggered by `--real-library`).
- **New Features**: Every new CLI command, search parameter, or SQLite function MUST have corresponding tests (Unit or Scenario).
- **Artifacts**: All temporary data must stay in `out/artifacts/<preset>/`. NEVER create files in `libs/`, `apps/`, or `tests/`.

## Reference Documentation
- **[Project Overview](docs/Project_Documentation.md)**: Architecture, directory layout, and CLI usage.
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
- **Database Layer Isolation**: Files in `libs/Indexer/`, `libs/Service/`, `libs/Query/`, and all files under `apps/` MUST NEVER include `<sqlite3.h>` directly. If you find yourself writing `sqlite3_` anywhere outside of `CSqliteDatabase` or `CSqliteStatement`, stop — you are in the wrong layer. All database interaction MUST go through `ISqlDatabase` and `ISqlStatement` interfaces, or through the `CDatabase` public API. The only files permitted to call the raw SQLite C API are `CSqliteDatabase.cpp`, `CSqliteStatement.cpp`, and `SqliteFunctions.cpp`.
- **Unicode Search**: All text searches MUST be case-insensitive for Cyrillic. This is achieved by using the `librium_upper()` custom SQLite function and searching against denormalized `search_title` or `search_name` columns. Never use `LIKE` on raw `title` or `name` columns for user queries.

### IPC Architecture Rules
- **IPC Architecture**:
    - **Transport Layer**: Only `libs/Transport` (Asio). Must remain format-agnostic.
    - **Protocol Layer**: Only `libs/Protocol` (JSON/Base64). Must be the ONLY place for `nlohmann/json` dependency.
    - **Service Layer**: Clean business logic. Must ONLY use `IRequest` and `IResponse` interfaces for communication. Interacts with `Database` to fulfill requests.

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
