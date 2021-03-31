"use strict";

const memwatch = require("@atsjj/memwatch");
const Pulse = require('..');
const WavWriter = require('wav').FileWriter;
const fs = require('fs');

async function main() {
    const ctx = new Pulse({
        client: 'test-client',
    });

    const opts = {
        channels:1,
        rate:16000,
        format:'s16le',
        flags:'adjust_latency',
        latency:10000
    };

    const rec = ctx.createRecordStream(opts);
    const out = new WavWriter('./out.wav', {
        sampleRate:16000,
        channels:1
    });

    rec.pipe(out);
    process.on('SIGINT', () => {
        rec.end();
        out.end();
        ctx.end();
    });
}

main();
