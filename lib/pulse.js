function addon(){
  var i = 0,
      errors = [];
  for(; i < arguments.length; ){
    try{
      return require(arguments[i++]);
    }catch(e){
      errors.push(e.message);
    }
  }
  throw new Error('Unable to load addon:\n' + errors.join('\n'));
}

var Pulse = addon('../build/Release/obj.target/pulse', '../build/Release/pulse', '../build/Debug/pulse'),
    Events = require('events'),
    Stream = require('stream'),
    util = require('util'),

    PulseContext = Pulse.Context,
    PulseStream = Pulse.Stream;

function str2bit(val, set, def){
  var bit = set[def];
  if('string' == typeof val){
    val = val.split(/[\s\,\.\+\|]/);
    for(var key in set){
      if(val.indexOf(key) >= 0){
        bit |= set[key];
      }
    }
  }
  return bit;
}

function str2num(val, set, def){
  if('string' == typeof val){
    if(val in set){
      return set[val];
    }
    val = val.toLowerCase();
    if(val in set){
      return set[val];
    }
    val = val.toUpperCase();
    if(val in set){
      return set[val];
    }
  }
  return def;
}

function num2str(val, set, def){
  for(var key in set){
    if(set[key] == val){
      return key;
    }
  }
  return def;
}

function Context(opts){
  if(!(this instanceof Context)){
    return new Context(opts);
  }

  opts = opts || {};

  var self = this,
      ctx = self.$ = new PulseContext(opts.client, function(state, error){
        self.emit('state', num2str(state, PulseContext.state));
        switch(state){
          case PulseContext.state.ready:
          self._connected = true;
          self.emit('connection');
          break;
          case PulseContext.state.failed:
          self.emit('error', new Error(error.message));
          break;
          case PulseContext.state.terminated:
          self._connected = false;
          self.emit('close');
          break;
        }
      });

  self._connected = false;

  process.nextTick(function(){
    try{
      ctx.connect(opts.server, str2bit(opts.flags, PulseContext.flags, 'noflags'));
    }catch(er){
      self.emit('error', er);
    }
  });
}

util.inherits(Context, Events.EventEmitter);

function whenConnection(func){
  if(this._connected){
    func.call(this);
  }else{
    this.once('connection', func);
  }
  return this;
}

Context.prototype._connection = whenConnection;

function infoListWrap(cb){
  return function(list){
    for(var i = 0; i < list.length; i++){
      if('format' in list[i]){
        list[i].format = num2str(list[i].format, PulseStream.format);
      }
    }
    cb(list);
  };
}

Context.prototype.source = function(cb){
  return this._connection(function getSource(){
    this.$.info(PulseContext.info.source_list, infoListWrap(cb));
  });
};

Context.prototype.sink = function(cb){
  return this._connection(function getSink(){
    this.$.info(PulseContext.info.sink_list, infoListWrap(cb));
  });
};

Context.prototype.record = Context.prototype.createRecordStream = function(opts){
  return new RecordStream(this, opts);
};

Context.prototype.playback = Context.prototype.createPlaybackStream = function(opts){
  return new PlaybackStream(this, opts);
};

Context.prototype.end = function(){
  this.emit('end');
  this.$.disconnect();
};

/* Streams */

function createStream(ctx, self, opts, type){
  opts = opts || {};

  self._connected = false;

  var stm = self.$ = new PulseStream(ctx.$, str2num(opts.format, PulseStream.format), opts.rate, opts.channels, opts.latency, opts.stream, function streamState(state, error){
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
  })[type], function(){
    self.$.disconnect();
  });

  ctx._connection(function connectStream(){
    stm.connect(opts.device, PulseStream.type[type], str2bit(opts.flags, PulseStream.flags));
  });

  self.play();

  return stm;
}

/* Record Stream */

function RecordStream(ctx, opts){
  var self = this;

  self._read_cb = function recordRead(chunk){
    self.push(chunk);
  }

  Stream.Readable.call(self, opts);
  createStream(ctx, self, opts, 'record');
}

util.inherits(RecordStream, Stream.Readable);

RecordStream.prototype._connection = whenConnection;

//RecordStream.prototype.latency = getLatency;

RecordStream.prototype._read = function(size){};

RecordStream.prototype.stop = function(){
  this.$.read(null);

  this.stopped = !(this.playing = false);
  this.emit('stop');

  return this;
};

RecordStream.prototype.play = function(){
  this.$.read(this._read_cb);

  this.stopped = !(this.playing = true);
  this.emit('play');

  return this;
};

RecordStream.prototype.end = function(){
  this.stop();
  this.emit('end');
};

/* Playback Stream */

function PlaybackStream(ctx, opts){
  Stream.Writable.call(this, opts);
  createStream(ctx, this, opts, 'playback');

  this._writableState.discard = 0;
}

util.inherits(PlaybackStream, Stream.Writable);

PlaybackStream.prototype._connection = whenConnection;

//PlaybackStream.prototype.latency = getLatency;

PlaybackStream.prototype._write = function(chunk, encoding, done){
  return this._connection(function playbackWrite(){
    var ws = this._writableState;

    if(ws.discard){
      ws.discard--;
      done();
    }

    if(this.playing){
      this.$.write(chunk, done);
    }else{
      done();
    }
  });
};

PlaybackStream.prototype.stop = function(){
  this.discard();

  this.stopped = !(this.playing = false);
  this.emit('stop');

  return this;
};

PlaybackStream.prototype.play = function(){
  this.stopped = !(this.playing = true);
  this.emit('play');

  return this;
};

PlaybackStream.prototype.discard = function(){
  var ws = this._writableState;

  ws.discard = ws.buffer.length;

  if(this._connected){
    this.$.write(null, null);
  }
};

function getLatency(){
  return this.$.latency();
}

module.exports = Context;
