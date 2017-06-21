#include "common.hh"
#include "context.hh"
#include "stream.hh"

namespace pulse {
  extern "C" void
  init(Local<Object> exports){
    HandleScope scope(exports->GetIsolate());
    
    Context::Init(exports);
    Stream::Init(exports);
  }
}

NODE_MODULE(pulse, pulse::init);
