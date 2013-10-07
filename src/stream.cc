#include "stream.hh"

#ifndef DEBUG_STREAM
#  undef LOG
#  define LOG(...)
#endif

namespace pulse {
  Stream::Stream(Context& context,
                 String::Utf8Value *stream_name,
                 const pa_sample_spec *sample_spec): ctx(context) {
    ctx.Ref();
    pa_stm = pa_stream_new(ctx.pa_ctx, stream_name ? **stream_name : "node-stream", sample_spec, NULL);
    pa_stream_set_state_callback(pa_stm, StateCallback, this);
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
    
    stm->pa_state = pa_stream_get_state(stm->pa_stm);
    
    if(!stm->state_callback.IsEmpty()){
      TryCatch try_catch;
      
      Handle<Value> args[] = {
        Number::New(stm->pa_state)
      };
      
      stm->state_callback->Call(stm->handle_, 1, args);

      HANDLE_CAUGHT(try_catch);
    }
  }
  
  void Stream::state_listener(Handle<Value> callback){
    if(callback->IsFunction()){
      state_callback = Persistent<Function>::New(Handle<Function>::Cast(callback));
    }else{
      state_callback.Dispose();
    }
  }
  
  int Stream::connect(String::Utf8Value *device_name, pa_stream_direction_t direction){
    switch(direction){
    case PA_STREAM_PLAYBACK:
      return pa_stream_connect_playback(pa_stm, device_name ? **device_name : NULL, NULL, PA_STREAM_NOFLAGS, NULL, NULL);
    case PA_STREAM_RECORD:
      return pa_stream_connect_record(pa_stm, device_name ? **device_name : NULL, NULL, PA_STREAM_NOFLAGS);
    case PA_STREAM_UPLOAD:
      return pa_stream_connect_upload(pa_stm, 0);
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

      stm->data();
    }
  }
  
  void Stream::data(){
    const void *data = NULL;
    size_t size;
    
    pa_stream_peek(pa_stm, &data, &size);
    pa_stream_drop(pa_stm);
    
    Buffer *buffer = Buffer::New((const char*)data, size);
    
    TryCatch try_catch;
    
    Handle<Value> args[] = {
      buffer->handle_
    };
    
    read_callback->Call(handle_, 1, args);
    
    HANDLE_CAUGHT(try_catch);
  }
  
  void Stream::read(Handle<Value> callback){
    if(callback->IsFunction()){
      pa_stream_drop(pa_stm);
      read_callback = Persistent<Function>::New(Handle<Function>::Cast(callback));
      //pa_stream_drop(pa_stm);
      //pa_stream_flush(pa_stm, NULL, NULL);
      pa_stream_set_read_callback(pa_stm, ReadCallback, this);
      pa_stream_cork(pa_stm, 0, NULL, NULL);
    }else{
      pa_stream_cork(pa_stm, 1, NULL, NULL);
      pa_stream_drop(pa_stm);
      //pa_stream_flush(pa_stm, NULL, NULL);
      pa_stream_set_read_callback(pa_stm, NULL, NULL);
      read_callback.Dispose();
      read_callback.Clear();
    }
  }
  
  /* write */
  
  void Stream::DrainCallback(pa_stream *s, int st, void *ud){
    Stream *stm = static_cast<Stream*>(ud);
    
    stm->drain(st);
  }

  void Stream::drain(int status){
    LOG("Stream::drain callback");
    
    if(!write_buffer.IsEmpty()){
      LOG("Stream::drain buffer del");
      write_buffer.Dispose();
      write_buffer.Clear();
    }
    
    if(!drain_callback.IsEmpty()){
      TryCatch try_catch;
      
      Handle<Value> args[1];
      
      if(status < 0){
        args[0] = EXCEPTION(Error, pa_strerror(status));
      }else{
        args[0] = Null();
      }
      
      Handle<Function> callback = drain_callback;
      
      LOG("Stream::drain callback del");
      drain_callback.Dispose();
      drain_callback.Clear();
      
      LOG("Stream::drain callback call");
      callback->Call(handle_, 1, args);

      HANDLE_CAUGHT(try_catch);
    }
  }
  
  static void DummyFree(void *p){}
  
  void Stream::write(Handle<Value> buffer, Handle<Value> callback){
    LOG("Stream::write begin");
    
    if(!write_buffer.IsEmpty()){
      LOG("Stream::write flush");
      pa_stream_flush(pa_stm, DrainCallback, this);
    }
    
    if(callback->IsFunction()){
      LOG("Stream::write callback add");
      drain_callback = Persistent<Function>::New(Handle<Function>::Cast(callback));
    }
    
    if(Buffer::HasInstance(buffer)){
      LOG("Stream::write buffer add");
      write_buffer = Persistent<Value>::New(buffer);
      LOG("Stream::write");
      //pa_stream_cork(pa_stm, 0, NULL, NULL);
      pa_stream_write(pa_stm, Buffer::Data(buffer), Buffer::Length(buffer), DummyFree, 0, PA_SEEK_RELATIVE);
    }else{
      //pa_stream_cork(pa_stm, 1, NULL, NULL);
    }

    LOG("Stream::write drain");
    pa_stream_drain(pa_stm, DrainCallback, this);
  }
  
  /* bindings */

  void
  Stream::Init(Handle<Object> target){
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    
    tpl->SetClassName(String::NewSymbol("PulseAudioStream"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    
    SetPrototypeMethod(tpl, "connect", Connect);
    SetPrototypeMethod(tpl, "disconnect", Disconnect);
    
    SetPrototypeMethod(tpl, "latency", Latency);
    SetPrototypeMethod(tpl, "read", Read);
    SetPrototypeMethod(tpl, "write", Write);
    
    Local<Function> cfn = tpl->GetFunction();
    
    target->Set(String::NewSymbol("Stream"), cfn);
    
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

    AddEmptyObject(cfn, state);
    DefineConstant(state, unconnected, PA_STREAM_UNCONNECTED);
    DefineConstant(state, creating, PA_STREAM_CREATING);
    DefineConstant(state, ready, PA_STREAM_READY);
    DefineConstant(state, failed, PA_STREAM_FAILED);
    DefineConstant(state, terminated, PA_STREAM_TERMINATED);
    
  }

  Handle<Value>
  Stream::New(const Arguments& args){
    HandleScope scope;

    JS_ASSERT(args.IsConstructCall());
    
    JS_ASSERT(args.Length() == 6);
    JS_ASSERT(args[0]->IsObject());
    
    Context *ctx = ObjectWrap::Unwrap<Context>(args[0]->ToObject());
    
    JS_ASSERT(ctx);
    
    pa_sample_spec ss = {
      .format = PA_SAMPLE_S16LE,
      .rate = 44100,
      .channels = 2
    };
    if(args[1]->IsUint32()){
      ss.format = pa_sample_format_t(args[1]->Uint32Value());
    }
    if(args[2]->IsUint32()){
      ss.rate = args[2]->Uint32Value();
    }
    if(args[3]->IsUint32()){
      ss.channels = uint8_t(args[3]->Uint32Value());
    }
    
    String::Utf8Value *stream_name = NULL;
    if(args[4]->IsString()){
      stream_name = new String::Utf8Value(args[4]->ToString());
    }
    
    /* initialize instance */
    Stream *stm = new Stream(*ctx, stream_name, &ss);
    
    if(stream_name){
      delete stream_name;
    }
    
    if(!stm->pa_stm){
      delete stm;
      THROW_SCOPE(Error, "Unable to create stream.");
    }
    stm->Wrap(args.This());
    
    if(args[5]->IsFunction()){
      stm->state_listener(args[5]);
    }
    
    return scope.Close(args.This());
  }

  Handle<Value>
  Stream::Connect(const Arguments& args){
    HandleScope scope;

    JS_ASSERT(args.Length() == 2);
    
    Stream *stm = ObjectWrap::Unwrap<Stream>(args.This());
    
    JS_ASSERT(stm);
    
    String::Utf8Value *device_name = NULL;
    if(args[0]->IsString()){
      device_name = new String::Utf8Value(args[0]->ToString());
    }

    pa_stream_direction_t sd = PA_STREAM_PLAYBACK;
    if(args[1]->IsUint32()){
      sd = pa_stream_direction_t(args[1]->Uint32Value());
    }
    
    int status = stm->connect(device_name, sd);
    if(device_name){
      delete device_name;
    }
    PA_ASSERT(status);
    
    return scope.Close(Undefined());
  }

  Handle<Value>
  Stream::Disconnect(const Arguments& args){
    HandleScope scope;
    
    Stream *stm = ObjectWrap::Unwrap<Stream>(args.This());
    
    JS_ASSERT(stm);
    
    stm->disconnect();
    
    return scope.Close(Undefined());
  }
  
  Handle<Value>
  Stream::Latency(const Arguments& args){
    HandleScope scope;
    
    Stream *stm = ObjectWrap::Unwrap<Stream>(args.This());
    
    JS_ASSERT(stm);
    
    pa_usec_t latency;
    int negative;
    
    PA_ASSERT(pa_stream_get_latency(stm->pa_stm, &latency, &negative));
    
    return scope.Close(Number::New(latency));
  }
  
  Handle<Value>
  Stream::Read(const Arguments& args){
    HandleScope scope;

    Stream *stm = ObjectWrap::Unwrap<Stream>(args.This());
    
    JS_ASSERT(stm);
    JS_ASSERT(args.Length() == 1);

    stm->read(args[0]);
    
    return scope.Close(Undefined());
  }
  
  Handle<Value>
  Stream::Write(const Arguments& args){
    HandleScope scope;
    
    Stream *stm = ObjectWrap::Unwrap<Stream>(args.This());
    
    JS_ASSERT(stm);
    JS_ASSERT(args.Length() == 2);
    
    stm->write(args[0], args[1]);
    
    return scope.Close(Undefined());
  }
}
