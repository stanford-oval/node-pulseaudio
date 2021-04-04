"use strict";

const Pulse = require('..');

async function main() {
    const ctx = new Pulse({
        client: 'test-client',
    });

    let testModule = "module-null-sink";
    let oModulesList;

    oModulesList = await ctx.modules();
    for (let i = 0; i < oModulesList.length; i++) {
        const m = oModulesList[i];
        if (m.name === testModule) {
            console.log("remove", testModule);
            await ctx.unloadModule(m.index);
        }
    }

    oModulesList = await ctx.modules();
    oModulesList.forEach((m) => {
        if (m.name === testModule)
            throw new Error("failed to remove " + testModule);
    });

    let result = await ctx.loadModule(testModule);
    console.log("load", testModule, "index", result);

    oModulesList = await ctx.modules();
    oModulesList.forEach((m) => {
        if (m.name === testModule)
            console.log("module", testModule, "found");
    });

    ctx.end();
}
module.exports = main;
if (!module.parent)
    main();
