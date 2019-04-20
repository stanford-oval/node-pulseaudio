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

#include "stream.hh"

#ifdef DEBUG_STREAM
#  undef LOG
#  define LOG(...)
#endif

namespace pulse {
#define LOG_BA(ba) LOG("buffer_attr{fragsize=%d,maxlength=%d,minreq=%d,prebuf=%d,tlength=%d}", ba.fragsize, ba.maxlength, ba.minreq, ba.prebuf, ba.tlength);
  
  Stream::Stream(Isolate *_isolate,
                 Context& context,
                 String::Utf8Value *stream_name,
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
  
  Stream::~Stream(){
    if(pa_stm){
      disconnect();
      pa_stream_unref(pa_stm);
    }
    ctx.Unref();
  }
  
  void Stream::StateCallback(pa_stream *s, void *ud){
    Stream *stm = static_cast<Stream*>(ud);
    HandleScope scope(stm->isolate);
    
    stm->pa_state = pa_stream_get_state(stm->pa_stm);
    
    if(!stm->state_callback.IsEmpty()){
      Local<Value> args[] = {
        Number::New(stm->isolate, stm->pa_state),
        Undefined(stm->isolate)
      };

      if(stm->pa_state == PA_STREAM_FAILED){
        args[1] = EXCEPTION(Error, stm->isolate, pa_strerror(pa_context_errno(stm->ctx.pa_ctx)));
      }

      MakeCallback(stm->isolate, stm->handle(stm->isolate), stm->state_callback.Get(stm->isolate), 2, args);
    }
  }

  void Stream::state_listener(Local<Value> callback){
    if(callback->IsFunction()){
      state_callback = Global<Function>(isolate, Local<Function>::Cast(callback));
    }else{
      state_callback.Reset();
    }
  }
  
  void Stream::BufferAttrCallback(pa_stream *s, void *ud){
    Stream *stm = static_cast<Stream*>(ud);
    HandleScope scope(stm->isolate);
    
    LOG_BA(stm->buffer_attr);
  }

  void Stream::LatencyCallback(pa_stream *s, void *ud){
    Stream *stm = static_cast<Stream*>(ud);
    HandleScope scope(stm->isolate);
    
    pa_usec_t usec;
    int neg;
    
    pa_stream_get_latency(stm->pa_stm, &usec, &neg);
    
    LOG("latency %s%8d us", neg ? "-" : "", (int)usec);
    
    stm->latency = usec;
  }
  
  int Stream::connect(String::Utf8Value *device_name, pa_stream_direction_t direction, pa_stream_flags_t flags){
    switch(direction){
    case PA_STREAM_PLAYBACK: {
      if(latency){
        buffer_attr.tlength = pa_usec_to_bytes(latency, &pa_ss);
      }
      
      LOG_BA(buffer_attr);
      
      pa_stream_set_write_callback(pa_stm, RequestCallback, this);
      pa_stream_set_underflow_callback(pa_stm, UnderflowCallback, this);
      
      return pa_stream_connect_playback(pa_stm, device_name ? **device_name : NULL, &buffer_attr, flags, NULL, NULL);
    }
    case PA_STREAM_RECORD: {
      if(latency){
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
  
  void Stream::disconnect(){
    pa_stream_disconnect(pa_stm);
  }

  void Stream::ReadCallback(pa_stream *s, size_t nb, void *ud){
    if(nb > 0){
      Stream *stm = static_cast<Stream*>(ud);
      HandleScope scope(stm->isolate);
      
      stm->data();
    }
  }
  
  void Stream::data(){
    if(read_callback.IsEmpty()){
      return;
    }
    
    const void *data = NULL;
    size_t size;
    
    pa_stream_peek(pa_stm, &data, &size);
    LOG("Stream::read callback %d", (int)size);
    if (data == NULL) {
        Local<Value> args[] = { Null(isolate) };
        MakeCallback(isolate, handle(isolate), read_callback.Get(isolate), 1, args);
    } else {
        Local<Object> buffer = Buffer::Copy(isolate, (const char*)data, size).ToLocalChecked();
        Local<Value> args[] = { buffer };
        MakeCallback(isolate, handle(isolate), read_callback.Get(isolate), 1, args);
    }
    pa_stream_drop(pa_stm);
  }

  void Stream::read(Local<Value> callback){
    if(callback->IsFunction()){
      pa_stream_drop(pa_stm);
      read_callback = Global<Function>(isolate, Local<Function>::Cast(callback));
      //pa_stream_drop(pa_stm);
      //pa_stream_flush(pa_stm, NULL, NULL);
      pa_stream_cork(pa_stm, 0, NULL, NULL);
    }else{
      pa_stream_cork(pa_stm, 1, NULL, NULL);
      pa_stream_drop(pa_stm);
      //pa_stream_flush(pa_stm, NULL, NULL);
      read_callback.Reset();
    }
  }

  /* write */
  
  void Stream::DrainCallback(pa_stream *s, int st, void *ud){
    Stream *stm = static_cast<Stream*>(ud);
    HandleScope scope(stm->isolate);

    stm->drain();
  }

  void Stream::drain(){
    LOG("Stream::drain");

    if(!write_buffer.IsEmpty()){
      //LOG("Stream::drain buffer del");
      write_buffer.Reset();
    }
    
    if(!drain_callback.IsEmpty()){
      Local<Function> callback = drain_callback.Get(isolate);
      
      //LOG("Stream::drain callback del");
      drain_callback.Reset();
      
      MakeCallback(isolate, handle(isolate), callback, 0, nullptr);
    }
  }

  static void DummyFree(void *p){}

  void Stream::RequestCallback(pa_stream *s, size_t length, void *ud){
    Stream *stm = static_cast<Stream*>(ud);
    HandleScope scope(stm->isolate);

    if(stm->request(length) < length){
      stm->drain();
    }
  }

  size_t Stream::request(size_t length){
    if(write_buffer.IsEmpty()){
      return length;
    }

    Local<Value> local_write_buffer = write_buffer.Get(isolate);
    size_t end_length = Buffer::Length(local_write_buffer) - write_offset;
    size_t write_length = length;

    if(!end_length){
      return 0;
    }

    if(write_length > end_length){
      write_length = end_length;
    }

    LOG("write req=%d offset=%d chunk=%d", length, write_offset, write_length);
    
    pa_stream_write(pa_stm, ((const char*)Buffer::Data(local_write_buffer)) + write_offset, write_length, DummyFree, 0, PA_SEEK_RELATIVE);
    
    write_offset += write_length;

    return write_length;
  }

  void Stream::UnderflowCallback(pa_stream *s, void *ud){
    Stream *stm = static_cast<Stream*>(ud);
    HandleScope scope(stm->isolate);
    
    stm->underflow();
  }

  void Stream::underflow(){
    LOG("underflow");

  }

  void Stream::write(Local<Value> buffer, Local<Value> callback){
    if(!write_buffer.IsEmpty()){
      //LOG("Stream::write flush");
      pa_stream_flush(pa_stm, DrainCallback, this);
    }

    if(callback->IsFunction()){
      //LOG("Stream::write callback add");
      drain_callback = Global<Function>(isolate, Local<Function>::Cast(callback));
    }

    if(Buffer::HasInstance(buffer)){
      //LOG("Stream::write buffer add");
      write_buffer = Global<Value>(isolate, buffer);
      write_offset = 0;
      LOG("Stream::write");

      if(pa_stream_is_corked(pa_stm)){
        pa_stream_cork(pa_stm, 0, NULL, NULL);
      }
      
      size_t length = pa_stream_writable_size(pa_stm);
      if(length > 0){
        if(request(length) < length){
          drain();
        }
      }
    }else{
      pa_stream_cork(pa_stm, 1, NULL, NULL);
    }
  }

  /* bindings */

  void
  Stream::Init(Local<Object> target) {
    Isolate *isolate = target->GetIsolate();
    Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
    
    tpl->SetClassName(String::NewFromOneByte(isolate, (const uint8_t*)"PulseAudioStream", NewStringType::kInternalized).ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    
    NODE_SET_PROTOTYPE_METHOD(tpl, "connect", Connect);
    NODE_SET_PROTOTYPE_METHOD(tpl, "disconnect", Disconnect);
    
    NODE_SET_PROTOTYPE_METHOD(tpl, "latency", Latency);
    NODE_SET_PROTOTYPE_METHOD(tpl, "read", Read);
    NODE_SET_PROTOTYPE_METHOD(tpl, "write", Write);

    Local<Function> cfn = tpl->GetFunction();
    
    target->Set(String::NewFromOneByte(isolate, (const uint8_t*) "Stream", NewStringType::kInternalized).ToLocalChecked(), cfn);

    AddEmptyObject(isolate, cfn, type);
    DefineConstant(isolate, type, playback, PA_STREAM_PLAYBACK);
    DefineConstant(isolate, type, record, PA_STREAM_RECORD);
    DefineConstant(isolate, type, upload, PA_STREAM_UPLOAD);

    AddEmptyObject(isolate, cfn, format);
    DefineConstant(isolate, format, U8, PA_SAMPLE_U8);
    DefineConstant(isolate, format, S16LE, PA_SAMPLE_S16LE);
    DefineConstant(isolate, format, S16BE, PA_SAMPLE_S16BE);
    DefineConstant(isolate, format, F32LE, PA_SAMPLE_FLOAT32LE);
    DefineConstant(isolate, format, F32BE, PA_SAMPLE_FLOAT32BE);
    DefineConstant(isolate, format, ALAW, PA_SAMPLE_ALAW);
    DefineConstant(isolate, format, ULAW, PA_SAMPLE_ULAW);
    DefineConstant(isolate, format, S32LE, PA_SAMPLE_S32LE);
    DefineConstant(isolate, format, S32BE, PA_SAMPLE_S32BE);
    DefineConstant(isolate, format, S24LE, PA_SAMPLE_S24LE);
    DefineConstant(isolate, format, S24BE, PA_SAMPLE_S24BE);
    DefineConstant(isolate, format, S24_32LE, PA_SAMPLE_S24_32LE);
    DefineConstant(isolate, format, S24_32BE, PA_SAMPLE_S24_32BE);

    AddEmptyObject(isolate, cfn, flags);
    DefineConstant(isolate, flags, noflags, PA_STREAM_NOFLAGS);
    DefineConstant(isolate, flags, start_corked, PA_STREAM_START_CORKED);
    DefineConstant(isolate, flags, interpolate_timing, PA_STREAM_INTERPOLATE_TIMING);
    DefineConstant(isolate, flags, not_monotonic, PA_STREAM_NOT_MONOTONIC);
    DefineConstant(isolate, flags, auto_timing_update, PA_STREAM_AUTO_TIMING_UPDATE);
    DefineConstant(isolate, flags, no_remap_channels, PA_STREAM_NO_REMAP_CHANNELS);
    DefineConstant(isolate, flags, no_remix_channels, PA_STREAM_NO_REMIX_CHANNELS);
    DefineConstant(isolate, flags, fix_format, PA_STREAM_FIX_FORMAT);
    DefineConstant(isolate, flags, fix_rate, PA_STREAM_FIX_RATE);
    DefineConstant(isolate, flags, fix_channels, PA_STREAM_FIX_CHANNELS);
    DefineConstant(isolate, flags, dont_move, PA_STREAM_DONT_MOVE);
    DefineConstant(isolate, flags, variable_rate, PA_STREAM_VARIABLE_RATE);
    DefineConstant(isolate, flags, peak_detect, PA_STREAM_PEAK_DETECT);
    DefineConstant(isolate, flags, start_muted, PA_STREAM_START_MUTED);
    DefineConstant(isolate, flags, adjust_latency, PA_STREAM_ADJUST_LATENCY);
    DefineConstant(isolate, flags, early_requests, PA_STREAM_EARLY_REQUESTS);
    DefineConstant(isolate, flags, dont_inhibit_auto_suspend, PA_STREAM_DONT_INHIBIT_AUTO_SUSPEND);
    DefineConstant(isolate, flags, start_unmuted, PA_STREAM_START_UNMUTED);
    DefineConstant(isolate, flags, fail_on_suspend, PA_STREAM_FAIL_ON_SUSPEND);
    DefineConstant(isolate, flags, relative_volume, PA_STREAM_RELATIVE_VOLUME);
    DefineConstant(isolate, flags, passthrough, PA_STREAM_PASSTHROUGH);

    AddEmptyObject(isolate, cfn, state);
    DefineConstant(isolate, state, unconnected, PA_STREAM_UNCONNECTED);
    DefineConstant(isolate, state, creating, PA_STREAM_CREATING);
    DefineConstant(isolate, state, ready, PA_STREAM_READY);
    DefineConstant(isolate, state, failed, PA_STREAM_FAILED);
    DefineConstant(isolate, state, terminated, PA_STREAM_TERMINATED);
  }

  void
  Stream::New(const FunctionCallbackInfo<Value>& args){
    Isolate *isolate = args.GetIsolate();

    JS_ASSERT(isolate, args.IsConstructCall());

    JS_ASSERT(isolate, args.Length() == 8);
    JS_ASSERT(isolate, args[0]->IsObject());
    JS_ASSERT(isolate, args[6]->IsObject());

    Context *ctx = ObjectWrap::Unwrap<Context>(args[0]->ToObject());

    JS_ASSERT(isolate, ctx);

    pa_sample_spec ss;

    ss.format = PA_SAMPLE_S16LE;
    ss.rate = 44100;
    ss.channels = 2;

    if(args[1]->IsUint32()){
      ss.format = pa_sample_format_t(args[1]->Uint32Value());
    }
    if(args[2]->IsUint32()){
      ss.rate = args[2]->Uint32Value();
    }
    if(args[3]->IsUint32()){
      ss.channels = uint8_t(args[3]->Uint32Value());
    }

    pa_usec_t latency = 0;
    if(args[4]->IsUint32()){
      latency = pa_usec_t(args[4]->Uint32Value());
    }

    String::Utf8Value *stream_name = NULL;
    if(args[5]->IsString()){
      stream_name = new String::Utf8Value(args[5]->ToString());
    }

    pa_proplist* props;
    props = maybe_build_proplist(isolate, args[6].As<Object>());

    /* initialize instance */
    Stream *stm = new Stream(isolate, *ctx, stream_name, &ss, latency, props);

    if(props) {
      pa_proplist_free(props);
    }
    
    if(stream_name){
      delete stream_name;
    }

    if(!stm->pa_stm){
      delete stm;
      THROW_SCOPE(Error, isolate, "Unable to create stream.");
    }
    stm->Wrap(args.This());

    if(args[7]->IsFunction()){
      stm->state_listener(args[7]);
    }

    args.GetReturnValue().Set(args.This());
  }

  void
  Stream::Connect(const FunctionCallbackInfo<Value>& args){
    Isolate *isolate = args.GetIsolate();

    JS_ASSERT(isolate, args.Length() == 3);

    Stream *stm = ObjectWrap::Unwrap<Stream>(args.This());

    JS_ASSERT(isolate, stm);

    String::Utf8Value *device_name = NULL;
    if(args[0]->IsString()){
      device_name = new String::Utf8Value(args[0]->ToString());
    }

    pa_stream_direction_t sd = PA_STREAM_PLAYBACK;
    if(args[1]->IsUint32()){
      sd = pa_stream_direction_t(args[1]->Uint32Value());
    }

    pa_stream_flags_t sf = PA_STREAM_NOFLAGS;
    if(args[2]->IsUint32()){
      sf = pa_stream_flags_t(args[2]->Uint32Value());
    }

    int status = stm->connect(device_name, sd, sf);
    if(device_name){
      delete device_name;
    }
    PA_ASSERT(isolate, status);

    args.GetReturnValue().SetUndefined();
  }

  void
  Stream::Disconnect(const FunctionCallbackInfo<Value>& args){
    Isolate *isolate = args.GetIsolate();
    
    Stream *stm = ObjectWrap::Unwrap<Stream>(args.This());
    
    JS_ASSERT(isolate, stm);

    stm->disconnect();

    args.GetReturnValue().SetUndefined();
  }

  void
  Stream::Latency(const FunctionCallbackInfo<Value>& args){
    Isolate *isolate = args.GetIsolate();

    Stream *stm = ObjectWrap::Unwrap<Stream>(args.This());

    JS_ASSERT(isolate, stm);

    pa_usec_t latency;
    int negative;

    PA_ASSERT(isolate, pa_stream_get_latency(stm->pa_stm, &latency, &negative));

    args.GetReturnValue().Set(Number::New(isolate, latency));
  }

  void
  Stream::Read(const FunctionCallbackInfo<Value>& args){
    Isolate *isolate = args.GetIsolate();

    Stream *stm = ObjectWrap::Unwrap<Stream>(args.This());

    JS_ASSERT(isolate, stm);
    JS_ASSERT(isolate, args.Length() == 1);

    stm->read(args[0]);

    args.GetReturnValue().SetUndefined();
  }

  void
  Stream::Write(const FunctionCallbackInfo<Value>& args){
    Isolate *isolate = args.GetIsolate();

    Stream *stm = ObjectWrap::Unwrap<Stream>(args.This());

    JS_ASSERT(isolate, stm);
    JS_ASSERT(isolate, args.Length() == 2);

    stm->write(args[0], args[1]);

    args.GetReturnValue().SetUndefined();
  }
}
