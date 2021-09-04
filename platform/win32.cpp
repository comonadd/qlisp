#include "platform.hpp"
// NOLINTNEXTLINE
#include "windows.h"
// NOLINTNEXTLINE
#include "psapi.h"

size_t get_total_memory_usage() {
  PROCESS_MEMORY_COUNTERS_EX pmc;
  GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&pmc,
                       sizeof(pmc));
  SIZE_T virtualMemUsedByMe = pmc.PrivateUsage;
  return virtualMemUsedByMe;
}
