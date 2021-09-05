#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

std::string read_whole_file_into_memory(char const *fp) {
  std::ifstream in(fp);
  std::string contents((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
  return contents;
}
