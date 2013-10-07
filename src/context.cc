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

  /* bindings */
  
  void
  Context::Init(Handle<Object> target){
    mainloop_api.userdata = uv_default_loop();
    
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    
    tpl->SetClassName(String::NewSymbol("PulseAudioContext"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    
    SetPrototypeMethod(tpl, "connect", Connect);
    SetPrototypeMethod(tpl, "disconnect", Disconnect);
    
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
}
