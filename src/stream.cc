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

#include "stream.hh"

#ifdef DEBUG_STREAM
#  undef LOG
#  define LOG(...)
#endif

namespace pulse {
#define LOG_BA(ba) LOG("buffer_attr{fragsize=%d,maxlength=%d,minreq=%d,prebuf=%d,tlength=%d}", ba.fragsize, ba.maxlength, ba.minreq, ba.prebuf, ba.tlength);
  
  Stream::Stream(v8::Isolate *_isolate,
                 Context& context,
                 Nan::Utf8String *stream_name,
                 const pa_sample_spec *sample_spec,
                 pa_usec_t initial_latency,
                 pa_proplist* props):
    isolate(_isolate), ctx(context), latency(initial_latency), write_offset(0) {
    
    ctx.Ref();
    
    pa_ss = *sample_spec;
    
    pa_stm = pa_stream_new_with_proplist(ctx.pa_ctx, stream_name ? **stream_name : "node-stream", &pa_ss, NULL, props);
    
    buffer_attr.fragsize = (uint32_t)-1;
    buffer_attr.maxlength = (uint32_t)-1;
    buffer_attr.minreq = (uint32_t)-1;
    buffer_attr.prebuf = (uint32_t)-1;
    buffer_attr.tlength = (uint32_t)-1;
    
    pa_stream_set_state_callback(pa_stm, StateCallback, this);
    
    pa_stream_set_buffer_attr_callback(pa_stm, BufferAttrCallback, this);
    pa_stream_set_latency_update_callback(pa_stm, LatencyCallback, this);
  }
  
  Stream::~Stream() {
    if (pa_stm) {
      disconnect();
      pa_stream_unref(pa_stm);
    }
    ctx.Unref();
  }
  
  void Stream::StateCallback(pa_stream *s, void *ud) {
    Stream *stm = static_cast<Stream*>(ud);
    Nan::HandleScope scope;
    
    stm->pa_state = pa_stream_get_state(stm->pa_stm);
    
    if (!stm->state_callback.IsEmpty()) {
      v8::Local<v8::Value> args[] = {
        Nan::New(stm->pa_state),
        Nan::Undefined()
      };

      if (stm->pa_state == PA_STREAM_FAILED)
        args[1] = Nan::Error(pa_strerror(pa_context_errno(stm->ctx.pa_ctx)));

      Nan::MakeCallback(stm->handle(), stm->state_callback.Get(stm->isolate), 2, args);
    }
  }

  void Stream::state_listener(v8::Local<v8::Value> callback) {
    if (callback->IsFunction()) {
      state_callback = Nan::Global<v8::Function>(callback.As<v8::Function>());
    } else {
      state_callback.Reset();
    }
  }
  
  void Stream::BufferAttrCallback(pa_stream *s, void *ud) {
    Nan::HandleScope scope;
#ifdef DEBUG
    Stream *stm = static_cast<Stream*>(ud);
    LOG_BA(stm->buffer_attr);
#endif
  }

  void Stream::LatencyCallback(pa_stream *s, void *ud) {
    Stream *stm = static_cast<Stream*>(ud);
    Nan::HandleScope scope;
    
    pa_usec_t usec;
    int neg;
    
    pa_stream_get_latency(stm->pa_stm, &usec, &neg);
    
    LOG("latency %s%8d us", neg ? "-" : "", (int)usec);
    
    stm->latency = usec;
  }
  
  int Stream::connect(Nan::Utf8String *device_name, pa_stream_direction_t direction, pa_stream_flags_t flags) {
    switch(direction) {
    case PA_STREAM_PLAYBACK: {
      if (latency) {
        buffer_attr.tlength = pa_usec_to_bytes(latency, &pa_ss);
      }
      
      LOG_BA(buffer_attr);
      
      pa_stream_set_write_callback(pa_stm, RequestCallback, this);
      pa_stream_set_underflow_callback(pa_stm, UnderflowCallback, this);
      
      return pa_stream_connect_playback(pa_stm, device_name ? **device_name : NULL, &buffer_attr, flags, NULL, NULL);
    }
    case PA_STREAM_RECORD: {
      if (latency) {
        buffer_attr.fragsize = pa_usec_to_bytes(latency, &pa_ss);
      }
      
      LOG_BA(buffer_attr);
      
      pa_stream_set_read_callback(pa_stm, ReadCallback, this);
      
      return pa_stream_connect_record(pa_stm, device_name ? **device_name : NULL, &buffer_attr, flags);
    }
    case PA_STREAM_UPLOAD: {
      return pa_stream_connect_upload(pa_stm, 0);
    }
    case PA_STREAM_NODIRECTION:
      break;
    }
    return 0;
  }
  
  void Stream::disconnect() {
    pa_stream_disconnect(pa_stm);
  }

  void Stream::ReadCallback(pa_stream *s, size_t nb, void *ud) {
    Stream *stm = static_cast<Stream*>(ud);
    Nan::HandleScope scope;

    if (nb > 0) {
      stm->data();
    }
  }
  
  void Stream::data() {
    if (read_callback.IsEmpty()) {
      return;
    }
    
    const void *data = NULL;
    size_t size;
    
    pa_stream_peek(pa_stm, &data, &size);
    LOG("Stream::read callback %d", (int)size);
    if (data == NULL) {
        v8::Local<v8::Value> args[] = { Null(isolate) };
        Nan::MakeCallback(handle(), read_callback.Get(isolate), 1, args);
    } else {
        v8::Local<v8::Object> buffer = Nan::CopyBuffer((const char*)data, size).ToLocalChecked();
        v8::Local<v8::Value> args[] = { buffer };
        Nan::MakeCallback(handle(), read_callback.Get(isolate), 1, args);
    }
    if (!(data == NULL && size == 0)) {
        pa_stream_drop(pa_stm);
    }
  }

  void Stream::read(v8::Local<v8::Value> callback) {
    if (callback->IsFunction()) {
      pa_stream_drop(pa_stm);
      read_callback = Nan::Global<v8::Function>(callback.As<v8::Function>());
      //pa_stream_drop(pa_stm);
      //pa_stream_flush(pa_stm, NULL, NULL);
      pa_stream_cork(pa_stm, 0, NULL, NULL);
    } else {
      pa_stream_cork(pa_stm, 1, NULL, NULL);
      pa_stream_drop(pa_stm);
      //pa_stream_flush(pa_stm, NULL, NULL);
      read_callback.Reset();
    }
  }

  /* write */
  
  void Stream::DrainCallback(pa_stream *s, int st, void *ud) {
    Stream *stm = static_cast<Stream*>(ud);
    Nan::HandleScope scope;

    stm->drain();
  }

  void Stream::drain() {
    LOG("Stream::drain");

    if (!write_buffer.IsEmpty()) {
      //LOG("Stream::drain buffer del");
      write_buffer.Reset();
    }
    
    if (!drain_callback.IsEmpty()) {
      v8::Local<v8::Function> callback = drain_callback.Get(isolate);
      
      //LOG("Stream::drain callback del");
      drain_callback.Reset();
      
      Nan::MakeCallback(handle(), callback, 0, nullptr);
    }
  }

  static void DummyFree(void *p) {}

  void Stream::RequestCallback(pa_stream *s, size_t length, void *ud) {
    Stream *stm = static_cast<Stream*>(ud);
    Nan::HandleScope scope;

    if (stm->request(length) < length) {
      stm->drain();
    }
  }

  size_t Stream::request(size_t length) {
    if (write_buffer.IsEmpty()) {
      return length;
    }

    v8::Local<v8::Value> local_write_buffer = write_buffer.Get(isolate);
    size_t end_length = node::Buffer::Length(local_write_buffer) - write_offset;
    size_t write_length = length;

    if (!end_length) {
      return 0;
    }

    if (write_length > end_length) {
      write_length = end_length;
    }

    LOG("write req=%d offset=%d chunk=%d", length, write_offset, write_length);
    
    pa_stream_write(pa_stm, ((const char*)node::Buffer::Data(local_write_buffer)) + write_offset, write_length, DummyFree, 0, PA_SEEK_RELATIVE);
    
    write_offset += write_length;

    return write_length;
  }

  void Stream::UnderflowCallback(pa_stream *s, void *ud) {
    Stream *stm = static_cast<Stream*>(ud);
    Nan::HandleScope scope;
    
    stm->underflow();
  }

  void Stream::underflow() {
    LOG("underflow");
  }

  void Stream::write(v8::Local<v8::Value> buffer, v8::Local<v8::Value> callback) {
    if (!write_buffer.IsEmpty()) {
      //LOG("Stream::write flush");
      pa_stream_flush(pa_stm, DrainCallback, this);
    }

    if (callback->IsFunction()) {
      //LOG("Stream::write callback add");
      drain_callback = Nan::Global<v8::Function>(callback.As<v8::Function>());
    }

    if (node::Buffer::HasInstance(buffer)) {
      //LOG("Stream::write buffer add");
      write_buffer = Nan::Global<v8::Value>(buffer);
      write_offset = 0;
      LOG("Stream::write");

      if (pa_stream_is_corked(pa_stm))
        pa_stream_cork(pa_stm, 0, NULL, NULL);
      
      size_t length = pa_stream_writable_size(pa_stm);
      if (length > 0) {
        if (request(length) < length) {
          drain();
        }
      }
    } else {
      pa_stream_flush(pa_stm, NULL, NULL);
    }
  }

  /* bindings */

  void
  Stream::Init(v8::Local<v8::Object> target) {
    v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
    
    tpl->SetClassName(Nan::New("PulseAudioStream").ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    
    Nan::SetPrototypeMethod(tpl, "connect", Connect);
    Nan::SetPrototypeMethod(tpl, "disconnect", Disconnect);
    
    Nan::SetPrototypeMethod(tpl, "latency", Latency);
    Nan::SetPrototypeMethod(tpl, "read", Read);
    Nan::SetPrototypeMethod(tpl, "write", Write);

    auto cfn = Nan::GetFunction(tpl).ToLocalChecked();
    Nan::Set(target, Nan::New("Stream").ToLocalChecked(), cfn);

    AddEmptyObject(cfn, type);
    DefineConstant(type, playback, PA_STREAM_PLAYBACK);
    DefineConstant(type, record, PA_STREAM_RECORD);
    DefineConstant(type, upload, PA_STREAM_UPLOAD);

    AddEmptyObject(cfn, format);
    DefineConstant(format, U8, PA_SAMPLE_U8);
    DefineConstant(format, S16LE, PA_SAMPLE_S16LE);
    DefineConstant(format, S16BE, PA_SAMPLE_S16BE);
    DefineConstant(format, F32LE, PA_SAMPLE_FLOAT32LE);
    DefineConstant(format, F32BE, PA_SAMPLE_FLOAT32BE);
    DefineConstant(format, ALAW, PA_SAMPLE_ALAW);
    DefineConstant(format, ULAW, PA_SAMPLE_ULAW);
    DefineConstant(format, S32LE, PA_SAMPLE_S32LE);
    DefineConstant(format, S32BE, PA_SAMPLE_S32BE);
    DefineConstant(format, S24LE, PA_SAMPLE_S24LE);
    DefineConstant(format, S24BE, PA_SAMPLE_S24BE);
    DefineConstant(format, S24_32LE, PA_SAMPLE_S24_32LE);
    DefineConstant(format, S24_32BE, PA_SAMPLE_S24_32BE);

    AddEmptyObject(cfn, flags);
    DefineConstant(flags, noflags, PA_STREAM_NOFLAGS);
    DefineConstant(flags, start_corked, PA_STREAM_START_CORKED);
    DefineConstant(flags, interpolate_timing, PA_STREAM_INTERPOLATE_TIMING);
    DefineConstant(flags, not_monotonic, PA_STREAM_NOT_MONOTONIC);
    DefineConstant(flags, auto_timing_update, PA_STREAM_AUTO_TIMING_UPDATE);
    DefineConstant(flags, no_remap_channels, PA_STREAM_NO_REMAP_CHANNELS);
    DefineConstant(flags, no_remix_channels, PA_STREAM_NO_REMIX_CHANNELS);
    DefineConstant(flags, fix_format, PA_STREAM_FIX_FORMAT);
    DefineConstant(flags, fix_rate, PA_STREAM_FIX_RATE);
    DefineConstant(flags, fix_channels, PA_STREAM_FIX_CHANNELS);
    DefineConstant(flags, dont_move, PA_STREAM_DONT_MOVE);
    DefineConstant(flags, variable_rate, PA_STREAM_VARIABLE_RATE);
    DefineConstant(flags, peak_detect, PA_STREAM_PEAK_DETECT);
    DefineConstant(flags, start_muted, PA_STREAM_START_MUTED);
    DefineConstant(flags, adjust_latency, PA_STREAM_ADJUST_LATENCY);
    DefineConstant(flags, early_requests, PA_STREAM_EARLY_REQUESTS);
    DefineConstant(flags, dont_inhibit_auto_suspend, PA_STREAM_DONT_INHIBIT_AUTO_SUSPEND);
    DefineConstant(flags, start_unmuted, PA_STREAM_START_UNMUTED);
    DefineConstant(flags, fail_on_suspend, PA_STREAM_FAIL_ON_SUSPEND);
    DefineConstant(flags, relative_volume, PA_STREAM_RELATIVE_VOLUME);
    DefineConstant(flags, passthrough, PA_STREAM_PASSTHROUGH);

    AddEmptyObject(cfn, state);
    DefineConstant(state, unconnected, PA_STREAM_UNCONNECTED);
    DefineConstant(state, creating, PA_STREAM_CREATING);
    DefineConstant(state, ready, PA_STREAM_READY);
    DefineConstant(state, failed, PA_STREAM_FAILED);
    DefineConstant(state, terminated, PA_STREAM_TERMINATED);
  }

  void
  Stream::New(const Nan::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate *isolate = args.GetIsolate();

    JS_ASSERT(args.IsConstructCall());

    JS_ASSERT(args.Length() == 8);
    JS_ASSERT(args[0]->IsObject());
    JS_ASSERT(args[6]->IsObject());

    v8::Local<v8::Object> ctx_object;
    if (!Nan::To<v8::Object>(args[0]).ToLocal(&ctx_object))
        return;
    Context *ctx = ObjectWrap::Unwrap<Context>(ctx_object);
    JS_ASSERT(ctx);

    pa_sample_spec ss;

    ss.format = PA_SAMPLE_S16LE;
    ss.rate = 44100;
    ss.channels = 2;

    if (args[1]->IsUint32()) {
      ss.format = pa_sample_format_t(Nan::To<uint32_t>(args[1]).FromJust());
    }
    if (args[2]->IsUint32()) {
      ss.rate = Nan::To<uint32_t>(args[2]).FromJust();
    }
    if (args[3]->IsUint32()) {
      ss.channels = uint8_t(Nan::To<uint32_t>(args[3]).FromJust());
    }

    pa_usec_t latency = 0;
    if (args[4]->IsUint32()) {
      latency = pa_usec_t(Nan::To<uint32_t>(args[4]).FromJust());
    }

    std::unique_ptr<Nan::Utf8String> stream_name;
    if (args[5]->IsString())
      stream_name.reset(new Nan::Utf8String(args[5]));

    auto props = maybe_build_proplist(args[6].As<v8::Object>());

    /* initialize instance */
    Stream *stm = new Stream(isolate, *ctx, stream_name.get(), &ss, latency, props.get());

    if (!stm->pa_stm) {
      delete stm;
      RET_ERROR(Error, "Unable to create stream.");
    }
    stm->Wrap(args.This());

    if (args[7]->IsFunction()) {
      stm->state_listener(args[7]);
    }

    args.GetReturnValue().Set(args.This());
  }

  void
  Stream::Connect(const Nan::FunctionCallbackInfo<v8::Value>& args) {
    JS_ASSERT(args.Length() == 3);

    Stream *stm = ObjectWrap::Unwrap<Stream>(args.This());
    JS_ASSERT(stm);

    std::unique_ptr<Nan::Utf8String> device_name;
    if (args[0]->IsString()) {
      device_name.reset(new Nan::Utf8String(args[0]));
    }

    pa_stream_direction_t sd = PA_STREAM_PLAYBACK;
    if (args[1]->IsUint32()) {
      sd = pa_stream_direction_t(Nan::To<uint32_t>(args[1]).FromJust());
    }

    pa_stream_flags_t sf = PA_STREAM_NOFLAGS;
    if (args[2]->IsUint32()) {
      sf = pa_stream_flags_t(Nan::To<uint32_t>(args[2]).FromJust());
    }

    int status = stm->connect(device_name.get(), sd, sf);
    PA_ASSERT(status);

    args.GetReturnValue().SetUndefined();
  }

  void
  Stream::Disconnect(const Nan::FunctionCallbackInfo<v8::Value>& args) {
    Stream *stm = ObjectWrap::Unwrap<Stream>(args.This());
    JS_ASSERT(stm);

    stm->disconnect();

    args.GetReturnValue().SetUndefined();
  }

  void
  Stream::Latency(const Nan::FunctionCallbackInfo<v8::Value>& args) {
    Stream *stm = ObjectWrap::Unwrap<Stream>(args.This());
    JS_ASSERT(stm);

    pa_usec_t latency;
    int negative;

    PA_ASSERT(pa_stream_get_latency(stm->pa_stm, &latency, &negative));

    args.GetReturnValue().Set(Nan::New(uint32_t(latency)));
  }

  void
  Stream::Read(const Nan::FunctionCallbackInfo<v8::Value>& args) {
    Stream *stm = ObjectWrap::Unwrap<Stream>(args.This());
    JS_ASSERT(stm);
    JS_ASSERT(args.Length() == 1);

    stm->read(args[0]);

    args.GetReturnValue().SetUndefined();
  }

  void
  Stream::Write(const Nan::FunctionCallbackInfo<v8::Value>& args) {
    Stream *stm = ObjectWrap::Unwrap<Stream>(args.This());
    JS_ASSERT(stm);
    JS_ASSERT(args.Length() == 2);

    stm->write(args[0], args[1]);

    args.GetReturnValue().SetUndefined();
  }
}
