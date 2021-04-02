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

#ifndef __CONTEXT_HH__
#define __CONTEXT_HH__

#include "common.hh"

namespace pulse {
  enum InfoType {
    INFO_SERVER,
    INFO_SOURCE_LIST,
    INFO_SINK_LIST,
    INFO_MODULE_LIST
  };
  
  class Context: public Nan::ObjectWrap {
    friend class Stream;
  private:
    pa_context *pa_ctx;
    v8::Isolate *isolate;

    Context(v8::Isolate *isolate, const Nan::Utf8String *client_name, pa_proplist *props);
    ~Context();
    
    /* state */
    Nan::Global<v8::Function> state_callback;
    pa_context_state_t pa_state;
    static void StateCallback(pa_context *c, void *ud);
    void state_listener(v8::Local<v8::Value> callback);
    
    /* connection */
    int connect(const Nan::Utf8String *server_name, pa_context_flags flags);
    void disconnect();

    static void EventCallback(pa_context *c, const char *name, pa_proplist *p, void *ud);
    
    /* introspection */
    void info(InfoType infotype, v8::Local<v8::Function> callback);

    /* volume control */
    void set_mute(InfoType infotype, uint32_t index, uint32_t mute, v8::Local<v8::Function> callback);
    void set_mute(InfoType infotype, const char* name, uint32_t mute, v8::Local<v8::Function> callback);
    void set_volume(InfoType infotype, uint32_t index, const pa_cvolume* volume, v8::Local<v8::Function> callback);
    void set_volume(InfoType infotype, const char* name, const pa_cvolume* volume, v8::Local<v8::Function> callback);

    /* module */
    void load_module(const char* name, const char* argument, v8::Local<v8::Function> callback);
    void unload_module(uint32_t index, v8::Local<v8::Function> callback);

  public:
    static pa_mainloop_api mainloop_api;

    static void Init(v8::Local<v8::Object> target);

    static void New(const Nan::FunctionCallbackInfo<v8::Value>& info);

    static void Connect(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void Disconnect(const Nan::FunctionCallbackInfo<v8::Value>& info);

    static void Info(const Nan::FunctionCallbackInfo<v8::Value>& info);

    static void SetVolume(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void SetMute(const Nan::FunctionCallbackInfo<v8::Value>& info);

    static void LoadModule(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void UnloadModule(const Nan::FunctionCallbackInfo<v8::Value>& info);
  };
}

#endif//__CONTEXT_HH__
