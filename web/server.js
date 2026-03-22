const express = require('express');
const { spawn } = require('child_process');
const net = require('net');
const fs = require('fs');
const path = require('path');

let webConfig;
let libriumConfig;
let metaDir;
const app = express();

// --- CONFIG ---
function loadConfig() {
    const configPath = path.resolve(__dirname, 'web_config.json');
    if (!fs.existsSync(configPath)) {
        console.error(`[ERROR] web_config.json not found at ${configPath}`);
        process.exit(1);
    }
    webConfig = JSON.parse(fs.readFileSync(configPath, 'utf8'));

    const libriumConfigPath = path.resolve(__dirname, webConfig.libriumConfig);
    if (!fs.existsSync(libriumConfigPath)) {
        console.error(`[ERROR] librium_config.json not found at ${libriumConfigPath}`);
        process.exit(1);
    }
    libriumConfig = JSON.parse(fs.readFileSync(libriumConfigPath, 'utf8'));

    metaDir = webConfig.metaDir || path.join(
        path.dirname(path.resolve(__dirname, libriumConfig.database.path)), 'meta'
    );

    const tempDir = path.resolve(__dirname, webConfig.tempDir);
    if (!fs.existsSync(tempDir)) {
        fs.mkdirSync(tempDir, { recursive: true });
    }
}

// --- LIBRIUM PROCESS ---
let libriumProcess = null;
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
        engineState = 'crashed';
        setTimeout(() => restartLibrium(), 5000);
    });
}

function restartLibrium() {
    console.log('[Server] Restarting Librium...');
    startLibrium();
    setTimeout(() => connectTcp(), 1000);
}

// --- TCP CLIENT ---
let socket = null;
let lineBuffer = '';
let busy = false;
const requestQueue = [];

let engineState = 'starting';  // 'starting' | 'ready' | 'importing' | 'upgrading' | 'crashed'
let lastProgress = { processed: 0, total: 0 };
let currentLineHandler = null;

function connectTcp() {
    socket = net.connect(webConfig.libriumPort, '127.0.0.1');
    socket.setEncoding('utf8');
    socket.setKeepAlive(true, 10000);

    socket.on('connect', () => {
        console.log('[TCP] Connected to Librium engine');
        engineState = 'ready';
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
        console.log('[TCP] Connection closed, will reconnect in 2s');
        engineState = 'starting';
        socket = null;
        setTimeout(() => connectTcp(), 2000);
    });
}

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

function sendCommand(action, params = {}) {
    return new Promise((resolve, reject) => {
        requestQueue.push({ action, params, resolve, reject });
        processQueue();
    });
}

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
        socket.write(encoded);
    } catch (e) {
        currentLineHandler = null;
        busy = false;
        reject(e);
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
        const params = {
            limit: parseInt(req.query.limit || 40),
            offset: parseInt(req.query.offset || 0)
        };
        ['title', 'author', 'series', 'genre', 'language'].forEach(k => {
            if (req.query[k]) params[k] = req.query[k];
        });

        const data = await sendCommand('query', params);
        res.json(data);
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

app.get('/api/books/:id', async (req, res) => {
    if (engineState !== 'ready') return res.status(503).json({ error: "Engine not ready" });
    try {
        const data = await sendCommand('get-book', { id: parseInt(req.params.id) });
        if (data.cover) {
            data.coverUrl = `/covers/${req.params.id}/cover.jpg`;
        }
        res.json(data);
    } catch (e) {
        res.status(404).json({ error: e.message });
    }
});

app.get('/api/download/:id', async (req, res) => {
    if (engineState !== 'ready') return res.status(503).json({ error: "Engine not ready" });
    try {
        const tempDir = path.resolve(__dirname, webConfig.tempDir);
        const data = await sendCommand('export', { id: parseInt(req.params.id), out: tempDir });
        
        if (!data.file) throw new Error("No file returned");
        
        const filePath = data.file;
        const filename = data.filename || path.basename(filePath);
        
        res.download(filePath, filename, (err) => {
            if (!err) {
                fs.unlink(filePath, () => {});
            }
        });
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

app.post('/api/import', async (req, res) => {
    if (engineState !== 'ready') return res.status(503).json({ error: "Engine busy" });
    engineState = 'importing';
    lastProgress = { processed: 0, total: 0 };
    res.status(202).json({ message: "Import started" });
    
    try {
        await sendCommand('import');
    } catch (e) {
        console.error('[Server] Import error:', e);
    } finally {
        engineState = 'ready';
    }
});

app.post('/api/upgrade', async (req, res) => {
    if (engineState !== 'ready') return res.status(503).json({ error: "Engine busy" });
    engineState = 'upgrading';
    lastProgress = { processed: 0, total: 0 };
    res.status(202).json({ message: "Upgrade started" });
    
    try {
        await sendCommand('upgrade');
    } catch (e) {
        console.error('[Server] Upgrade error:', e);
    } finally {
        engineState = 'ready';
    }
});

// --- COVERS CACHE (LRU) ---
const MAX_CACHE_SIZE = 500; // Количество обложек в памяти
const coverCache = new Map();

function getCoverFromCache(bookId) {
    if (coverCache.has(bookId)) {
        const data = coverCache.get(bookId);
        coverCache.delete(bookId);
        coverCache.set(bookId, data); // Переносим в конец (самый свежий)
        return data;
    }
    return null;
}

function addCoverToCache(bookId, data) {
    if (coverCache.size >= MAX_CACHE_SIZE) {
        const firstKey = coverCache.keys().next().value;
        coverCache.delete(firstKey);
    }
    coverCache.set(bookId, data);
}

// --- COVERS ENDPOINT ---
app.get('/covers/:id/*', (req, res) => {
    const bookId = req.params.id;
    
    // 1. Пробуем взять из RAM-кэша
    const cachedData = getCoverFromCache(bookId);
    if (cachedData) {
        res.setHeader('Content-Type', 'image/jpeg');
        res.setHeader('Cache-Control', 'public, max-age=3600'); // 1 час в браузере
        return res.send(cachedData);
    }

    // 2. Если нет в кэше — читаем с диска
    const dir = path.join(metaDir, bookId);
    if (!fs.existsSync(dir)) return res.status(404).end();
    
    try {
        const files = fs.readdirSync(dir);
        const coverFile = files.find(f => f.startsWith('cover.'));
        if (!coverFile) return res.status(404).end();
        
        const filePath = path.join(dir, coverFile);
        const data = fs.readFileSync(filePath);
        
        addCoverToCache(bookId, data);
        
        res.setHeader('Content-Type', 'image/jpeg');
        res.setHeader('Cache-Control', 'public, max-age=3600');
        res.send(data);
    } catch (e) {
        res.status(404).end();
    }
});

// --- STARTUP ---
async function main() {
    loadConfig();
    startLibrium();
    await new Promise(r => setTimeout(r, 1500));
    connectTcp();
    
    app.listen(webConfig.webPort, '0.0.0.0', () => {
        console.log(`[Server] Librium Web UI running at http://localhost:${webConfig.webPort}`);
    });
}

main().catch(console.error);
