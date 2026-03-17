# AI Assistant Context and Rules for Librium

## Project Identity
- **Project Name**: Librium (C++20 Indexing Library)
- **Architecture**: Modular. No `libs/core` layer. Each directory in `libs/` is a standalone static library.

## Critical Naming Rules
- **File Naming**: ALWAYS use **PascalCase** for all project-specific files (`ThisIsFile.cpp`, `RunTests.py`). Standard build files (`CMakeLists.txt`, `vcpkg.json`) are exceptions.
- **CMake Targets**: ALWAYS use **PascalCase** for library and binary targets (`Database`, `Librium`).
- **Classes**: Always prefix with `C` (`CDatabase`).
- **Interfaces**: Always prefix with `I` (`IDatabase`).
- **Structs**: Always prefix with `S` (`SBookRecord`).
- **Enums**: Always prefix with `E` (`ELogLevel`).
- **Member Variables**: Always prefix with `m_` (`m_dbPath`).

## Coding & Formatting Style
- **Braces**: ALWAYS use **Allman style** (opening brace on a NEW line) for functions, classes, structs, and control blocks.
- **Parameters**: Keep function parameters on a single line unless they are too numerous or exceed line length limits.
- **Namespaces**: Use C++20 nested namespace syntax (`namespace Librium::Db { ... }`).

## Build & Environment
- **Generator**: Visual Studio 18 2026.
- **Preset**: `x64-debug`.
- **Build Command**: `.\Build.ps1 -Preset x64-debug`.
- **Dependencies**: Managed via `vcpkg` in manifest mode.

## Testing Standards
- **Master Script**: ALWAYS verify changes using `.\RunAllTests.ps1 -Preset x64-debug`.
- **Unit Tests**: Catch2, located in `tests/Unit`.
- **Integration Tests**: Python 3, located in `tests/Integration`.

## Refactoring Guidelines
- **No Monoliths**: Avoid creating large, all-encompassing libraries.
- **Modularity**: New features should go into new modules in `libs/` with their own `CMakeLists.txt`.
- **Includes**: Every library must add `libs/` to its include paths. Headers must be included as `#include "ModuleName/Header.hpp"`.

## Safety & Precision Rules
- **Dependency Protection**: NEVER modify files in `vcpkg_installed/`, `out/`, `.git/`, or `.vs/`. All mass-refactoring (regex) MUST be restricted to a whitelist: `libs/`, `apps/`, `tests/`.
- **Accurate Refactoring**: 
    - Use word boundaries (`\b`) in regex to avoid partial matches.
    - Double-check that renaming internal symbols doesn't break CLI arguments or string literals (e.g., `--author` should not become `--CAuthor`).
    - When adding prefixes, ensure `#include` directives stay prefix-free if the file name hasn't changed.
- **Verification Workflow**: After any structural change or mass-rename, ALWAYS run:
    1. `.\Build.ps1 -Preset x64-debug`
    2. `.\RunAllTests.ps1 -Preset x64-debug`

## Documentation & Communication
- **English Only**: ALL comments, documentation, and messages MUST be in English.
- **No Transliteration**: Transliteration (writing localized words in Latin) is strictly forbidden.
- **Sync**: Keep `docs/Project_Documentation.md` and `docs/Code_Style_Guidelines.md` updated with ANY structural or naming changes.
- **Maintenance**: ALWAYS check existing documentation in the `docs/` directory during research. If your changes affect documented behavior or architecture, update the relevant files immediately.
- **Commands**: Refer to `docs/Adding_New_Commands.md` when extending the CLI.
