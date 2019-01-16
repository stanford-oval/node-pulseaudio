#include "common.hh"
#include "context.hh"
#include "stream.hh"

namespace pulse {
  void
  init(Local<Object> exports){
    HandleScope scope(exports->GetIsolate());
    
    Context::Init(exports);
    Stream::Init(exports);
  }
}

NODE_MODULE(NODE_GYP_MODULE_NAME, pulse::init);
