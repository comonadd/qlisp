#ifndef INTERP_HPP
#define INTERP_HPP

#include <unordered_map>
#include <filesystem>
#include <string>
#include "types.hpp"

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
  u32 line = 0;
  u32 col = 0;
};

extern InterpreterState IS;

bool load_file(path file_to_read);
void init_interp();
void run_interp();

#endif
