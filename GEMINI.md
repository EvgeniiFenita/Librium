# AI Assistant Context and Rules for Librium

## Project Identity
- **Project Name**: Librium (C++20 Indexing Library)
- **Architecture**: Modular. Standalone static libraries in `libs/`.

## Critical Naming Rules
- **File Naming**: ALWAYS use **PascalCase** for all project-specific files (`ThisIsFile.cpp`, `run.py`). 
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
- **Thread Safety**: ALWAYS wrap thread entry functions in `try-catch` blocks to prevent `std::terminate` on exceptions.

## Build & Environment
- **Generator**: Ninja (preferred) or Visual Studio 18 2026.
- **Master Pipeline**: `python scripts/run.py --preset <preset> [--real-library <path>]`.
- **Presets**: `x64-debug`, `x64-release`, `linux-debug`, `linux-release`.
- **Dependencies**: `vcpkg` in manifest mode. Requires `VCPKG_ROOT` env var.

## Testing Standards
- **Unified Runner**: ALWAYS verify changes using `python scripts/run.py`.
- **Stages**: 1. Unit (Catch2), 2. Integration (Python), 3. Real Library (Heavy indexing).
- **Artifacts**: All temporary data must stay in `out/build/<preset>/`. NEVER create files in `libs/`, `apps/`, or `tests/`.

## Reference Documentation
- **[Project Overview](docs/Project_Documentation.md)**: Architecture, directory layout, and CLI usage.
- **[Code Style & Unicode](docs/Code_Style_Guidelines.md)**: Strict naming, formatting, and Unicode path handling rules.
- **[Code Review Checklist](docs/CodeReviewInstructions.md)**: List of common pitfalls and quality standards.
- **[CLI Extensibility](docs/Adding_New_Commands.md)**: Guide for adding new subcommands to the application.

## Safety & Precision Rules
- **Clean Console**: Libraries (`libs/`) must NEVER use `std::cout`/`std::cerr`. Use `LOG_*` macros instead.
- **Mass-refactoring Whitelist**: `libs/`, `apps/`, `tests/`. NEVER touch `vcpkg_installed/` or `out/`.
- **Verification Workflow**: After any structural change, run `python scripts/run.py --preset x64-debug`.

## Communication
- **English Only**: ALL comments, docs, and messages must be in English.
- **No Transliteration**: Strictly forbidden.
- **Maintenance**: Check documentation in `docs/` during research. Update relevant files if architecture or rules change.
