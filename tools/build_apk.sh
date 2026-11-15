#!/usr/bin/env bash
set -euo pipefail
# JAVA_HOME can point to a stale sdkman "current" symlink; pick a JDK that exists (Gradle 8.12 + AGP need 17-21).
for J in /usr/lib/jvm/java-17-openjdk-amd64 /usr/local/sdkman/candidates/java/21.0.13.0+2-ms; do
  if [ -x "$J/bin/javac" ]; then export JAVA_HOME="$J"; break; fi
done
[ -x "${JAVA_HOME:-/nonexistent}/bin/javac" ] || { echo "FAILED: no usable JDK found"; exit 1; }
echo "JAVA_HOME=$JAVA_HOME"
SDK=/workspaces/tools/rexglue
GAME=/workspaces/wwe13-recomp
APKP=$SDK/thirdparty/sdl3/android-project
T=/workspaces/android-sdk/ndk/28.2.13676358/toolchains/llvm/prebuilt/linux-x86_64/bin
PREFIX=$SDK/out/install/android-arm64
fail() { echo "FAILED: $1"; [ -n "${2:-}" ] && tail -15 "$2"; exit 1; }

cd "$SDK"
for C in Release RelWithDebInfo; do
  echo "== SDK build $C"
  cmake --build out/build/android-arm64 --config $C -j4 > /tmp/sdk_$C.log 2>&1 || fail "SDK build $C" /tmp/sdk_$C.log
  echo "== SDK install $C"
  cmake --install out/build/android-arm64 --config $C --prefix "$PREFIX" > /tmp/inst_$C.log 2>&1 || fail "SDK install $C" /tmp/inst_$C.log
done
cmp -s out/android-arm64/Release/librexcore.a "$PREFIX/lib/librexcore.a" || fail "installed librexcore.a is stale"
cmp -s out/android-arm64/RelWithDebInfo/librexcorerd.a "$PREFIX/lib/librexcorerd.a" || fail "installed librexcorerd.a is stale"
echo "== installed SDK libs match the fresh build"

cd "$GAME"
echo "== game build"
rm -f "$GAME/out/build/android-arm64/libmain.so"  # installed libs keep old mtimes; force relink
cmake --build out/build/android-arm64 --config RelWithDebInfo --target main -j2 > /tmp/game.log 2>&1 || fail "game build" /tmp/game.log
grep -E "error|FAILED|Linking CXX shared" /tmp/game.log || true
[ -f "$GAME/out/build/android-arm64/libmain.so" ] || fail "libmain.so was not relinked" /tmp/game.log
grep -q "Linking CXX shared" /tmp/game.log || fail "no link step ran" /tmp/game.log

echo "== strip + package"
cd "$APKP"
$T/llvm-strip --strip-unneeded -o app/src/main/jniLibs/arm64-v8a/libmain.so "$GAME/out/build/android-arm64/libmain.so"
ANDROID_HOME=/workspaces/android-sdk ./gradlew assembleDebug --max-workers=2 > /tmp/gradle.log 2>&1 || fail "gradle" /tmp/gradle.log
grep -E "BUILD" /tmp/gradle.log
ls -l app/build/outputs/apk/debug/app-debug.apk
echo "thermal_in_libmain=$(grep -c 'rate-census-thermal' "$GAME/out/build/android-arm64/libmain.so")"
echo
echo "TERMUX:"
echo "gh codespace ssh -c ubiquitous-system-g57w6vq79xvc9g9x -- 'cat $APKP/app/build/outputs/apk/debug/app-debug.apk' > ~/storage/downloads/WWE13-debug-$(date +%H%M).apk"
