//
// This file is part of node-pulseaudio
//
// Copyright © 2013  Kayo Phoenix <kayo@illumium.org>
//             2017-2019 The Board of Trustees of the Leland Stanford Junior University
//
// This library is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this library. If not, see <http://www.gnu.org/licenses/>.
"use strict";

const Pulse = require('bindings')('pulse.node');
const Events = require('events');
const Stream = require('stream');

const PulseContext = Pulse.Context;
const PulseStream = Pulse.Stream;

function str2bit(val, set, def) {
    let bit = set[def];
    if ('string' === typeof val) {
        val = val.split(/[\s,.+|]/);
        for (var key in set) {
            if (val.indexOf(key) >= 0)
                bit |= set[key];
        }
    }
    return bit;
}

function str2num(val, set, def) {
    if ('string' === typeof val) {
        if (val in set)
            return set[val];
        val = val.toLowerCase();
        if (val in set)
            return set[val];
        val = val.toUpperCase();
        if (val in set)
            return set[val];
    }
    return def;
}

function num2str(val, set, def) {
    for (var key in set) {
        if (set[key] === val)
            return key;
    }
    return def;
}

function waitConnection(obj) {
    if (obj._connected)
        return Promise.resolve();

    return new Promise((resolve, reject) => {
        obj.once('connection', () => {
            obj.removeListener('error', reject);
            resolve();
        });
        obj.once('error', reject);
    });
}

function makePromise(obj) {
    let _resolve;
    const promise = new Promise((resolve, reject) => {
        _resolve = (res) => {
            obj.removeListener('error', reject);
            resolve(res);
        };
        obj.once('error', reject);
    });
    return [promise, _resolve];
}

function infoListWrap(obj) {
    let _resolve;
    const promise = new Promise((resolve, reject) => {
        _resolve = (list) => {
            obj.removeListener('error', reject);
            resolve(list);
        };
        obj.once('error', reject);
    });

    return [promise, (list) => {
        for (var i = 0; i < list.length; i++) {
            if ('format' in list[i])
                list[i].format = num2str(list[i].format, PulseStream.format);
        }
        _resolve(list);
    }];
}

class Context extends Events.EventEmitter {
    constructor(opts) {
        super();
        opts = opts || {};

        const ctx = this.$ = new PulseContext(opts.client, opts.properties || {}, (state, error) => {
            this.emit('state', num2str(state, PulseContext.state));

            switch(state){
            case PulseContext.state.ready:
                this._connected = true;
                this.emit('connection');
                break;
            case PulseContext.state.failed:
                this.emit('error', new Error(error.message));
                break;
            case PulseContext.state.terminated:
                this._connected = false;
                this.emit('close');
                break;
            }
        });

        this._connected = false;

        process.nextTick(() => {
            try{
                ctx.connect(opts.server, str2bit(opts.flags, PulseContext.flags, 'noflags'));
            }catch(er){
                this.emit('error', er);
            }
        });
    }

    _connection(cb) {
        if (this._connected)
            cb.call(this);
        else
            this.once('connection', cb);
    }

    async setSinkMute(sink, mute) {
        await waitConnection(this);
        const [promise, cb] = makePromise(this);
        this.$.set_mute(PulseContext.info.sink_list, sink, mute ? 1 : 0, cb);
        return promise;
    }

    async setSinkVolume(sink, volume) {
        await waitConnection(this);
        const [promise, cb] = makePromise(this);
        this.$.set_volume(PulseContext.info.sink_list, sink, volume, cb);
        return promise;
    }

    async setSourceMute(sink, mute) {
        await waitConnection(this);
        const [promise, cb] = makePromise(this);
        this.$.set_mute(PulseContext.info.source_list, sink, mute ? 1 : 0, cb);
        return promise;
    }

    async setSourceVolume(sink, volume) {
        await waitConnection(this);
        const [promise, cb] = makePromise(this);
        this.$.set_volume(PulseContext.info.source_list, sink, volume, cb);
        return promise;
    }

    async info() {
        await waitConnection(this);
        const [promise, cb] = makePromise(this);
        this.$.info(PulseContext.info.server, cb);
        return promise;
    }

    async modules() {
        await waitConnection(this);
        const [promise, cb] = makePromise(this);
        this.$.info(PulseContext.info.module_list, cb);
        return promise;
    }

    async source() {
        await waitConnection(this);
        const [promise, cb] = infoListWrap(this);
        this.$.info(PulseContext.info.source_list, cb);
        return promise;
    }

    async sink() {
        await waitConnection(this);
        const [promise, cb] = infoListWrap(this);
        this.$.info(PulseContext.info.sink_list, cb);
        return promise;
    }

    async loadModule(name, args) {
        await waitConnection(this);
        const [promise, cb] = makePromise(this);
        this.$.load_module(name, args ? args : "", cb);
        return promise;
    }

    async unloadModule(index) {
        await waitConnection(this);
        const [promise, cb] = makePromise(this);
        this.$.unload_module(index, cb);
        return promise;
    }

    createRecordStream(opts) {
        return new RecordStream(this, opts);
    }
    createPlaybackStream(opts) {
        return new PlaybackStream(this, opts);
    }

    end() {
        this.emit('end');
        this.$.disconnect();
    }
}

/* Streams */

function createStream(ctx, self, opts, type){
    opts = opts || {};

    self._connected = false;

    const stm = self.$ = new PulseStream(ctx.$, str2num(opts.format, PulseStream.format), opts.rate, opts.channels, opts.latency, opts.stream, opts.properties || {}, (state, error) => {
        self.emit('state', num2str(state, PulseStream.state));
        switch(state){
        case PulseStream.state.ready:
            self._connected = true;
            self.emit('connection');
            break;
        case PulseStream.state.failed:
            self.emit('error', new Error(error.message));
            break;
        case PulseStream.state.terminated:
            self._connected = false;
            self.emit('close');
            break;
        }
    });

    self.on(({
        record: 'end',
        playback: 'finish'
    })[type], () => {
        self.$.disconnect();
    });

    ctx._connection(() => {
        stm.connect(opts.device, PulseStream.type[type], str2bit(opts.flags, PulseStream.flags));
    });

    self.play();

    return stm;
}

/* Record Stream */

class RecordStream extends Stream.Readable {
    constructor(ctx, opts) {
        super(opts);

        this._read_cb = (chunk) => {
            this.push(chunk);
            // for some unknown reason, the following cleans correctly the references
            // it's necessary to avoid memory leak on chunks
            chunk = null;
        };

        createStream(ctx, this, opts, 'record');
    }

    _read(size) {
    }

    stop() {
        this.$.read(null);

        this.stopped = !(this.playing = false);
        this.emit('stop');

        return this;
    }

    play() {
        this.$.read(this._read_cb);

        this.stopped = !(this.playing = true);
        this.emit('play');

        return this;
    }

    end() {
        this.stop();
        this.push(null);
    }
}

/* Playback Stream */

class PlaybackStream extends Stream.Writable {
    constructor (ctx, opts){
        super(opts);
        createStream(ctx, this, opts, 'playback');

        this._writableState.discard = 0;
    }

    async _write(chunk, encoding, done) {
        const ws = this._writableState;

        if (ws.discard > 0) {
            ws.discard --;
            done();
            return;
        }

        try {
            await waitConnection(this);

            if (this.playing)
                this.$.write(chunk, done);
            else
                done();
        } catch(e) {
            done(e);
        }
    }

    stop() {
        this.discard();

        this.stopped = !(this.playing = false);
        this.emit('stop');

        return this;
    }

    play() {
        this.stopped = !(this.playing = true);
        this.emit('play');

        return this;
    }

    discard() {
        var ws = this._writableState;

        ws.discard = ws.bufferedRequestCount;

        if (this._connected)
            this.$.write(null, null);
    }
}

module.exports = Context;
