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
                
                // Pop the next mock response
                const resData = engineResponses.length > 0 ? engineResponses.shift() : { status: 'ok', data: {} };
                const resBase64 = Buffer.from(JSON.stringify(resData)).toString('base64') + '\n';
                socket.write(resBase64);
            }
        });
    });

    mockTcpServer.listen(0, '127.0.0.1', () => {
        mockTcpPort = mockTcpServer.address().port;
        
        // 3. Initialize server.js config to point to our test environment
        loadConfig({
            webConfig: {
                libriumPort: mockTcpPort,
                webPort: 0, // Random port
                tempDir: tempPath,
                metaDir: metaPath
            },
            libriumConfig: {}
        });

        // 4. Start the app server and connect the proxy (without starting the real engine)
        const config = {
            webConfig: {
                libriumExe: "fake.exe",
                libriumConfig: "fake_config.json",
                libriumPort: mockTcpPort,
                webPort: 0, // Random port
                tempDir: tempPath,
                metaDir: metaPath
            },
            libriumConfig: {
                database: { path: "fake.db" }
            }
        };
        main(config, false).then(server => {
            httpServer = server;
            setTimeout(done, 200);
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

    mockTcpServer.close(() => {
        httpServer.close(() => {
            try {
                fs.rmSync(tempTestDir, { recursive: true, force: true });
            } catch (e) {}
            done();
        });
    });
});

describe('Security & Validation', () => {
    it('should block Path Traversal attempts on /covers', async () => {
        // ID should be strictly numeric
        const res1 = await request(httpServer).get('/covers/../../etc/passwd/cover.jpg');
        expect(res1.status).toBe(400);

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

    it('should return 500 if engine returns an error', async () => {
        engineResponses.push({ status: 'error', error: 'Internal C++ Error' });
        const res = await request(httpServer).get('/api/stats');
        expect(res.status).toBe(500);
        expect(res.body.error).toBe('Internal C++ Error');
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
