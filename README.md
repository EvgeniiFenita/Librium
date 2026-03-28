# Librium

A tool for indexing and browsing home book collections in INPX format ([lib.rus.ec](https://lib.rus.ec), [Flibusta](https://flibusta.is), and similar).

You point it at a folder with an `.inpx` index and ZIP archives of FB2 books — it builds a local SQLite database and starts a web UI where you can search and download books.

## What it does

- Parses `.inpx` collection index and imports book metadata into a local SQLite database
- Optionally opens each FB2 file to extract annotations, cover images, and publisher info
- Provides a web interface for searching by title, author, series, genre, or language
- Serves FB2 downloads directly from the original archives; EPUB conversion is supported if you supply an external converter
- Supports incremental updates — re-indexes only newly added archives

## Requirements

- **Windows or Linux**
- [CMake 3.25+](https://cmake.org/)
- [vcpkg](https://vcpkg.io/) with `VCPKG_ROOT` environment variable pointing to your vcpkg installation
- Visual Studio 2026+ (Windows) or GCC/Clang with C++20 support (Linux)
- [Node.js 18+](https://nodejs.org/) — for the web interface
- [Python 3.10+](https://python.org/) — for the build and run scripts

## Building

```bash
# Debug build + run all tests
python scripts/Run.py --preset x64-debug

# Release build
python scripts/Run.py --preset x64-release

# Linux (builds inside Docker)
python scripts/Run.py --preset linux-release
```

All output goes to `out/`. Source directories are never touched by the build.

## Running

```bash
# Launch the web interface with a real library
# (path to the folder that contains your .inpx file and ZIP archives)
python scripts/Run.py --preset x64-release --web --library "D:/lib.rus.ec"

# Launch with a small auto-generated demo library (~350 books)
python scripts/Run.py --preset x64-debug --web --demo
```

Then open `http://localhost:8080` in your browser.

On first run, click **Import** to index the collection. This is non-destructive: re-running Import adds new books but does not remove stale records from archives that no longer exist in the INPX. This can take a while depending on collection size and whether FB2 parsing is enabled. Subsequent **Upgrade** runs only process new archives.

## Configuration

The engine reads `librium_config.json` (auto-generated on first run). The most useful options:

```json
{
    "library": {
        "inpxPath": "./lib.rus.ec/lib.rus.ec.inpx",
        "archivesDir": "./lib.rus.ec"
    },
    "import": {
        "parseFb2": true,
        "threadCount": 4
    },
    "filters": {
        "includeLanguages": ["ru"]
    }
}
```

Set `parseFb2: false` to skip opening individual book files — indexing will be significantly faster but without annotations and covers.

## EPUB support

Librium does not convert books itself. If you have [fb2converter](https://github.com/rupor-github/fb2converter), set the path to its binary in `web_config.json`:

```json
{ "fb2cngExe": "/path/to/fb2c" }
```

## Docker

```bash
# Build a deployable Docker image
python scripts/Run.py --preset linux-release --image
```

```yaml
# docker-compose.yml environment variables
LIBRIUM_LIBRARY_PATH: /library   # required: path to your library
WEB_PORT: 8080
```

## Search syntax

| Query | Meaning |
|-------|---------|
| `Pushkin` | Starts with "Pushkin" |
| `=Pushkin` | Exactly "Pushkin" |
| `*robot` | Contains "robot" |

Search is case-insensitive, including Cyrillic.

## Architecture

```
Browser  →  Node.js (Express)  →  Librium (C++, TCP)  →  SQLite
```

The C++ engine runs as a persistent TCP server on `localhost:9001`. The Node.js layer proxies browser requests and caches cover images in memory.

## License

GPL v3. See [LICENSE](LICENSE).
