// WWE '13 Android port: arena crowd density (Full / Half / Off).
#pragma once

#include <filesystem>

namespace wwe13 {

// Reads WWE13_ARENA_CROWD ("off", "half", anything else = full). For half / off, arena files (pac/bg/bg*.pac)
// are served from copies without (half of) their crowd people, made the first time each arena is opened and
// cached in `cache_dir`.
void InitializeArenaCrowd(const std::filesystem::path& cache_dir);

// The file the game should open instead of `original` ("" = the original). Exposed for tests.
std::filesystem::path ArenaCrowdCopy(const std::filesystem::path& original);

}  // namespace wwe13
