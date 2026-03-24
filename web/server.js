'use strict';

const express = require('express');
const fs = require('fs');
const path = require('path');

const { startLibrium, killLibrium } = require('./lib/engineProcess');
const {
    connectTcp: connectTcpClient,
    sendCommand,
    setEngineState,
    getEngineState,
    getLastProgress,
    resetProgress,
    destroySocket,
} = require('./lib/engineClient');

const { createBooksRouter } = require('./routes/books');
const { createLibraryRouter } = require('./routes/library');
const { createCoversRouter } = require('./routes/covers');
const { createDownloadRouter } = require('./routes/download');

// --- GLOBAL STATE ---
let fb2cngExe = null;
let webConfig;
let libriumConfig;
let metaDir;

const app = express();
app.use(express.json());

/**
 * Load configuration files with validation and error handling.
 */
function loadConfig(configOverride = null) {
    if (configOverride) {
        webConfig = configOverride.webConfig;
        libriumConfig = configOverride.libriumConfig;
        metaDir = webConfig.metaDir || path.resolve(__dirname, 'meta');
        const tempDir = path.resolve(__dirname, webConfig.tempDir || 'temp');
        if (!fs.existsSync(tempDir)) {
            fs.mkdirSync(tempDir, { recursive: true });
        }
        return;
    }

    const configPath = path.resolve(__dirname, 'web_config.json');
    if (!fs.existsSync(configPath)) {
        console.error(`[ERROR] web_config.json not found at ${configPath}`);
        process.exit(1);
    }

    try {
        webConfig = JSON.parse(fs.readFileSync(configPath, 'utf8'));
    } catch (e) {
        console.error(`[ERROR] Failed to parse web_config.json: ${e.message}`);
        process.exit(1);
    }

    const required = ['libriumExe', 'libriumConfig', 'libriumPort', 'webPort', 'tempDir'];
    for (const field of required) {
        if (webConfig[field] === undefined) {
            console.error(`[ERROR] Missing mandatory field in web_config.json: ${field}`);
            process.exit(1);
        }
    }

    const libriumConfigPath = path.resolve(__dirname, webConfig.libriumConfig);
    if (!fs.existsSync(libriumConfigPath)) {
        console.error(`[ERROR] librium_config.json not found at ${libriumConfigPath}`);
        process.exit(1);
    }

    try {
        libriumConfig = JSON.parse(fs.readFileSync(libriumConfigPath, 'utf8'));
    } catch (e) {
        console.error(`[ERROR] Failed to parse librium_config.json: ${e.message}`);
        process.exit(1);
    }

    metaDir = webConfig.metaDir || path.join(
        path.dirname(path.resolve(__dirname, libriumConfig.database.path)), 'meta'
    );

    const tempDir = path.resolve(__dirname, webConfig.tempDir);
    if (!fs.existsSync(tempDir)) {
        fs.mkdirSync(tempDir, { recursive: true });
    }
}

// --- LIBRIUM PROCESS ---

function restartLibrium() {
    if (getEngineState() !== 'crashed') return;
    console.log('[Server] Restarting Librium...');
    startLibrium(webConfig, () => {
        setEngineState('crashed');
        setTimeout(() => restartLibrium(), 5000);
    });
    setTimeout(() => connectTcp(), 1000);
}

// --- TCP CLIENT WRAPPER ---
// Supplies webConfig automatically so the public API is connectTcp(retryCount?).
function connectTcp(retryCount = 0) {
    connectTcpClient(webConfig, retryCount);
}

// --- ROUTES ---

app.use((req, res, next) => {
    const start = Date.now();
    res.on('finish', () => {
        const duration = Date.now() - start;
        console.log(`[HTTP] ${req.method} ${req.url} - ${res.statusCode} (${duration}ms)`);
    });
    next();
});

app.use(express.static(path.join(__dirname, 'public')));

app.get('/api/config', (req, res) => {
    res.json({ epubEnabled: !!fb2cngExe });
});

app.use('/api', createLibraryRouter({
    sendCommand,
    getEngineState,
    setEngineState,
    getLastProgress,
    resetProgress,
}));

app.use('/api/books', createBooksRouter({
    sendCommand,
    getEngineState,
}));

app.use('/api/download', createDownloadRouter({
    sendCommand,
    getEngineState,
    getWebConfig: () => webConfig,
    getFb2cngExe: () => fb2cngExe,
}));

app.use('/covers', createCoversRouter({
    getMetaDir: () => metaDir,
}));

// --- GRACEFUL SHUTDOWN ---
function shutdown(exit = true) {
    console.log('[Server] Shutting down...');
    setEngineState('crashed');
    destroySocket();
    killLibrium();
    if (exit) process.exit(0);
}

process.on('SIGTERM', () => shutdown(true));
process.on('SIGINT', () => shutdown(true));

// --- STARTUP ---
async function main(configOverride = null, startEngine = true) {
    loadConfig(configOverride);

    const toolsDir = path.resolve(__dirname, webConfig.toolsDir || 'tools');
    const exeName = process.platform === 'win32' ? 'fbc.exe' : 'fbc';
    const potentialExePath = path.join(toolsDir, exeName);

    if (webConfig.fb2cngExe && fs.existsSync(path.resolve(__dirname, webConfig.fb2cngExe))) {
        fb2cngExe = path.resolve(__dirname, webConfig.fb2cngExe);
    } else if (fs.existsSync(potentialExePath)) {
        fb2cngExe = potentialExePath;
    }

    if (fb2cngExe) {
        console.log(`[Server] EPUB converter found: ${fb2cngExe}`);
    } else {
        console.warn(`[Server] EPUB converter (fbc) not found in ${toolsDir}. EPUB download will be disabled.`);
    }

    if (startEngine) {
        startLibrium(webConfig, () => {
            setEngineState('crashed');
            setTimeout(() => restartLibrium(), 5000);
        });
    }
    connectTcp();
    return app.listen(webConfig.webPort, '0.0.0.0', () => {
        console.log(`[Server] Librium Web UI running at http://localhost:${webConfig.webPort}`);
    });
}

if (require.main === module) {
    main().catch(e => {
        console.error('[Server] Fatal error during startup:', e);
        process.exit(1);
    });
}

module.exports = { app, loadConfig, connectTcp, setEngineState, shutdown, main };
