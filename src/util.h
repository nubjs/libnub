#ifndef LIBNUB_UTIL_H_
#define LIBNUB_UTIL_H_

#include <stdlib.h>  /* abort */
#include <stdio.h>   /* fprintf, fflush */

#define CHECK(expression)                                                     \
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

#if defined(NUB_DEBUG)
# define ASSERT(expression)  CHECK(expression)
#else
# define ASSERT(expression)
#endif

#define CHECK_EQ(a, b) CHECK((a) == (b))
#define CHECK_GE(a, b) CHECK((a) >= (b))
#define CHECK_GT(a, b) CHECK((a) > (b))
#define CHECK_LE(a, b) CHECK((a) <= (b))
#define CHECK_LT(a, b) CHECK((a) < (b))
#define CHECK_NE(a, b) CHECK((a) != (b))

#define UNREACHABLE() CHECK("Unreachable" && 0)

#endif  /* LIBNUB_UTIL_H_ */
