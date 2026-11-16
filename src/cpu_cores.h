// WWE '13 Android port: keep the game's threads on the phone's fastest CPU cores (launcher: Fast CPU cores).
#pragma once

namespace wwe13 {

// Reads WWE13_BIG_CORES; "1" pins every thread of the process (and the threads they start later) to the cores
// whose top clock is at least 3/4 of the fastest one. Does nothing when all cores are alike.
void InitializeCpuCores();

}  // namespace wwe13
