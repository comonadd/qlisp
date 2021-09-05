#ifndef UTIL_HPP
#define UTIL_HPP

#if DEBUG
inline void assert_stmt(bool expr, std::string msg) {
  if (!expr) {
    printf("Assertion failed: %s\n", msg.c_str());
  }
}
#else
inline void assert_stmt(bool expr, std::string msg) {}
#endif

std::string read_whole_file_into_memory(char const *fp);

#endif
