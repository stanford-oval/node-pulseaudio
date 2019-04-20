//
// This file is part of node-pulseaudio
//
// Copyright © 2013  Kayo Phoenix <kayo@illumium.org>
//             2017-2019 The Board of Trustees of the Leland Stanford Junior University
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

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

#define EXCEPTION(_type_, _isolate_, _msg_) Exception::_type_(String::NewFromOneByte(_isolate_, (const uint8_t*)_msg_, NewStringType::kNormal).ToLocalChecked())
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
                String::NewFromOneByte(_isolate_, (const uint8_t*) #name, NewStringType::kInternalized).ToLocalChecked(),         \
                Number::New(_isolate_, constant),                                  \
                static_cast<PropertyAttribute>(ReadOnly|DontDelete)).FromJust()

#define AddEmptyObject(_isolate_, target, name)                                    \
  Local<Object> name = Object::New(_isolate_);                                     \
  (target)->DefineOwnProperty(_isolate_->GetCurrentContext(),                      \
                String::NewFromOneByte(_isolate_, (const uint8_t*) #name, NewStringType::kInternalized).ToLocalChecked(),         \
                name,                                                              \
                static_cast<PropertyAttribute>(ReadOnly|DontDelete)).FromJust()
  

  inline pa_proplist*
  maybe_build_proplist(v8::Isolate *isolate, v8::Local<v8::Object> fromjs)
  {
    pa_proplist *props;

    if (fromjs.IsEmpty())
        return nullptr;

    props = pa_proplist_new();
    auto prop_names = fromjs->GetOwnPropertyNames();
    for (uint32_t i = 0; i < prop_names->Length(); i++) {
      auto name = prop_names->Get(i);
      if (!name->IsString()) {
        THROW_ERROR(TypeError, isolate, "Property name must be a string.");
        pa_proplist_free(props);
        return nullptr;
      }

      auto value = fromjs->Get(name);
      if (value.IsEmpty()) {
        pa_proplist_free(props);
        return nullptr;
      }
      if (!value->IsString()) {
        THROW_ERROR(TypeError, isolate, "Property value must be a string.");
        pa_proplist_free(props);
        return nullptr;
      }

      String::Utf8Value c_name(isolate, name);
      String::Utf8Value c_value(isolate, value);

      pa_proplist_sets(props, *c_name, *c_value);
    }

    return props;
  }
}

#endif//__COMMON_HH__
