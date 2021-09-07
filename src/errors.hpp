#ifndef ERRORS_HPP
#define ERRORS_HPP

#include <fmt/core.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>

#include "interpreter.hpp"

size_t list_length(const Object *);

using fmt::format;

inline void error_msg(const std::string &msg) {
  printf("Error in %s at [%d:%d]: %s\n", IS.file_name, IS.line, IS.col,
         msg.c_str());
}

inline void eof_error() { error_msg("EOF"); }

inline void error_builtin_arg_mismatch_function(char const *fname,
                                                size_t expected,
                                                Object const *expr) {
  // 1 for the function name to call
  size_t got = list_length(expr) - 1;
  error_msg(format("Built-in {} expected {} arguments, got {}", fname, expected,
                   got));
}

#endif
