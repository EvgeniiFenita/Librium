'use strict';

const express = require('express');
const fsPromises = require('fs/promises');
const path = require('path');
const { getCoverFromCache, addCoverToCache } = require('../lib/coverCache');

/**
 * @param {object} deps
 * @param {function} deps.getMetaDir - Returns the current metaDir string (set after loadConfig).
 */
function createCoversRouter({ getMetaDir }) {
    const router = express.Router();

    router.get('/:id/:filename', async (req, res) => {
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

        const base = path.resolve(getMetaDir());
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

    return router;
}

module.exports = { createCoversRouter };
