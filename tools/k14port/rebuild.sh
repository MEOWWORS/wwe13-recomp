#!/usr/bin/env bash
# Rebuild the 2K14 SDK, reinstall it, relink the game and repackage the test APK.
set -u
K=/workspaces/k14; P=$K/out/install/android-arm64
cd $K || exit 1
echo "== SDK build (SDL is rebuilt: a few minutes)"
NINJA_STATUS="[%p %f/%t] " cmake --build out/build/android-arm64 --config Release -j4 2>&1 | tee /workspaces/k14build.log \
  | grep --line-buffered -o '^\[ *[0-9]*% [0-9]*/[0-9]*\]' | awk -F'[][%]' '{p=$2+0; if (p>=last+10 || p==100) {print; last=p; fflush()}}'
grep -q 'FAILED:' /workspaces/k14build.log && { echo "SDK build FAILED"; grep -m5 -A4 'error' /workspaces/k14build.log | cut -c1-250; exit 1; }
echo "== SDK install"
cmake --install out/build/android-arm64 --config Release --prefix $P > /workspaces/k14install.log 2>&1 || { echo "install FAILED"; tail -5 /workspaces/k14install.log; exit 1; }
cp $K/out/linux-amd64/Release/rexglue $P/bin/rexglue   # the install brings the Android converter; codegen runs on the PC
for f in $P/lib/*.a $P/lib/*.so; do b=$K/out/android-arm64-v8a/Release/$(basename $f); [ -f $b ] && cmp -s $b $f && ln -f $b $f; done
echo "OpenGL ES 2 renderer in SDL: $(grep -c 'define SDL_VIDEO_RENDER_OGL_ES2 1' $(find $K/out/build/android-arm64 -name SDL_build_config.h | head -1))"
echo "== game relink"
bash /workspaces/k14port/game.sh > /dev/null 2>&1
tail -n +8 /workspaces/k14step3.txt | grep -v '^== \(failed files\|link errors\)$' | cut -c1-250 | head -20
[ -f /workspaces/wwe13-k14/out/build/android-arm64/libmain.so ] || { echo "libmain.so missing"; exit 1; }
echo "== APK"
bash /workspaces/k14port/apk.sh
