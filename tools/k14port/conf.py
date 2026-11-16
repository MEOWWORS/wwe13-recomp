import re, subprocess, sys
P = sys.argv[1]
old = open('/workspaces/wwe13-recomp/out/build/android-arm64/CMakeCache.txt').read()
def get(k):
    m = re.search(r'^' + re.escape(k) + r':[A-Z]+=(.*)$', old, re.M)
    return m.group(1) if m else None
fix = lambda v: v.replace('/workspaces/tools/rexglue', '/workspaces/k14').replace('/workspaces/wwe13-recomp', '/workspaces/wwe13-k14')
args = ['cmake', '-S', '/workspaces/wwe13-k14', '-B', '/workspaces/wwe13-k14/out/build/android-arm64',
        '-G', get('CMAKE_GENERATOR') or 'Ninja Multi-Config']
seen = set()
for k in ['CMAKE_TOOLCHAIN_FILE', 'ANDROID_ABI', 'ANDROID_PLATFORM', 'ANDROID_STL', 'CMAKE_PREFIX_PATH',
          'CMAKE_FIND_ROOT_PATH', 'rexglue_DIR', 'SDL3_DIR']:
    v = get(k)
    if v and 'NOTFOUND' not in v:
        args.append('-D%s=%s' % (k, fix(v)))
        seen.add(k)
if 'CMAKE_PREFIX_PATH' not in seen:
    args.append('-DCMAKE_PREFIX_PATH=' + P)
# The SDK package pulls in fmt, spdlog, utf8cpp, SDL3, VulkanHeaders and VulkanMemoryAllocator. The Android
# toolchain only searches inside the NDK sysroot, so point CMake at each package folder in the SDK install directly.
import os
for name in ['fmt', 'spdlog', 'utf8cpp', 'SDL3', 'VulkanHeaders', 'VulkanMemoryAllocator', 'rexglue']:
    for root, dirs, files in os.walk(P):
        if name + 'Config.cmake' in files or name.lower() + '-config.cmake' in files:
            args = [a for a in args if not a.startswith('-D%s_DIR=' % name)]
            args.append('-D%s_DIR=%s' % (name, root))
            break
    else:
        print('  (no package folder for %s in the SDK install)' % name)
args.append('-DCMAKE_FIND_ROOT_PATH=' + P)
# Show the real reason if the SDK package still is not found.
g = '/workspaces/wwe13-k14/generated/rexglue.cmake'
t = open(g).read()
if ' QUIET CONFIG)' in t:
    open(g, 'w').write(t.replace(' QUIET CONFIG)', ' CONFIG)'))
flags = (get('CMAKE_CXX_FLAGS') or '').strip()
if '_LIBCPP_ENABLE_EXPERIMENTAL' not in flags:
    flags = (flags + ' -D_LIBCPP_ENABLE_EXPERIMENTAL').strip()
args += ['-DCMAKE_CXX_FLAGS=' + flags, '-DCMAKE_BUILD_TYPE=Release', '-DWWE13_SDK_ROOT=/workspaces/k14']
for a in args:
    if a.startswith('-D'):
        print('  ' + a[:110].replace(P, '<sdk>'))
with open('/workspaces/gameconf.log', 'w') as log:
    r = subprocess.call(args, stdout=log, stderr=subprocess.STDOUT)
print('configure ok' if r == 0 else 'configure FAILED (see the report)')
sys.exit(r)
