"use strict";

const Pulse = require('..');

function sleep(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
}

async function testSink(ctx, server) {
    const current = (await ctx.sink()).find((sink) => sink.name === server.default_sink_name);

    console.log('set sink mute');
    await ctx.setSinkMute(server.default_sink_name, 1);
    await sleep(5000);

    console.log('set sink unmute');
    await ctx.setSinkMute(server.default_sink_name, 0);
    await sleep(5000);

    console.log('set sink volume');
    await ctx.setSinkVolume(server.default_sink_name, [65536/2, 65536/2]);
    await sleep(5000);

    console.log('restoring');
    await ctx.setSinkMute(server.default_sink_name, current.mute);
    await ctx.setSinkVolume(server.default_sink_name, current.volume);
}

async function testSource(ctx, server) {
    const current = (await ctx.source()).find((source) => source.name === server.default_source_name);

    console.log('set source mute');
    await ctx.setSourceMute(server.default_source_name, 1);
    await sleep(5000);

    console.log('set source unmute');
    await ctx.setSourceMute(server.default_source_name, 0);
    await sleep(5000);

    console.log('set source volume');
    await ctx.setSourceVolume(server.default_source_name, [65536/2, 65536/2]);
    await sleep(5000);

    console.log('restoring');
    await ctx.setSourceMute(server.default_source_name, current.mute);
    await ctx.setSourceVolume(server.default_source_name, current.volume);
}

async function main() {
    const ctx = new Pulse();

    ctx.on('state', (state) => {
        console.log('context:', state);
    });

    const server = await ctx.info();
    console.log('server:', server);

    await testSink(ctx, server);
    await testSource(ctx, server);

    ctx.end();
}
module.exports = main;
if (!module.parent)
    main();
