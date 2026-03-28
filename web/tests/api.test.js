const request = require('supertest');
const net = require('net');
const fs = require('fs');
const path = require('path');
const os = require('os');
const { app, loadConfig, connectTcp, setEngineState, shutdown, main } = require('../server');

let mockTcpServer;
let mockTcpPort = 0;
let tempTestDir = '';
let engineResponses = [];
let lastEngineParams = null;
let activeSockets = new Set();
let httpServer;

beforeAll((done) => {
    // 1. Create a temporary directory for meta/ and temp/
    const baseTemp = fs.mkdtempSync(path.join(os.tmpdir(), 'librium-test-'));
    tempTestDir = path.resolve(baseTemp);
    
    const metaPath = path.join(tempTestDir, 'meta');
    const tempPath = path.join(tempTestDir, 'temp');
    fs.mkdirSync(metaPath);
    fs.mkdirSync(tempPath);

    // Create a dummy cover for book ID 1
    const book1Dir = path.join(metaPath, '1');
    fs.mkdirSync(book1Dir);
    fs.writeFileSync(path.join(book1Dir, 'cover.jpg'), 'fake_image_data');

    // 2. Start mock TCP server (simulating C++ Engine)
    mockTcpServer = net.createServer((socket) => {
        activeSockets.add(socket);
        socket.on('close', () => activeSockets.delete(socket));
        socket.setEncoding('utf8');
        
        let buffer = '';
        socket.on('data', (data) => {
            buffer += data;
            let idx;
            while ((idx = buffer.indexOf('\n')) !== -1) {
                const line = buffer.slice(0, idx).trim();
                buffer = buffer.slice(idx + 1);
                
                try {
                    const req = JSON.parse(Buffer.from(line, 'base64').toString('utf8'));
                    lastEngineParams = req.params;

                    if (req.action === 'export' && req.params.out) {
                        // Create a dummy file for res.download to find
                        const outDir = req.params.out;
                        if (!fs.existsSync(outDir)) fs.mkdirSync(outDir, { recursive: true });
                        fs.writeFileSync(path.join(outDir, 'test.fb2'), 'fake_book_content');
                    }
                } catch(e) {}

                // If no responses queued, don't send anything (simulate timeout)
                if (engineResponses.length === 0) continue;

                const resData = engineResponses.shift();
                const resBase64 = Buffer.from(JSON.stringify(resData)).toString('base64') + '\n';
                socket.write(resBase64);
            }
        });
    });

    mockTcpServer.listen(0, '127.0.0.1', () => {
        mockTcpPort = mockTcpServer.address().port;
        
        // 3. Start the app server and connect the proxy (without starting the real engine)
        const config = {
            webConfig: {
                libriumExe: "fake.exe",
                libriumConfig: "fake_config.json",
                libriumPort: mockTcpPort,
                webPort: 0, // Random port
                tempDir: tempPath,
                metaDir: metaPath,
                toolsDir: path.join(tempTestDir, 'tools'),
                fb2cngExe: "", // No override, will check toolsDir (which is empty)
                tcpTimeout: 500 // Increased from 200 to 500 for stability
            },
            libriumConfig: {
                database: { path: "fake.db" }
            }
        };
        main(config, false).then(server => {
            httpServer = server;
            setTimeout(done, 500);
        });
    });
});

afterAll((done) => {
    // Teardown
    shutdown(false);
    
    // Force close all connections to the mock server
    for (const socket of activeSockets) {
        socket.destroy();
    }
    activeSockets.clear();

    if (httpServer) {
        httpServer.close(() => {
            mockTcpServer.close(() => {
                try {
                    fs.rmSync(tempTestDir, { recursive: true, force: true });
                } catch (e) {}
                done();
            });
        });
    } else {
        mockTcpServer.close(() => {
            done();
        });
    }
});

afterEach(() => {
    engineResponses.length = 0;
    lastEngineParams = null;
});

/**
 * Wait for the engine proxy to reach the 'ready' state.
 * Required after tests that deliberately trigger a socket disconnect (e.g. BUG-1 timeout).
 */
async function waitForEngine(timeoutMs = 3000) {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
        try {
            const res = await request(httpServer).get('/api/status');
            if (res.body.state === 'ready') return;
        } catch (e) { /* server may still be starting */ }
        await new Promise(r => setTimeout(r, 100));
    }
    throw new Error('Engine did not reach ready state within timeout');
}

beforeEach(async () => {
    await waitForEngine();
}, 5000);

describe('Security & Validation', () => {
    it('should block Path Traversal attempts on /covers', async () => {
        // Now that the route is /covers/:id/:filename, /covers/../../ is not even matched
        // and Express returns 404 by default.
        const res1 = await request(httpServer).get('/covers/../../etc/passwd/cover.jpg');
        expect(res1.status).toBe(404);

        // This matches the route but fails the regex check for :id, returning 400
        const res2 = await request(httpServer).get('/covers/1abc/cover.jpg');
        expect(res2.status).toBe(400);
    });

    it('should reject non-numeric IDs on /api/books/:id', async () => {
        const res = await request(httpServer).get('/api/books/invalid_id');
        expect(res.status).toBe(400);
        expect(res.body.error).toBe('Invalid book ID');
    });

    it('should sanitize limit and offset parameters', async () => {
        engineResponses.push({ status: 'ok', data: { totalFound: 0, books: [] } });
        const res = await request(httpServer).get('/api/books?limit=99999999&offset=-50');
        expect(res.status).toBe(200);
    });
});

describe('API Endpoints (Mocked Engine)', () => {
    it('should return stats', async () => {
        engineResponses.push({ status: 'ok', data: { total_books: 42 } });
        const res = await request(httpServer).get('/api/stats');
        expect(res.status).toBe(200);
        expect(res.body.total_books).toBe(42);
    });

    it('should forward indexed_archives from engine stats response', async () => {
        engineResponses.push({ status: 'ok', data: { total_books: 100, total_authors: 50, indexed_archives: 7 } });
        const res = await request(httpServer).get('/api/stats');
        expect(res.status).toBe(200);
        expect(res.body.indexed_archives).toBe(7);
    });

    it('should return stats without indexed_archives field when engine omits it', async () => {
        engineResponses.push({ status: 'ok', data: { total_books: 5 } });
        const res = await request(httpServer).get('/api/stats');
        expect(res.status).toBe(200);
        expect(res.body.total_books).toBe(5);
        expect(res.body.indexed_archives).toBeUndefined();
    });

    it('should return 500 if engine returns an error', async () => {
        engineResponses.push({ status: 'error', error: 'Internal C++ Error' });
        const res = await request(httpServer).get('/api/stats');
        expect(res.status).toBe(500);
        expect(res.body.error).toBe('Internal C++ Error');
    });

    it('should return epubEnabled: false when fbc is not configured', async () => {
        const res = await request(httpServer).get('/api/config');
        expect(res.status).toBe(200);
        expect(res.body.epubEnabled).toBe(false);
    });
});

describe('Static Files & Caching', () => {
    it('should serve a valid cover dynamically', async () => {
        const res = await request(httpServer).get('/covers/1/cover');
        expect(res.status).toBe(200);
        expect(res.headers['content-type']).toBe('image/jpeg');
        expect(res.body.toString()).toBe('fake_image_data');
    });

    it('should return 404 for a missing cover', async () => {
        const res = await request(httpServer).get('/covers/999/cover');
        expect(res.status).toBe(404);
    });
});

describe('Regression Tests (Bugs)', () => {
    it('should timeout if engine does not respond (BUG-1)', async () => {
        // No responses pushed to engineResponses -> engine will time out in 200ms
        const res = await request(httpServer).get('/api/stats');
        expect(res.status).toBe(500);
        expect(res.body.error).toContain('timed out');
    });

    it('should use unique output directories for exports (BUG-3)', async () => {
        // 1. First download
        engineResponses.push({ status: 'ok', data: { file: 'test.fb2' } });
        const res1 = await request(httpServer).get('/api/download/1');
        const firstOutDir = lastEngineParams.out;
        expect(res1.status).toBe(200);

        // 2. Second download
        engineResponses.push({ status: 'ok', data: { file: 'test.fb2' } });
        const res2 = await request(httpServer).get('/api/download/2');
        const secondOutDir = lastEngineParams.out;
        expect(res2.status).toBe(200);

        expect(firstOutDir).toBeDefined();
        expect(secondOutDir).toBeDefined();
        expect(firstOutDir).not.toBe(secondOutDir);
    });
});

describe('Download Endpoint', () => {
    it('should deliver file when engine returns a relative filename', async () => {
        // Verifies fix for path.resolve(data.file) → path.resolve(uniqueOutDir, data.file).
        // Engine returns just the filename; server must resolve it against uniqueOutDir.
        engineResponses.push({ status: 'ok', data: { file: 'test.fb2' } });
        const res = await request(httpServer).get('/api/download/1');
        expect(res.status).toBe(200);
        expect(res.headers['content-disposition']).toMatch(/attachment/i);
    });

    it('should reject path traversal embedded in the engine-returned filename', async () => {
        // Engine (or attacker who compromised it) returns a filename with traversal sequences.
        // The server must detect that the resolved path escapes uniqueOutDir and return 500.
        engineResponses.push({ status: 'ok', data: { file: '../../../etc/passwd' } });
        const res = await request(httpServer).get('/api/download/1');
        expect(res.status).toBe(500);
        expect(res.body.error).toBe('Invalid file path from engine');
    });

    it('should return 500 when engine returns no file path', async () => {
        engineResponses.push({ status: 'ok', data: {} });
        const res = await request(httpServer).get('/api/download/1');
        expect(res.status).toBe(500);
        expect(res.body.error).toBe('No file returned from engine');
    });

    it('should reject non-numeric book IDs on /api/download/:id', async () => {
        const res = await request(httpServer).get('/api/download/invalid');
        expect(res.status).toBe(400);
        expect(res.body.error).toBe('Invalid book ID');
    });

    it('should return 501 for EPUB format if fb2cng is not configured', async () => {
        const res = await request(httpServer).get('/api/download/1?format=epub');
        expect(res.status).toBe(501);
        expect(res.body.error).toContain('EPUB conversion is not configured');
    });
});

describe('LRU Cover Cache', () => {
    it('should serve the same content on a repeated cover request', async () => {
        const res1 = await request(httpServer).get('/covers/1/cover');
        expect(res1.status).toBe(200);
        expect(res1.headers['content-type']).toBe('image/jpeg');
        expect(res1.headers['cache-control']).toBe('public, max-age=3600');

        // Second request — content must be identical (served from LRU cache or disk)
        const res2 = await request(httpServer).get('/covers/1/cover');
        expect(res2.status).toBe(200);
        expect(res2.headers['content-type']).toBe('image/jpeg');
        expect(res2.headers['cache-control']).toBe('public, max-age=3600');
        expect(res2.body.toString()).toBe(res1.body.toString());
    });

    it('should return 404 for a book with no cover directory', async () => {
        const res = await request(httpServer).get('/covers/42/cover');
        expect(res.status).toBe(404);
    });
});
