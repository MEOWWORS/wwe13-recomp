// WWE '13 Android port: Fast CPU cores (see cpu_cores.h).

#include "cpu_cores.h"

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#if defined(__linux__)
#include <sched.h>
#endif

#include <rex/logging.h>

namespace wwe13 {

void InitializeCpuCores() {
  const char* value = std::getenv("WWE13_BIG_CORES");
  if (!value || std::strcmp(value, "1") != 0) {
    REXLOG_INFO("[wwe13-cpu] fast CPU cores: off");
    return;
  }
#if defined(__linux__)
  std::vector<long> top(64, 0);
  long fastest = 0;
  int count = 0;
  for (int cpu = 0; cpu < 64; ++cpu) {
    char path[96];
    std::snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", cpu);
    std::ifstream file(path);
    long khz = 0;
    if (file >> khz && khz > 0) {
      top[cpu] = khz;
      fastest = std::max(fastest, khz);
      count = cpu + 1;
    }
  }
  if (fastest <= 0) {
    REXLOG_INFO("[wwe13-cpu] fast CPU cores: no CPU clock information, left as is");
    return;
  }
  cpu_set_t set;
  CPU_ZERO(&set);
  int chosen = 0, known = 0;
  std::string list;
  for (int cpu = 0; cpu < count; ++cpu) {
    if (top[cpu] <= 0) continue;
    ++known;
    if (top[cpu] * 4 >= fastest * 3) {
      CPU_SET(cpu, &set);
      ++chosen;
      list += (list.empty() ? "" : ",") + std::to_string(cpu);
    }
  }
  if (chosen == known || chosen < 2) {
    REXLOG_INFO("[wwe13-cpu] fast CPU cores: {} of {} cores qualify, left as is", chosen, known);
    return;
  }
  int pinned = 0, failed = 0;
  std::error_code error;
  for (const auto& entry : std::filesystem::directory_iterator("/proc/self/task", error)) {
    const int tid = std::atoi(entry.path().filename().string().c_str());
    if (tid <= 0) continue;
    if (sched_setaffinity(tid, sizeof(set), &set) == 0) ++pinned; else ++failed;
  }
  REXLOG_INFO("[wwe13-cpu] fast CPU cores: cpus {} ({} of {}), {} threads pinned, {} failed", list, chosen, known,
              pinned, failed);
#else
  REXLOG_INFO("[wwe13-cpu] fast CPU cores: not supported on this system");
#endif
}

}  // namespace wwe13
