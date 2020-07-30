//
// This file is part of node-pulseaudio
//
// Copyright © 2013  Kayo Phoenix <kayo@illumium.org>
//             2017-2019 The Board of Trustees of the Leland Stanford Junior University
//
// This library is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this library. If not, see <http://www.gnu.org/licenses/>.

#ifndef __COMMON_HH__
#define __COMMON_HH__

extern "C" {
#include <pulse/pulseaudio.h>
}

#include <v8.h>
#include <nan.h>
#include <node.h>
#include <uv.h>
#include <memory>

#include "debug.hh"

//#include <string>
//#include <sstream>

namespace pulse {
#define THROW_ERROR(_type_, _msg_) Nan::ThrowError(Nan::_type_(_msg_))
#define RET_ERROR(_type_, _msg_) { THROW_ERROR(_type_, _msg_); return; }

#ifdef DIE_ON_EXCEPTION
#define HANDLE_CAUGHT(_isolate_, _try_)         \
  if(_try_.HasCaught()){                        \
    FatalException(_isolate_, _try_);           \
  }
#else
#define HANDLE_CAUGHT(_isolate_, _try_)         \
  if(_try_.HasCaught()){                        \
    DisplayExceptionLine(_try_);                \
  }
#endif

#define PA_ASSERT(action) {                       \
    int __status = (action);                      \
    if(__status < 0){                             \
      RET_ERROR(Error, pa_strerror(__status));    \
    }                                             \
  }

#define JS_ASSERT(condition)                                   \
  if(!(condition)){                                            \
    RET_ERROR(Error, "Assertion failed: `" #condition "`!");   \
  }

#define DefineConstant(target, name, constant)                                         \
  Nan::DefineOwnProperty(target, Nan::New(#name).ToLocalChecked(), Nan::New(constant), \
                         static_cast<v8::PropertyAttribute>(v8::ReadOnly|v8::DontDelete))

#define AddEmptyObject(target, name)                                               \
  v8::Local<v8::Object> name = Nan::New<v8::Object>();                                     \
  Nan::DefineOwnProperty(target, Nan::New(#name).ToLocalChecked(), name,           \
                         static_cast<v8::PropertyAttribute>(v8::ReadOnly|v8::DontDelete))

  struct proplist_deleter {
    void operator()(pa_proplist* ptr) {
      pa_proplist_free(ptr);
    }
  };

  inline std::unique_ptr<pa_proplist, proplist_deleter>
  maybe_build_proplist(v8::Local<v8::Object> fromjs)
  {
    std::unique_ptr<pa_proplist, proplist_deleter> props;

    if (fromjs.IsEmpty())
        return nullptr;

    props.reset(pa_proplist_new());
    v8::Local<v8::Array> prop_names;
    if (!Nan::GetOwnPropertyNames(fromjs).ToLocal(&prop_names))
      return nullptr;

    for (uint32_t i = 0; i < prop_names->Length(); i++) {
      v8::Local<v8::Value> name, value;

      if (!Nan::Get(prop_names, i).ToLocal(&name))
        return nullptr;
      if (!name->IsString()) {
        THROW_ERROR(TypeError, "Property name must be a string.");
        return nullptr;
      }

      if (!Nan::Get(fromjs, name).ToLocal(&value))
        return nullptr;
      if (!value->IsString()) {
        THROW_ERROR(TypeError, "Property value must be a string.");
        return nullptr;
      }

      Nan::Utf8String c_name(name);
      Nan::Utf8String c_value(value);

      pa_proplist_sets(props.get(), *c_name, *c_value);
    }

    return props;
  }
}

#endif//__COMMON_HH__
