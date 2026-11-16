# Round 2 of game fixes for the 2K14 SDK. Every edit checks its spot first; nothing is written if one fails.
G = "/workspaces/wwe13-k14"
new = {}
def load(p):
    return new[p] if p in new else open(p).read()
def edit(p, pairs, count=1):
    s = load(p)
    for a, b in pairs:
        if a not in s and b in s:
            continue
        assert s.count(a) == count, (p, a[:70], s.count(a))
        s = s.replace(a, b)
    new[p] = s
A = G + "/src/wwe13_app.h"
# v0.10's app has no window-title or graphics-failure hooks: keep the functions, drop 'override'.
edit(A, [("std::string_view GetWindowTitle() const override {", "std::string_view GetWindowTitle() const {"),
         ("void OnGraphicsSetupFailed(rex::X_STATUS status) override {", "void OnGraphicsSetupFailed(rex::X_STATUS status) {")])
edit(A, [("rex::log::GPU,", "rex::log::gpu(),")], count=2)  # log categories are functions in v0.10
edit(G + "/src/resolve_coherency.cpp", [
 ("    graphics->command_processor()->MakeGpuWritesVisibleToCpu(source.u32, uint32_t(size));\n",
  "    // 2K14 SDK: no GPU->CPU readback helper yet (to port: retro wrestler textures can render black).\n"
  "    (void)source;\n    (void)size;\n")])
edit(G + "/src/rex_compat.h", [("#ifdef REX_CALL_FUNC\n", "#define PPC_QUERY_TIMEBASE REX_QUERY_TIMEBASE\n#ifdef REX_CALL_FUNC\n")])
for p, t in new.items():
    open(p, "w").write(t)
print("fix2 ok:", len(new), "files")
