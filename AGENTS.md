# AGENTS.md — Librium

> **Conflict resolution:** This file wins over all other documents.
> Order of precedence: `AGENTS.md` → `docs/ProjectDocumentation.md` → everything else.

---

## Project Identity

**Librium** — high-performance C++20 tool for indexing and browsing home book collections in INPX format (Librusec, Flibusta).

Architecture: **C++20 persistent TCP engine** + **Node.js (Express) web proxy** + **SQLite**.

```
Browser  →  Node.js (Express, :8080)  →  Librium.exe (C++, TCP :9001)  →  SQLite
```

---

## Build & Test — Always Use This

```bash
# Debug build + all tests
python scripts/Run.py --preset x64-debug

# Release build
python scripts/Run.py --preset x64-release

# Launch web interface
python scripts/Run.py --preset x64-release --web --library "D:/lib.rus.ec"

# Demo mode (~350 books)
python scripts/Run.py --preset x64-debug --web --demo
```

**Rule:** NEVER run tests directly (no `npm test`, no `ctest`, no direct Python scripts).
ALL testing goes through `python scripts/Run.py` only. This keeps artifacts in `out/`.

After any structural change, always run `python scripts/Run.py --preset x64-debug` to verify.

---

## Workflow for Every Task

1. Read `docs/ProjectDocumentation.md` to understand the affected module; use relevant skills for details.
2. Identify which `libs/` module is affected; check its layer boundaries.
3. Write code following all rules below.
4. Run `$code-review` skill to audit the result.
5. Run `python scripts/Run.py --preset x64-debug`.
6. Propose a commit message draft — **NEVER commit without explicit user approval**.

---

## Critical Naming Rules

| Entity | Convention | Example |
|---|---|---|
| Files (project) | PascalCase | `AppConfig.cpp`, `Run.py` |
| CMake targets | PascalCase | `Database`, `Librium` |
| Classes | `C` prefix + PascalCase | `CDatabase` |
| Interfaces | `I` prefix + PascalCase | `IDatabase` |
| Structs | `S` prefix + PascalCase | `SBookRecord` |
| Enums | `E` prefix + PascalCase | `ELogLevel` |
| Enum values | PascalCase | `Success`, `Playing` |
| Member variables | `m_` prefix | `m_dbPath` |
| Methods/Functions | PascalCase | `GetUserById` |
| Parameters | camelCase | `userId`, `queryText` |

Documentation files in `docs/` use PascalCase without underscores.

---

## Code Style — Critical Rules

**Braces:** Allman style (opening brace on new line) for classes, structs, functions, control blocks.
Single-statement `if`/`else`/`for`/`while` bodies may omit braces.
Exception: namespace opening brace stays on same line.

```cpp
// Correct
class CUserService
{
    void DoWork()
    {
        if (condition)
        {
            DoSomething();
        }
    }
};

// Namespace exception
namespace Librium::Db {

} // namespace Librium::Db
```

**Access specifiers:** `public` first, then `protected`, then `private`. No empty lines after specifier.

**Key rules (violations = invalid code):**
- `std::jthread` for thread lifecycle, not `std::thread`
- `nullptr` not `NULL`
- `override` on all virtual method overrides
- `[[nodiscard]]` when ignoring return value is a bug
- No `using namespace` in headers
- `const` wherever possible
- Use `LOG_DEBUG/INFO/WARN/ERROR` macros — never `std::cout`/`std::cerr` in `libs/`
- Unicode paths: always `Utils::CStringUtils::Utf8ToPath()`, never direct string → path conversion

---

## Top LLM Mistakes — Verify Before Finishing

**1. snake_case for methods** — Wrong: `get_user_by_id()` → Right: `GetUserById()`

**2. Raw SQLite outside CSqliteDatabase:**
```cpp
// WRONG
sqlite3_exec(m_db, query, nullptr, nullptr, nullptr);

// RIGHT
m_database->Execute(SqlQueries::kSelectBookById, params);
```

**3. Path from UTF-8 string directly:**
```cpp
// WRONG
std::filesystem::path p = someUtf8String;

// RIGHT
auto p = Utils::CStringUtils::Utf8ToPath(someUtf8String);
```

**4. Inline SQL strings** — All SQL must be `constexpr std::string_view` in `SqlQueries.hpp`.

**5. `UPPER()` instead of `librium_upper()`** — Standard UPPER() breaks Cyrillic.

**6. Silent exception handlers:**
```cpp
// WRONG
catch (...) { }

// RIGHT
catch (const std::exception& e)
{
    LOG_ERROR("Operation failed: {}", e.what());
}
```

**7. K&R brace style** — Allman only. No `void DoWork() {` on same line.

**8. snake_case for parameters** — Wrong: `int user_id` → Right: `int userId`

---

## Architecture Layer Rules

**Layer isolation (hard rule):**
- `libs/Indexer/` and `libs/Service/` → NEVER include `<sqlite3.h>` directly
- Only these 3 files may call raw SQLite C API: `CSqliteDatabase.cpp`, `CSqliteStatement.cpp`, `SqliteFunctions.cpp`
- Business logic uses only `IBookWriter` (import) and `IBookReader` (search)

**IPC layers:**
- Transport (`libs/Transport`) — Asio only, format-agnostic
- Protocol (`libs/Protocol`) — JSON/Base64, ONLY place for `nlohmann/json`
- Service (`libs/Service`) — business logic, uses only `IRequest`/`IResponse`

**Sanctioned exceptions:**
- `libs/Config/AppConfig.cpp` may use `nlohmann/json` for config persistence
- `libs/Log/Logger.cpp` may use `std::fputs`/`std::fflush` directly

---

## Database Rules

- All SQL strings → `constexpr std::string_view` in `SqlQueries.hpp`
- Use `SQLITE_TRANSIENT` (not `SQLITE_STATIC`) for temporary string bindings
- After `sqlite3_step()` returning `SQLITE_ROW`: call `sqlite3_reset()` immediately
- Bulk import: use `BeginBulkImport()`/`EndBulkImport()` for pragma toggling
- Unicode search: use `librium_upper()` against `search_title`/`search_name` columns

---

## C++ Correctness Rules

- Destructors calling throwing operations: wrap in `try-catch`, log `LOG_WARN`, swallow
- Fatal paths (schema creation, bulk import setup): `LOG_ERROR` then re-throw
- RAII: wrap all third-party handles (`sqlite3`, `zip_t`, etc.) in `unique_ptr` with custom deleters immediately on acquisition
- Use `CImportGuard` (or equivalent) to restore state after exceptions in bulk ops
- Bulk ops: always use archive-aware scheduling to minimize disk thrashing

---

## Test Immutability

**NEVER rewrite tests.** When adapting tests to code changes:
1. Minimal change — fix only what broke
2. Never delete test cases, sections, assertions, input data, or expected values
3. Never rename test cases/sections unless explicitly asked
4. Never change assertion logic
5. Never "simplify" or "refactor" tests as part of unrelated tasks

---

## Commit Rules

Format: `<Type>: <Short Summary>` (imperative mood, capitalized Type)

Types: `Feature`, `Refactor`, `Fix`, `Test`, `Docs`, `Build`

```
Fix: Prevent TCP deadlock in web interface     ✅
Feature: Add book cover extraction support     ✅
fix(web): deadlock                             ❌  (lowercase, has scope)
Added test                                     ❌  (past tense)
```

**No auto-commits.** Propose draft and wait for explicit user approval.

---

## No-Touch Zones

- `vcpkg_installed/` — never modify
- `out/` — never modify; fully auto-generated
- `vcpkg.json`, `CMakePresets.json` — only when task explicitly requires it

Refactoring scope: `libs/`, `apps/`, `tests/` only.

---

## Communication

- Code, comments, docs, commit messages: **English only**
- Chat responses to user: **Russian**
- Transliteration: strictly forbidden

---

## Skills Available

Use `$` to invoke skills explicitly, or they trigger implicitly when relevant:

| Skill | When to use |
|---|---|
| `$adding-commands` | Adding a new IServiceAction command |
| `$code-review` | Auditing code for violations and bugs |
| `$code-style` | Quick reference for naming/formatting rules |
| `$commit` | Writing commit messages |
| `$web-interface` | Working on Node.js web layer |

---

## Reference Docs

- `docs/ProjectDocumentation.md` — full architecture, directory layout, module dependency table
