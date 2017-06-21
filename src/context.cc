#include "context.hh"

namespace pulse {
  
  Context::Context(Isolate *isolate, String::Utf8Value *client_name) : isolate(isolate) {
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
    
    if(!ctx->state_callback.IsEmpty()) {
      Local<Value> args[] = {
        Number::New(ctx->isolate, ctx->pa_state),
        Undefined(ctx->isolate)
      };

      if(ctx->pa_state == PA_CONTEXT_FAILED){
        args[1] = EXCEPTION(Error, ctx->isolate, pa_strerror(pa_context_errno(ctx->pa_ctx)));
      }

      MakeCallback(ctx->isolate, ctx->handle(ctx->isolate), ctx->state_callback.Get(ctx->isolate), 2, args);
    }
  }

  void Context::state_listener(Local<Value> callback){
    if(callback->IsFunction()){
      state_callback = Global<Function>(isolate, callback.As<Function>());
    }else{
      state_callback.Reset();
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
  public:
    Isolate *isolate;
    Global<Object> self;
    Global<Function> callback;
    int argc;
    Global<Value> *argv;

    Pending(Isolate *isolate_, Local<Object> self_, Local<Function> callback_) : isolate(isolate_),
        self(isolate_, self_), callback(isolate_, callback_) {
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
      argv = new Global<Value>[num];
    }
    bool Args() const{
      return argc;
    }
    void Return(int argc, Local<Value> argv[]){
      MakeCallback(isolate, self.Get(isolate), callback.Get(isolate), argc, argv);
      delete this;
    }
    void Return(){
      unique_ptr<Local<Value>[]> local_argv(new Local<Value>[argc]);
      for (int i = 0; i < argc; i++)
        local_argv[i] = argv[i].Get(isolate);
      Return(argc, local_argv.get());
    }
    void Throw(Local<Value> error){
      Return(1, &error);
    }
  };

  template<typename pa_type_info>
  static void InfoListCallback(pa_context *c, const pa_type_info *i, int eol, void *ud){
    Pending *p = static_cast<Pending*>(ud);

    if(!p->Args()){
      p->Args(1);
      p->argv[0] = Global<Value>(p->isolate, Array::New(p->isolate));
    }

    if(eol){
      p->Return();
    }else{
      Local<Object> info = Object::New(p->isolate);

#define field(_type_, _name_, ...) info->Set(String::NewFromOneByte(p->isolate, (const uint8_t*) #_name_), _type_::New(p->isolate, i->__VA_ARGS__ _name_))
#define field_str(_name_, ...) info->Set(String::NewFromOneByte(p->isolate, (const uint8_t*) #_name_), String::NewFromUtf8(p->isolate, i->__VA_ARGS__ _name_))
      field_str(name);
      field(Number, index);
      field_str(description);
      field(Number, format, sample_spec.);
      field(Number, rate, sample_spec.);
      field(Number, channels, sample_spec.);
      field(Boolean, mute);
      field(Number, latency);
      field_str(driver);
#undef field
#undef field_str

      Local<Value> local_argv0 = p->argv[0].Get(p->isolate);
      Local<Array> list = local_argv0.As<Array>();

      list->Set(list->Length(), info);
    }
  }

  void Context::info(InfoType infotype, Local<Function> callback){
    Pending *p = new Pending(callback->GetIsolate(), handle(), callback);
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
  Context::Init(Local<Object> target){
    mainloop_api.userdata = uv_default_loop();
    Isolate *isolate = target->GetIsolate();

    Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);

    tpl->SetClassName(String::NewFromOneByte(isolate, (const uint8_t*)"PulseAudioContext"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(tpl, "connect", Connect);
    NODE_SET_PROTOTYPE_METHOD(tpl, "disconnect", Disconnect);
    NODE_SET_PROTOTYPE_METHOD(tpl, "info", Info);

    Local<Function> cfn = tpl->GetFunction();

    target->Set(String::NewFromOneByte(isolate, (const uint8_t*) "Context"), cfn);

    AddEmptyObject(isolate, cfn, flags);
    DefineConstant(isolate, flags, noflags, PA_CONTEXT_NOFLAGS);
    DefineConstant(isolate, flags, noautospawn, PA_CONTEXT_NOAUTOSPAWN);
    DefineConstant(isolate, flags, nofail, PA_CONTEXT_NOFAIL);

    AddEmptyObject(isolate, cfn, state);
    DefineConstant(isolate, state, unconnected, PA_CONTEXT_UNCONNECTED);
    DefineConstant(isolate, state, connecting, PA_CONTEXT_CONNECTING);
    DefineConstant(isolate, state, authorizing, PA_CONTEXT_AUTHORIZING);
    DefineConstant(isolate, state, setting_name, PA_CONTEXT_SETTING_NAME);
    DefineConstant(isolate, state, ready, PA_CONTEXT_READY);
    DefineConstant(isolate, state, failed, PA_CONTEXT_FAILED);
    DefineConstant(isolate, state, terminated, PA_CONTEXT_TERMINATED);

    AddEmptyObject(isolate, cfn, info);
    DefineConstant(isolate, info, source_list, INFO_SOURCE_LIST);
    DefineConstant(isolate, info, sink_list, INFO_SINK_LIST);
  }

  void
  Context::New(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();

    JS_ASSERT(isolate, args.IsConstructCall());

    JS_ASSERT(isolate, args.Length() == 2);

    String::Utf8Value *client_name = NULL;

    if(args[0]->IsString()){
      client_name = new String::Utf8Value(args[0]->ToString());
    }

    /* initialize instance */
    Context *ctx = new Context(isolate, client_name);

    if(client_name){
      delete client_name;
    }

    if(!ctx->pa_ctx){
      THROW_SCOPE(Error, isolate, "Unable to create context.");
    }

    ctx->Wrap(args.This());

    if(args[1]->IsFunction()){
      ctx->state_listener(args[1]);
    }

    args.GetReturnValue().Set(args.This());
  }

  void
  Context::Connect(const FunctionCallbackInfo<Value>& args){
    Isolate *isolate = args.GetIsolate();

    JS_ASSERT(isolate, args.Length() == 2);

    Context *ctx = ObjectWrap::Unwrap<Context>(args.This());

    JS_ASSERT(isolate, ctx);

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

    PA_ASSERT(isolate, status);

    args.GetReturnValue().SetUndefined();
  }

  void
  Context::Disconnect(const FunctionCallbackInfo<Value>& args){
    Isolate *isolate = args.GetIsolate();

    Context *ctx = ObjectWrap::Unwrap<Context>(args.This());

    JS_ASSERT(isolate, ctx);

    ctx->disconnect();

    args.GetReturnValue().SetUndefined();
  }

  void
  Context::Info(const FunctionCallbackInfo<Value>& args){
    Isolate *isolate = args.GetIsolate();

    JS_ASSERT(isolate, args.Length() > 1);
    JS_ASSERT(isolate, args[0]->IsUint32());
    JS_ASSERT(isolate, args[1]->IsFunction());

    Context *ctx = ObjectWrap::Unwrap<Context>(args.This());

    JS_ASSERT(isolate, ctx);

    ctx->info(InfoType(args[0]->Uint32Value()), args[1].As<Function>());

    args.GetReturnValue().SetUndefined();
  }
}
