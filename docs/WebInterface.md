# Librium Web Interface

The **Librium Web Interface** is a modern, responsive Single Page Application (SPA) designed for easy home use. It provides a visual way to browse, search, and download books from your INPX collection.

---

## 1. Architecture

The web part follows a **Proxy-Backend** pattern to bridge the C++ Engine's TCP protocol with standard web technologies.

```text
┌────────────────────────┐      ┌────────────────────────┐      ┌────────────────────────┐
│   Browser (Frontend)   │ ◄──► │   Node.js (Backend)    │ ◄──► │   Librium.exe (C++)    │
│   Vanilla JS / CSS     │ HTTP │   Express / Net        │ TCP  │   Engine Core          │
└────────────────────────┘      └────────────────────────┘      └────────────────────────┘
```

### Components
- **Frontend**: A clean Single Page Application built with Vanilla JavaScript and CSS.
  - **Grid View**: Displays book covers in a responsive grid.
  - **Infinite Scroll**: Loads books automatically as you scroll down using `IntersectionObserver`.
  - **Search Bar**: Supports filtering by Title, Author, Series, Genre, and Language.
  - **Book Modal**: Shows full book metadata, large cover (correctly sized), annotation, and download buttons. **📥 FB2** is always available; **📥 EPUB** appears only when the fbc converter is configured on the server.
  - **Author Display**: Intelligent formatting (e.g., "Author A, Author B" or "Author A + N" for multiple authors).
  - **Progress Overlay**: Appears during initial import or manual library rebuilds.
  - **Interrupted Import Banner**: Displayed on page load if the engine reports books in the database but no indexed archives (`total_books > 0` and `indexed_archives = 0`). This indicates an import was interrupted by a system reboot or crash. The banner prompts the user to press **Upgrade** to complete indexing.
- **Backend (Node.js)**:
  - **`server.js`** — Thin orchestrator: loads config, wires `lib/` modules and `routes/` together, starts the Express server.
  - **`lib/engineProcess.js`** — Spawns and monitors the `Librium.exe` process lifecycle.
  - **`lib/engineClient.js`** — Manages the persistent TCP connection to the C++ Engine: request queue with timeouts, Base64/JSON protocol decoding, and engine state machine (`starting` → `ready` → `importing` / `upgrading` → `crashed`).
  - **`lib/coverCache.js`** — LRU in-memory cache. Stores up to **50 MB** of recently accessed covers in RAM (supports JPG, PNG, and WebP).
  - **`routes/books.js`** — `GET /api/books` and `GET /api/books/:id` endpoints.
  - **`routes/covers.js`** — `GET /covers/:id/:filename` — serves cover images with LRU caching.
  - **`routes/download.js`** — `GET /api/download/:id` — FB2 and optional EPUB download via fbc converter.
  - **`routes/library.js`** — Import, upgrade, stats endpoints. The `/api/stats` response forwards all fields from the engine's `stats` command, including `total_books`, `total_authors`, and `indexed_archives`.
  - **EPUB Conversion**: Optionally converts exported FB2 files to EPUB2 format using the **fbc** (fb2cng) converter. The converter is downloaded automatically by `Run.py --web` and placed in the `tools/` directory. Controlled by `toolsDir` and `fb2cngExe` config fields. If the converter is absent, FB2 download remains available and EPUB buttons are hidden in the UI.
  - **Security**: Validates all incoming book IDs and sanitizes query parameters to prevent attacks.
  - **Static Server**: Serves the frontend files and cover images from the `meta/` directory.

---

## 2. Directory Structure

```text
web/
├── public/                # Static frontend assets
│   ├── index.html         # Main SPA layout
│   ├── style.css          # Dark-themed styles (Plex/Calibre inspired)
│   └── app.js             # Frontend logic & state management
├── lib/                   # Shared backend modules
│   ├── engineProcess.js   # Engine process spawning and lifecycle
│   ├── engineClient.js    # TCP connection, command queue, state machine
│   └── coverCache.js      # LRU in-memory cover cache (50 MB cap)
├── routes/                # Express route handlers (one file per concern)
│   ├── books.js           # GET /api/books, GET /api/books/:id
│   ├── covers.js          # GET /covers/:id/:filename
│   ├── download.js        # GET /api/download/:id (FB2 & EPUB)
│   └── library.js         # import, upgrade, stats endpoints
├── tests/
│   └── api.test.js        # Jest/Supertest API tests (source only; deps installed to out/)
├── server.js              # Thin orchestrator: wires lib/ and routes/, starts Express
├── package.json           # Node dependencies (Express, Jest)
└── web_config.example.json  # Config template (copy to web_config.json and adjust for local use)
```

---

## 3. Performance & Optimization

### LRU In-Memory Caching
To handle large libraries (e.g., 60GB of covers) without overwhelming the client or the server's disk:
- **Server-Side**: The Node.js proxy maintains a **Least Recently Used (LRU)** cache in RAM for covers. Up to **50 MB** of the most recently accessed covers are kept in memory.
- **Client-Side**: Covers are served with a `Cache-Control: max-age=3600` header (1 hour), balancing responsiveness with local storage usage on mobile devices.

### Smart Pagination
The frontend requests books in batches of 40. The `IntersectionObserver` pre-fetches the next batch before the user reaches the bottom, ensuring a smooth "infinite" scrolling experience.

---

## 4. Usage & Deployment

### Local Development / Windows
The easiest way to run the web interface is using the unified automation script.

**With a real library:**
```powershell
python scripts/Run.py --preset x64-release --web --library "C:\Path\To\Your\Library"
```

**With a synthetic demo library (~350 books, no real data required):**
```powershell
python scripts/Run.py --web --demo
```

The `--demo` flag generates a synthetic library on the first run and reuses it on subsequent runs. All generated files are placed under `out/artifacts/web/demo_lib/`.

`--library` and `--demo` are mutually exclusive.

**What this script does:**
1. Builds the C++ Engine in the selected preset.
2. In `--demo` mode: generates a synthetic library under `out/artifacts/web/demo_lib/`.
3. Detects your `.inpx` and archives folder (or uses the generated one).
4. Copies web source to `out/artifacts/web/` and downloads the **fbc** EPUB converter to `out/artifacts/web/tools/` (skipped if already present).
5. Generates necessary JSON configs in `out/artifacts/web/`.
6. Installs `npm` dependencies.
7. Launches the web server. The URL is `http://localhost:8080` by default (configurable via `webHost` and `webPort`).

### Initial Import
On the first launch, if the database is empty, the interface will automatically trigger a full library import. A progress bar will be displayed. You can also manually trigger an incremental update using the **"Обновить" (Update)** button in the header.

### Configuration Reference (`web_config.json`)

| Field | Type | Description |
|---|---|---|
| `libriumExe` | string | Absolute path to `Librium.exe` (or `Librium` on Linux). |
| `libriumConfig` | string | Absolute path to the C++ Engine's `librium_config.json`. |
| `libriumPort` | number | TCP port the C++ Engine listens on (default: `9001`). |
| `webPort` | number | HTTP port for the web server (default: `8080`). |
| `webHost` | string | Bind address for the web server (default: `0.0.0.0` — all interfaces). Set to `127.0.0.1` to restrict access to localhost only. |
| `tempDir` | string | Directory for temporary export files. Cleaned up after each download. |
| `metaDir` | string | Directory where the C++ Engine stores extracted cover images (`meta/`). |
| `toolsDir` | string | Directory containing third-party tools. The **fbc** converter is looked up here as `fbc` / `fbc.exe`. |
| `fb2cngExe` | string | Optional explicit path to the fbc converter binary. Overrides `toolsDir` lookup when non-empty. |

---

## 5. HTTP API Reference

The Node.js backend exposes the following HTTP endpoints:

| Method | Path | Description | Response |
|--------|------|-------------|----------|
| `GET` | `/api/config` | Server capabilities | `{ "epubEnabled": boolean }` |
| `GET` | `/api/status` | Engine state and import/upgrade progress | `{ "state": string, "progress": { "processed": number, "total": number } }` |
| `GET` | `/api/stats` | Library statistics (forwarded from C++ engine) | `{ "total_books": number, "total_authors": number, "indexed_archives": number }` |
| `POST` | `/api/import` | Trigger a non-destructive library re-index (async, returns 202). Adds new books; does not remove stale records from archives no longer in INPX. | `{ "message": string }` |
| `POST` | `/api/upgrade` | Start incremental upgrade (async, returns 202) | `{ "message": string }` |
| `GET` | `/api/books` | Search/list books | `{ "totalFound": number, "books": [...] }` |
| `GET` | `/api/books/:id` | Get single book details | Book object with optional `coverUrl` |
| `GET` | `/api/download/:id` | Download book file (FB2 or EPUB) | Binary file stream |
| `GET` | `/covers/:id/cover` | Serve cover image (LRU cached) | Image binary (JPG/PNG/WebP) |

### Query Parameters for `GET /api/books`

| Parameter | Type | Description |
|-----------|------|-------------|
| `limit` | number | Max books to return (default: 40, max: 500) |
| `offset` | number | Pagination offset |
| `title` | string | Title filter (supports search operators: `=exact`, `*contains`, default: prefix) |
| `author` | string | Author filter (supports search operators) |
| `series` | string | Series filter (supports search operators) |
| `genre` | string | Genre filter (prefix match) |
| `language` | string | Language code filter |

### Engine State Values (`/api/status`)

| State | Meaning |
|-------|---------|
| `starting` | Engine is connecting or initializing |
| `ready` | Engine is idle and accepting commands |
| `importing` | Full re-import is in progress |
| `upgrading` | Incremental upgrade is in progress |
| `crashed` | Engine process exited unexpectedly |

### `POST /api/import` and `POST /api/upgrade`

Both endpoints return **HTTP 202** immediately and run the operation asynchronously. Poll `GET /api/status` to track progress and wait for `state` to return to `"ready"`.

---

## 6. Maintenance

- **Artifacts**: All web-related data (database, logs, temporary exports, covers) are stored in `out/artifacts/web/`.
- **Resetting**: To start fresh, simply delete the `out/artifacts/web/` directory.
- **Logs**: Backend logs are printed to the console; C++ Engine logs are written to `out/artifacts/web/librium.log`.

---

## 6. Automated Testing

The web interface includes a backend API test suite to ensure security and reliability.

### Key Features
- **Framework**: Built with **Jest** and **Supertest**.
- **Engine Mocking**: Uses a lightweight mock TCP server to simulate the C++ Engine, allowing tests to run instantly without compiling the main project.
- **Full Isolation**: Tests are executed in an ephemeral environment within `out/artifacts/<preset>/web_test/`. This keeps the `web/` source directory clean and prevents dependency conflicts. The fbc converter is **not** downloaded during tests — the suite validates the "converter absent" path only.
- **Clean Lifecycle**: Implements explicit resource cleanup (sockets, servers, timers) to ensure 100% reliable test execution.

### Coverage
- `GET /api/config` — returns `{ epubEnabled: false }` when fbc is not configured.
- `GET /api/download/:id?format=epub` — returns `501` when fbc is not configured.
- Path traversal, ID validation, export error paths, LRU cover cache, engine mock responses.

### Running Tests
Web API tests run as part of the standard CI pipeline:

```powershell
python scripts/Run.py --preset x64-debug
```

This will automatically prepare the isolated test environment, install npm dependencies in `out/artifacts/<preset>/web_test/`, and execute the test suite. The `web/` source directory is never modified.
