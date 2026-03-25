# Code Review Instructions for Librium

This document defines the checklist and report format for code reviews of the **Librium** project.

## Scope

A review covers a comprehensive, read-only audit of the codebase. Reviewers must not modify any files or apply fixes during the review — issues are reported only.

## What to audit

Scan ALL source files under:
- `libs/` (Config, Database, Fb2, Indexer, Inpx, Log, Protocol, Service, Transport, Utils, Zip)
- `apps/Librium/` (Main.cpp, Version.hpp, ProtocolProgressReporter.hpp)
- `tests/` (Unit, Scenarios)
- `CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json`
- `scripts/Run.py`, `scripts/ScenarioTester.py`

---

## Review categories

Analyze every file for the following, in this priority order:

### 🔴 CRITICAL — Security & Data Integrity
- **SQL Injection**: Improperly sanitized inputs in `Database` or `Query` modules.
- **Buffer Overflows**: Unsafe string or buffer handling, especially when parsing INPX or FB2. **WinAPI Note**: Ensure `WideCharToMultiByte` and `MultiByteToWideChar` lengths are handled correctly (exclude `\0` from `std::string` length).
- **Binary Data Handling**: Base64 decoding in `Fb2Parser` must be robust against whitespace and malformed input. Ensure cover images are not held in memory longer than necessary.
- **Resource Leaks**: SQLite handles, prepared statements, file descriptors (ZIP), or memory not freed on error paths. **Mandatory**: Use RAII wrappers (e.g., `unique_ptr` with custom deleters) for all third-party handles.
- **File System Operations**: Ensure `std::filesystem` usage correctly handles Unicode paths via `Utf8ToPath`. Cover writing logic in `Indexer` must have proper error handling and directory management.
- **Hardcoded Paths**: Sensitive local paths or credentials left in code or example configs.
- **Thread Safety**: Race conditions in `Log` (singleton), `ThreadSafeQueue`, or `Database` during multithreaded indexing. **Lifecycle**: Ensure `std::jthread` is used for automatic joining.

### 🔴 CRITICAL — Bugs and correctness
- **Logic Errors**: Flaws in INPX streaming, FB2 parsing, or query filtering logic.
- **Algorithm Correctness**: Incorrect or edge-case-prone implementation of algorithms.
- **Undefined Behavior**: Signed overflows, invalid iterator use, uninitialized variables.
- **SQLite Binding Errors**: Use of `SQLITE_STATIC` for temporary objects (ensure `SQLITE_TRANSIENT` is used where needed).
- **SQLite Cursor Lifecycle**: Any `sqlite3_step()` returning `SQLITE_ROW` on a SELECT statement holds an open read cursor. **`sqlite3_reset()` MUST be called immediately after reading the result** — before the function returns. Failure to do so causes `SQLITE_LOCKED` on subsequent DDL statements (`DROP INDEX`, `CREATE INDEX`) in the same connection, even within the same transaction. INSERT statements (`SQLITE_DONE`) do NOT hold cursors.
- **Exception Safety**: Exceptions thrown across boundaries without proper catching or RAII cleanup. **Guard Logic**: Use scope guards (like `CImportGuard`) to restore system state (indexes, PRAGMAs) after exceptions.
- **Fatal Errors Must Rethrow**: In critical paths (schema creation, bulk import setup), always `LOG_ERROR` then `throw;` — never silently continue. Example: `CDatabaseSchema::Exec()` and `BeginBulkImport()`.
- **Destructor Exception Handling**: Destructors that call potentially-throwing operations (e.g., `ISqlStatement::Reset()`) must wrap the call in try-catch, log via `LOG_WARN`, and swallow — never let exceptions escape a destructor. Example: `CSqlStmtResetGuard` destructor pattern.

### 🟠 HIGH — Architecture & Modularity
- **Layer Violations**: Libraries in `libs/` depending on `apps/` or circular dependencies between libraries.
- **Database Layer Isolation**: Business logic consumers (`CIndexer`, `CLibraryApi`) MUST interact with the database ONLY through role-specific interfaces: `IBookWriter` (import/indexing path) and `IBookReader` (search/query path). `CDatabase` is the concrete implementation — it must not be referenced directly from `libs/Indexer/` or `libs/Service/`. `ISqlDatabase` and `ISqlStatement` are low-level interfaces internal to the Database module and MUST NOT be used outside it. Direct calls to `sqlite3_*` API outside of `CSqliteDatabase.cpp`, `CSqliteStatement.cpp`, and `SqliteFunctions.cpp` are forbidden. **Search strategy**: (1) grep `libs/Indexer/` and `libs/Service/` for `CDatabase` — any direct use is a violation; (2) grep all files outside the three permitted files for the token `sqlite3_` — any match is an architecture violation.
- **Monolith Re-emergence**: Any attempt to re-introduce a "core" layer or tightly coupled modules.
- **Include Strategy**: Headers must be included as `#include "ModuleName/Header.hpp"`. No `C/I/E/S` prefixes in filenames.
- **Module Isolation**: Each module in `libs/` should be a standalone static library.

### 🟠 HIGH — Code style violations (per Code_Style_Guidelines.md)
For EVERY violation found, cite the file, line number, and the offending code snippet. Check all rules:
- **Naming**: 
    - Classes: `C` prefix (`CDatabase`).
    - Interfaces: `I` prefix (`ICommand`).
    - Structs: `S` prefix (`SBookRecord`).
    - Enums: `E` prefix (`ELogLevel`).
    - Member variables: `m_` prefix (`m_dbPath`).
    - Functions/Methods: **PascalCase** (`ExecuteQuery`).
    - Parameters: **camelCase** (`queryText`).
    - Enum values: **PascalCase** (`Success`).
- **File Naming**: **PascalCase** for ALL project files (`AppConfig.cpp`).
- **Target Naming**: **PascalCase** for CMake targets (`Database`, `Librium`).
- **Language**: **STRICTLY NO TRANSLITERATION**. Everything (comments, logs, messages) must be in **English**.
- **Brace Style**: **Allman style** (opening brace on a NEW line) for functions, classes, structs, and control blocks.
- **Namespaces**: C++20 nested syntax (`namespace Librium::Db { ... }`). Opening brace on the **same line**.
- **Access Specifiers**: No empty lines after `public:`, `private:`, `protected:`.
- **Include Order**: 1. Header, 2. Std, 3. Third-party, 4. Project.

### 🟡 MEDIUM — Maintainability
- **Algorithm Efficiency**: Inefficient data structures or O(N^2) complexity where O(N log N) or better is expected.
- **Dead Code**: Unused functions, variables, or commented-out code. **Search strategy**: look for private or free functions/methods that are never called anywhere in the codebase; methods declared in a header but with no callers (grep for the method name across all `.cpp` files).
- **Complexity**: Overly long functions (e.g., in `Indexer.cpp` or `Database.cpp`) that should be refactored.
- **Logging**: Use logging macros (`LOG_INFO`, `LOG_ERROR`, etc.) instead of direct `CLogger` calls.
- **Single Responsibility**: Classes or functions doing too many unrelated things.
- **Modern C++**: `nullptr` over `NULL`, `override` for virtual methods, `[[nodiscard]]` for important returns.

### 🟡 MEDIUM — Error handling
- **Swallowed Exceptions**: Catch blocks that do nothing or only log without re-throwing if critical.
- **Inconsistent Reporting**: Mixing `std::cerr`, `printf`, and `LOG_*` macros.
- **Opaque Errors**: Throwing `std::runtime_error` without helpful context.

### 🔵 LOW — C++ Best Practices & Quality
- **Typos and Grammar**: Typos in comments, log messages, documentation, or string literals.
- **C++20 Features**: Opportunities to use `std::span`, `std::format`, `ranges`, or `concepts`.
- **Optimization**: Missing `const&`, unnecessary copies, opportunities for `std::move`.
- **Constants**: Use of magic numbers instead of `constexpr` or named enums.

### 🔵 LOW — Build System & Tests
- **CMake**: Missing warning flags or hardcoded absolute paths. Target names must be PascalCase.
- **Test Gaps**: Modules with no negative tests (e.g., corrupted ZIP or malformed INPX).
- **Scenario Coverage**: Every new feature or fix should have a new `.json` scenario in `tests/Scenarios/`.
- **Unit Test Coverage**: `tests/Unit/` covers filters (genres/size/authors/keywords), string encoding (UTF-8/CP1251/UTF-16), Base64, thread-safe concurrency, database transactions and bulk import/index operations, INPX streaming and edge cases, ZIP RAII, logger configuration and concurrent logging, query edge cases and combined filters, config utilities, JSON Protocol serialization (`CJsonProtocol`), App Service command dispatching (`CAppService`), Library API operations (`CLibraryApi`), and Indexer background tasks (`CIndexer::Run()`, `CIndexer::RequestStop()`). New modules should have matching unit tests.
- **Keyword Filtering**: `SFiltersConfig::excludeKeywords` is parsed, serialized, and applied in `CBookFilter::ShouldInclude()` via substring matching against `rec.keywords`. Unit tests covering positive matches, partial matches, empty-keywords passthrough, and exclusion reason are in `tests/Unit/TestBookFilter.cpp`.

---

## Executive Summary
<2–4 sentence summary of overall quality, most critical issues, and risk level>

---

## 🔴 CRITICAL

### [SHORT TITLE]
- **File:** `path/to/File.cpp:42`
- **Issue:** Clear description of the problem
- **Impact:** What can go wrong
- **Recommendation:** Fix direction (no code changes)

---

## 🟠 HIGH
...

## 🟡 MEDIUM
...

## 🔵 LOW
...

---

## Statistics
| Category | Count |
|---|---|
| Critical | N |
| High | N |
| Medium | N |
| Low | N |
| **Total** | **N** |

---

## Files reviewed
<list all files examined>
```

---

## Rules for the Reviewer

1. **Read-Only**: Do not modify any file.
2. **Precision**: Cite exact file paths and line numbers.
3. **English Only**: The report must be written in English.
4. **No Noise**: Skip categories if a file is clean.
5. **Prioritize**: Focus on Security and Architecture first.
