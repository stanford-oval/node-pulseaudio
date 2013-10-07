#ifndef __STREAM_HH__
#define __STREAM_HH__

#include "common.hh"
#include "context.hh"

namespace pulse {
  class Stream: public ObjectWrap {
  protected:
    Context& ctx;
    pa_stream *pa_stm;
    
    Stream(Context& context, String::Utf8Value *stream_name, const pa_sample_spec *sample_spec);
    ~Stream();
    
    /* state */
    Persistent<Function> state_callback;
    pa_stream_state_t pa_state;
    static void StateCallback(pa_stream *s, void *ud);
    void state_listener(Handle<Value> callback);
    
    /* connection */
    int connect(String::Utf8Value *device_name, pa_stream_direction_t direction);
    void disconnect();

    /* read */
    Persistent<Function> read_callback;
    static void ReadCallback(pa_stream *s, size_t nb, void *ud);
    void data();
    void read(Handle<Value> callback);
    
    /* write */
    Persistent<Function> drain_callback;
    Persistent<Value> write_buffer;
    static void DrainCallback(pa_stream *s, int st, void *ud);
    void drain(int status);
    void write(Handle<Value> buffer, Handle<Value> callback);
    
  public:
    static pa_mainloop_api mainloop_api;

    /* bindings */
    static void Init(Handle<Object> target);
    
    static Handle<Value> New(const Arguments& args);
    
    static Handle<Value> Connect(const Arguments& args);
    static Handle<Value> Disconnect(const Arguments& args);
    
    static Handle<Value> Latency(const Arguments& args);
    
    static Handle<Value> Read(const Arguments& args);
    static Handle<Value> Write(const Arguments& args);
  };
}

#endif//__STREAM_HH__
