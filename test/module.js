"use strict";

const Pulse = require('..');
const fs = require('fs');

async function main() {
    const ctx = new Pulse({
        client: 'test-client',
    });

    let oModulesList;

    oModulesList = await ctx.modules();
    for (let i = 0; i < oModulesList.length; i++) {
        const m = oModulesList[i];
        if (m.name == "module-null-sink") {
            console.log("remove module-null-sink");
            await ctx.unloadModule(m.index);
        }
    }

    oModulesList = await ctx.modules();
    oModulesList.forEach((m) => {
        if (m.name == "module-null-sink") {
            console.log("failed to remove module-null-sink");
            process.exit(1);
        }
    });

    let result = await ctx.loadModule("module-null-sink");
    console.log("load module, index", result);

    oModulesList = await ctx.modules();
    oModulesList.forEach((m) => {
        if (m.name == "module-null-sink") {
            console.log("module found");
            return true;
        }
    });

    ctx.end();
}

main();
