'use strict';

const express = require('express');
const fs = require('fs');
const fsPromises = require('fs/promises');
const path = require('path');
const crypto = require('crypto');
const util = require('util');
const { execFile } = require('child_process');
const execFileAsync = util.promisify(execFile);

/**
 * @param {object} deps
 * @param {function} deps.sendCommand
 * @param {function} deps.getEngineState
 * @param {function} deps.getWebConfig
 * @param {function} deps.getFb2cngExe
 */
function createDownloadRouter({ sendCommand, getEngineState, getWebConfig, getFb2cngExe }) {
    const router = express.Router();

    router.get('/:id', async (req, res) => {
        if (getEngineState() !== 'ready') {
            return res.status(503).json({ error: "Engine not ready" });
        }
        const id = parseInt(req.params.id, 10);
        if (isNaN(id) || id <= 0) {
            return res.status(400).json({ error: "Invalid book ID" });
        }

        const format = req.query.format === 'epub' ? 'epub' : 'fb2';
        const fb2cngExe = getFb2cngExe();

        if (format === 'epub' && !fb2cngExe) {
            return res.status(501).json({ error: "EPUB conversion is not configured on the server." });
        }

        try {
            const webConfig = getWebConfig();
            const tempDir = path.resolve(__dirname, '..', webConfig.tempDir);
            const uniqueOutDir = path.join(tempDir, crypto.randomUUID());
            fs.mkdirSync(uniqueOutDir, { recursive: true });

            const data = await sendCommand('export', { id, out: uniqueOutDir });

            if (!data.file) {
                fs.rmSync(uniqueOutDir, { recursive: true, force: true });
                throw new Error("No file returned from engine");
            }

            const filePath = path.resolve(uniqueOutDir, data.file);
            const resolvedOutDir = path.resolve(uniqueOutDir);
            if (!filePath.startsWith(resolvedOutDir + path.sep)) {
                fs.rmSync(uniqueOutDir, { recursive: true, force: true });
                return res.status(500).json({ error: 'Invalid file path from engine' });
            }

            let finalFilePath = filePath;
            let filename = data.filename || path.basename(filePath);

            if (format === 'epub') {
                const epubOutDir = path.join(uniqueOutDir, 'epub_out');
                fs.mkdirSync(epubOutDir, { recursive: true });
                try {
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
                } catch (e) {
                    fs.rmSync(uniqueOutDir, { recursive: true, force: true });
                    console.error(`[HTTP] EPUB conversion failed for book ${id}:`, e);
                    return res.status(500).json({ error: 'EPUB conversion failed' });
                }
            }

            res.download(finalFilePath, filename, (err) => {
                fs.rmSync(uniqueOutDir, { recursive: true, force: true });
                if (err) {
                    console.error(`[HTTP] Download error for book ${id}:`, err);
                }
            });
        } catch (e) {
            res.status(500).json({ error: e.message });
        }
    });

    return router;
}

module.exports = { createDownloadRouter };
