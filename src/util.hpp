#ifndef UTIL_HPP
#define UTIL_HPP

#include <stdlib.h>

#include <string>

#if DEBUG
#define assert_stmt(__expr, __msg)                                    \
  {                                                                   \
    if (!(__expr)) {                                                  \
      printf("Assertion failed: %s\n", (std::string(__msg)).c_str()); \
      exit(1);                                                        \
    }                                                                 \
  }
#else
#define assert_stmt(__expr, __msg)
#endif

std::string read_whole_file_into_memory(char const* fp);

#endif
