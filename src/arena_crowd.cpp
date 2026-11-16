// WWE '13 Android port: arena crowd density (Full / Half / Off).
//
// An arena file (pac/bg/bgNN.pac) is an EPAC archive holding the stage index (a PACH); its entries 0x4E20 and
// 0x4E2A are the crowd packs, each a PACH of JBOY people models. For half / off, every (or every other) person
// keeps its model but draws nothing: its triangle strips get no indices and its bounding sphere no radius, and
// its now unused vertex data is zeroed so the re-packed file stays smaller than the original. The copy is padded
// to the original's exact size, so the size the game was told when it listed the folder stays right.
// Arenas stored XMemCompress'd are unpacked first and served uncompressed.
//
// The approach and the archive code (third_party/svrfmt: pac.cpp, pac.h, bytes.h) come from the WWE SmackDown vs.
// Raw 2011 PC port's crowd option (crowd_off.cpp), MIT licence, (c) 2026 KaikoClanworth1 and contributors.

#include "arena_crowd.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include <system_error>
#include <vector>

#include <rex/filesystem/devices/host_path_device.h>
#include <rex/logging.h>
#include <rex/system/lzx.h>

#include "svrfmt/pac.h"

namespace wwe13 {
namespace {

namespace fs = std::filesystem;
using svrfmt::Bytes;

enum class Level { kFull, kHalf, kOff };
Level g_level = Level::kFull;
fs::path g_cache_dir;
std::mutex g_mutex;

uint32_t Be32(const uint8_t* p) {
  return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | uint32_t(p[3]);
}
void PutBe32(uint8_t* p, uint32_t v) {
  p[0] = uint8_t(v >> 24);
  p[1] = uint8_t(v >> 16);
  p[2] = uint8_t(v >> 8);
  p[3] = uint8_t(v);
}

// One WWE '13 JBOY person drawing nothing (in place, same size). Returns false if it is not that layout.
bool EmptyPerson(Bytes& d) {
  constexpr size_t B = 8, kMesh = 0xB8;
  if (d.size() < 0x48 || std::memcmp(d.data(), "JBOY", 4)) return false;
  const size_t len = Be32(&d[4]);
  if (8 + len > d.size()) return false;
  auto in = [&](size_t o, size_t n) { return o + n <= 8 + len; };
  const uint32_t nmesh = Be32(&d[0x18]), mptr = Be32(&d[0x1C]);
  if (!nmesh || !in(B + mptr, kMesh * size_t(nmesh))) return false;
  if (!(Be32(&d[B + mptr + 0x78]) < 0x10000 && d[B + mptr + 0x7C] == 'y')) return false;
  for (uint32_t i = 0; i < nmesh; ++i) {
    uint8_t* q = &d[B + mptr + kMesh * i];
    const uint32_t vc = Be32(q), nstrip = Be32(q + 4);
    const uint32_t nw = std::max<uint32_t>(1, Be32(q + 0x5C));
    const uint32_t vblk = Be32(q + 0x68), wptr = Be32(q + 0x6C), uvptr = Be32(q + 0x70), iptr = Be32(q + 0x9C);
    if (!in(B + iptr, 12 * size_t(nstrip))) return false;
    for (uint32_t k = 0; k < nstrip; ++k) {
      uint8_t* h = &d[B + iptr + 12 * k];
      const uint32_t count = Be32(h + 4), at = Be32(h + 8);
      if (in(B + at, 2 * size_t(count))) std::memset(&d[B + at], 0, 2 * size_t(count));
      PutBe32(h + 4, 0);
    }
    if (in(B + vblk, 4)) {
      const size_t vd = B + Be32(&d[B + vblk]);
      if (in(vd, 28 * size_t(vc))) std::memset(&d[vd], 0, 28 * size_t(vc));
    }
    if (in(B + wptr, 8 * size_t(vc) * nw)) std::memset(&d[B + wptr], 0, 8 * size_t(vc) * nw);
    if (in(B + uvptr, 8 * size_t(vc))) std::memset(&d[B + uvptr], 0, 8 * size_t(vc));
    PutBe32(q + 0xA8 + 12, 0);  // bounding sphere radius
  }
  return true;
}


// XMemCompress (0x0FF512ED) files, as some WWE '13 arenas are stored: a header, a bit-packed table of each chunk's
// unpacked size, then chunks of LZX frames ({u16 packed size} for 32 KB, or {0xFF, u16 unpacked, u16 packed}).
// Layout as read by the SvR 2011 port's WWE '13 importer (wwe13.cpp, XcompressDecode); LZX by the SDK (libmspack).
bool IsXcompress(const Bytes& d) { return d.size() >= 16 && Be32(d.data()) == 0x0FF512EDu; }

bool XcompressDecode(const Bytes& d, Bytes& out) {
  if (!IsXcompress(d)) return false;
  const uint32_t flags = Be32(&d[12]);
  const uint32_t n = (flags >> 6) & 0xFFFF, bits = (flags & 0xC00000) ? 32 : 20;
  const uint32_t shift = (flags >> 4) & 3;
  const size_t chunk = size_t(0x8000) << shift;
  const uint32_t window = uint32_t(1) << (15 + shift);
  const size_t table = ((size_t(bits) * n + 31) >> 5) * 4, data = 16 + table;
  if (!n || data > d.size()) return false;
  std::vector<uint32_t> sizes(n);
  uint64_t total = 0;
  for (uint32_t i = 0; i < n; ++i) {
    uint64_t v = 0;
    for (uint32_t b = 0; b < bits; ++b) {
      const uint64_t q = uint64_t(i) * bits + b;
      v = v << 1 | ((d[16 + q / 8] >> (7 - q % 8)) & 1);
    }
    sizes[i] = uint32_t(v);
    total += v;
  }
  if (total > (uint64_t(256) << 20)) return false;
  out.assign(size_t(total), 0);
  auto frames_at = [&](size_t p, size_t unpacked, Bytes& frames, size_t* after) {
    frames.clear();
    size_t got = 0;
    while (got < unpacked && p + 2 <= d.size()) {
      size_t c, u = 0x8000;
      if (d[p] == 0xFF) {
        if (p + 5 > d.size()) return false;
        u = size_t(d[p + 1]) << 8 | d[p + 2];
        c = size_t(d[p + 3]) << 8 | d[p + 4];
        p += 5;
      } else {
        c = size_t(d[p]) << 8 | d[p + 1];
        p += 2;
      }
      if (!c || p + c > d.size()) return false;
      frames.insert(frames.end(), d.begin() + ptrdiff_t(p), d.begin() + ptrdiff_t(p + c));
      if (c & 1) frames.push_back(0);  // the LZX reader works in 16-bit words
      p += c;
      got += u ? u : 0x8000;
    }
    *after = p;
    return got >= unpacked;
  };
  Bytes frames;
  size_t op = 0, next = data;
  for (uint32_t i = 0; i < n; ++i) {
    // chunk i: usually at offset i * chunk (chunk 0 after the header); some files pack them back to back
    const size_t slot = i ? size_t(i) * chunk : data;
    int r = -2;
    size_t after = 0;
    std::vector<size_t> starts{slot};
    if (next != slot) starts.push_back(next);
    for (const size_t start : starts) {
      if (start + 2 + sizes[i] <= d.size() && d[start] == 0 && d[start + 1] == 0) {  // a stored chunk
        std::memcpy(out.data() + op, &d[start + 2], sizes[i]);
        after = start + 2 + sizes[i];
        r = 0;
        break;
      }
      if (!frames_at(start, sizes[i], frames, &after)) continue;
      r = lzx_decompress(frames.data(), frames.size(), out.data() + op, sizes[i], window, nullptr, 0);
      if (!r) break;
    }
    if (r) return false;
    next = after;
    op += sizes[i];
  }
  return true;
}

// The arena without (half of) its crowd. False: no crowd found, or the copy would not fit.
bool Strip(const Bytes& file, Bytes& out, int& emptied, bool half) {
  // Compressed arenas are unpacked; their copy is written uncompressed (as other arenas are stored), so it is
  // bigger than the file it replaces - the SDK reports the copy's own size to the game.
  Bytes unpacked;
  const bool compressed = IsXcompress(file);
  if (compressed && !XcompressDecode(file, unpacked)) return false;
  const Bytes& in = compressed ? unpacked : file;
  svrfmt::Epac epac;
  if (!svrfmt::EpacRead(in, epac)) return false;
  for (auto& group : epac.groups) {
    for (auto& entry : group.entries) {
      if (!svrfmt::IsPach(entry.data)) continue;
      std::vector<svrfmt::PachEntry> entries;
      if (!svrfmt::PachRead(entry.data, entries)) return false;
      bool any = false;
      for (auto& e : entries) {
        if (e.id != 0x4E20 && e.id != 0x4E2A) continue;
        const bool packed = svrfmt::IsBpe(e.data);
        std::vector<svrfmt::PachEntry> people;
        if (!svrfmt::PachRead(svrfmt::Unpack(e.data), people)) continue;
        int index = 0;
        for (auto& person : people) {
          if (half && index++ % 2 == 0) continue;  // half: every other person stays
          Bytes raw = svrfmt::Unpack(person.data);
          if (!EmptyPerson(raw)) continue;
          person.data = svrfmt::IsBpe(person.data) ? svrfmt::BpeEncode(raw) : raw;
          ++emptied;
          any = true;
        }
        const Bytes pach = svrfmt::PachWrite(people);
        e.data = packed ? svrfmt::BpeEncode(pach) : pach;
      }
      if (!any) continue;
      entry.data = svrfmt::PachWrite(entries);
      out = svrfmt::EpacWrite(epac);
      if (out.size() > in.size()) return false;  // never more than the arena's own unpacked size
      if (!compressed) out.resize(in.size(), 0);  // stored arenas keep their exact size
      return true;
    }
  }
  return false;
}

std::string Stamp(const fs::path& p) {
  std::error_code e1, e2;
  const auto size = fs::file_size(p, e1);
  const auto time = fs::last_write_time(p, e2).time_since_epoch().count();
  return e1 || e2 ? "" : std::to_string(uint64_t(size)) + " " + std::to_string(int64_t(time));
}

std::string ReadText(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(f), {});
}

bool IsArenaFile(const fs::path& p) {
  std::string name = p.filename().string(), dir = p.parent_path().filename().string();
  for (auto& c : name) c = char(std::tolower(static_cast<unsigned char>(c)));
  for (auto& c : dir) c = char(std::tolower(static_cast<unsigned char>(c)));
  return dir == "bg" && name.size() > 6 && name.rfind("bg", 0) == 0 && name.size() - name.rfind(".pac") == 4;
}

fs::path Redirect(const fs::path& host_path) {
  const fs::path copy = ArenaCrowdCopy(host_path);
  return copy.empty() ? host_path : copy;
}

}  // namespace

fs::path ArenaCrowdCopy(const fs::path& original) {
  if (g_level == Level::kFull || g_cache_dir.empty() || !IsArenaFile(original)) return {};
  const bool half = g_level == Level::kHalf;
  const fs::path copy = g_cache_dir / ((half ? "half_" : "off_") + original.filename().string());
  const fs::path stamp_file = copy.string() + ".stamp";
  const std::string want = Stamp(original);
  if (want.empty()) return {};
  std::lock_guard lock(g_mutex);
  std::error_code ec;
  const std::string have = ReadText(stamp_file);
  if (have == want && fs::exists(copy, ec)) return copy;
  if (have == want + " original") return {};

  const auto t0 = std::chrono::steady_clock::now();
  std::ifstream f(original, std::ios::binary | std::ios::ate);
  const std::streamoff size = f ? std::streamoff(f.tellg()) : 0;
  Bytes in(size > 0 ? size_t(size) : 0);
  f.seekg(0);
  if (!in.empty()) f.read(reinterpret_cast<char*>(in.data()), std::streamsize(in.size()));
  if (!f || in.empty()) return {};
  Bytes out;
  int emptied = 0;
  fs::create_directories(g_cache_dir, ec);
  if (!Strip(in, out, emptied, half)) {
    std::ofstream(stamp_file, std::ios::binary) << want << " original";
    REXLOG_INFO("[wwe13-crowd] {} kept as is ({})", original.filename().string(),
                emptied ? "the copy came out bigger" : "no crowd found");
    return {};
  }
  const fs::path tmp = copy.string() + ".tmp";
  {
    std::ofstream o(tmp, std::ios::binary | std::ios::trunc);
    o.write(reinterpret_cast<const char*>(out.data()), std::streamsize(out.size()));
    if (!o) return {};
  }
  fs::rename(tmp, copy, ec);
  if (ec) return {};
  std::ofstream(stamp_file, std::ios::binary) << want;
  REXLOG_INFO("[wwe13-crowd] {} {} ({} people emptied, {:.2f} s)", original.filename().string(),
              half ? "with half its crowd" : "without its crowd", emptied,
              std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
  return copy;
}

void InitializeArenaCrowd(const fs::path& cache_dir) {
  const char* value = std::getenv("WWE13_ARENA_CROWD");
  const std::string v = value ? value : "";
  g_level = v == "off" ? Level::kOff : v == "half" ? Level::kHalf : Level::kFull;
  g_cache_dir = cache_dir;
  REXLOG_INFO("[wwe13-crowd] WWE13_ARENA_CROWD='{}': {}", v,
              g_level == Level::kOff ? "off" : g_level == Level::kHalf ? "half" : "full");
  if (g_level != Level::kFull) rex::filesystem::SetHostPathRedirect(&Redirect);
}

}  // namespace wwe13
