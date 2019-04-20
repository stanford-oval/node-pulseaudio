"use strict";

var Pulse = require('..');

var ctx = new Pulse({
    client: 'test-client',
    properties: {
        'application.id': 'node-pulseaudio-test-client'
    }
});

ctx.on('state', (state) => {
    console.log('context:', state);
});


var opts = {
//    highWaterMark: 160,
    channels:1,
    rate:8000,
    format:'s16le',
//    flags:'interpolate_timing+auto_timing_update+adjust_latency',
//    flags:'interpolate_timing+auto_timing_update',
    flags:'adjust_latency',
//    flags:'early_requests',
    latency:10000,

    properties: {
        'media.role': 'phone',
    }
};
var rec = ctx.createRecordStream(opts);
rec.play();
rec.on('data', () => {});

rec.on('state', (state) => {
    console.log('record:', state);
});

setTimeout(() => {
    rec.end();
    ctx.end();
}, 30000);
