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

#include "debug.hh"

//#include <string>
//#include <sstream>

namespace pulse {
  using namespace std;
  using namespace v8;
  using namespace node;
  
#define EXCEPTION(_type_, _msg_) Exception::_type_(String::New(_msg_))
#define THROW_ERROR(_type_, _msg_) ThrowException(EXCEPTION(_type_, _msg_))
#define RET_ERROR(_type_, _msg_) return THROW_ERROR(_type_, _msg_)
#define THROW_SCOPE(_type_, _msg_) return scope.Close(THROW_ERROR(_type_, _msg_))

#ifdef DIE_ON_EXCEPTION
#define HANDLE_CAUGHT(_try_)                    \
  if(_try_.HasCaught()){                        \
    FatalException(_try_);                      \
  }
#else
#define HANDLE_CAUGHT(_try_)                    \
  if(_try_.HasCaught()){                        \
    DisplayExceptionLine(_try_);                \
  }
#endif

#define PA_ASSERT(action) {                       \
    int __status = (action);                      \
    if(__status < 0){                             \
      THROW_SCOPE(Error, pa_strerror(__status));  \
    }                                             \
  }

#define JS_ASSERT(condition)                                   \
  if(!(condition)){                                            \
    THROW_SCOPE(Error, "Assertion failed: `" #condition "`!"); \
  }

#define DefineConstant(target, name, constant)                          \
  (target)->Set(String::NewSymbol(#name),                               \
                Number::New(constant),                                  \
                static_cast<PropertyAttribute>(ReadOnly|DontDelete))
  
#define AddEmptyObject(target, name)                                  \
  Local<Object> name = Object::New();                                 \
  (target)->Set(String::NewSymbol(#name),                             \
                name,                                                 \
                static_cast<PropertyAttribute>(ReadOnly|DontDelete))
  
}

#endif//__COMMON_HH__
