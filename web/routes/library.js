'use strict';

const express = require('express');

/**
 * @param {object} deps
 * @param {function} deps.sendCommand
 * @param {function} deps.getEngineState
 * @param {function} deps.setEngineState
 * @param {function} deps.getLastProgress
 * @param {function} deps.resetProgress
 */
function createLibraryRouter({ sendCommand, getEngineState, setEngineState, getLastProgress, resetProgress }) {
    const router = express.Router();

    router.get('/status', (req, res) => {
        res.json({ state: getEngineState(), progress: getLastProgress() });
    });

    router.get('/stats', async (req, res) => {
        if (getEngineState() !== 'ready') return res.status(503).json({ error: "Engine not ready" });
        try {
            const data = await sendCommand('stats');
            res.json(data);
        } catch (e) {
            res.status(500).json({ error: e.message });
        }
    });

    router.post('/import', async (req, res) => {
        if (getEngineState() !== 'ready') return res.status(503).json({ error: "Engine busy or not ready" });
        setEngineState('importing');
        resetProgress();
        res.status(202).json({ message: "Import started" });

        try {
            await sendCommand('import', {}, 0);
        } catch (e) {
            console.error('[Server] Import error:', e);
        } finally {
            if (getEngineState() === 'importing') setEngineState('ready');
        }
    });

    router.post('/upgrade', async (req, res) => {
        if (getEngineState() !== 'ready') return res.status(503).json({ error: "Engine busy or not ready" });
        setEngineState('upgrading');
        resetProgress();
        res.status(202).json({ message: "Upgrade started" });

        try {
            await sendCommand('upgrade', {}, 0);
        } catch (e) {
            console.error('[Server] Upgrade error:', e);
        } finally {
            if (getEngineState() === 'upgrading') setEngineState('ready');
        }
    });

    return router;
}

module.exports = { createLibraryRouter };
