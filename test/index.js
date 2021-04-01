"use strict";

process.on('unhandledRejection', (up) => { throw up; });

require('./echo');
require('./info');
require('./volume');
require('./module');
