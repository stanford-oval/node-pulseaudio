#include "context.hh"

#ifndef DEBUG_UV_MAINLOOP
#  undef LOG
#  define LOG(...)
#endif

/* io */

struct pa_io_event {
  uv_poll_t p;
  pa_mainloop_api *a;
  int fd;
  pa_io_event_flags_t ev;
  pa_io_event_cb_t cb;
  void *ud;
  pa_io_event_destroy_cb_t dc;
};

static void
io_cb(uv_poll_t* p, int st, int ev){
  pa_io_event *e = (pa_io_event*)p->data;

  if(e->cb){
    pa_io_event_flags_t pa_ev = PA_IO_EVENT_NULL;
    
    if(st == 0){
      if(e->ev & PA_IO_EVENT_INPUT && ev & UV_READABLE){
        pa_ev = pa_io_event_flags_t(pa_ev | PA_IO_EVENT_INPUT);
      }
      if(e->ev & PA_IO_EVENT_OUTPUT && ev & UV_WRITABLE){
        pa_ev = pa_io_event_flags_t(pa_ev | PA_IO_EVENT_OUTPUT);
      }
    }else{
      if(e->ev & PA_IO_EVENT_HANGUP && (st == UV_EOF ||
                                        st == UV_EPIPE ||
                                        st == UV_ESPIPE ||
                                        st == UV_SHUTDOWN ||
                                        st == UV_ETIMEDOUT)){
        pa_ev = pa_io_event_flags_t(pa_ev | PA_IO_EVENT_HANGUP);
      }
      if(e->ev & PA_IO_EVENT_ERROR){
        pa_ev = pa_io_event_flags_t(pa_ev | PA_IO_EVENT_ERROR);
      }
    }
    
    if(pa_ev != PA_IO_EVENT_NULL){
      e->cb(e->a, e, e->fd, pa_ev, e->ud);
    }
  }
}

static inline int io_events(pa_io_event_flags_t ev){
  return (ev & PA_IO_EVENT_INPUT ? UV_READABLE : 0) | (ev & PA_IO_EVENT_OUTPUT ? UV_WRITABLE : 0);
}

static pa_io_event *
io_new(pa_mainloop_api *a,
       int fd,
       pa_io_event_flags_t ev,
       pa_io_event_cb_t cb,
       void *ud){
  
  uv_loop_t *m;
  pa_io_event *e;
  
  assert(a);
  assert(a->userdata);
  assert(fd >= 0);
  assert(cb);
  
  m = (uv_loop_t*)a->userdata;
  
  e = pa_xnew0(pa_io_event, 1);

  LOG("io_new(fd=%u,ev=%s%s)->0x%x", fd, ev & PA_IO_EVENT_INPUT ? "in" : 0, ev & PA_IO_EVENT_OUTPUT ? "out" : "", e);
  
  e->a = a;
  e->fd = fd;
  e->ev = ev;
  e->cb = cb;
  e->ud = ud;
  
  uv_poll_init_socket(m, &e->p, fd);

  e->p.data = e;
  
  uv_poll_start(&e->p, io_events(ev), io_cb);
  
  return e;
}

static void
io_enable(pa_io_event *e,
          pa_io_event_flags_t ev){
  assert(e);
  
  LOG("io_enable(0x%x,ev=%s%s)", e, ev & PA_IO_EVENT_INPUT ? "in" : 0, ev & PA_IO_EVENT_OUTPUT ? "out" : "");
  
  if(e->ev != ev){
    uv_poll_stop(&e->p);
    uv_poll_start(&e->p, io_events(ev), io_cb);
    
    e->ev = ev;
  }
}

static void
io_free(pa_io_event *e){
  assert(e);

  LOG("io_free(0x%x)", e);
  
  uv_poll_stop(&e->p);
  
  if(e->dc){
    e->dc(e->a, e, e->ud);
  }
}

static void
io_set_destroy(pa_io_event *e,
               pa_io_event_destroy_cb_t cb){
  assert(e);
  
  e->dc = cb;
}

/* time */

struct pa_time_event {
  uv_timer_t t;
  pa_mainloop_api *a;
  timeval tv;
  pa_time_event_cb_t cb;
  void *ud;
  pa_time_event_destroy_cb_t dc;
};

static void
timer_cb(uv_timer_t* t,
         int st){
  pa_time_event *e = (pa_time_event*)t->data;
  
  if(e->cb){
    if(st == 0){
      e->cb(e->a, e, &e->tv, e->ud);
    }
  }
}

static inline uint64_t
timeval_to_millisec(const struct timeval *tv){
  struct timeval ct;
  gettimeofday(&ct, NULL);
  
  struct timeval dt;
  timersub(tv, &ct, &dt);
  
  return dt.tv_sec * 1000 + dt.tv_usec / 1000000;
}

static pa_time_event *
time_new(pa_mainloop_api *a,
         const struct timeval *tv,
         pa_time_event_cb_t cb,
         void *ud){
  
  uv_loop_t *m;
  pa_time_event *e;
  
  assert(a);
  assert(a->userdata);
  assert(cb);
  
  m = (uv_loop_t*)a->userdata;
  
  e = pa_xnew0(pa_time_event, 1);
  
  LOG("time_new(tv=%d:%d)->0x%x dt=%d", tv->tv_sec, tv->tv_usec, e, timeval_to_millisec(tv));
  
  e->a = a;
  e->cb = cb;
  e->ud = ud;
  e->tv = *tv;
  
  uv_timer_init(m, &e->t);
  
  e->t.data = e;
  
  uv_timer_start(&e->t, timer_cb, timeval_to_millisec(tv), 0);
  
  return e;
}

static void
time_restart(pa_time_event *e,
             const struct timeval *tv){
  assert(e);
  
  LOG("time_restart(0x%x,tv=%d:%d) dt=%d", e, tv->tv_sec, tv->tv_usec, timeval_to_millisec(tv));
  
  uv_timer_start(&e->t, timer_cb, timeval_to_millisec(tv), 0);
}

static void
time_free(pa_time_event *e){
  assert(e);
  
  LOG("time_free(0x%x)", e);
  
  uv_timer_stop(&e->t);
  
  if(e->dc){
    e->dc(e->a, e, e->ud);
  }
}

static void
time_set_destroy(pa_time_event *e,
                 pa_time_event_destroy_cb_t cb){
  assert(e);
  
  e->dc = cb;
}

/* defer */

struct pa_defer_event {
  uv_idle_t i;
  pa_mainloop_api *a;
  bool en;
  pa_defer_event_cb_t cb;
  void *ud;
  pa_defer_event_destroy_cb_t dc;
};

static void
defer_cb(uv_idle_t* i,
         int st){
  
  pa_defer_event *e = (pa_defer_event*)i->data;
  
  if(e->cb){
    if(st == 0){
      e->cb(e->a, e, e->ud);
    }
  }
}

static pa_defer_event *
defer_new(pa_mainloop_api *a,
          pa_defer_event_cb_t cb,
          void *ud){
  
  uv_loop_t *m;
  pa_defer_event *e;
  
  assert(a);
  assert(a->userdata);
  assert(cb);
  
  m = (uv_loop_t*)a->userdata;
  
  e = pa_xnew0(pa_defer_event, 1);
  
  LOG("defer_new()->0x%x", e);
  
  e->a = a;
  e->en = true;
  e->cb = cb;
  e->ud = ud;
  
  uv_idle_init(m, &e->i);
  e->i.data = e;
  
  uv_idle_start(&e->i, defer_cb);
  
  return e;
}

static void
defer_enable(pa_defer_event *e,
             int en){
  assert(e);
  
  LOG("defer_enable(0x%x,en=%u)", e, en);

  if(e->en && !en){
    uv_idle_stop(&e->i);
  }else if(!e->en && en){
    uv_idle_start(&e->i, defer_cb);
  }
  
  e->en = en;
}

static void
defer_free(pa_defer_event *e){
  assert(e);
  
  LOG("defer_free(0x%x)", e);
  
  if(e->en){
    uv_idle_stop(&e->i);
  }
  
  if(e->dc){
    e->dc(e->a, e, e->ud);
  }
}

static void
defer_set_destroy(pa_defer_event *e,
                  pa_defer_event_destroy_cb_t cb){
  assert(e);
  
  e->dc = cb;
}

static void
quit(pa_mainloop_api *a,
     int retval){
  // ignore
  LOG("quit()");
}

pa_mainloop_api pulse::Context::mainloop_api = {
  .userdata = NULL,
  
  .io_new = io_new,
  .io_enable = io_enable,
  .io_free = io_free,
  .io_set_destroy = io_set_destroy,
  
  .time_new = time_new,
  .time_restart = time_restart,
  .time_free = time_free,
  .time_set_destroy = time_set_destroy,
  
  .defer_new = defer_new,
  .defer_enable = defer_enable,
  .defer_free = defer_free,
  .defer_set_destroy = defer_set_destroy,
  
  .quit = quit
};
