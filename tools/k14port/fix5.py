# The 2K14 SDK keeps the GPU emulation in a plugin (librexgpu-xenos.so) and loads it only when --gpu_plugin names it,
# looking for it next to the executable, which on Android is not the app's library folder. Load it here instead: the
# Android linker finds it by name in the APK's native libraries.
p = "/workspaces/wwe13-k14/src/wwe13_app.h"
s = open(p).read()
pairs = [
 ('#include "virtual_gamepad.h"\n#endif\n',
  '#include "virtual_gamepad.h"\n#include <dlfcn.h>\n#include <rex/system/gpu_plugin.h>\n#endif\n'),
 ("      return input;\n    };\n#endif\n",
  "      return input;\n    };\n"
  "    if (!config.graphics) {\n"
  "      namespace sys = rex::system;\n"
  "      void* plugin = dlopen(\"librexgpu-xenos.so\", RTLD_NOW);\n"
  "      auto abi = plugin ? reinterpret_cast<sys::GpuAbiVersionFn>(dlsym(plugin, sys::kGpuAbiVersionSymbol)) : nullptr;\n"
  "      auto create = plugin ? reinterpret_cast<sys::GpuCreateFn>(dlsym(plugin, sys::kGpuCreateSymbol)) : nullptr;\n"
  "      if (abi && create && abi() == sys::kGpuPluginAbiVersion) {\n"
  "        sys::GpuCreateInfo info{};\n"
  "        info.struct_size = sizeof(info);\n"
  "        info.backend = \"vulkan\";\n"
  "        config.graphics.reset(create(sys::kGpuPluginAbiVersion, &info));\n"
  "      }\n"
  "      REXLOG_INFO(\"[wwe13] GPU plugin librexgpu-xenos.so: {}\", config.graphics ? \"loaded\"\n"
  "                  : plugin ? \"loaded, but it made no graphics system\" : dlerror());\n"
  "    }\n"
  "#endif\n"),
]
for a, b in pairs:
    if b in s:
        continue
    assert s.count(a) == 1, (a[:50], s.count(a))
    s = s.replace(a, b)
open(p, "w").write(s)
print("fix5 ok")
