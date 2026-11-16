#!/usr/bin/env bash
# Builds the game (wwe13-k14) against the installed 2K14 SDK. Report: /workspaces/k14step3.txt
P=/workspaces/k14/out/install/android-arm64
R=/workspaces/k14step3.txt
LIBDIR=$P/lib
[ -f "$LIBDIR/librexruntime.so" ] || { echo "The SDK install has no librexruntime.so"; exit 1; }
echo "== SDK libs: $(ls "$LIBDIR" | grep -c '\.a$') static, $(ls "$LIBDIR" | grep -c '\.so$') shared"
n=0
for f in "$LIBDIR"/*.a "$LIBDIR"/*.so; do
  b=/workspaces/k14/out/android-arm64-v8a/Release/$(basename "$f")
  [ -f "$b" ] && cmp -s "$b" "$f" && ln -f "$b" "$f" && n=$((n+1))
done
echo "== $n installed files share disk space with the build"; df -h /workspaces | tail -1
cd /workspaces/wwe13-k14 || exit 1
if [ -f /workspaces/k14/thirdparty/imgui/backends/imgui_impl_sdlrenderer3.cpp ]; then
  sed -i 's|${CMAKE_CURRENT_SOURCE_DIR}/../tools/rexglue/thirdparty/imgui|${CMAKE_CURRENT_SOURCE_DIR}/../k14/thirdparty/imgui|' CMakeLists.txt
  echo "== imgui: new SDK copy"
else
  echo "== imgui: old SDK copy"
fi
echo "== SDL: the new SDK puts SDL3 inside librexruntime.so; the launcher uses its headers only"
python3 - <<'PY'
p = "/workspaces/wwe13-k14/CMakeLists.txt"
s = open(p).read()
pairs = [
    ('if(NOT TARGET SDL3::SDL3-shared)\n    message(FATAL_ERROR "Installed SDK does not export SDL3::SDL3-shared")',
     'if(NOT TARGET SDL3::Headers)\n    message(FATAL_ERROR "Installed SDK does not export SDL3::Headers")'),
    ("target_link_libraries(wwe13_launcher_imgui PUBLIC SDL3::SDL3-shared)",
     "target_link_libraries(wwe13_launcher_imgui PUBLIC SDL3::Headers)  # SDL itself: librexruntime.so"),
]
for a, b in pairs:
    if b in s:
        continue
    assert s.count(a) == 1, ("CMakeLists.txt", a[:60], s.count(a))
    s = s.replace(a, b)
open(p, "w").write(s)
print("   CMakeLists.txt ok")
PY
[ $? -eq 0 ] || exit 1
echo "== configure"
if python3 /workspaces/k14port/conf.py "$P"; then
  echo "== game build (long: 633 game files on 2 cores)"
  NINJA_STATUS="[%p %f/%t] " cmake --build out/build/android-arm64 --config Release --target main -j2 -- -k 0 2>&1 \
    | tee /workspaces/gamebuild.log | grep --line-buffered -o '^\[ *[0-9]*% [0-9]*/[0-9]*\]' \
    | awk -F'[][%]' '{p=$2+0; if (p!=last) {print; last=p; fflush()}}'
fi
{
  echo "== configure log"; tail -40 /workspaces/gameconf.log 2>/dev/null
  echo "== game errors: $(grep -c 'error:' /workspaces/gamebuild.log 2>/dev/null)"
  echo "== distinct errors (count, file:line: message)"
  grep 'error:' /workspaces/gamebuild.log 2>/dev/null | sed 's|/workspaces/wwe13-k14/||; s|/workspaces/k14/out/install/android-arm64/|<sdk>/|' \
    | sort | uniq -c | sort -rn | cut -c1-300 | head -150
  echo "== failed files"; grep '^FAILED:' /workspaces/gamebuild.log 2>/dev/null | cut -c1-200
  echo "== link errors"; grep -A3 'undefined symbol' /workspaces/gamebuild.log 2>/dev/null | cut -c1-250 | head -80
} > "$R"
echo "== game errors: $(grep -c 'error:' /workspaces/gamebuild.log 2>/dev/null)"
ls -l out/build/android-arm64/Release/libmain.so out/build/android-arm64/libmain.so 2>/dev/null
df -h /workspaces | tail -1
echo "Report: $R"
