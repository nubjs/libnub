#ifndef LIBNUB_HELPER_H_
#define LIBNUB_HELPER_H_

#include "uv.h"

#include <stdlib.h>  /* abort */
#include <stdio.h>   /* fprintf, fflush */


#define ASSERT(expression)                                                    \
  do {                                                                        \
     if (!(expression)) {                                                     \
       fprintf(stderr,                                                        \
               "Assertion failed in %s on line %d: %s\n",                     \
               __FILE__,                                                      \
               __LINE__,                                                      \
               #expression);                                                  \
       fflush(stderr);                                                        \
       abort();                                                               \
     }                                                                        \
  } while (0)


/* Just sugar for wrapping the main() for a task or helper. */
#define TEST_IMPL(name)                                                       \
  int run_test_##name(void);                                                  \
  int run_test_##name(void)

#define BENCHMARK_IMPL(name)                                                  \
  int run_bench_##name(void);                                                 \
  int run_bench_##name(void)

#endif  /* LIBNUB_HELPER_H_ */
