"use strict";

const Pulse = require('..');

async function main() {
    const ctx = new Pulse();

    ctx.on('state', (state) => {
        console.log('context:', state);
    });

    const server = await ctx.info();
    console.log('server:', server);

    let list = await ctx.source();
    console.log('source:', list);

    list = await ctx.sink();
    console.log('sink:', list);

    ctx.end();
}
module.exports = main;
if (!module.parent)
    main();
