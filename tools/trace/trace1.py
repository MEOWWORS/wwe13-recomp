# Frame trace (WWE13_FRAME_TRACE=1) in the current SDK + the launcher turning it on for this test build.
import re, shutil, sys
SDK, GAME, HERE = "/workspaces/tools/rexglue", "/workspaces/wwe13-recomp", sys.argv[1]
new = {}
def load(p): return new.get(p) or open(p).read()
def include(p):
    s = load(p)
    if "<rex/graphics/frame_trace.h>" in s: return s
    m = re.search(r"^#include <rex/[^\n]*\n", s, re.M)
    assert m, (p, "no rex include")
    return s[:m.end()] + "#include <rex/graphics/frame_trace.h>\n" + s[m.end():]
def edit(p, a, b, count=1):
    s = include(p) if p.startswith(SDK) else load(p)
    if b not in s:
        assert s.count(a) == count, (p, a[:60], s.count(a))
        s = s.replace(a, b)
    new[p] = s
FT = "rex::graphics::frame_trace::"
CP = SDK + "/src/graphics/vulkan/command_processor.cpp"
edit(CP, "      diagnostics::rate_census::CpPhase::kDraw);\n",
     "      diagnostics::rate_census::CpPhase::kDraw);\n  " + FT + "Draw();\n")
edit(CP, "  diagnostics::rate_census::RecordPresent();\n",
     "  diagnostics::rate_census::RecordPresent();\n  " + FT + "EndFrame();\n")
edit(CP, "LogResolveDiagnostic(written_address, written_length);\n",
     "LogResolveDiagnostic(written_address, written_length);\n"
     "  " + FT + "Event(\"resolve -> {:08X} {} KB\", written_address, written_length / 1024);\n", count=2)
RT = SDK + "/src/graphics/vulkan/render_target_cache.cpp"
edit(RT, "    g_rt_census.transfer_entries += entries;\n",
     "    g_rt_census.transfer_entries += entries;\n"
     "    if (" + FT + "Enabled()) {\n"
     "      for (uint32_t i = 0; i < render_target_count; ++i) {\n"
     "        if (!render_targets[i]) continue;\n"
     "        const uint32_t k = render_targets[i]->key().key;\n"
     "        " + FT + "Event(\"rt{} base={} pitch={} msaa={} {} fmt={} transfers={}\", i, k & 0x7FF, (k >> 11) & 0xFF,\n"
     "                     1u << ((k >> 19) & 3), ((k >> 21) & 1) ? \"depth\" : \"color\", (k >> 22) & 0xF,\n"
     "                     render_target_transfers[i].size());\n"
     "      }\n"
     "    }\n")
TC = SDK + "/src/graphics/pipeline/texture/cache.cpp"
edit(TC, "  if (!PrepareTextureLoad(texture, pending_load, pending_ranges, pending_range_count)) {\n    return true;\n  }\n",
     "  if (!PrepareTextureLoad(texture, pending_load, pending_ranges, pending_range_count)) {\n    return true;\n  }\n"
     "  " + FT + "Event(\"texload {:08X} {}x{} fmt={}\", uint32_t(texture.key().base_page) << 12,\n"
     "      uint32_t(texture.key().width_minus_1) + 1, uint32_t(texture.key().height_minus_1) + 1,\n"
     "      uint32_t(texture.key().format));\n")
L = GAME + "/launcher2/core/launch.cpp"
edit(L, '      {"WWE13_TOUCH_SCALE", std::to_string(effective.touch_size)},\n',
     '      {"WWE13_TOUCH_SCALE", std::to_string(effective.touch_size)},\n'
     '      {"WWE13_FRAME_TRACE", "1"},  // test build: one traced frame every 10 s in the log\n')
shutil.copyfile(HERE + "/frame_trace.h", SDK + "/include/rex/graphics/frame_trace.h")
for p, t in new.items():
    open(p, "w").write(t)
print("trace ok:", len(new), "files")
