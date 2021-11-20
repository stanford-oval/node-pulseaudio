//
// This file is part of node-pulseaudio
//
// Copyright © 2021 The Board of Trustees of the Leland Stanford Junior University
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

import * as events from 'events';
import * as stream from 'stream';

declare class PulseAudio extends events.EventEmitter {
    constructor(options ?: {
        client ?: string;
        server ?: string;
        flags ?: string;
        properties ?: Record<string, string>
    });

    on(ev : 'connection', cb : () => void) : this;
    on(ev : 'error', cb : (err : Error) => void) : this;
    on(ev : 'close', cb : () => void) : this;
    on(ev : 'end', cb : () => void) : this;

    setSinkMute(sink : string|number, mute : boolean) : Promise<void>;
    setSinkVolume(sink : string|number, volume : number[]) : Promise<void>;

    setSourceMute(sink : string|number, mute : boolean) : Promise<void>;
    setSourceVolume(sink : string|number, volume : number[]) : Promise<void>;

    info() : Promise<PulseAudio.ServerInfo>;
    modules() : Promise<PulseAudio.ModuleInfo[]>;
    source() : Promise<PulseAudio.SourceOrSinkInfo[]>;
    sink() : Promise<PulseAudio.SourceOrSinkInfo[]>;

    loadModule(name : string, args ?: string) : Promise<void>;
    unloadModule(index : number) : Promise<void>;

    createRecordStream(opts ?: PulseAudio.StreamOptions) : PulseAudio.RecordStream;
    createPlaybackStream(opts ?: PulseAudio.StreamOptions) : PulseAudio.PlaybackStream;

    end() : void;
}

declare namespace PulseAudio {
    export interface SourceOrSinkInfo {
        name : string;
        index : number;
        description : string;
        format : string;
        rate : number;
        channels : number;
        mute : boolean;
        latency : number;
        driver : string;
        volume : number[];
    }

    export interface ServerInfo {
        user_name : string;
        host_name : string;
        server_version : string;
        server_name : string;
        default_sink_name : string;
        default_source_name : string;
        cookie : number;
    }

    export interface ModuleInfo {
        name : string;
        index : number;
        argument : string;
        n_used : number;
    }

    export interface StreamOptions {
        format ?: string;
        rate ?: number;
        channels ?: number;
        latency ?: number;
        stream ?: string;
        properties ?: Record<string, string>;
        device ?: string;
        flags ?: string;
    }

    export interface PlaybackStream extends stream.Writable {
        stop() : void;
        play() : void;
        discard() : void;
    }

    export interface RecordStream extends stream.Readable {
        stop() : void;
        play() : void;
        end() : void;
    }
}

export = PulseAudio;
