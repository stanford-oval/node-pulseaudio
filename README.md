# Overview

[![Build Status](https://travis-ci.com/stanford-oval/node-pulseaudio.svg?branch=master)](https://travis-ci.com/stanford-oval/node-pulseaudio) [![Coverage Status](https://coveralls.io/repos/github/stanford-oval/node-pulseaudio/badge.svg?branch=master)](https://coveralls.io/github/stanford-oval/node-pulseaudio?branch=master) [![Dependency Status](https://david-dm.org/stanford-oval/node-pulseaudio/status.svg)](https://david-dm.org/stanford-oval/node-pulseaudio)

[PulseAudio](http://www.freedesktop.org/wiki/Software/PulseAudio/) is an free cross-platform audio server. This addon may be usefull for creating **PulseAudio** clients on **JS**, which runs on **NodeJS**.
You can retrieve source/sink info from server, create Record and Playback streams.

This module provides [libuv](https://github.com/joyent/libuv)-based **MainLoop API** for **PulseAudio** Context, it means that client uses same thread, where **V8** runs.
Mainloop API consists of three things: I/O Event Polling, Deferred Calls and Timers. Resolution of timers limited by milliseconds.

This is a fork of [node-pulseaudio](https://bitbucket.org/kayo/node-pulseaudio), which was unmaintained.

# Installation

Install module with npm:

    npm install pulseaudio2

# Usage

Require and use:

    var PulseAudio = require('pulseaudio');
    var context = PulseAudio();
    var player = context.createPlaybackStream();
    player.write(data);
    player.on('drain', function(){
      player.end();
    });

## High-Level API

### Context

Context is a base communication point for client.

    var context = new PulseAudio({
      client: "my-awesome-app",           // optional client name ("node-pulse" by default)
      server: "my-preferred-server",      // optional server name
      flags: "noflags|noautospawn|nofail" // optional connection flags (see PulseAudio documentation)
    });

You can listen context state.

    context.on('state', function(state){
      // state == "connecting|authorizing|setting_name|ready|terminated"
    });

And exceptions errors.

    context.on('error', function(error){
      //
    });

Before we can operating with context we may wait until connection will be established.

    context.on('connection', function(){
      // operations
      context.end();
    });

But usually it doesn't require, because any operations execute deferred.

How we can retrieve `sink` / `source` lists from server.

    const list = await context.[sink|source]();
    // list[0].name - name of first sink/source

And open streams.

### Streams

Streams designed for sound i/o.

We can create `record` / `playback` streams.

    var stream = context.createPlaybackStream({
      stream: "my-awesome-stream",                         // optional stream name ("node-stream" by default)
      device: "my-preferred-device",                       // optional device name
      format: "U8|S(16|24|32)(LE|BE)|F32(BE|LE)|(A|U)LAW", // optional sample format ("S16LE" by default)
      rate: 8000|22050|44100|48000|96000|192000|N,         // optional sample rate (44100 by default)
      channels: 1|2|N,                                     // optional channels (2 (stereo) by default)
      latency: 250000,                                     // optional latency in microseconds
      flags: 'adjust_latency|early_requests|...'           // optional flags (see Pulseaudio docs)
    });

But really streams will be initialized after context connection established.

You can monitor stream state.

    stream.on('state', function(state){
      // state == "creating|ready|terminated"
    });

And stream errors.

    stream.on('error', function(error){
      //
    });

Before we can operating with stream we must wait until connection will be established.

    stream.on('connection', function(){
      //
    });

We can `write` / `read` data to / from stream by using **NodeJS** **Stream API**.

    stream.write(chunk);
    stream.on('data', function(chunk){
      //
    });
    recorder.pipe(player);
    // and many more

In addition, we may pause / resume streams by calling `stop` / `play` methods.

    stream.stop();
    stream.play();

Stopping discards any unplayed samples from stream.

Of course, we can listen `stop` / `play` events and check `stopped` / `playing` properties.

Note that we don't need to use `pause` / `resume` methods with sound streams.

# Licensing

This addon are available under GNU Lesser General Public License version 3 or later.

node-pulseaudio — [PulseAudio](http://www.freedesktop.org/wiki/Software/PulseAudio/) integration addon for [NodeJS](http://nodejs.org/).  
Copyright © 2013  Kayo Phoenix <kayo@illumium.org>  
            2017-2019 The Board of Trustees of the Leland Stanford Junior University  

This library is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this library. If not, see <http://www.gnu.org/licenses/>.
