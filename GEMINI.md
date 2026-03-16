# AI Assistant Context and Rules for Librium

## Project Identity
- **Project Name**: Librium (C++20 Indexing Library)
- **Architecture**: Modular. No `libs/core` layer. Each directory in `libs/` is a standalone static library.

## Critical Naming Rules
- **File Naming**: ALWAYS use **PascalCase** for all project-specific files (`ThisIsFile.cpp`, `RunTests.py`). Standard build files (`CMakeLists.txt`, `vcpkg.json`) are exceptions.
- **CMake Targets**: ALWAYS use **PascalCase** for library and binary targets (`Database`, `Indexer`).
- **Classes**: Always prefix with `C` (`CDatabase`).
- **Interfaces**: Always prefix with `I` (`IDatabase`).

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

## Documentation & Communication
- **English Only**: ALL comments, documentation, and messages MUST be in English.
- **No Transliteration**: Transliteration (writing localized words in Latin) is strictly forbidden.
- **Sync**: Keep `docs/Project_Documentation.md` and `docs/Code_Style_Guidelines.md` updated with ANY structural or naming changes.
- **Precedence**: Instructions in `docs/Code_Style_Guidelines.md` are foundational.
