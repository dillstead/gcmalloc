// https://modernc.gforge.inria.fr
// Chapter 16: Function-like macros
// Turn on debug logging on command line:
//   make NDEBUG=1
#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <stdio.h>
#include <stdbool.h>

#ifdef NDEBUG
#define TRACE_ON 0
#else
#define TRACE_ON 1
#endif

#define LOG_FIRST(...) LOG_FIRST0( __VA_ARGS__ , 0)
#define LOG_FIRST0(_0, ...) _0

#define LOG_LAST(...) LOG_LAST0( __VA_ARGS__ , 0)
#define LOG_LAST0(_0 , ...) __VA_ARGS__

#define LOG_TRACE(F, ...)                       \
do {                                            \
  if (TRACE_ON)                                 \
      fprintf ( stderr , F "\n", __VA_ARGS__ ); \
} while (false)

#define LOG(...)                                 \
    LOG_TRACE(LOG_FIRST( __VA_ARGS__ ) " %.0d" , \
              LOG_LAST( __VA_ARGS__ ))

#endif
