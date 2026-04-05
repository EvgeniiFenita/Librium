---
name: web-interface
description: >
  Use when working on the Librium Node.js web layer (web/ directory).
  Triggers on tasks like "fix a web API bug", "add a new HTTP endpoint",
  "modify the frontend", "change the cover cache", "update web config",
  or any work touching web/server.js, web/routes/, web/lib/, or web/public/.
  Do NOT use for C++ engine changes (those go through $adding-commands).
---

# Librium Web Interface

## Architecture

```
Browser (Vanilla JS) ←HTTP→ Node.js Express (web/) ←TCP→ Librium.exe (C++)
```

## Directory Layout

```
web/
├── public/
│   ├── index.html          # SPA layout
│   ├── style.css           # Dark theme (Plex/Calibre inspired)
│   └── app.js              # Frontend logic & state
├── lib/
│   ├── engineProcess.js    # Engine process lifecycle
│   ├── engineClient.js     # TCP connection, request queue, state machine
│   └── coverCache.js       # LRU in-memory cache (50 MB cap)
├── routes/
│   ├── books.js            # GET /api/books, GET /api/books/:id
│   ├── covers.js           # GET /covers/:id/:filename
│   ├── download.js         # GET /api/download/:id (FB2 & EPUB)
│   └── library.js          # import, upgrade, stats
├── tests/
│   └── api.test.js         # Jest/Supertest (deps in out/, not web/)
├── server.js               # Thin orchestrator
├── package.json
└── web_config.example.json
```

## Engine State Machine (engineClient.js)

```
starting → ready → importing / upgrading → ready
                                         ↓
                                       crashed
```

## HTTP API

| Method | Path | Description |
|---|---|---|
| `GET` | `/api/config` | `{ "epubEnabled": boolean }` |
| `GET` | `/api/status` | `{ "state": string, "progress": {...} }` |
| `GET` | `/api/stats` | `{ "total_books", "total_authors", "indexed_archives" }` |
| `POST` | `/api/import` | Full re-index (async, 202). Does NOT remove stale records |
| `POST` | `/api/upgrade` | Incremental update (async, 202) |
| `GET` | `/api/books` | Search with limit/offset/title/author/series/genre/language |
| `GET` | `/api/books/:id` | Single book details |
| `GET` | `/api/download/:id` | Binary file stream (FB2 or EPUB) |
| `GET` | `/covers/:id/cover` | Cover image (LRU cached) |

`POST /api/import` and `POST /api/upgrade` return 202 immediately.
Poll `GET /api/status` until `state === "ready"` to track completion.

## Query Parameters — GET /api/books

| Param | Notes |
|---|---|
| `limit` | Default 40, max 500 |
| `offset` | Pagination |
| `title` | `=exact`, `*contains`, default: prefix |
| `author` | Same operators |
| `series` | Same operators |
| `genre` | Prefix only |
| `language` | Language code |

## Configuration (web_config.json)

| Field | Description |
|---|---|
| `libriumExe` | Path to `Librium.exe` |
| `libriumConfig` | Path to `librium_config.json` |
| `libriumPort` | TCP port for engine (default 9001) |
| `webPort` | HTTP port (default 8080) |
| `webHost` | Bind address (default `0.0.0.0`; use `127.0.0.1` for localhost-only) |
| `tempDir` | Temp dir for export files |
| `metaDir` | Dir with extracted covers (`meta/`) |
| `toolsDir` | Dir for fbc converter |
| `fb2cngExe` | Optional explicit path to fbc binary (overrides toolsDir lookup) |

## Key Implementation Notes

**Cover cache (coverCache.js):** LRU, 50 MB cap, supports JPG/PNG/WebP.
Served with `Cache-Control: max-age=3600`.

**EPUB conversion:** fbc (fb2cng) converter downloaded by `Run.py --web` to `tools/`.
If absent → FB2 download works, EPUB buttons hidden in UI.

**Interrupted import banner:** Shown when `total_books > 0` AND `indexed_archives === 0`.
Prompts user to press Upgrade.

**Frontend pagination:** Batches of 40 books. `IntersectionObserver` pre-fetches next batch.

## Testing

Tests run in full isolation — deps install to `out/`, never to `web/`:

```bash
python scripts/Run.py --preset x64-debug
```

Test coverage: `/api/config`, EPUB 501 when fbc absent, path traversal, ID
validation, export errors, LRU cache, engine mock responses.

## Rules

- All web artifacts go to `out/artifacts/<preset>/web_test/` — never modify `web/` during tests
- Validate all incoming book IDs and sanitize query parameters
- `server.js` stays thin — logic belongs in `lib/` or `routes/`
- Do not add `nlohmann/json` or C++ dependencies to the Node.js layer
