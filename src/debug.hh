#ifndef __DEBUG_HH__
#define __DEBUG_HH__

#ifdef DEBUG
#  include <cstdio>
#  define LOG(fmt, args...) fprintf(stderr, __FILE__ ":%d\t" fmt "\n", __LINE__, ##args)
#  define O2S(fmt, args...) operator const char*()const{       \
    static char __str_buf__[255];                              \
    __str_buf__[0] = '\0';                                     \
    snprintf(__str_buf__, sizeof(__str_buf__), fmt, ##args);   \
    return __str_buf__;                                        \
  }
#  define O2P(obj) ((const char *)obj)
#else// DEBUG
#  define LOG(fmt, args...)
#  define O2S(fmt, args...)
#  define O2P(obj)
#endif// DEBUG

#endif//__DEBUG_HH__
