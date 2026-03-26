#!/bin/bash
# Librium container startup script.
#
# Configuration is resolved in the following priority order:
#   1. /data/librium_config.json  — if this file exists in the data volume, it is
#      used as-is. All LIBRIUM_* import/filter/logging env vars are ignored.
#   2. Environment variables      — all AppConfig parameters are exposed as optional
#      env vars with sensible defaults (see docker-compose.yml for the full list).
#
# After resolving the engine config, the Node.js web server is launched.
set -e

# ---------------------------------------------------------------------------
# Helper: convert a comma-separated string to a JSON array of strings.
#   to_json_array ""           → []
#   to_json_array "ru,uk,be"  → ["ru","uk","be"]
# ---------------------------------------------------------------------------
to_json_array() {
    local input="${1:-}"
    if [ -z "$input" ]; then
        echo "[]"
        return
    fi
    local result="["
    local first=1
    IFS=',' read -ra parts <<< "$input"
    for part in "${parts[@]}"; do
        local item
        item=$(echo "$part" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
        [ "$first" -eq 1 ] && first=0 || result="${result},"
        result="${result}\"${item}\""
    done
    echo "${result}]"
}

# ---------------------------------------------------------------------------
# Required environment variables
# ---------------------------------------------------------------------------
: "${LIBRIUM_LIBRARY_PATH:?LIBRIUM_LIBRARY_PATH must be set (path to the library folder inside the container, e.g. /library)}"

# ---------------------------------------------------------------------------
# Auto-detect INPX file (first *.inpx found in the library folder)
# ---------------------------------------------------------------------------
if [ -z "${LIBRIUM_INPX_FILE}" ]; then
    INPX_PATH=$(find "${LIBRIUM_LIBRARY_PATH}" -maxdepth 1 -name "*.inpx" | head -n 1)
    if [ -z "${INPX_PATH}" ]; then
        echo "[ERROR] No .inpx file found in ${LIBRIUM_LIBRARY_PATH}"
        exit 1
    fi
    echo "[entrypoint] Auto-detected INPX: ${INPX_PATH}"
else
    INPX_PATH="${LIBRIUM_LIBRARY_PATH}/${LIBRIUM_INPX_FILE}"
fi

# ---------------------------------------------------------------------------
# Auto-detect archives directory
# Preference order:
#   1. LIBRIUM_ARCHIVES_DIR env var (explicit override)
#   2. lib.rus.ec/ subfolder
#   3. Any subfolder containing ZIP files
#   4. The library root itself
# ---------------------------------------------------------------------------
if [ -z "${LIBRIUM_ARCHIVES_DIR}" ]; then
    if [ -d "${LIBRIUM_LIBRARY_PATH}/lib.rus.ec" ]; then
        LIBRIUM_ARCHIVES_DIR="${LIBRIUM_LIBRARY_PATH}/lib.rus.ec"
    else
        LIBRIUM_ARCHIVES_DIR=$(find "${LIBRIUM_LIBRARY_PATH}" -maxdepth 1 -type d -exec sh -c \
            'ls "$1"/*.zip >/dev/null 2>&1 && echo "$1"' _ {} \; | grep -v "^${LIBRIUM_LIBRARY_PATH}$" | head -n 1)
        if [ -z "${LIBRIUM_ARCHIVES_DIR}" ]; then
            LIBRIUM_ARCHIVES_DIR="${LIBRIUM_LIBRARY_PATH}"
        fi
    fi
    echo "[entrypoint] Auto-detected archives dir: ${LIBRIUM_ARCHIVES_DIR}"
fi

# ---------------------------------------------------------------------------
# Optional environment variables (with defaults)
# ---------------------------------------------------------------------------
WEB_PORT="${WEB_PORT:-8080}"
LIBRIUM_PORT="${LIBRIUM_PORT:-9001}"

# ---------------------------------------------------------------------------
# Persistent data directories (must be backed by a volume in production)
# ---------------------------------------------------------------------------
mkdir -p /data/meta /data/temp

# ---------------------------------------------------------------------------
# Resolve C++ engine configuration
#
# Priority 1: user-supplied config file in the data volume.
# Priority 2: generate from environment variables.
# ---------------------------------------------------------------------------
ENGINE_CONFIG=/app/web/librium_config.json

if [ -f "/data/librium_config.json" ]; then
    echo "[entrypoint] Using /data/librium_config.json (user-supplied config)"
    cp /data/librium_config.json "${ENGINE_CONFIG}"
else
    # --- Import tuning ---
    LIBRIUM_THREAD_COUNT="${LIBRIUM_THREAD_COUNT:-4}"
    LIBRIUM_PARSE_FB2="${LIBRIUM_PARSE_FB2:-true}"
    LIBRIUM_TRANSACTION_BATCH_SIZE="${LIBRIUM_TRANSACTION_BATCH_SIZE:-1000}"
    LIBRIUM_SQLITE_CACHE_SIZE="${LIBRIUM_SQLITE_CACHE_SIZE:--64000}"
    LIBRIUM_SQLITE_MMAP_SIZE="${LIBRIUM_SQLITE_MMAP_SIZE:-268435456}"

    # --- Filter lists (comma-separated → JSON arrays) ---
    EXCLUDE_LANGUAGES_JSON=$(to_json_array "${LIBRIUM_EXCLUDE_LANGUAGES:-}")
    INCLUDE_LANGUAGES_JSON=$(to_json_array "${LIBRIUM_INCLUDE_LANGUAGES:-}")
    EXCLUDE_GENRES_JSON=$(to_json_array "${LIBRIUM_EXCLUDE_GENRES:-}")
    INCLUDE_GENRES_JSON=$(to_json_array "${LIBRIUM_INCLUDE_GENRES:-}")
    EXCLUDE_AUTHORS_JSON=$(to_json_array "${LIBRIUM_EXCLUDE_AUTHORS:-}")
    EXCLUDE_KEYWORDS_JSON=$(to_json_array "${LIBRIUM_EXCLUDE_KEYWORDS:-}")
    LIBRIUM_MIN_FILE_SIZE="${LIBRIUM_MIN_FILE_SIZE:-0}"
    LIBRIUM_MAX_FILE_SIZE="${LIBRIUM_MAX_FILE_SIZE:-0}"

    # --- Logging ---
    LIBRIUM_LOG_LEVEL="${LIBRIUM_LOG_LEVEL:-info}"
    LIBRIUM_LOG_PROGRESS_INTERVAL="${LIBRIUM_LOG_PROGRESS_INTERVAL:-1000}"

    cat > "${ENGINE_CONFIG}" <<EOF
{
    "database": {
        "path": "/data/library.db"
    },
    "library": {
        "inpxPath": "${INPX_PATH}",
        "archivesDir": "${LIBRIUM_ARCHIVES_DIR}"
    },
    "import": {
        "parseFb2": ${LIBRIUM_PARSE_FB2},
        "threadCount": ${LIBRIUM_THREAD_COUNT},
        "transactionBatchSize": ${LIBRIUM_TRANSACTION_BATCH_SIZE},
        "sqliteCacheSize": ${LIBRIUM_SQLITE_CACHE_SIZE},
        "sqliteMmapSize": ${LIBRIUM_SQLITE_MMAP_SIZE}
    },
    "filters": {
        "excludeLanguages": ${EXCLUDE_LANGUAGES_JSON},
        "includeLanguages": ${INCLUDE_LANGUAGES_JSON},
        "excludeGenres": ${EXCLUDE_GENRES_JSON},
        "includeGenres": ${INCLUDE_GENRES_JSON},
        "minFileSize": ${LIBRIUM_MIN_FILE_SIZE},
        "maxFileSize": ${LIBRIUM_MAX_FILE_SIZE},
        "excludeAuthors": ${EXCLUDE_AUTHORS_JSON},
        "excludeKeywords": ${EXCLUDE_KEYWORDS_JSON}
    },
    "logging": {
        "level": "${LIBRIUM_LOG_LEVEL}",
        "file": "/data/librium.log",
        "progressInterval": ${LIBRIUM_LOG_PROGRESS_INTERVAL}
    }
}
EOF
fi

# ---------------------------------------------------------------------------
# Generate web_config.json (Node.js server configuration)
# ---------------------------------------------------------------------------
cat > /app/web/web_config.json <<EOF
{
    "libriumExe": "/app/librium/Librium",
    "libriumConfig": "${ENGINE_CONFIG}",
    "libriumPort": ${LIBRIUM_PORT},
    "webPort": ${WEB_PORT},
    "tempDir": "/data/temp",
    "metaDir": "/data/meta",
    "toolsDir": "/app/tools",
    "fb2cngExe": "/app/tools/fbc"
}
EOF

exec node /app/web/server.js
