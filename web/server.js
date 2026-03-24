const express = require('express');
const { spawn, execFile } = require('child_process');
const net = require('net');
const fs = require('fs');
const fsPromises = require('fs/promises');
const path = require('path');
const crypto = require('crypto');
const util = require('util');
const execFileAsync = util.promisify(execFile);

// --- GLOBAL STATE ---
let fb2cngExe = null;
let webConfig;
let libriumConfig;
let metaDir;
let libriumProcess = null;
let socket = null;
let lineBuffer = '';
let busy = false;
let reconnectTimer = null;
const requestQueue = [];
let TCP_TIMEOUT = 30000; // 30 seconds

let engineState = 'starting';  // 'starting' | 'ready' | 'importing' | 'upgrading' | 'crashed'
let lastProgress = { processed: 0, total: 0 };
let currentLineHandler = null;

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

    // Validate mandatory fields
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

/**
 * Start the Librium C++ engine process.
 */
function startLibrium() {
    const exePath = path.resolve(__dirname, webConfig.libriumExe);
    const configPath = path.resolve(__dirname, webConfig.libriumConfig);

    console.log(`[Server] Starting Engine: ${exePath}`);
    libriumProcess = spawn(
        exePath,
        ['--config', configPath, '--port', String(webConfig.libriumPort)],
        { cwd: path.dirname(exePath), stdio: ['ignore', 'pipe', 'pipe'] }
    );

    libriumProcess.stdout.on('data', d => process.stdout.write('[Librium] ' + d));
    libriumProcess.stderr.on('data', d => process.stderr.write('[Librium] ' + d));

    libriumProcess.on('exit', (code) => {
        console.error(`[Librium] Process exited with code ${code}`);
        setEngineState('crashed');
        // Restart after delay
        setTimeout(() => restartLibrium(), 5000);
    });
}

function restartLibrium() {
    if (engineState !== 'crashed') return;
    console.log('[Server] Restarting Librium...');
    startLibrium();
    // Attempt to reconnect after the process has had a moment to start
    setTimeout(() => connectTcp(), 1000);
}

// --- TCP CLIENT ---

/**
 * Change engine state and handle transitions.
 */
function setEngineState(newState) {
    if (engineState === newState) return;
    console.log(`[Engine] State changed: ${engineState} -> ${newState}`);
    engineState = newState;

    if (newState === 'crashed') {
        // Reject all pending requests in the queue
        while (requestQueue.length > 0) {
            const { reject } = requestQueue.shift();
            reject(new Error('Engine crashed'));
        }
        if (currentLineHandler) {
            currentLineHandler({ status: 'error', error: 'Engine crashed' });
            currentLineHandler = null;
            busy = false;
        }
    }
}

/**
 * Connect to the Librium engine via TCP.
 */
function connectTcp(retryCount = 0) {
    if (reconnectTimer) {
        clearTimeout(reconnectTimer);
        reconnectTimer = null;
    }
    if (socket) {
        socket.destroy();
        socket = null;
    }
    lineBuffer = '';

    // Update timeout from config if available
    if (webConfig && webConfig.tcpTimeout) {
        TCP_TIMEOUT = webConfig.tcpTimeout;
    }

    console.log(`[TCP] Connecting to Librium engine at 127.0.0.1:${webConfig.libriumPort} (Attempt ${retryCount + 1})...`);
    socket = net.connect(webConfig.libriumPort, '127.0.0.1');
    socket.setEncoding('utf8');
    socket.setKeepAlive(true, 10000);

    socket.on('connect', () => {
        console.log('[TCP] Connected to Librium engine');
        setEngineState('ready');
        processQueue();
    });

    socket.on('data', (chunk) => {
        lineBuffer += chunk;
        let idx;
        while ((idx = lineBuffer.indexOf('\n')) !== -1) {
            const line = lineBuffer.slice(0, idx).trim();
            lineBuffer = lineBuffer.slice(idx + 1);
            if (line) handleLine(line);
        }
    });

    socket.on('error', (err) => {
        console.error('[TCP] Socket error:', err.message);
    });

    socket.on('close', () => {
        if (engineState !== 'crashed') {
            console.log('[TCP] Connection closed');
            setEngineState('starting');
            // Exponential backoff for reconnection
            const delay = Math.min(1000 * Math.pow(2, retryCount), 10000);
            reconnectTimer = setTimeout(() => connectTcp(retryCount + 1), delay);
        }
        busy = false;
        if (currentLineHandler) {
            currentLineHandler({ status: 'error', error: 'Connection closed' });
            currentLineHandler = null;
        }
    });
}

/**
 * Handle incoming Base64-encoded JSON lines from the engine.
 */
function handleLine(base64Line) {
    let msg;
    try {
        msg = JSON.parse(Buffer.from(base64Line, 'base64').toString('utf8'));
    } catch (e) {
        console.error('[TCP] Failed to parse line:', e.message);
        return;
    }

    if (msg.status === 'progress') {
        lastProgress = msg.data;
        return;
    }

    if (currentLineHandler) {
        const handler = currentLineHandler;
        currentLineHandler = null;
        handler(msg);
    }
}

/**
 * Send a command to the engine and wait for a response.
 */
function sendCommand(action, params = {}, timeoutMs = TCP_TIMEOUT) {
    return new Promise((resolve, reject) => {
        let timeout = null;

        const wrappedResolve = (val) => { if (timeout) clearTimeout(timeout); resolve(val); };
        const wrappedReject = (err) => { if (timeout) clearTimeout(timeout); reject(err); };

        if (timeoutMs > 0) {
            timeout = setTimeout(() => {
                const idx = requestQueue.findIndex(r => r.reject === wrappedReject);
                if (idx !== -1) {
                    requestQueue.splice(idx, 1);
                    wrappedReject(new Error(`Command '${action}' timed out (queue)`));
                } else if (currentLineHandler) {
                    // Command is currently being processed by the engine.
                    // Destroy the socket to prevent the late response from
                    // corrupting the next queued command (no request IDs in protocol).
                    // socket.on('close') will schedule reconnect and resume the queue.
                    currentLineHandler = null;
                    busy = false;
                    wrappedReject(new Error(`Command '${action}' timed out (active)`));
                    if (socket) {
                        socket.destroy();
                    } else {
                        processQueue();
                    }
                }
            }, timeoutMs);
        }

        requestQueue.push({ action, params, resolve: wrappedResolve, reject: wrappedReject });
        processQueue();
    });
}

/**
 * Process the next command in the queue.
 */
function processQueue() {
    if (busy || requestQueue.length === 0 || !socket || engineState === 'starting' || engineState === 'crashed') return;
    busy = true;

    const { action, params, resolve, reject } = requestQueue.shift();
    const payload = JSON.stringify({ action, params });
    const encoded = Buffer.from(payload).toString('base64') + '\n';

    currentLineHandler = (msg) => {
        busy = false;
        processQueue();
        if (msg.status === 'ok') {
            resolve(msg.data);
        } else {
            reject(new Error(msg.error || 'Unknown engine error'));
        }
    };

    try {
        socket.write(encoded, (err) => {
            if (err) {
                currentLineHandler = null;
                busy = false;
                reject(err);
                processQueue();
            }
        });
    } catch (e) {
        currentLineHandler = null;
        busy = false;
        reject(e);
        processQueue();
    }
}

// --- EXPRESS HTTP API ---
app.use((req, res, next) => {
    const start = Date.now();
    res.on('finish', () => {
        const duration = Date.now() - start;
        console.log(`[HTTP] ${req.method} ${req.url} - ${res.statusCode} (${duration}ms)`);
    });
    next();
});

app.use(express.static(path.join(__dirname, 'public')));

app.get('/api/status', (req, res) => {
    res.json({ state: engineState, progress: lastProgress });
});

app.get('/api/stats', async (req, res) => {
    if (engineState !== 'ready') return res.status(503).json({ error: "Engine not ready" });
    try {
        const data = await sendCommand('stats');
        res.json(data);
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

app.get('/api/books', async (req, res) => {
    if (engineState !== 'ready') return res.status(503).json({ error: "Engine not ready" });
    try {
        const limit = Math.min(Math.max(parseInt(req.query.limit, 10) || 40, 1), 500);
        const offset = Math.max(parseInt(req.query.offset, 10) || 0, 0);

        const params = { limit, offset };
        ['title', 'author', 'series', 'genre', 'language'].forEach(k => {
            if (req.query[k] && typeof req.query[k] === 'string') {
                params[k] = req.query[k].trim();
            }
        });

        const data = await sendCommand('query', params);
        res.json(data);
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

app.get('/api/books/:id', async (req, res) => {
    if (engineState !== 'ready') return res.status(503).json({ error: "Engine not ready" });
    const id = parseInt(req.params.id, 10);
    if (isNaN(id) || id <= 0) return res.status(400).json({ error: "Invalid book ID" });

    try {
        const data = await sendCommand('get-book', { id });
        if (data.cover) {
            data.coverUrl = `/covers/${id}/cover`;
        }
        res.json(data);
    } catch (e) {
        res.status(404).json({ error: e.message });
    }
});

app.get('/api/config', (req, res) =>
{
    res.json({ epubEnabled: !!fb2cngExe });
});

app.get('/api/download/:id', async (req, res) =>
{
    if (engineState !== 'ready')
    {
        return res.status(503).json({ error: "Engine not ready" });
    }
    const id = parseInt(req.params.id, 10);
    if (isNaN(id) || id <= 0)
    {
        return res.status(400).json({ error: "Invalid book ID" });
    }

    const format = req.query.format === 'epub' ? 'epub' : 'fb2';

    if (format === 'epub' && !fb2cngExe)
    {
        return res.status(501).json({ error: "EPUB conversion is not configured on the server." });
    }

    try
    {
        const tempDir = path.resolve(__dirname, webConfig.tempDir);
        const uniqueOutDir = path.join(tempDir, crypto.randomUUID());
        fs.mkdirSync(uniqueOutDir, { recursive: true });

        const data = await sendCommand('export', { id, out: uniqueOutDir });
        
        if (!data.file)
        {
            fs.rmSync(uniqueOutDir, { recursive: true, force: true });
            throw new Error("No file returned from engine");
        }
        
        const filePath = path.resolve(uniqueOutDir, data.file);
        const resolvedOutDir = path.resolve(uniqueOutDir);
        if (!filePath.startsWith(resolvedOutDir + path.sep))
        {
            fs.rmSync(uniqueOutDir, { recursive: true, force: true });
            return res.status(500).json({ error: 'Invalid file path from engine' });
        }

        let finalFilePath = filePath;
        let filename = data.filename || path.basename(filePath);

        if (format === 'epub')
        {
            const epubOutDir = path.join(uniqueOutDir, 'epub_out');
            fs.mkdirSync(epubOutDir, { recursive: true });
            try
            {
                await execFileAsync(
                    fb2cngExe,
                    ['convert', '--to', 'epub2', '--nodirs', '--overwrite', filePath, epubOutDir],
                    { timeout: 120000 }
                );
                const epubFiles = fs.readdirSync(epubOutDir).filter(f => f.endsWith('.epub'));
                if (epubFiles.length === 0)
                    throw new Error('Converter produced no EPUB file');
                finalFilePath = path.join(epubOutDir, epubFiles[0]);
                filename = path.basename(filename, path.extname(filename)) + '.epub';
            }
            catch (e)
            {
                fs.rmSync(uniqueOutDir, { recursive: true, force: true });
                console.error(`[HTTP] EPUB conversion failed for book ${id}:`, e);
                return res.status(500).json({ error: 'EPUB conversion failed' });
            }
        }

        res.download(finalFilePath, filename, (err) =>
        {
            fs.rmSync(uniqueOutDir, { recursive: true, force: true });
            if (err)
            {
                console.error(`[HTTP] Download error for book ${id}:`, err);
            }
        });
    }
    catch (e)
    {
        res.status(500).json({ error: e.message });
    }
});

app.post('/api/import', async (req, res) => {
    if (engineState !== 'ready') return res.status(503).json({ error: "Engine busy or not ready" });
    setEngineState('importing');
    lastProgress = { processed: 0, total: 0 };
    res.status(202).json({ message: "Import started" });
    
    try {
        await sendCommand('import', {}, 0);
    } catch (e) {
        console.error('[Server] Import error:', e);
    } finally {
        if (engineState === 'importing') setEngineState('ready');
    }
});

app.post('/api/upgrade', async (req, res) => {
    if (engineState !== 'ready') return res.status(503).json({ error: "Engine busy or not ready" });
    setEngineState('upgrading');
    lastProgress = { processed: 0, total: 0 };
    res.status(202).json({ message: "Upgrade started" });
    
    try {
        await sendCommand('upgrade', {}, 0);
    } catch (e) {
        console.error('[Server] Upgrade error:', e);
    } finally {
        if (engineState === 'upgrading') setEngineState('ready');
    }
});

// --- COVERS CACHE (LRU) ---
const MAX_CACHE_SIZE = 50 * 1024 * 1024; // 50 MB
let currentCacheSize = 0;
const coverCache = new Map();

function getCoverFromCache(bookId) {
    if (coverCache.has(bookId)) {
        const data = coverCache.get(bookId);
        coverCache.delete(bookId);
        coverCache.set(bookId, data); // Move to end (MRU)
        return data;
    }
    return null;
}

function addCoverToCache(bookId, data) {
    if (coverCache.has(bookId)) {
        currentCacheSize -= coverCache.get(bookId).data.length;
        coverCache.delete(bookId);
    }
    coverCache.set(bookId, data);
    currentCacheSize += data.data.length;

    while (currentCacheSize > MAX_CACHE_SIZE) {
        const firstKey = coverCache.keys().next().value;
        currentCacheSize -= coverCache.get(firstKey).data.length;
        coverCache.delete(firstKey);
    }
}

// --- COVERS ENDPOINT ---
app.get('/covers/:id/:filename', async (req, res) => {
    const bookIdRaw = req.params.id;
    if (!/^\d+$/.test(bookIdRaw)) {
        return res.status(400).end();
    }
    const bookId = parseInt(bookIdRaw, 10);
    
    const cached = getCoverFromCache(bookId);
    if (cached) {
        res.setHeader('Content-Type', cached.mime);
        res.setHeader('Cache-Control', 'public, max-age=3600');
        return res.send(cached.data);
    }

    const base = path.resolve(metaDir);
    const dir = path.resolve(base, String(bookId));
    
    const relativePath = path.relative(base, dir);
    if (relativePath.startsWith('..') || path.isAbsolute(relativePath)) {
        return res.status(403).end();
    }

    try {
        await fsPromises.access(dir);
        const files = await fsPromises.readdir(dir);
        const coverFile = files.find(f => f.startsWith('cover.'));
        if (!coverFile) {
            return res.status(404).end();
        }
        
        const filePath = path.join(dir, coverFile);
        const data = await fsPromises.readFile(filePath);
        const ext = path.extname(coverFile).toLowerCase();
        const mimeMap = {
            '.jpg': 'image/jpeg',
            '.jpeg': 'image/jpeg',
            '.png': 'image/png',
            '.webp': 'image/webp'
        };
        const mime = mimeMap[ext] || 'image/jpeg';
        
        addCoverToCache(bookId, { data, mime });
        
        res.setHeader('Content-Type', mime);
        res.setHeader('Cache-Control', 'public, max-age=3600');
        res.send(data);
    } catch (e) {
        res.status(404).end();
    }
});

// --- GRACEFUL SHUTDOWN ---
function shutdown(exit = true) {
    console.log('[Server] Shutting down...');
    setEngineState('crashed');
    if (reconnectTimer) clearTimeout(reconnectTimer);
    if (socket) socket.destroy();
    if (libriumProcess) libriumProcess.kill();
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

    if (webConfig.fb2cngExe && fs.existsSync(path.resolve(__dirname, webConfig.fb2cngExe)))
    {
        fb2cngExe = path.resolve(__dirname, webConfig.fb2cngExe);
    }
    else if (fs.existsSync(potentialExePath))
    {
        fb2cngExe = potentialExePath;
    }

    if (fb2cngExe)
    {
        console.log(`[Server] EPUB converter found: ${fb2cngExe}`);
    }
    else
    {
        console.warn(`[Server] EPUB converter (fbc) not found in ${toolsDir}. EPUB download will be disabled.`);
    }

    if (startEngine)
    {
        startLibrium();
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
