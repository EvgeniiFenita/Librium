# Librium — Project Documentation

**Librium** is a high-performance C++20 toolset designed to parse, index, and query large home library collections distributed in INPX format (e.g., Librusec or Flibusta archives).

---

## 1. Project Architecture

The project is organized into independent, reusable static libraries and a single persistent Engine application. All project-specific targets and files follow the **PascalCase** naming convention.

### Module Overview

| Module | Responsibility | Dependencies |
| :--- | :--- | :--- |
| **Log** | Thread-safe singleton logger. Supports multiple outputs and static configuration. | **Utils** |
| **Zip** | RAII-based ZIP archive management. Unified smart pointer interface for archives and files. | `libzip`, `zlib`, **Log** |
| **Fb2** | XML parser for FictionBook 2.0 metadata using `pugixml`. Extracts text info (annotation, keywords, etc.) and cover images (base64-decoded). | `pugixml`, **Log**, **Utils** |
| **Inpx** | High-speed parser for `.inpx` collection indices. | **Zip**, **Log**, **Utils** |
| **Config** | JSON-based configuration, path helpers, `CAppPaths` utility class, and book filter logic. | **Inpx**, **Utils**, `nlohmann_json` |
| **Utils** | Common technical utilities (Base64, Thread-safe queue, String helpers, `CStringUtils::Utf8ToPath`). No external dependencies. | None |
| **Database** | Abstraction layer for SQL databases. Low-level SQL access is encapsulated in `ISqlDatabase`/`ISqlStatement` interfaces. Business consumers interact through role-specific interfaces: `IBookWriter` (import/indexing) and `IBookReader` (search/query). `CDatabase` implements both. Includes full query and search engine logic. | **Fb2**, **Inpx**, **Log**, `Sqlite3Lib` |
| **Service** | Engine core using the Command pattern. Abstracts communication via `IRequest`/`IResponse` interfaces. | **Database**, **Indexer**, **Config**, **Inpx**, **Fb2**, **Zip**, **Log**, **Utils** |
| **Protocol** | Implementation of communication formats (e.g., JSON over Base64). | **Service**, **Utils**, `nlohmann_json` |
| **Transport** | Network communication layer (Localhost TCP via **Asio**). | **Log**, `asio` |

---

## 2. Directory Layout

```text
Librium/
├── apps/
│   └── Librium/            ← Persistent Engine application
│       ├── Main.cpp
│       ├── Version.hpp
│       └── ProtocolProgressReporter.hpp
├── libs/
│   ├── Config/             ← App settings & Unicode path helpers
│   ├── Database/           ← SQLite schema, data persistence & search logic
│   ├── Fb2/                ← Metadata extraction from books
│   ├── Indexer/            ← Multi-threaded indexing logic (Producer/Worker/Writer pipeline)
│   ├── Inpx/               ← Collection index parsing
│   ├── Log/                ← Centralized logging with CLogger
│   ├── Protocol/           ← JSON Serialization & Base64 protocol
│   ├── Service/            ← Business logic & Action dispatching
│   │   └── Actions/        ← IServiceAction implementations (Actions.hpp / Actions.cpp)
│   ├── Transport/          ← Asio-based TCP server
│   ├── Utils/              ← Shared utilities (Base64, Queue, etc.)
│   └── Zip/                ← Unicode-aware archive handling
├── scripts/                ← Unified Python automation pipeline
│   ├── Run.py              ← Master entry point (build, test, web server, smoke test)
│   ├── Core.py             ← Shared core logic & paths
│   ├── LibraryGenerator.py ← Synthetic data generator
│   ├── ScenarioTester.py   ← JSON scenario runner
│   └── SmokeTester.py      ← End-to-end validation
├── tests/
│   ├── Scenarios/          ← Data-driven E2E scenarios (.json), organized by category
│   └── Unit/               ← Catch2 test suite (self-contained)
├── web/                    ← Web Interface (Node.js & Vanilla JS)
│   ├── public/             ← Static frontend files
│   ├── lib/                ← Shared backend modules
│   │   ├── engineProcess.js  ← Engine process spawning/lifecycle
│   │   ├── engineClient.js   ← TCP connection, command queue, state machine
│   │   └── coverCache.js     ← LRU in-memory cover cache
│   ├── routes/             ← Express route handlers
│   │   ├── books.js          ← GET /api/books, GET /api/books/:id
│   │   ├── covers.js         ← GET /covers/:id/cover
│   │   ├── download.js       ← GET /api/download/:id (FB2 & EPUB)
│   │   └── library.js        ← import, upgrade, stats endpoints
│   ├── tests/              ← Jest/Supertest API tests (source only; deps go to out/)
│   ├── server.js           ← Thin orchestrator: wires lib/ and routes/
│   ├── package.json        ← Node dependencies (Express, Jest)
│   └── web_config.example.json ← Config template (copy and adjust for local use)
├── Dockerfile.linux        ← Linux build environment
├── CMakeLists.txt          ← Root build configuration
└── CMakePresets.json       ← Cross-platform build presets
```

---

## 3. Automation Pipeline (Python)

The project uses a unified Python-based pipeline for all platforms. **All temporary artifacts (DBs, logs, configs) are stored in `out/artifacts/<preset>/`**, keeping the source tree clean.

### Master Entry Point: `scripts/Run.py`
This is the recommended way to work with the project. It automatically handles build and all test stages.

```powershell
# Build and run all tests (Unit + Scenario + Web API) on Windows
python scripts/Run.py --preset x64-release

# Linux CI via Docker (Unit + Scenario only; web tests are skipped in Docker)
python scripts/Run.py --preset linux-release

# Smoke test against a real library
python scripts/Run.py --preset x64-release --library "C:/Path/To/Library"

# Clean build
python scripts/Run.py --preset x64-debug --clean
```

### Test Stages
1.  **Stage 1: UNIT**: Fast C++ unit tests (Catch2). Fully self-contained, no external scripts required.
    - **Coverage**: Filters (genres/size/authors/keywords), string encoding (UTF-8/CP1251/UTF-16) and sanitization, Base64, thread-safe concurrency, database transactions and `get-book` field completeness, INPX streaming and edge cases, ZIP RAII and edge cases, FB2 cover extraction and encoding edge cases, logger configuration, search query parser, config utilities and edge cases.
    - **Crash Diagnostics**: `TestMain.cpp` writes `unit_tests.log` next to `UnitTests.exe` on every run for post-mortem analysis.
2.  **Stage 2: SCENARIO**: Behavioral tests.
    - Uses `LibraryGenerator.py` to create a "miniature" realistic library.
    - Communicates with `Librium.exe` via **TCP sockets**.
    - Covers 8 scenario categories: search operators, query filters, parsing & stats, upgrade/re-import, export, utility (covers), protocol error resilience, data integrity.
3.  **Stage 3: WEB API**: Jest/Supertest tests for the Node.js proxy. Run in an isolated artifact directory; the `web/` source directory is never touched.
    - Not supported in Docker mode (silently skipped).
4.  **Stage 4: SYNTHETIC SMOKE TEST**: Full end-to-end import/search/export cycle on a generated 100-book library. Validates the complete engine pipeline. Runs on every CI invocation, including Docker mode.
5.  **Smoke (--library)**: Full end-to-end validation on a real `Lib.rus.ec` library. Triggered by passing `--library PATH` (without `--web`). Runs instead of the standard CI pipeline.

---

## 4. Dependencies

Librium uses **vcpkg** in manifest mode for dependency management:
-   `asio`: Cross-platform networking (standalone).
-   `nlohmann-json`: JSON for modern C++.
-   `pugixml`: Light-weight XML parser.
-   `libzip`: Zip archive handling.
-   `catch2`: Unit testing framework.
-   `sqlite3`: Built from amalgamation (included in project).

---

## 5. Interface & Protocol (Engine Mode)

Librium works as a persistent high-performance **Engine**. It must be started with a mandatory configuration file and a port number.

### Startup
```bash
Librium.exe --config <path_to_config.json> --port <number>
```

### Inter-Process Communication (IPC)
The engine uses a tiered IPC architecture:
1.  **Transport Layer**: TCP Localhost server (127.0.0.1) using standalone **Asio**.
2.  **Protocol Layer**: Base64-encoded JSON messages.
3.  **Service Layer**: Abstract `IRequest` and `IResponse` interfaces.

### Protocol Format
-   **Communication**: Line-based. Every message is a single **Base64-encoded JSON** string ending with a newline (`\n`).
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
| `query`  | `title`, `author`, `genre`, `series`, `language`, `lib-id`, `archive`, `date-from`, `date-to`, `rating-min`, `rating-max`, `with-annotation`, `limit`, `offset` | Search books in the database. Supports search operators in `title`, `author`, and `series`. |
| `stats`  | *none* | Get database summary (books/authors count). |
| `get-book` | `id` (int) | Get full metadata of a single book by ID. Includes `"cover"` path if available. |
| `export` | `id` (int), `out` (directory path) | Extract a book from a ZIP archive into the specified directory. Response includes `data.file` — absolute path to the extracted file. |

### Search Syntax
The `query` action supports extended operators for the `title`, `author`, and `series` parameters:
- **Prefix (Default)**: `Push` finds "Pushkin". (Generates `LIKE 'Push%'`)
- **Exact Match**: `=Pushkin` finds exactly "Pushkin". (Generates `= 'Pushkin'`)
- **Contains**: `*robot` finds "I, Robot" and "The Robots of Dawn". (Generates `LIKE '%robot%'`)

All searches are **Unicode-aware and case-insensitive** for Cyrillic characters using the internal `librium_upper()` function.


---

## 6. Unicode & Path Handling (Crucial)

To ensure Windows compatibility with non-ASCII paths:
- **Never** use `std::filesystem::path::string()` for paths that might contain Unicode.
- **Always** use `Librium::Config::Utf8ToPath(std::string)` when converting UTF-8 (from JSON/CLI) to a path object.
- **Zip Archives**: The `ZipReader` is specialized to handle Unicode paths on Windows using Win32 wide-character APIs.

---

## 7. Performance & Robustness

### High-Performance Import
- **Bulk Write Mode**: During mass indexing, the database toggles `PRAGMA journal_mode = OFF` and `PRAGMA synchronous = OFF` via `BeginBulkImport()`. This provides a 30-50% speed boost. The `CImportGuard` RAII object ensures WAL mode and synchronous=NORMAL are restored even if the process crashes.
- **Optimized Page Size**: New databases are initialized with `PRAGMA page_size = 16384` to reduce B-Tree depth and improve disk I/O for millions of records.
- **Archive-Aware Scheduling**: The indexer groups all books by archive during pre-scan. Each `SWorkItem` in the work queue represents a complete archive batch — all books from one ZIP. A worker thread picks up an entire batch, opens the ZIP exactly once, processes all books sequentially, then closes it. This eliminates redundant ZIP opens caused by multiple threads competing for the same archive.
- **Fast Upgrade**: The `upgrade` command automatically skips archives that are already marked as indexed in the database, reducing incremental update time from minutes to seconds.
- **Index Management**: Search indexes are dropped before bulk imports and recreated afterward to maintain constant O(1) insertion speed. `CImportGuard` manages this lifecycle automatically.
- **Search Optimizations**: Fast case-insensitive search is achieved through denormalized columns (`search_title` in `books`, `search_name` in `authors` and `series`) indexed for prefix searching. This avoids expensive function calls in `WHERE` clauses.
- **Database Caching**: Internal memory caches for Authors, Genres, Series, and Publishers IDs minimize SQLite lookup overhead during mass indexing.
- **Worker Parallelism**: Supports high thread counts (up to 32+ threads) for simultaneous FB2 parsing and XML processing.

### Data Quality & Robustness
- **Recursive FB2 Parsing**: Text metadata (like annotations) is extracted recursively, ensuring clean text even when nested within complex XML tags (e.g., `<strong>`, `<a>`, `<v>`).
- **Encoding Auto-Detection**: The engine prioritizes UTF-8 validation. If a file is not valid UTF-8, it automatically attempts conversion from Windows-1251 (CP1251), ensuring readability of legacy Russian books.
- **Safe JSON Protocol**: Outgoing JSON messages use a byte-replacement strategy for invalid UTF-8 sequences to prevent engine crashes on "dirty" library data.

### Comprehensive Validation
The `SmokeTester.py` script performs multi-stage validation:
1. **Clean Import**: Verifies progress reporting and 100% completion.
2. **Data Integrity**: Randomly samples 10 books and inspects every database field for completeness and correct encoding.
3. **Incremental Logic**: Verifies that a secondary `upgrade` command correctly skips all existing archives.
4. **Export Integrity**: Validates exported books by checking file size and searching for valid XML signatures (`<FictionBook`) in the content.
5. **Cover Extraction**: Verifies that cover images are correctly extracted from FB2 and saved to disk.

---

## 8. Meta Storage and Management

Librium extracts cover images from FB2 files during the indexing process and stores them as standalone files to ensure fast access without re-parsing archives.

- **Storage Location**: Metadata files are stored in a `meta/` directory located in the same folder as the SQLite database.
- **Directory Structure**: Each book has its own subfolder named after its database `id` (e.g., `meta/123/`).
- **Filename**: The cover image is saved as `cover.<ext>`, where `<ext>` is determined by the `content-type` in the FB2 file (typically `.jpg`, `.png`, or `.webp`).
- **Lifecycle**: When the `import` command is used (full re-indexing), the `meta/` directory is automatically cleaned to prevent orphaned metadata from previous collections.
- **Extraction Logic**: 
  - The parser looks for the `<coverpage>` tag in the `<title-info>` section.
  - The referenced `<binary>` block is decoded from Base64.
  - All whitespace within the Base64 block is automatically stripped during parsing.
- **API Access**: The `get-book` action automatically searches for files starting with `cover.` in the book's meta directory and returns the absolute path if found.
- **Error Handling**: If a cover cannot be written to disk (e.g., due to permissions), the error is logged, but the indexing of the book record itself continues.

---

## 9. Web Interface

Librium includes a modern, dark-themed web interface for browsing and downloading books within a local network.

- **Technology**: Node.js (Proxy) + Vanilla JS/CSS (Frontend).
- **Key Features**:
  - Responsive book grid with covers.
  - Infinite scroll and advanced search.
  - One-click downloads in **FB2** format; **EPUB** download available when the fbc converter is configured.
  - In-memory LRU caching for performance.
  - `/api/config` endpoint exposes server capabilities (e.g., `epubEnabled`) to the frontend.
- **Detailed Docs**: See **[Web Interface Documentation](Web_Interface.md)**.
- **Quick Start**:
  ```powershell
  python scripts/Run.py --preset x64-release --web --library "C:/Path/To/Library"
  ```

