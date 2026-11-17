# Two fixes from our old SDK that the game needs on the 2K14 SDK:
# 1) Game speed: frame_step.h scales the game's time step to the real frame time, so below 60 fps the game keeps its
#    normal speed instead of running in slow motion (frame_step_fix, on by default).
# 2) Textures with the "invalid" type flag that are otherwise consistent are used as textures, as the real GPU does.
#    The 2K14 SDK skipped them (missing textures) and logged a warning for every one: 44,796 lines in one match.
import os, re, shutil
OLD, K = "/workspaces/tools/rexglue", "/workspaces/k14"
new = {}
def load(p): return new.get(p) or open(p).read()

src_fs = OLD + "/src/graphics/vulkan/frame_step.h"
assert os.path.isfile(src_fs), "frame_step.h missing in the old SDK"
C = K + "/src/graphics/vulkan/command_processor.cpp"
s = load(C)
if '#include "frame_step.h"' not in s:
    inc = "#include <rex/graphics/vulkan/command_processor.h>\n"
    assert s.count(inc) == 1, "command_processor.h include"
    s = s.replace(inc, inc + '#include "frame_step.h"  // WWE13: game speed at any frame rate\n'
                  'REXCVAR_DEFINE_BOOL(frame_step_fix, true, "GPU", "WWE13: scale the game time step to the real frame time.");\n')
    m = re.search(r"void VulkanCommandProcessor::IssueSwap\([^)]*\)\s*\{\n", s)
    assert m, "IssueSwap"
    s = s[:m.end()] + ("  if (REXCVAR_GET(frame_step_fix)) {\n"
                       "    framestep::Tick(memory_->TranslateVirtual<uint8_t*>(0x82000000));\n"
                       "  }\n") + s[m.end():]
    new[C] = s

T = K + "/src/graphics/pipeline/texture/cache.cpp"
s = load(T)
if "IsSelfConsistentInvalidTextureFetch" not in s:
    old = open(OLD + "/src/graphics/pipeline/texture/cache.cpp").read()
    m = re.search(r"namespace \{\n\n// The real Xenos hardware binds.*?\n\}  // namespace\n", old, re.S)
    assert m and "IsSelfConsistentInvalidTextureFetch" in m.group(0), "helper not found in the old SDK"
    anchor = "void TextureCache::BindingInfoFromFetchConstant("
    assert s.count(anchor) == 1, "BindingInfoFromFetchConstant"
    s = s.replace(anchor, m.group(0) + "\n" + anchor)
    a = "      if (REXCVAR_GET(gpu_allow_invalid_fetch_constants)) {\n        break;\n      }\n"
    assert s.count(a) == 1, ("invalid fetch check", s.count(a))
    s = s.replace(a, a + "      if (IsSelfConsistentInvalidTextureFetch(fetch)) {\n"
                         "        break;  // WWE13: the real GPU samples these\n      }\n")
    new[T] = s

dst_fs = K + "/src/graphics/vulkan/frame_step.h"
if not os.path.exists(dst_fs):
    shutil.copyfile(src_fs, dst_fs)
for p, t in new.items():
    open(p, "w").write(t)
print("fix7 ok:", len(new), "files edited, frame_step.h", "copied" if new else "present")
