var Pulse = require('..');

var ctx = new Pulse({
  client: 'test-client'
});

ctx.on('state', function(state){
  console.log('context:', state);
});

//ctx.on('connection', function()
{
  var opts = {
//    highWaterMark: 160,
    channels:1,
    rate:8000,
    format:'s16le',
//    flags:'interpolate_timing+auto_timing_update+adjust_latency',
//    flags:'interpolate_timing+auto_timing_update',
    flags:'adjust_latency',
//    flags:'early_requests',
    latency:10000
  };

  var rec = ctx.createRecordStream(opts),
      play = ctx.createPlaybackStream(opts);

  rec.on('state', function(state){
    console.log('record:', state);
  });
  play.on('state', function(state){
    console.log('playback:', state);
  });

  rec.pipe(play);

  setTimeout(function(){
    rec.end();
    play.end();
    ctx.end();
  }, 5000);
}
//);
