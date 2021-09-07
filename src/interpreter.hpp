#ifndef INTERP_HPP
#define INTERP_HPP

#include <unordered_map>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>
#include <fstream>
#include <list>

#include "types.hpp"

const auto GC_INTERVAL = std::chrono::milliseconds(5000);
const auto GC_LOG_FILE = "lisp-gc.log";

using std::filesystem::path;

struct Object;

using SymVars = std::unordered_map<std::string, Object *>;
struct SymTable {
  SymVars map;
  SymTable *prev;
};

struct InterpreterState {
  const char *text;
  int text_pos = 0;
  int text_len;
  SymTable *symtable;
  // current module info
  const char* file_name = nullptr;
  u32 line = 1;
  u32 col = 0;
  bool running = false;
  // Pool of all objects allocated. Needed for GC
  // @PERFORMANCE: Custom allocator?
  std::list<Object*> objects_pool;
};

struct GarbageCollector {
  std::thread* thread = nullptr;
  std::ofstream* log_file = nullptr;
};

extern InterpreterState IS;

bool load_file(path file_to_read);
void init_interp();
void run_interp();

#endif
