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

#include "context.hh"

namespace pulse {
  
  Context::Context(v8::Isolate *isolate, const Nan::Utf8String *client_name, pa_proplist *props) : isolate(isolate) {
    pa_ctx = pa_context_new_with_proplist(&mainloop_api, client_name ? **client_name : "node-pulse", props);
    pa_context_set_state_callback(pa_ctx, StateCallback, this);
  }
  
  Context::~Context() {
    if (pa_ctx) {
      disconnect();
      pa_context_unref(pa_ctx);
    }
  }

  void Context::StateCallback(pa_context *c, void *ud) {
    Context *ctx = static_cast<Context*>(ud);
    Nan::HandleScope scope;
    
    ctx->pa_state = pa_context_get_state(ctx->pa_ctx);
    
    if (!ctx->state_callback.IsEmpty()) {
      v8::Local<v8::Value> args[] = {
        Nan::New(ctx->pa_state),
        Nan::Undefined()
      };

      if (ctx->pa_state == PA_CONTEXT_FAILED)
        args[1] = Nan::Error(pa_strerror(pa_context_errno(ctx->pa_ctx)));

      Nan::MakeCallback(ctx->handle(), ctx->state_callback.Get(ctx->isolate), 2, args);
    }
  }

  void Context::state_listener(v8::Local<v8::Value> callback) {
    if (callback->IsFunction()) {
      state_callback = Nan::Global<v8::Function>(callback.As<v8::Function>());
    } else {
      state_callback.Reset();
    }
  }

  int Context::connect(const Nan::Utf8String *server_name, pa_context_flags flags) {
    return pa_context_connect(pa_ctx, server_name ? **server_name : NULL, flags, NULL);
  }

  void Context::disconnect() {
    pa_context_disconnect(pa_ctx);
  }

  void Context::EventCallback(pa_context *c, const char *name, pa_proplist *p, void *ud) {
    
  }
  
  class Pending {
  public:
    v8::Isolate *isolate;
    Nan::Global<v8::Object> self;
    Nan::Global<v8::Function> callback;
    std::vector<Nan::Global<v8::Value>> argv;

    Pending(v8::Isolate *isolate_, v8::Local<v8::Object> self_, v8::Local<v8::Function> callback_) : isolate(isolate_),
        self(self_), callback(callback_) {}
    Pending(const Pending&) = delete;
    Pending(Pending&&) = delete;
    Pending& operator=(const Pending&) = delete;
    Pending&& operator=(Pending&&) = delete;

    size_t Args() {
      return argv.size();
    }
    void Args(size_t num) {
      argv.resize(num);
    }
    void Return() {
      std::vector<v8::Local<v8::Value>> local_argv(argv.size());
      for (size_t i = 0; i < argv.size(); i++)
        local_argv[i] = argv[i].Get(isolate);
      Nan::MakeCallback(self.Get(isolate), callback.Get(isolate), local_argv.size(), &local_argv.front());
    }
  };

  template<typename pa_type_info>
  static void InfoListCallback(pa_context *c, const pa_type_info *i, int eol, void *ud) {
    Pending *p = static_cast<Pending*>(ud);
    Nan::HandleScope scope;

    if (!p->Args()) {
      p->Args(1);
      p->argv[0] = Nan::Global<v8::Value>(Nan::New<v8::Array>());
    }

    if (eol) {
      p->Return();
      delete p;
    } else {
      auto info = Nan::New<v8::Object>();

      Nan::Set(info, Nan::New("name").ToLocalChecked(), Nan::New(i->name).ToLocalChecked());
      Nan::Set(info, Nan::New("index").ToLocalChecked(), Nan::New(i->index));
      Nan::Set(info, Nan::New("description").ToLocalChecked(), Nan::New(i->description).ToLocalChecked());
      Nan::Set(info, Nan::New("format").ToLocalChecked(), Nan::New(i->sample_spec.format));
      Nan::Set(info, Nan::New("rate").ToLocalChecked(), Nan::New(i->sample_spec.rate));
      Nan::Set(info, Nan::New("channels").ToLocalChecked(), Nan::New(i->sample_spec.channels));
      Nan::Set(info, Nan::New("mute").ToLocalChecked(), Nan::New(i->mute));
      Nan::Set(info, Nan::New("latency").ToLocalChecked(), Nan::New(uint32_t(i->latency)));
      Nan::Set(info, Nan::New("driver").ToLocalChecked(), Nan::New(i->driver).ToLocalChecked());

      v8::Local<v8::Array> list = p->argv[0].Get(p->isolate).As<v8::Array>();
      Nan::Set(list, list->Length(), info);
    }
  }

  static void ServerInfoCallback(pa_context *c, const pa_server_info *i, void *ud) {
    Pending *p = static_cast<Pending*>(ud);
    Nan::HandleScope scope;

    if (!p->Args()) {
      p->Args(1);
      p->argv[0] = Nan::Global<v8::Value>(Nan::New<v8::Object>());
    }

    auto info = p->argv[0].Get(p->isolate).As<v8::Object>();

    Nan::Set(info, Nan::New("user_name").ToLocalChecked(), Nan::New(i->user_name).ToLocalChecked());
    Nan::Set(info, Nan::New("host_name").ToLocalChecked(), Nan::New(i->host_name).ToLocalChecked());
    Nan::Set(info, Nan::New("server_version").ToLocalChecked(), Nan::New(i->server_version).ToLocalChecked());
    Nan::Set(info, Nan::New("server_name").ToLocalChecked(), Nan::New(i->server_name).ToLocalChecked());
    Nan::Set(info, Nan::New("default_sink_name").ToLocalChecked(), Nan::New(i->default_sink_name).ToLocalChecked());
    Nan::Set(info, Nan::New("default_source_name").ToLocalChecked(), Nan::New(i->default_source_name).ToLocalChecked());
    Nan::Set(info, Nan::New("cookie").ToLocalChecked(), Nan::New(i->cookie));

    p->Return();
  }

  void Context::info(InfoType infotype, v8::Local<v8::Function> callback) {
    Pending *p = new Pending(callback->GetIsolate(), handle(), callback);
    switch(infotype) {
    case INFO_SERVER:
      pa_context_get_server_info(pa_ctx, ServerInfoCallback, p);
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
  Context::Init(v8::Local<v8::Object> target) {
    mainloop_api.userdata = uv_default_loop();

    auto tpl = Nan::New<v8::FunctionTemplate>(New);

    tpl->SetClassName(Nan::New("PulseAudioContext").ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    Nan::SetPrototypeMethod(tpl, "connect", Connect);
    Nan::SetPrototypeMethod(tpl, "disconnect", Disconnect);
    Nan::SetPrototypeMethod(tpl, "info", Info);

    auto cfn = Nan::GetFunction(tpl).ToLocalChecked();
    Nan::Set(target, Nan::New("Context").ToLocalChecked(), cfn);

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
    DefineConstant(info, server, INFO_SERVER);
    DefineConstant(info, source_list, INFO_SOURCE_LIST);
    DefineConstant(info, sink_list, INFO_SINK_LIST);
  }

  void
  Context::New(const Nan::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate *isolate = args.GetIsolate();

    JS_ASSERT(args.IsConstructCall());

    JS_ASSERT(args.Length() == 3);
    JS_ASSERT(args[1]->IsObject());

    std::unique_ptr<Nan::Utf8String> client_name;
    if (args[0]->IsString())
      client_name.reset(new Nan::Utf8String(args[0]));

    auto props = maybe_build_proplist(args[1].As<v8::Object>());
    if (!props)
      return;

    /* initialize instance */
    Context *ctx = new Context(isolate, client_name.get(), props.get());

    if (!ctx->pa_ctx) {
      RET_ERROR(Error, "Unable to create context.");
    }

    ctx->Wrap(args.This());

    if (args[2]->IsFunction()) {
      ctx->state_listener(args[2]);
    }

    args.GetReturnValue().Set(args.This());
  }

  void
  Context::Connect(const Nan::FunctionCallbackInfo<v8::Value>& args) {
    JS_ASSERT(args.Length() == 2);

    Context *ctx = ObjectWrap::Unwrap<Context>(args.This());
    JS_ASSERT(ctx);

    std::unique_ptr<Nan::Utf8String> server_name;
    if (args[0]->IsString())
      server_name.reset(new Nan::Utf8String(args[1]));

    pa_context_flags flags = PA_CONTEXT_NOFLAGS;
    if (args[1]->IsUint32())
      flags = pa_context_flags(Nan::To<uint32_t>(args[1]).FromJust());
    
    int status = ctx->connect(server_name.get(), flags);
    
    PA_ASSERT(status);

    args.GetReturnValue().SetUndefined();
  }

  void
  Context::Disconnect(const Nan::FunctionCallbackInfo<v8::Value>& args) {
    Context *ctx = ObjectWrap::Unwrap<Context>(args.This());
    JS_ASSERT(ctx);

    ctx->disconnect();

    args.GetReturnValue().SetUndefined();
  }

  void
  Context::Info(const Nan::FunctionCallbackInfo<v8::Value>& args) {
    JS_ASSERT(args.Length() > 1);
    JS_ASSERT(args[0]->IsUint32());
    JS_ASSERT(args[1]->IsFunction());

    Context *ctx = ObjectWrap::Unwrap<Context>(args.This());
    JS_ASSERT(ctx);

    ctx->info(InfoType(Nan::To<uint32_t>(args[0]).FromJust()), args[1].As<v8::Function>());

    args.GetReturnValue().SetUndefined();
  }
}

