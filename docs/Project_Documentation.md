# Librium — Project Documentation

**Librium** is a high-performance C++20 toolset designed to parse, index, and query large home library collections distributed in INPX format (e.g., Librusec or Flibusta archives).

---

## 1. Project Architecture

The project is organized into independent, reusable static libraries and a single persistent Engine application. All project-specific targets and files follow the **PascalCase** naming convention.

### Module Overview

| Module | Responsibility | Dependencies |
| :--- | :--- | :--- |
| **Log** | Thread-safe singleton logger. Supports multiple outputs and static configuration. | None |
| **Zip** | RAII-based ZIP archive management. Unified smart pointer interface for archives and files. | `libzip`, `zlib` |
| **Fb2** | XML parser for FictionBook 2.0 metadata using `pugixml`. Extracts text info (annotation, etc.) but ignores covers. | `pugixml` |
| **Inpx** | High-speed parser for `.inpx` collection indices. | **Zip**, **Config** |
| **Config** | JSON-based configuration and cross-platform path helpers (`Utf8ToPath`). | **Inpx**, `nlohmann_json` |
| **Database** | Abstraction layer for SQL databases. Generic logic is isolated from application logic via `ISqlDatabase` and `ISqlStatement` interfaces. | **Fb2**, **Inpx**, `sqlite3_lib` |
| **QueryLib** | Search engine and JSON serialization for query results. | **Database**, `nlohmann_json` |
| **Service** | Engine core using the Command pattern to dispatch JSON actions. | **Database**, **Indexer**, **QueryLib**, **Utils** |
| **Utils** | Common technical utilities (Base64, Thread-safe queue, String helpers). | None |

---

## 2. Directory Layout

```text
Librium/
├── apps/
│   └── Librium/            ← Persistent Engine application
├── libs/
│   ├── Config/             ← App settings & Unicode path helpers
│   ├── Database/           ← SQLite schema & data persistence
│   ├── Fb2/                ← Metadata extraction from books
│   ├── Indexer/            ← Multi-threaded indexing logic
│   ├── Inpx/               ← Collection index parsing
│   ├── Log/                ← Centralized logging with CLogger
│   ├── Query/              ← Search logic (CMake target: QueryLib)
│   ├── Service/            ← JSON Protocol & Action dispatching
│   ├── Utils/              ← Shared utilities (Base64, Queue, etc.)
│   └── Zip/                ← Unicode-aware archive handling
├── scripts/                ← Unified Python automation pipeline
│   ├── run.py              ← Master entry point (orchestrator)
│   ├── build.py            ← Build automation script
│   └── test.py             ← Multi-stage test runner
├── tests/
│   ├── Scenarios/          ← Data-driven E2E scenarios (.json)
│   └── Unit/               ← Catch2 test suite (self-contained)
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
1.  **Stage 1: UNIT**: Fast C++ unit tests (Catch2). Fully self-contained, no external scripts required.
2.  **Stage 2: SCENARIO**: Behavioral tests. 
    - Uses `LibGen.py` to create a "miniature" realistic library.
    - Communicates with `Librium.exe` via Base64-JSON protocol.
    - Includes **Smoke (Real)** test if `--real-library` path is provided.

---

## 4. Dependencies

Librium uses **vcpkg** in manifest mode for dependency management:
-   `nlohmann-json`: JSON for modern C++.
-   `pugixml`: Light-weight XML parser.
-   `libzip`: Zip archive handling.
-   `catch2`: Unit testing framework.
-   `sqlite3`: Built from amalgamation (included in project).

---

## 5. Interface & Protocol (Engine Mode)

Librium works as a persistent high-performance **Engine**. It must be started with a mandatory configuration file.

### Startup
```bash
Librium.exe --config <path_to_config.json>
```

### Protocol Specification
-   **Communication**: Via `stdin` (requests) and `stdout` (responses).
-   **Format**: Every message is a single **Base64-encoded JSON** string ending with a newline (`\n`).
-   **Logs**: Human-readable logs and audit trails are written to the file specified in the config (default: `Librium.log`).

### Message Types

#### 1. Command (Request)
```json
{
  "action": "action_name",
  "params": { ... }
}
```

#### 2. Response (Final)
```json
{
  "status": "ok",
  "data": { ... }
}
```
Or in case of failure:
```json
{
  "status": "error",
  "error": "Description of what went wrong"
}
```

#### 3. Progress Update (Intermediate)
During long operations (`import`, `upgrade`), the engine emits periodic updates:
```json
{
  "status": "progress",
  "data": {
    "processed": 1500,
    "total": 100000
  }
}
```

### Supported Actions
| Action | Parameters | Description |
| :--- | :--- | :--- |
| `import` | *none* | Full library re-indexing using paths from config. |
| `upgrade`| *none* | Incremental update (add only new archives). |
| `query`  | `title`, `author`, `genre`, `series`, `limit`, `offset` | Search books in the database. |
| `stats`  | *none* | Get database summary (books/authors count). |
| `get-book` | `id` (int) | Get full metadata of a single book by ID. |
| `export` | `id` (int), `out` (path) | Extract a book from a ZIP archive. |

---

## 6. Unicode & Path Handling (Crucial)

To ensure Windows compatibility with non-ASCII paths:
- **Never** use `std::filesystem::path::string()` for paths that might contain Unicode.
- **Always** use `Librium::Config::Utf8ToPath(std::string)` when converting UTF-8 (from JSON/CLI) to a path object.
- **Zip Archives**: The `ZipReader` is specialized to handle Unicode paths on Windows using Win32 wide-character APIs.

---

## 7. Performance & Robustness

### High-Performance Import
- **Archive-Aware Scheduling**: The indexer groups books by archive before processing. This ensures that each ZIP file is opened once and read linearly, minimizing HDD disk thrashing.
- **Fast Upgrade**: The `upgrade` command automatically skips archives that are already marked as indexed in the database, reducing incremental update time from minutes to seconds.
- **Index Management**: Search indexes are dropped before bulk imports and recreated afterward to maintain constant O(1) insertion speed.
- **Database Caching**: Internal memory caches for Authors, Genres, Series, and Publishers IDs minimize SQLite lookup overhead during mass indexing.
- **Worker Parallelism**: Supports high thread counts (up to 32+ threads) for simultaneous FB2 parsing and XML processing.

### Data Quality & Robustness
- **Recursive FB2 Parsing**: Text metadata (like annotations) is extracted recursively, ensuring clean text even when nested within complex XML tags (e.g., `<strong>`, `<a>`, `<v>`).
- **Encoding Auto-Detection**: The engine prioritizes UTF-8 validation. If a file is not valid UTF-8, it automatically attempts conversion from Windows-1251 (CP1251), ensuring readability of legacy Russian books.
- **Safe JSON Protocol**: Outgoing JSON messages use a byte-replacement strategy for invalid UTF-8 sequences to prevent engine crashes on "dirty" library data.

### Comprehensive Validation
The `RealLibraryTest.py` script performs multi-stage validation:
1. **Clean Import**: Verifies progress reporting and 100% completion.
2. **Data Integrity**: Randomly samples 10 books and inspects every database field for completeness and correct encoding.
3. **Incremental Logic**: Verifies that a secondary `upgrade` command correctly skips all existing archives.
4. **Export Integrity**: Validates exported books by checking file size and searching for valid XML signatures (`<FictionBook`) in the content.

