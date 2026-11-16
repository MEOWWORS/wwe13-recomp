# Round 1 of game fixes for the 2K14 SDK (ReXGlue v0.10). Every edit checks its spot first; nothing is written if one fails.
import glob, os, re, sys
G, K, P = "/workspaces/wwe13-k14", "/workspaces/k14", "/workspaces/k14/out/install/android-arm64"
new = {}
def load(p):
    return new[p] if p in new else open(p).read()
def edit(p, pairs, optional=False):
    s = load(p)
    for a, b in pairs:
        if b in s and a not in s:
            continue  # already applied
        assert s.count(a) == 1, (p, a[:70], s.count(a))
        s = s.replace(a, b)
    new[p] = s

pch = G + "/generated/default/wwe13_pch.h"
assert os.path.isfile(pch), "generated/default/wwe13_pch.h is missing"

# ---------------------------------------------------------------- old SDK names our hooks use
compat = '''// WWE13 on the 2K14 SDK (ReXGlue v0.10): the old v0.7 names our game hooks use.
// v0.10 renamed PPC_* to REX_* and moved the guest memory macros into the generated header.
#pragma once

#include "generated/default/wwe13_pch.h"

#ifndef PPC_FUNC
#define PPC_FUNC(x) REX_FUNC(x)
#endif
#define PPC_RAW_ADDR(x) REX_RAW_ADDR(x)
#define PPC_PHYS_HOST_OFFSET(x) REX_PHYS_HOST_OFFSET(x)
#define PPC_LOAD_U8(x) REX_LOAD_U8(x)
#define PPC_LOAD_U16(x) REX_LOAD_U16(x)
#define PPC_LOAD_U32(x) REX_LOAD_U32(x)
#define PPC_LOAD_U64(x) REX_LOAD_U64(x)
#define PPC_LOAD_STRING(x, len) REX_LOAD_STRING(x, len)
#define PPC_STORE_U8(x, y) REX_STORE_U8(x, y)
#define PPC_STORE_U16(x, y) REX_STORE_U16(x, y)
#define PPC_STORE_U32(x, y) REX_STORE_U32(x, y)
#define PPC_STORE_U64(x, y) REX_STORE_U64(x, y)
#define PPC_MM_LOAD_U8(x) REX_MM_LOAD_U8(x)
#define PPC_MM_LOAD_U16(x) REX_MM_LOAD_U16(x)
#define PPC_MM_LOAD_U32(x) REX_MM_LOAD_U32(x)
#define PPC_MM_LOAD_U64(x) REX_MM_LOAD_U64(x)
#define PPC_MM_STORE_U8(x, y) REX_MM_STORE_U8(x, y)
#define PPC_MM_STORE_U16(x, y) REX_MM_STORE_U16(x, y)
#define PPC_MM_STORE_U32(x, y) REX_MM_STORE_U32(x, y)
#define PPC_MM_STORE_U64(x, y) REX_MM_STORE_U64(x, y)
#define PPC_IS_MMIO_ADDR(x) REX_IS_MMIO_ADDR(x)
#define PPC_MEMORY_SIZE REX_MEMORY_SIZE
#ifdef REX_CALL_FUNC
#define PPC_CALL_FUNC REX_CALL_FUNC
#endif
'''
new[G + "/src/rex_compat.h"] = compat
uses_old = re.compile(r"\bPPC_(FUNC|RAW_ADDR|LOAD_|STORE_|MM_|IS_MMIO|MEMORY_SIZE|CALL_FUNC|PHYS_HOST)")
changed = []
for p in sorted(glob.glob(G + "/src/**/*.cpp", recursive=True) + glob.glob(G + "/src/**/*.h", recursive=True)):
    if p.endswith("/rex_compat.h"):
        continue
    s = load(p)
    t = s.replace('"generated/wwe13_config.h"', '"generated/default/wwe13_pch.h"')
    t = t.replace('"generated/wwe13_init.h"', '"generated/default/wwe13_init.h"')
    if "<rex/ppc/memory.h>" in t:
        t = t.replace("#include <rex/ppc/memory.h>", '#include "rex_compat.h"')
    elif uses_old.search(t) and '"rex_compat.h"' not in t:
        m = re.search(r"^#include [<\"].*$", t, re.M)
        assert m, (p, "no #include line")
        t = t[:m.end()] + '\n#include "rex_compat.h"' + t[m.end():]
    if t != s:
        new[p] = t
        changed.append(os.path.relpath(p, G))

# ---------------------------------------------------------------- PC keyboard typing hook: not in the 2K14 SDK
edit(G + "/src/text_entry_keyboard.cpp", [
 ("    input_system->SetTextEntryTarget(&g_text_entry_target);\n",
  "    // 2K14 SDK: no PC keyboard text entry hook (Android has no PC keyboard here).\n"
  "    (void)input_system;\n    (void)g_text_entry_target;\n"),
])

# ---------------------------------------------------------------- touch controls: the v0.10 input driver interface
H = G + "/src/virtual_gamepad.h"
edit(H, [
 ("#include <rex/ui/window_listener.h>\n", "#include <rex/ui/window_listener.h>\n#include <rex/input/device.h>\n\n#include <vector>\n"),
 ("  rex::X_RESULT GetCapabilities(\n      uint32_t user_index, uint32_t flags,",
  "  void EnumerateDevices(std::vector<rex::input::DeviceInfo>& out) override;\n"
  "  rex::X_RESULT GetDeviceCapabilities(\n      rex::input::DeviceId device, uint32_t flags,"),
 ("  rex::X_RESULT GetState(\n      uint32_t user_index,", "  rex::X_RESULT GetDeviceState(\n      rex::input::DeviceId device,"),
 ("  rex::X_RESULT SetState(\n      uint32_t user_index,", "  rex::X_RESULT SetDeviceVibration(\n      rex::input::DeviceId device,"),
 ("  rex::X_RESULT GetKeystroke(\n      uint32_t user_index, uint32_t flags,",
  "  rex::X_RESULT GetDeviceKeystroke(\n      rex::input::DeviceId device, uint32_t flags,"),
])
C = G + "/src/virtual_gamepad.cpp"
pick = "  const uint32_t user_index = device == kTouchPad ? 0u : 1u;\n"
edit(C, [
 ("rex::X_STATUS VirtualGamepadInputDriver::Setup() {",
  "// The 2K14 SDK asks each driver for its devices: the touch controls are one pad.\n"
  "constexpr rex::input::DeviceId kTouchPad{1};\n\n"
  "void VirtualGamepadInputDriver::EnumerateDevices(std::vector<rex::input::DeviceInfo>& out) {\n"
  "  rex::input::DeviceInfo info;\n  info.id = kTouchPad;\n  info.name = \"Touch controls\";\n"
  "  info.guid = \"wwe13-touch\";\n  out.push_back(std::move(info));\n}\n\n"
  "rex::X_STATUS VirtualGamepadInputDriver::Setup() {"),
 ("VirtualGamepadInputDriver::GetCapabilities(\n    uint32_t user_index, uint32_t flags,\n    rex::input::X_INPUT_CAPABILITIES* out_caps) {\n",
  "VirtualGamepadInputDriver::GetDeviceCapabilities(\n    rex::input::DeviceId device, uint32_t flags,\n    rex::input::X_INPUT_CAPABILITIES* out_caps) {\n" + pick),
 ("VirtualGamepadInputDriver::GetState(\n    uint32_t user_index, rex::input::X_INPUT_STATE* out_state) {\n",
  "VirtualGamepadInputDriver::GetDeviceState(\n    rex::input::DeviceId device, rex::input::X_INPUT_STATE* out_state) {\n" + pick),
 ("VirtualGamepadInputDriver::SetState(\n    uint32_t user_index, rex::input::X_INPUT_VIBRATION* vibration) {\n",
  "VirtualGamepadInputDriver::SetDeviceVibration(\n    rex::input::DeviceId device, rex::input::X_INPUT_VIBRATION* vibration) {\n" + pick),
 ("VirtualGamepadInputDriver::GetKeystroke(\n    uint32_t user_index, uint32_t flags,\n",
  "VirtualGamepadInputDriver::GetDeviceKeystroke(\n    rex::input::DeviceId device, uint32_t flags,\n"),
 ("  (void)flags;\n  (void)out_keystroke;\n", "  (void)flags;\n  (void)out_keystroke;\n" + pick),
])

# ---------------------------------------------------------------- SDK: the drawer the touch artwork is uploaded with
for inc in (K + "/include/rex/ui/imgui_drawer.h", P + "/include/rex/ui/imgui_drawer.h"):
    edit(inc, [
     ("  bool HasDialogs() const { return !dialogs_.empty(); }\n",
      "  bool HasDialogs() const { return !dialogs_.empty(); }\n"
      "  // WWE13: the touch-control artwork is uploaded through this drawer.\n"
      "  ImmediateDrawer* immediate_drawer() const { return immediate_drawer_; }\n"),
    ])

for p, text in new.items():
    with open(p, "w") as f:
        f.write(text)
print("fix1 ok:", len(new), "files;", "old names mapped in:", ", ".join(changed) or "none")
