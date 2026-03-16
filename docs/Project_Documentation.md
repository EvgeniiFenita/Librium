# Librium — Project Documentation

**Librium** (formerly libindexer) is a high-performance C++20 toolset designed to parse, index, and query large home library collections distributed in INPX format (e.g., Librusec or Flibusta archives).

---

## 1. Project Architecture

The project is organized into independent, reusable static libraries and specialized applications. All project-specific targets and files follow the **PascalCase** naming convention.

### Module Overview

| Module | Responsibility | Dependencies |
| :--- | :--- | :--- |
| **Log** | Thread-safe singleton logger with `std::format` support. | None |
| **Zip** | RAII wrapper around `libzip` for memory-efficient extraction. | `libzip`, `zlib` |
| **Fb2** | XML parser for FictionBook 2.0 metadata (annotations, covers, genres). | `pugixml` |
| **Inpx** | High-speed parser for `.inpx` and `.inp` collection indices. | **Zip** |
| **Config** | JSON-based application configuration management. | **Inpx**, `nlohmann_json` |
| **Database** | Thread-safe SQLite wrapper with optimized batch insertion logic. | **Fb2**, **Inpx**, `sqlite3` |
| **QueryLib** | Search engine and JSON serialization for query results. | **Database**, `nlohmann_json` |

---

## 2. Directory Layout

```text
Librium/
├── apps/
│   ├── Indexer/            ← Indexer application (multithreaded E2E pipeline)
│   └── Query/              ← Query application (CLI search interface)
├── libs/
│   ├── Config/             ← App settings & JSON loading
│   ├── Database/           ← SQLite schema & data persistence
│   ├── Fb2/                ← Metadata extraction from books
│   ├── Inpx/               ← Collection index parsing
│   ├── Log/                ← Centralized logging
│   ├── Query/              ← Search logic (CMake target: QueryLib)
│   └── Zip/                ← Archive handling
├── tests/
│   ├── Integration/        ← Python-based E2E scenarios
│   └── Unit/               ← Catch2 test suite & test data generator
├── docs/                   ← Project documentation & style guides
├── CMakeLists.txt          ← Root build configuration
└── RunAllTests.ps1         ← Master test runner script
```

---

## 3. Build & Test

### Requirements
- **C++20** compatible compiler (MSVC 19.40+, GCC 13+, Clang 16+)
- **CMake 3.25+**
- **vcpkg** (Manifest Mode)
- **Python 3** (for integration tests and data generation)

### Build Instructions (Windows)
The project uses CMake Presets. Use the root build script for convenience:

```powershell
# Default build (x64-debug)
.\Build.ps1

# Build with clean (removes old cache)
.\Build.ps1 -Clean

# Release build
.\Build.ps1 -Preset x64-release
```

### Running Tests
The master script runs both Unit (C++) and Integration (Python) tests:
```powershell
# Default tests (x64-debug)
.\RunAllTests.ps1

# Release tests
.\RunAllTests.ps1 -Preset x64-release
```

---

## 4. CLI Usage Examples

### Building the Database (Indexer)

1.  **Initialize a default configuration**:
    ```powershell
    .\Indexer.exe init-config --output AppConfig.json
    ```

2.  **Run a full import**:
    ```powershell
    .\Indexer.exe import --config AppConfig.json --threads 8
    ```

3.  **Upgrade an existing database** (only process new archives):
    ```powershell
    .\Indexer.exe upgrade --config AppConfig.json
    ```

4.  **Show database statistics**:
    ```powershell
    .\Indexer.exe stats --db library.db
    ```

### Querying the Database (Query)

The `Query` tool searches the database and outputs results in a structured JSON format.

1.  **Search by author and language**:
    ```powershell
    .\Query.exe --db library.db --output results.json --author "Толстой" --language "ru"
    ```

2.  **Filter by genre and rating**:
    ```powershell
    .\Query.exe --db library.db --output top_sf.json --genre "sf" --rating-min 5
    ```

3.  **Find a specific book by ID**:
    ```powershell
    .\Query.exe --db library.db --output book.json --lib-id "200007"
    ```

4.  **Pagination and Limit**:
    ```powershell
    .\Query.exe --db library.db --output page2.json --limit 20 --offset 20
    ```

5.  **Only books with annotations**:
    ```powershell
    .\Query.exe --db library.db --output with_desc.json --with-annotation
    ```

---

## 5. Configuration (AppConfig.json)

The configuration file defines paths and filters for the indexing process.

```json
{
  "database":  { "path": "library.db" },
  "library":   { "inpxPath": "lib.inpx", "archivesDir": "C:/Books/Archives" },
  "import":    { "parseFb2": true, "threadCount": 4, "mode": "full" },
  "filters": {
    "includeLanguages": ["ru", "en"],
    "excludeGenres": ["humor_anekdot"],
    "minFileSize": 10240
  },
  "logging": { "level": "info", "file": "indexer.log" }
}
```

---

## 6. Development Guidelines

- **Naming**: Use **PascalCase** for all files and CMake targets.
- **Classes**: Prefix with `C` (e.g., `CDatabase`).
- **Safety**: Prefer RAII and smart pointers. Avoid raw `sqlite3` calls outside the `Database` module.
- **Includes**: Use the module-relative format: `#include "Database/Database.hpp"`.
