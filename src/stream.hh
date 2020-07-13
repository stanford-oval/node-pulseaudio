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

#ifndef __STREAM_HH__
#define __STREAM_HH__

#include "common.hh"
#include "context.hh"

namespace pulse {
  class Stream: public Nan::ObjectWrap {
  private:
    v8::Isolate *isolate;
    Context& ctx;
    pa_sample_spec pa_ss;
    pa_stream *pa_stm;
    
    Stream(v8::Isolate *isolate, Context& context, Nan::Utf8String *stream_name, const pa_sample_spec *sample_spec, pa_usec_t initial_latency, pa_proplist* props);
    ~Stream();

    static void BufferAttrCallback(pa_stream *s, void *ud);
    static void LatencyCallback(pa_stream *s, void *ud);
    
    /* state */
    Nan::Global<v8::Function> state_callback;
    pa_stream_state_t pa_state;
    static void StateCallback(pa_stream *s, void *ud);
    void state_listener(v8::Local<v8::Value> callback);
    
    /* connection */
    int connect(Nan::Utf8String *device_name, pa_stream_direction_t direction, pa_stream_flags_t flags);
    void disconnect();

    /* read */
    Nan::Global<v8::Function> read_callback;
    static void ReadCallback(pa_stream *s, size_t nb, void *ud);
    void data();
    void read(v8::Local<v8::Value> callback);
    
    /* write */
    pa_usec_t latency; /* latency in micro seconds */
    pa_buffer_attr buffer_attr;

    Nan::Global<v8::Function> drain_callback;
    Nan::Global<v8::Value> write_buffer;
    size_t write_offset;
    
    static void DrainCallback(pa_stream *s, int st, void *ud);
    void drain();
    
    static void RequestCallback(pa_stream *s, size_t len, void *ud);
    size_t request(size_t len);

    static void UnderflowCallback(pa_stream *s, void *ud);
    void underflow();

    void write(v8::Local<v8::Value> buffer, v8::Local<v8::Value> callback);
    
  public:
    static pa_mainloop_api mainloop_api;

    /* bindings */
    static void Init(v8::Local<v8::Object> target);

    static void New(const Nan::FunctionCallbackInfo<v8::Value>& args);

    static void Connect(const Nan::FunctionCallbackInfo<v8::Value>& args);
    static void Disconnect(const Nan::FunctionCallbackInfo<v8::Value>& args);

    static void Latency(const Nan::FunctionCallbackInfo<v8::Value>& args);

    static void Read(const Nan::FunctionCallbackInfo<v8::Value>& args);
    static void Write(const Nan::FunctionCallbackInfo<v8::Value>& args);
  };
}

#endif//__STREAM_HH__
