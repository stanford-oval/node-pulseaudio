#include "context.hh"

namespace pulse {
  
  Context::Context(String::Utf8Value *client_name){
    pa_ctx = pa_context_new(&mainloop_api, client_name ? **client_name : "node-pulse");
    pa_context_set_state_callback(pa_ctx, StateCallback, this);
  }
  
  Context::~Context(){
    if(pa_ctx){
      disconnect();
      pa_context_unref(pa_ctx);
    }
  }

  void Context::StateCallback(pa_context *c, void *ud){
    Context *ctx = static_cast<Context*>(ud);
    
    ctx->pa_state = pa_context_get_state(ctx->pa_ctx);
    
    if(!ctx->state_callback.IsEmpty()){
      TryCatch try_catch;
      
      Handle<Value> args[] = {
        Number::New(ctx->pa_state)
      };
      
      ctx->state_callback->Call(ctx->handle_, 1, args);
      
      HANDLE_CAUGHT(try_catch);
    }
  }
  
  void Context::state_listener(Handle<Value> callback){
    if(callback->IsFunction()){
      state_callback = Persistent<Function>::New(Handle<Function>::Cast(callback));
    }else{
      state_callback.Dispose();
    }
  }

  int Context::connect(String::Utf8Value *server_name, pa_context_flags flags){
    return pa_context_connect(pa_ctx, server_name ? **server_name : NULL, flags, NULL);
  }

  void Context::disconnect(){
    pa_context_disconnect(pa_ctx);
  }

  void Context::EventCallback(pa_context *c, const char *name, pa_proplist *p, void *ud){
    
  }
  
  class Pending {
  protected:
    Persistent<Object> self;
    Persistent<Function> callback;
    int argc;
  public:
    Persistent<Value> *argv;
    
    Pending(Handle<Object> self_, Handle<Function> callback_){
      self = Persistent<Object>::New(self_);
      callback = Persistent<Function>::New(callback_);
      argc = 0;
      argv = NULL;
    }
    ~Pending(){
      if(argc > 0){
        delete[] argv;
      }
    }
    void Args(int num){
      argc = num;
      argv = new Persistent<Value>[num];
    }
    bool Args() const{
      return argc;
    }
    void Return(int argc, Handle<Value> argv[]){
      TryCatch try_catch;
      
      callback->Call(self, argc, argv);
      
      HANDLE_CAUGHT(try_catch);
      
      delete this;
    }
    void Return(){
      Return(argc, argv);
    }
    void Throw(Handle<Value> error){
      Return(1, &error);
    }
  };
  
  template<typename pa_type_info>
  static void InfoListCallback(pa_context *c, const pa_type_info *i, int eol, void *ud){
    Pending *p = static_cast<Pending*>(ud);

    if(!p->Args()){
      p->Args(1);
      p->argv[0] = Persistent<Value>::New(Array::New());
    }
    
    if(eol){
      p->Return();
    }else{
      Handle<Object> info = Object::New();
      
#define field(_type_, _name_, _sub_...) info->Set(String::NewSymbol(#_name_), _type_::New(i->_sub_ _name_))
      field(String, name);
      field(Number, index);
      field(String, description);
      field(Number, format, sample_spec.);
      field(Number, rate, sample_spec.);
      field(Number, channels, sample_spec.);
      field(Boolean, mute);
      field(Number, latency);
      field(String, driver);
      
      Handle<Array> list = p->argv[0].As<Array>();
      
      list->Set(list->Length(), info);
    }
  }
  
  void Context::info(InfoType infotype, Handle<Function> callback){
    Pending *p = new Pending(handle_, callback);
    switch(infotype){
    case INFO_SOURCE_LIST:
      pa_context_get_source_info_list(pa_ctx, InfoListCallback<pa_source_info>, p);
      break;
    case INFO_SINK_LIST:
      pa_context_get_sink_info_list(pa_ctx, InfoListCallback<pa_sink_info>, p);
      break;
    }
  }
  
  /* bindings */
  
  void
  Context::Init(Handle<Object> target){
    mainloop_api.userdata = uv_default_loop();
    
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    
    tpl->SetClassName(String::NewSymbol("PulseAudioContext"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    
    SetPrototypeMethod(tpl, "connect", Connect);
    SetPrototypeMethod(tpl, "disconnect", Disconnect);
    SetPrototypeMethod(tpl, "info", Info);
    
    Local<Function> cfn = tpl->GetFunction();
    
    target->Set(String::NewSymbol("Context"), cfn);
    
    AddEmptyObject(cfn, flags);
    DefineConstant(flags, noflags, PA_CONTEXT_NOFLAGS);
    DefineConstant(flags, noautospawn, PA_CONTEXT_NOAUTOSPAWN);
    DefineConstant(flags, nofail, PA_CONTEXT_NOFAIL);
    
    AddEmptyObject(cfn, state);
    DefineConstant(state, unconnected, PA_CONTEXT_UNCONNECTED);
    DefineConstant(state, connecting, PA_CONTEXT_CONNECTING);
    DefineConstant(state, authorizing, PA_CONTEXT_AUTHORIZING);
    DefineConstant(state, setting_name, PA_CONTEXT_SETTING_NAME);
    DefineConstant(state, ready, PA_CONTEXT_READY);
    DefineConstant(state, failed, PA_CONTEXT_FAILED);
    DefineConstant(state, terminated, PA_CONTEXT_TERMINATED);

    AddEmptyObject(cfn, info);
    DefineConstant(info, source_list, INFO_SOURCE_LIST);
    DefineConstant(info, sink_list, INFO_SINK_LIST);
  }

  Handle<Value>
  Context::New(const Arguments& args){
    HandleScope scope;

    JS_ASSERT(args.IsConstructCall());
    
    JS_ASSERT(args.Length() == 2);
    
    String::Utf8Value *client_name = NULL;
    
    if(args[0]->IsString()){
      client_name = new String::Utf8Value(args[0]->ToString());
    }

    /* initialize instance */
    Context *ctx = new Context(client_name);

    if(client_name){
      delete client_name;
    }
    
    if(!ctx->pa_ctx){
      THROW_SCOPE(Error, "Unable to create context.");
    }
    
    ctx->Wrap(args.This());

    if(args[1]->IsFunction()){
      ctx->state_listener(args[1]);
    }
    
    return scope.Close(args.This());
  }

  Handle<Value>
  Context::Connect(const Arguments& args){
    HandleScope scope;

    JS_ASSERT(args.Length() == 2);
    
    Context *ctx = ObjectWrap::Unwrap<Context>(args.This());
    
    JS_ASSERT(ctx);
    
    String::Utf8Value *server_name = NULL;
    
    if(args[1]->IsString()){
      server_name = new String::Utf8Value(args[1]->ToString());
    }
    
    pa_context_flags flags = PA_CONTEXT_NOFLAGS;

    if(args[2]->IsUint32()){
      flags = pa_context_flags(args[2]->Uint32Value());
    }
    
    int status = ctx->connect(server_name, flags);
    
    if(server_name){
      delete server_name;
    }
    
    PA_ASSERT(status);
    
    return scope.Close(Undefined());
  }
  
  Handle<Value>
  Context::Disconnect(const Arguments& args){
    HandleScope scope;
    
    Context *ctx = ObjectWrap::Unwrap<Context>(args.This());
    
    JS_ASSERT(ctx);
    
    ctx->disconnect();
    
    return scope.Close(Undefined());
  }
  
  Handle<Value>
  Context::Info(const Arguments& args){
    HandleScope scope;
    
    JS_ASSERT(args.Length() > 1);
    JS_ASSERT(args[0]->IsUint32());
    JS_ASSERT(args[1]->IsFunction());
    
    Context *ctx = ObjectWrap::Unwrap<Context>(args.This());
    
    JS_ASSERT(ctx);
    
    ctx->info(InfoType(args[0]->Uint32Value()), args[1].As<Function>());
    
    return scope.Close(Undefined());
  }
}
