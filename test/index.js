"use strict";

process.on('unhandledRejection', (up) => { throw up; });

async function seq(array) {
    for (const mod of array) {
        console.log(`Running ${mod}`);
        await require(mod)();
    }
}

seq([
('./echo'),
('./info'),
('./volume'),
('./module')
]);
