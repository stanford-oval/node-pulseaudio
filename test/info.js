"use strict";

const Pulse = require('..');

async function main() {
    const ctx = new Pulse();

    ctx.on('state', (state) => {
        console.log('context:', state);
    });

    let list = await ctx.source();
    console.log('source:', list);

    list = await ctx.sink();
    console.log('sink:', list);

    ctx.end();
}
main();
