'use strict';

const express = require('express');

/**
 * @param {object} deps
 * @param {function} deps.sendCommand
 * @param {function} deps.getEngineState
 */
function createBooksRouter({ sendCommand, getEngineState }) {
    const router = express.Router();

    router.get('/', async (req, res) => {
        if (getEngineState() !== 'ready') return res.status(503).json({ error: "Engine not ready" });
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

    router.get('/:id', async (req, res) => {
        if (getEngineState() !== 'ready') return res.status(503).json({ error: "Engine not ready" });
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

    return router;
}

module.exports = { createBooksRouter };
