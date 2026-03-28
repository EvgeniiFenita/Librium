'use strict';

const net = require('net');

let socket = null;
let lineBuffer = '';
let busy = false;
let reconnectTimer = null;
const requestQueue = [];
let TCP_TIMEOUT = 30000;

const MAX_LINE_BUFFER_BYTES = 10 * 1024 * 1024; // 10 MB

let engineState = 'starting';
let lastProgress = { processed: 0, total: 0 };
let currentLineHandler = null;

/**
 * Change engine state. Handles queue cleanup for 'crashed' and 'stopping' transitions.
 * @param {string} newState
 */
function setEngineState(newState) {
    if (engineState === newState) return;
    console.log(`[Engine] State changed: ${engineState} -> ${newState}`);
    engineState = newState;

    if (newState === 'crashed' || newState === 'stopping') {
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

function getEngineState() { return engineState; }
function getLastProgress() { return lastProgress; }
function resetProgress() { lastProgress = { processed: 0, total: 0 }; }

/**
 * Connect to the Librium engine via TCP.
 * @param {object} webConfig
 * @param {number} [retryCount]
 */
function connectTcp(webConfig, retryCount = 0) {
    if (reconnectTimer) {
        clearTimeout(reconnectTimer);
        reconnectTimer = null;
    }
    if (socket) {
        socket.destroy();
        socket = null;
    }
    lineBuffer = '';

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
        if (lineBuffer.length > MAX_LINE_BUFFER_BYTES) {
            console.error('[TCP] Line buffer overflow — destroying socket to prevent OOM');
            socket.destroy();
            return;
        }
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
        if (engineState !== 'crashed' && engineState !== 'stopping') {
            console.log('[TCP] Connection closed');
            setEngineState('starting');
            const delay = Math.min(1000 * Math.pow(2, retryCount), 10000);
            reconnectTimer = setTimeout(() => connectTcp(webConfig, retryCount + 1), delay);
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
 * @param {string} base64Line
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
 * Process the next command in the queue.
 */
function processQueue() {
    if (busy || requestQueue.length === 0 || !socket || engineState === 'starting' || engineState === 'crashed' || engineState === 'stopping') return;
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

/**
 * Send a command to the engine and wait for a response.
 * @param {string} action
 * @param {object} [params]
 * @param {number} [timeoutMs]
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
 * Destroy the TCP socket and clean up timers.
 */
function destroySocket() {
    if (reconnectTimer) clearTimeout(reconnectTimer);
    if (socket) socket.destroy();
}

module.exports = {
    connectTcp,
    sendCommand,
    setEngineState,
    getEngineState,
    getLastProgress,
    resetProgress,
    destroySocket,
};
