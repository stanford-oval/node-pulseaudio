"use strict";

const Pulse = require('..');
const wav = require('wav');
const fs = require('fs');

async function main() {
    const ctx = new Pulse();

    ctx.on('state', (state) => {
        console.log('context:', state);
    });

    const r1 = new wav.Reader();
    fs.createReadStream(process.argv[2]).pipe(r1);

    const r2 = new wav.Reader();
    fs.createReadStream(process.argv[3]).pipe(r2);

    r1.pause();
    r1.on('format', (fmt) => {
        console.log(fmt);

        const opts = {
            highWaterMark: 0,
            latency: 200000, // in usec
            channels: fmt.channels,
            rate: fmt.sampleRate,
            format: (fmt.signed ? 'S' : 'U') + fmt.bitDepth + fmt.endianness,
        };
        const play = ctx.createPlaybackStream(opts);

        let d1 = 0;
        r1.on('data', (data) => {
            const ok = play.write(data);
            console.log('r1 write', ok);
            d1 += data.length / (fmt.bitDepth / 8 * fmt.sampleRate * fmt.channels);
        });
        r1.on('end', () => {
            setTimeout(() => {
                play.discard();
                console.log('played r1');

                setTimeout(() => {

                let d2 = 0;
                r2.on('data', (data) => {
                    const ok = play.write(data);
                    console.log('r2 write', ok);
                    d2 += data.length / (fmt.bitDepth / 8 * fmt.sampleRate * fmt.channels);
                });
                r2.on('end', () => {
                    setTimeout(() => {
                        play.end();
                        ctx.end();
                    }, (d1+d2) * 1000);
                });
                r2.resume();
                }, 5000);
            }, d1 * 1000 * 0.2);
        });
        r1.resume();
    });
}
main();
