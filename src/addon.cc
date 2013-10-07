#include "common.hh"
#include "context.hh"
#include "stream.hh"

namespace pulse {
  extern "C" void
  init(Handle<Object> exports){
    HandleScope scope;
    
    Context::Init(exports);
    Stream::Init(exports);
  }
}

NODE_MODULE(pulse, pulse::init);
