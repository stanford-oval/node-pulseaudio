"use strict";

const Pulse = require('..');
const wav = require('wav');
const fs = require('fs');

async function main() {
    const ctx = new Pulse();

    ctx.on('state', (state) => {
        console.log('context:', state);
    });

    const reader = new wav.Reader();
    fs.createReadStream(process.argv[2]).pipe(reader);

    reader.pause();
    reader.on('format', (fmt) => {
        console.log(fmt);

        const opts = {
            channels: fmt.channels,
            rate: fmt.sampleRate,
            format: (fmt.signed ? 'S' : 'U') + fmt.bitDepth + fmt.endianness,
            latency: 500000 // in us
        };
        const play = ctx.createPlaybackStream(opts);

        let duration = 0;
        reader.on('data', (data) => {
            play.write(data);
            duration += data.length / (fmt.bitDepth / 8 * fmt.sampleRate * fmt.channels);
        });
        reader.on('end', () => {
            setTimeout(() => {
                play.end();
                ctx.end();
            }, duration * 1000);
        });
        reader.resume();
    });
}
main();
