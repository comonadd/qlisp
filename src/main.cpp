#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "interpreter.hpp"
#include "objects.hpp"
#include "platform/platform.hpp"
#include "util.hpp"

struct Arguments {
  std::vector<char *> ordered_args;
  bool run_interp = false;
};

Arguments *parse_args(int argc, char **argv) {
  auto *res = new Arguments();
  int argidx = 1;
  while (argidx < argc) {
    char *arg = argv[argidx];
    if (arg[0] == '-') {
      int arg_len = strlen(arg);
      if (arg_len == 1) {
        printf(
            "Argument at position %i is invalid: Dash is followed by nothing",
            argidx);
        return nullptr;
      }
      if (arg[1] == '-') {
        // long named argument
        char *arg_payload = arg + 2;
        if (!strcmp(arg_payload, "interpreter")) {
          res->run_interp = true;
        } else {
          printf("Error: Unknown argument %s\n", arg);
          return nullptr;
        }
      } else {
        // short argument (expect one character)
        if (arg_len != 2) {
          printf(
              "Short arguments starting with a single dash can only be "
              "followed by a single character. Found %s\n",
              arg);
          return nullptr;
        }
        char arg_payload = *(arg + 1);
        switch (arg_payload) {
          case 'i': {
            res->run_interp = true;
          } break;
          default: {
            printf("Error: Unknown argument: %s\n", arg);
            return nullptr;
          } break;
        }
      }
    } else {
      // read ordered argument
      res->ordered_args.push_back(arg);
    }
    ++argidx;
  }
  return res;
}

int main(int argc, char **argv) {
  Arguments *args = parse_args(argc, argv);
  if (args == nullptr) {
    return -1;
  }
  init_interp();
  if (args->run_interp) {
    printf("Running interpreter\n");
    run_interp();
  } else {
    for (auto &file_to_read : args->ordered_args) {
      load_file(file_to_read);
    }
  }
  return 0;
}
