#ifndef UTIL_HPP
#define UTIL_HPP

#include <stdlib.h>

#if DEBUG
inline void assert_stmt(bool expr, std::string msg) {
  if (!expr) {
    printf("Assertion failed: %s\n", msg.c_str());
    exit(1);
  }
}
#else
inline void assert_stmt(bool expr, std::string msg) {}
#endif

std::string read_whole_file_into_memory(char const *fp);

#endif
