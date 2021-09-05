#ifndef INTERP_HPP
#define INTERP_HPP

#include <unordered_map>
#include <filesystem>
#include <string>

using std::filesystem::path;

struct Object;

using SymVars = std::unordered_map<std::string, Object *>;
struct SymTable {
  SymVars map;
  SymTable *prev;
};

// Interpreter state
struct {
  const char *text;
  int text_pos = 0;
  int text_len;
  SymTable *symtable;
} IS;

bool load_file(path file_to_read);
void init_interp();
void run_interp();

#endif
