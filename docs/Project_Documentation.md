# Librium — Project Documentation

**Librium** is a high-performance C++20 toolset designed to parse, index, and query large home library collections distributed in INPX format (e.g., Librusec or Flibusta archives).

---

## 1. Project Architecture

The project is organized into independent, reusable static libraries and a single unified CLI application. All project-specific targets and files follow the **PascalCase** naming convention.

### Module Overview

| Module | Responsibility | Dependencies |
| :--- | :--- | :--- |
| **Log** | Thread-safe singleton logger. Silent by default, supports file and console outputs. | None |
| **Zip** | Unicode-aware RAII wrapper around `libzip`. Supports long paths and кириллица. | `libzip`, `zlib` |
| **Fb2** | XML parser for FictionBook 2.0 metadata using `pugixml`. | `pugixml` |
| **Inpx** | High-speed parser for `.inpx` and `.inp` collection indices. | **Zip**, **Config** |
| **Config** | JSON-based configuration and cross-platform path helpers (`Utf8ToPath`). | **Inpx**, `nlohmann_json` |
| **Database** | Thread-safe SQLite wrapper with optimized batch insertion. | **Fb2**, **Inpx**, `sqlite3` |
| **QueryLib** | Search engine and JSON serialization for query results. | **Database**, `nlohmann_json` |

---

## 2. Directory Layout

```text
Librium/
├── apps/
│   └── Librium/            ← Unified CLI application
├── libs/
│   ├── Config/             ← App settings & Unicode path helpers
│   ├── Database/           ← SQLite schema & data persistence
│   ├── Fb2/                ← Metadata extraction from books
│   ├── Inpx/               ← Collection index parsing
│   ├── Log/                ← Centralized logging
│   ├── Query/              ← Search logic (CMake target: QueryLib)
│   └── Zip/                ← Unicode-aware archive handling
├── scripts/                ← Unified Python automation pipeline
│   ├── run.py              ← Master entry point (orchestrator)
│   ├── build.py            ← Build automation script
│   └── test.py             ← Multi-stage test runner
├── tests/
│   ├── Scenarios/          ← Data-driven E2E scenarios (.json)
│   └── Unit/               ← Catch2 test suite
├── Dockerfile.linux        ← Linux build environment
├── CMakeLists.txt          ← Root build configuration
└── CMakePresets.json       ← Cross-platform build presets
```

---

## 3. Automation Pipeline (Python)

The project uses a unified Python-based pipeline for all platforms. **All temporary artifacts (DBs, logs, configs) are stored in `out/build/<preset>/`**, keeping the source tree clean.

### Master Entry Point: `scripts/run.py`
This is the recommended way to work with the project. It automatically handles build and all test stages.

```powershell
# Build and run all tests (Unit + Integration) on Windows
python scripts/run.py --preset x64-release

# Full pipeline on Linux (via Docker) including Real Library tests
python scripts/run.py --preset linux-release --real-library "C:/Path/To/Library"

# Clean build
python scripts/run.py --preset x64-debug --clean
```

### Test Stages in `scripts/test.py`
1.  **Stage 1: UNIT**: Fast C++ unit tests (Catch2).
2.  **Stage 2: SCENARIO**: Behavioral tests. 
    - Uses `LibGen.py` to create a "miniature" realistic library.
    - Executes CLI commands via `ScenarioRunner.py` and validates JSON/text outputs.
    - Includes **Smoke (Real)** test if `--real-library` path is provided.

---

## 4. Platform Support

### Windows (Native)
- **Compiler**: MSVC 19.40+ (Visual Studio 2022)
- **Environment**: Requires `VCPKG_ROOT` environment variable.
- **Unicode**: Full support for Cyrillic paths in archives and configurations.

### Linux (Docker / WSL)
- **Compiler**: GCC 13+
- **Runner**: Uses `Dockerfile.linux` (Ubuntu 24.04).
- **Usage**: Use `linux-debug` or `linux-release` presets with `scripts/run.py`.

---

## 5. CLI Usage Examples

### Building the Database
1.  **Initialize config**: `.\Librium.exe init-config --output AppConfig.json`
2.  **Import**: `.\Librium.exe import --config AppConfig.json`
3.  **Stats**: `.\Librium.exe stats --db library.db`

### Querying and Exporting
1.  **Search**: `.\Librium.exe query --db library.db --author "Толстой" --output out.json`
2.  **Export**: `.\Librium.exe export --config AppConfig.json --id 123 --out ./Books/`

---

## 6. Unicode & Path Handling (Crucial)

To ensure Windows compatibility with non-ASCII paths:
- **Never** use `std::filesystem::path::string()` for paths that might contain Unicode.
- **Always** use `Librium::Config::Utf8ToPath(std::string)` when converting UTF-8 (from JSON/CLI) to a path object.
- **Zip Archives**: The `ZipReader` is specialized to handle Unicode paths on Windows using Win32 wide-character APIs.
