/**
 * WWE13 diagnostic: with WWE13_FRAME_TRACE=1, one whole frame every 10 seconds is written to the log as the ordered
 * list of what the GPU emulation did in it: each render target switch (with its EDRAM transfers), how many draws went
 * into it, each resolve (copy out of EDRAM into a texture) and each texture (re)load. That frame list shows which of
 * the game's passes cost the copies and transfers. Off: one cached getenv check per call site.
 */
#pragma once

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <rex/logging.h>

namespace rex::graphics::frame_trace {

inline bool Enabled() {
  static const bool on = [] {
    const char* v = std::getenv("WWE13_FRAME_TRACE");
    return v && v[0] == '1';
  }();
  return on;
}

struct State {
  bool recording = false;
  uint64_t next_ns = 0;
  uint32_t frame = 0, draws = 0;
  std::vector<std::string> lines;
};
inline State& Get() {
  static State state;
  return state;
}

inline void FlushDraws() {
  State& s = Get();
  if (s.draws) {
    s.lines.push_back(fmt::format("  draws x{}", s.draws));
    s.draws = 0;
  }
}

inline void Draw() {
  if (Enabled() && Get().recording) ++Get().draws;
}

template <typename... Args>
inline void Event(fmt::format_string<Args...> format, Args&&... args) {
  if (!Enabled() || !Get().recording) return;
  FlushDraws();
  if (Get().lines.size() < 6000) Get().lines.push_back(fmt::format(format, std::forward<Args>(args)...));
}

// Called once per guest frame (swap).
inline void EndFrame() {
  if (!Enabled()) return;
  State& s = Get();
  ++s.frame;
  const uint64_t now = uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                    std::chrono::steady_clock::now().time_since_epoch()).count());
  if (s.recording) {
    FlushDraws();
    REXGPU_INFO("[frame-trace] begin frame {} ({} entries)", s.frame, s.lines.size());
    for (const std::string& line : s.lines) REXGPU_INFO("[frame-trace] {}", line);
    REXGPU_INFO("[frame-trace] end frame {}", s.frame);
    s.lines.clear();
    s.recording = false;
    s.next_ns = now + 10000000000ull;
  } else if (!s.next_ns) {
    s.next_ns = now + 10000000000ull;
  } else if (now >= s.next_ns) {
    s.recording = true;
  }
}

}  // namespace rex::graphics::frame_trace
