"use strict";

const Pulse = require('..');

async function main() {
    const ctx = new Pulse({
        client: 'test-client',
    });

    ctx.on('state', (state) => {
        console.log('context:', state);
    });

    const opts = {
        //    highWaterMark: 160,
        channels:1,
        rate:8000,
        format:'s16le',
        //    flags:'interpolate_timing+auto_timing_update+adjust_latency',
        //    flags:'interpolate_timing+auto_timing_update',
        flags:'adjust_latency',
        //    flags:'early_requests',
        latency:10000
    };

    const rec = ctx.createRecordStream(opts),
      play = ctx.createPlaybackStream(opts);

    rec.on('state', (state) => {
    console.log('record:', state);
    });
    play.on('state', (state) => {
    console.log('playback:', state);
    });

    rec.pipe(play);

    await new Promise((resolve) => { setTimeout(resolve, 5000); });

    rec.end();
    play.end();
    ctx.end();
}
module.exports = main;
if (!module.parent)
    main();
