var Pulse = require('..');

var ctx = new Pulse();

ctx.on('state', function(state){
  console.log('context:', state);
});

ctx.on('connection', function(){
  ctx.source(function(list){
    console.log('source:', list);
    ctx.source(function(list){
      console.log('sink:', list);
      ctx.end();
    });
  });
});
