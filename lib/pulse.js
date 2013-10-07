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

var Pulse = addon('../build/Release/pulse', '../build/Debug/pulse'),
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
      ctx = self.$ = new PulseContext(opts.client, function(state){
        self.emit('state', num2str(state, PulseContext.state));
        switch(state){
          case PulseContext.state.ready:
          self.emit('connection');
          break;
          case PulseContext.state.failed:
          self.emit('error');
          break;
          case PulseContext.state.terminated:
          self.emit('close');
          break;
        }
      });

  process.nextTick(function(){
    ctx.connect(opts.server, str2bit(opts.flags, PulseContext.flags, 'noflags'));
  });
}

util.inherits(Context, Events.EventEmitter);

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
  this.$.info(PulseContext.info.source_list, infoListWrap(cb));
};

Context.prototype.sink = function(cb){
  this.$.info(PulseContext.info.sink_list, infoListWrap(cb));
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

  var stm = self.$ = new PulseStream(ctx.$, str2num(opts.format, PulseStream.format), opts.rate, opts.channels, opts.stream, function(state){
    self.emit('state', num2str(state, PulseStream.state));
    switch(state){
      case PulseStream.state.ready:
      self.emit('connection');
      break;
      case PulseStream.state.failed:
      self.emit('error');
      break;
      case PulseStream.state.terminated:
      self.emit('close');
      break;
    }
  });

  self.on(self.readable ? 'end' : self.writable ? 'finish' : 'close', function(){
    self.$.disconnect();
  });

  process.nextTick(function(){
    stm.connect(opts.device, PulseStream.type[type]);
  });

  return stm;
}

/* Record Stream */

function RecordStream(ctx, opts){
  var self = this;

  Stream.Readable.call(self, opts);
  createStream(ctx, self, opts, 'record');

  self._read_cb = function(chunk){
    self.push(chunk);
  }

  self.play();
}

util.inherits(RecordStream, Stream.Readable);

//RecordStream.prototype.latency = getLatency;

RecordStream.prototype._read = function(size){};

RecordStream.prototype.stop = function(){
  this.$.read(null);
};

RecordStream.prototype.play = function(){
  this.$.read(this._read_cb);
};

RecordStream.prototype.end = function(){
  this.stop();
  this.emit('end');
};

/* Playback Stream */

function PlaybackStream(ctx, opts){
  Stream.Writable.call(this, opts);
  createStream(ctx, this, opts, 'playback');

  this._write_on = true;
}

util.inherits(PlaybackStream, Stream.Writable);

//PlaybackStream.prototype.latency = getLatency;

PlaybackStream.prototype._write = function(chunk, encoding, callback){
  if(this._write_on){
    this.$.write(chunk, callback);
  }else{
    callback();
  }
};

PlaybackStream.prototype.stop = function(){
  this.$.write(null, null);

  var buffer = this._writableState.buffer;
  for(; buffer.shift(); );

  this._write_on = false;
};

PlaybackStream.prototype.play = function(){
  this._write_on = true;
};

function getLatency(){
  return this.$.latency();
}

module.exports = Context;
