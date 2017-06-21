#ifndef __STREAM_HH__
#define __STREAM_HH__

#include "common.hh"
#include "context.hh"

namespace pulse {
  class Stream: public ObjectWrap {
  protected:
    Isolate *isolate;
    Context& ctx;
    pa_sample_spec pa_ss;
    pa_stream *pa_stm;
    
    Stream(Isolate *isolate, Context& context, String::Utf8Value *stream_name, const pa_sample_spec *sample_spec, pa_usec_t initial_latency);
    ~Stream();

    static void BufferAttrCallback(pa_stream *s, void *ud);
    static void LatencyCallback(pa_stream *s, void *ud);
    
    /* state */
    Global<Function> state_callback;
    pa_stream_state_t pa_state;
    static void StateCallback(pa_stream *s, void *ud);
    void state_listener(Local<Value> callback);
    
    /* connection */
    int connect(String::Utf8Value *device_name, pa_stream_direction_t direction, pa_stream_flags_t flags);
    void disconnect();

    /* read */
    Global<Function> read_callback;
    static void ReadCallback(pa_stream *s, size_t nb, void *ud);
    void data();
    void read(Local<Value> callback);
    
    /* write */
    pa_usec_t latency; /* latency in micro seconds */
    pa_buffer_attr buffer_attr;

    Global<Function> drain_callback;
    Global<Value> write_buffer;
    size_t write_offset;
    
    static void DrainCallback(pa_stream *s, int st, void *ud);
    void drain();
    
    static void RequestCallback(pa_stream *s, size_t len, void *ud);
    size_t request(size_t len);

    static void UnderflowCallback(pa_stream *s, void *ud);
    void underflow();

    void write(Local<Value> buffer, Local<Value> callback);
    
  public:
    static pa_mainloop_api mainloop_api;

    /* bindings */
    static void Init(Handle<Object> target);

    static void New(const FunctionCallbackInfo<Value>& args);

    static void Connect(const FunctionCallbackInfo<Value>& args);
    static void Disconnect(const FunctionCallbackInfo<Value>& args);

    static void Latency(const FunctionCallbackInfo<Value>& args);

    static void Read(const FunctionCallbackInfo<Value>& args);
    static void Write(const FunctionCallbackInfo<Value>& args);
  };
}

#endif//__STREAM_HH__
