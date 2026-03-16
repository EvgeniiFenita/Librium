# Code Review Instructions for Librium

You are a senior C++ software engineer performing a thorough code review of the **Librium** project.

## Your task

Perform a comprehensive, read-only audit of the entire codebase. Do NOT modify any files. Do NOT fix anything. Your sole deliverable is a structured review report saved to `ReviewReport.md`.

## What to audit

Scan ALL source files under:
- `libs/` (Config, Database, Fb2, Inpx, Log, Query, Zip)
- `apps/` (Indexer, Query)
- `tests/` (Unit, Integration)
- `CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json`
- `Build.ps1`, `RunAllTests.ps1`

---

## Review categories

Analyze every file for the following, in this priority order:

### 🔴 CRITICAL — Security & Data Integrity
- **SQL Injection**: Improperly sanitized inputs in `Database` or `QueryLib` modules.
- **Buffer Overflows**: Unsafe string or buffer handling, especially when parsing INPX or FB2.
- **Resource Leaks**: SQLite handles, file descriptors (ZIP), or memory not freed on error paths.
- **Hardcoded Paths**: Sensitive local paths or credentials left in code or example configs.
- **Thread Safety**: Race conditions in `Log` (singleton), `ThreadSafeQueue`, or `Database` during multithreaded indexing.

### 🔴 CRITICAL — Bugs and correctness
- **Logic Errors**: Flaws in INPX streaming, FB2 parsing, or query filtering logic.
- **Undefined Behavior**: Signed overflows, invalid iterator use, uninitialized variables.
- **SQLite Binding Errors**: Use of `SQLITE_STATIC` for temporary objects (ensure `SQLITE_TRANSIENT` is used where needed).
- **Exception Safety**: Exceptions thrown across boundaries without proper catching or RAII cleanup.

### 🟠 HIGH — Architecture & Modularity
- **Layer Violations**: Libraries in `libs/` depending on `apps/` or circular dependencies between libraries.
- **Monolith Re-emergence**: Any attempt to re-introduce a "core" layer or tightly coupled modules.
- **Dependency Graph**: Does `Zip` depend on `Log`? Does `Database` only depend on what's necessary?
- **Include Strategy**: Headers must be included as `#include "ModuleName/Header.hpp"`.

### 🟠 HIGH — Code style violations (per Code_Style_Guidelines.md)
For EVERY violation found, cite the file, line number, rule number, and the offending code snippet. Check all rules:
- **Naming**: Classes (C prefix), Interfaces (I), m_ member variables, PascalCase for functions.
- **File Naming**: **PascalCase** for ALL files (`ThisIsFile.cpp`). No `C/I/E` prefixes in filenames.
- **Target Naming**: **PascalCase** for CMake targets (`Database`, `Indexer`).
- **Language**: **STRICTLY NO TRANSLITERATION**. Everything (comments, logs, messages) must be in **English**.
- **Brace Style**: New line for all blocks except namespaces.
- **Namespaces**: Nested `::` syntax, brace on same line, closing comment.
- **Modern C++**: Smart pointers over raw new/delete, `nullptr` over `NULL`.

### 🟡 MEDIUM — Maintainability
- **Dead Code**: Unused functions, variables, or commented-out code.
- **Complexity**: Overly long functions (e.g., in `Indexer.cpp` or `Database.cpp`) that should be refactored.
- **Documentation**: Missing or outdated comments in headers.
- **Single Responsibility**: Classes or functions doing too many unrelated things.

### 🟡 MEDIUM — Error handling
- **Swallowed Exceptions**: Catch blocks that do nothing or only log without re-throwing if critical.
- **Inconsistent Reporting**: Mixing `std::cerr`, `printf`, and `CLogger` inconsistently.
- **Opaque Errors**: Throwing `std::runtime_error` without helpful context (e.g., failing to include the filename in a "file not found" error).

### 🔵 LOW — C++20 Best Practices
- **Modern Features**: Opportunities to use `std::span`, `std::format`, `ranges`, or `concepts`.
- **Optimization**: Missing `const&`, unnecessary copies, opportunities for `std::move`.
- **Constants**: Use of magic numbers instead of `constexpr` or named enums.

### 🔵 LOW — Build System & Tests
- **CMake**: Missing warning flags or hardcoded absolute paths.
- **Test Gaps**: Modules with no negative tests (e.g., what happens if a ZIP is corrupted or an INPX line is malformed?).
- **Integration Coverage**: Missing edge cases in `RunIntegrationTests.py`.

---

## Output format

Write the report to `ReviewReport.md` with this exact structure:
```markdown
# Librium — Code Review Report
**Date:** <date>
**Reviewer:** Gemini CLI
**Scope:** Full codebase audit (libs/, apps/, tests/, build system)

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
