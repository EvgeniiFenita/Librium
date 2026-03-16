# AI Assistant Context and Rules for libindexer_project

## Role and Context
You are working on **libindexer_project**, a C++ indexing library.
This project uses **CMake** as its build system and **vcpkg (manifest mode)** for dependency management.

## Build Instructions
When compiling or checking the code, always use the following commands:
```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug
```

## Code Style
**CRITICAL**: You must strictly adhere to the project's code style guidelines. 
Before writing or refactoring any code, ensure you are following the rules defined in the documentation:
- 📖 [Code Style Guidelines](docs/Code_Style_Guidelines.md)
- 📚 [Full Project Documentation](docs/Project_Documentation.md)

## General Rules
1. **Documentation Updates**: Always update the documentation in `docs/Project_Documentation.md` and `docs/Code_Style_Guidelines.md` if you make any architectural, structural, or dependency changes that affect what is written there. Keep the documentation in sync with the codebase.
