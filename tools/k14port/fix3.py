# Round 3: three settings the 2K14 SDK dropped. The game keeps them (diagnostics log them, the debug overlay reads
# debug_ui), so a launcher argument naming them is still accepted.
G = "/workspaces/wwe13-k14"
p = G + "/src/release_diagnostics.cpp"
s = open(p).read()
pairs = [
 ("REXCVAR_DECLARE(std::int32_t, resolution_scale);",
  'REXCVAR_DEFINE_INT32(resolution_scale, 1, "GPU", "WWE13: logged only (not in the 2K14 SDK)");'),
 ("REXCVAR_DECLARE(bool, debug_ui);",
  'REXCVAR_DEFINE_BOOL(debug_ui, false, "UI/Debug", "WWE13: debug overlays (not in the 2K14 SDK)");'),
 ("REXCVAR_DECLARE(std::string, vulkan_device_id);",
  'REXCVAR_DEFINE_STRING(vulkan_device_id, "", "UI/Vulkan", "WWE13: logged only (not in the 2K14 SDK)");'),
]
for a, b in pairs:
    if a not in s and b in s:
        continue
    assert s.count(a) == 1, (a, s.count(a))
    s = s.replace(a, b)
open(p, "w").write(s)
print("fix3 ok")
