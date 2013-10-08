var Pulse = require('..');

var ctx = new Pulse({
  client: 'test-client'
});

ctx.on('state', function(state){
  console.log('context:', state);
});

ctx.on('connection', function(){
  var opts = {
    channels:1,
    rate:8000,
    format:'s16le'
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
});
