'use strict';

const { spawn } = require('child_process');
const path = require('path');

let libriumProcess = null;

/**
 * Start the Librium C++ engine process.
 * @param {object} webConfig - Loaded web configuration.
 * @param {function} onExit - Called when the process exits (code).
 */
function startLibrium(webConfig, onExit) {
    const exePath = path.resolve(__dirname, '..', webConfig.libriumExe);
    const configPath = path.resolve(__dirname, '..', webConfig.libriumConfig);

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
        onExit(code);
    });
}

/**
 * Kill the engine process if running.
 */
function killLibrium() {
    if (libriumProcess) {
        libriumProcess.kill();
        libriumProcess = null;
    }
}

module.exports = { startLibrium, killLibrium };
