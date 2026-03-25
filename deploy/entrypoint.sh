#!/bin/bash
# Librium container startup script.
# Generates runtime configuration files from environment variables,
# then launches the Node.js web server.
set -e

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
# Generate librium_config.json (C++ engine configuration)
# ---------------------------------------------------------------------------
cat > /app/web/librium_config.json <<EOF
{
    "database": {
        "path": "/data/library.db"
    },
    "library": {
        "inpxPath": "${INPX_PATH}",
        "archivesDir": "${LIBRIUM_ARCHIVES_DIR}"
    },
    "import": {
        "parseFb2": true,
        "threadCount": 4,
        "transactionBatchSize": 1000,
        "mode": "full"
    },
    "filters": {
        "excludeLanguages": [],
        "includeLanguages": [],
        "excludeGenres": [],
        "includeGenres": [],
        "minFileSize": 0,
        "maxFileSize": 0,
        "excludeAuthors": [],
        "excludeKeywords": []
    },
    "logging": {
        "level": "info",
        "file": "/data/librium.log",
        "progressInterval": 1000
    }
}
EOF

# ---------------------------------------------------------------------------
# Generate web_config.json (Node.js server configuration)
# ---------------------------------------------------------------------------
cat > /app/web/web_config.json <<EOF
{
    "libriumExe": "/app/librium/Librium",
    "libriumConfig": "/app/web/librium_config.json",
    "libriumPort": ${LIBRIUM_PORT},
    "webPort": ${WEB_PORT},
    "tempDir": "/data/temp",
    "metaDir": "/data/meta",
    "toolsDir": "/app/tools",
    "fb2cngExe": "/app/tools/fbc"
}
EOF

exec node /app/web/server.js
