#ifndef __COMMON_HH__
#define __COMMON_HH__

extern "C" {
#include <pulse/pulseaudio.h>
}

#include <v8.h>
#include <node.h>
#include <node_version.h>
#include <node_object_wrap.h>
#include <node_buffer.h>
#include <uv.h>
#include <memory>

#include "debug.hh"

//#include <string>
//#include <sstream>

namespace pulse {
  using namespace std;
  using namespace v8;
  using namespace node;

#define EXCEPTION(_type_, _isolate_, _msg_) Exception::_type_(String::NewFromOneByte(_isolate_, (const uint8_t*)_msg_))
#define THROW_ERROR(_type_, _isolate_, _msg_) _isolate_->ThrowException(EXCEPTION(_type_, _isolate_, _msg_))
#define RET_ERROR(_type_, _isolate_, _msg_) { THROW_ERROR(_type_, _isolate_, _msg_); return; }
#define THROW_SCOPE(_type_, _isolate_, _msg_) { THROW_ERROR(_type_, _isolate_, _msg_); return; }

#ifdef DIE_ON_EXCEPTION
#define HANDLE_CAUGHT(_isolate_, _try_)                    \
  if(_try_.HasCaught()){                        \
    FatalException(_isolate_, _try_);                      \
  }
#else
#define HANDLE_CAUGHT(_isolate_, _try_)                    \
  if(_try_.HasCaught()){                        \
    DisplayExceptionLine(_try_);                \
  }
#endif

#define PA_ASSERT(_isolate_, action) {                       \
    int __status = (action);                      \
    if(__status < 0){                             \
      THROW_SCOPE(Error, _isolate_, pa_strerror(__status));  \
    }                                             \
  }

#define JS_ASSERT(_isolate_, condition)                                   \
  if(!(condition)){                                            \
    THROW_SCOPE(Error, _isolate_, "Assertion failed: `" #condition "`!"); \
  }

#define DefineConstant(_isolate_, target, name, constant)                          \
  (target)->DefineOwnProperty(_isolate_->GetCurrentContext(),                      \
                String::NewFromOneByte(_isolate_, (const uint8_t*) #name),         \
                Number::New(_isolate_, constant),                                  \
                static_cast<PropertyAttribute>(ReadOnly|DontDelete)).FromJust()

#define AddEmptyObject(_isolate_, target, name)                                    \
  Local<Object> name = Object::New(_isolate_);                                     \
  (target)->DefineOwnProperty(_isolate_->GetCurrentContext(),                      \
                String::NewFromOneByte(_isolate_, (const uint8_t*) #name),         \
                name,                                                              \
                static_cast<PropertyAttribute>(ReadOnly|DontDelete)).FromJust()
  
}

#endif//__COMMON_HH__
