---
name: code-review
description: >
  Use when performing a code audit or review of Librium source files.
  Triggers on tasks like "review this code", "audit the codebase", "check for
  violations", "run a code review", or "what's wrong with this file".
  Produces a structured report with severity levels. Read-only — never modifies files.
---

# Code Review for Librium

**Scope:** Read-only audit. Report issues only — do not apply fixes.

## Files to Audit

- `libs/` — all modules
- `apps/Librium/` — `Main.cpp`, `Version.hpp.in`, `ProtocolProgressReporter.hpp`
- `tests/` — Unit and Scenarios
- `CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json`
- `scripts/Run.py`, `scripts/ScenarioTester.py`

---

## Review Categories (priority order)

### 🔴 CRITICAL — Security & Data Integrity

- **SQL injection** — unsanitized inputs in Database/Query modules
- **Buffer overflows** — unsafe string/buffer handling in INPX or FB2 parsing. WinAPI: `WideCharToMultiByte`/`MultiByteToWideChar` lengths must exclude `\0`
- **Binary data** — Base64 decoding in Fb2Parser must handle whitespace and malformed input
- **Resource leaks** — SQLite handles, statements, file descriptors, ZIP handles not freed on error paths. Mandatory RAII wrappers
- **Unicode paths** — `std::filesystem` must use `Utf8ToPath()`. Cover-writing must have error handling
- **Hardcoded paths/credentials** — in code or example configs
- **Thread safety** — race conditions in Log singleton, `ThreadSafeQueue`, Database during indexing. Use `std::jthread`

### 🔴 CRITICAL — Bugs & Correctness

- **Logic errors** — INPX streaming, FB2 parsing, query filtering
- **Undefined behavior** — signed overflows, invalid iterator use, uninitialized vars
- **SQLite binding** — `SQLITE_STATIC` on temporary objects (must be `SQLITE_TRANSIENT`)
- **SQLite cursor lifecycle** — any `sqlite3_step()` returning `SQLITE_ROW` holds a cursor. `sqlite3_reset()` must be called immediately after reading — before function returns. Failure → `SQLITE_LOCKED` on DDL. INSERT (`SQLITE_DONE`) does NOT hold cursors
- **Exception safety** — exceptions across boundaries without RAII. Missing `CImportGuard`
- **Fatal paths** — schema creation, bulk import setup must `LOG_ERROR` then re-throw, never silently continue
- **Destructor safety** — destructors calling throwing ops must `try-catch`, `LOG_WARN`, swallow

### 🟠 HIGH — Architecture & Modularity

- **Layer violations** — `libs/` depending on `apps/`, or circular deps
- **Database isolation** — `libs/Indexer/` and `libs/Service/` must never reference `CDatabase` directly or include `<sqlite3.h>`. Search strategy: grep for `CDatabase` in `libs/Indexer/` and `libs/Service/`; grep for `sqlite3_` outside the 3 permitted files
- **`ISqlDatabase`/`ISqlStatement`** — internal to Database module, must not be used elsewhere
- **Protocol purity** — `nlohmann/json` only in `libs/Protocol/` and the sanctioned `libs/Config/AppConfig.cpp` exception
- **Include strategy** — `#include "ModuleName/Header.hpp"`, no C/I/E/S prefixes in filenames

### 🟠 HIGH — Code Style Violations

For every violation: cite file, line number, offending snippet.

- **Naming:** classes `C`, interfaces `I`, structs `S`, enums `E`, members `m_`, methods PascalCase, parameters camelCase, enum values PascalCase
- **File/target naming:** PascalCase
- **No transliteration** — everything in English
- **Brace style:** Allman (opening brace on new line) for functions/classes/structs/control blocks
- **Namespaces:** C++20 nested syntax, opening brace same line, close with comment
- **Access specifiers:** no empty lines after `public:`/`private:`/`protected:`
- **Include order:** 1. own header, 2. std, 3. third-party, 4. project

### 🟡 MEDIUM — Maintainability

- **Dead code** — unused private/free functions never called anywhere
- **Complexity** — overly long functions that should be extracted
- **Logging** — direct `CLogger` calls instead of `LOG_*` macros
- **Modern C++** — missing `nullptr`, `override`, `[[nodiscard]]`

### 🟡 MEDIUM — Error Handling

- **Swallowed exceptions** — catch blocks that do nothing
- **Inconsistent output** — mixing `std::cerr`, `printf`, `LOG_*`
- **Opaque errors** — `std::runtime_error` without context

### 🔵 LOW — Quality & Best Practices

- **Typos** — in comments, log messages, string literals, docs
- **C++20 opportunities** — `std::span`, `std::format`, ranges, concepts
- **Missing `const&`**, unnecessary copies, missing `std::move`
- **Magic numbers** — use `constexpr` or named enums

### 🔵 LOW — Build & Tests

- **CMake** — missing warning flags, hardcoded absolute paths, non-PascalCase targets
- **Test gaps** — modules missing negative tests (corrupted ZIP, malformed INPX)
- **New features without scenarios** — every new command/parameter needs a `.json` scenario

---

## Report Format

```
## Executive Summary
<2–4 sentences: overall quality, most critical issues, risk level>

## 🔴 CRITICAL

### [SHORT TITLE]
- **File:** `path/to/File.cpp:42`
- **Issue:** Clear description
- **Impact:** What can go wrong
- **Recommendation:** Fix direction (no code changes)

## 🟠 HIGH
...

## 🟡 MEDIUM
...

## 🔵 LOW
...

## Statistics
| Category | Count |
|---|---|
| Critical | N |
| High | N |
| Medium | N |
| Low | N |
| **Total** | **N** |

## Files reviewed
<list>
```

## Reviewer Rules

1. **Read-only** — never modify any file
2. **Cite exactly** — file path + line number + snippet for every violation
3. **English only** in the report
4. **No noise** — skip categories if clean
5. **Prioritize** — Security and Architecture first
