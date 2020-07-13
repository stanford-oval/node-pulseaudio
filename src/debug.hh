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
