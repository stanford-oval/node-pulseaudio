"use strict";

const Pulse = require('..');
const fs = require('fs');

async function main() {
    const ctx = new Pulse({
        client: 'test-client',
    });

    let testModule = "module-augment-properties";
    let oModulesList;

    oModulesList = await ctx.modules();
    for (let i = 0; i < oModulesList.length; i++) {
        const m = oModulesList[i];
        if (m.name == testModule) {
            console.log("remove", testModule);
            await ctx.unloadModule(m.index);
        }
    }

    oModulesList = await ctx.modules();
    oModulesList.forEach((m) => {
        if (m.name == testModule) {
            console.log("failed to remove", testModule);
            process.exit(1);
        }
    });

    let result = await ctx.loadModule(testModule);
    console.log("load", testModule, "index", result);

    oModulesList = await ctx.modules();
    oModulesList.forEach((m) => {
        if (m.name == testModule) {
            console.log("module", testModule, "found");
            return true;
        }
    });

    ctx.end();
}

main();
