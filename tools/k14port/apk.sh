#!/usr/bin/env bash
# Test APK for the 2K14 SDK build: its own app id (installs next to the current WWE13), SDL inside librexruntime.so.
set -u
for J in /usr/lib/jvm/java-17-openjdk-amd64 /usr/local/sdkman/candidates/java/21.0.13.0+2-ms; do
  if [ -x "$J/bin/javac" ]; then export JAVA_HOME="$J"; break; fi
done
[ -x "${JAVA_HOME:-/nonexistent}/bin/javac" ] || { echo "FAILED: no usable JDK found"; exit 1; }
OLD=/workspaces/tools/rexglue/thirdparty/sdl3/android-project
APKP=/workspaces/apk-k14
LIB=/workspaces/k14/out/install/android-arm64/lib
MAIN=/workspaces/wwe13-k14/out/build/android-arm64/libmain.so
T=/workspaces/android-sdk/ndk/28.2.13676358/toolchains/llvm/prebuilt/linux-x86_64/bin
[ -f "$MAIN" ] || { echo "FAILED: libmain.so is not built"; exit 1; }

echo "== project copy (no build outputs)"
mkdir -p "$APKP"
rm -rf "$APKP/app/src/main/jniLibs"
(cd "$OLD" && tar cf - --exclude='./app/build' --exclude='./build' --exclude='./.gradle' --exclude='./app/src/main/jniLibs' .) \
  | (cd "$APKP" && tar xf -) || exit 1
J="$APKP/app/src/main/jniLibs/arm64-v8a"
mkdir -p "$J"

N=$(( $(cat /workspaces/k14port/apk_build_no 2>/dev/null || echo 0) + 1 )); echo $N > /workspaces/k14port/apk_build_no
NAME="WWE13-2K14SDK-b$N"
echo "== $NAME: app id, name, libraries"
G=$(ls "$APKP"/app/build.gradle* | head -1)
sed -i 's/applicationId "com.catonix.wwe13"/applicationId "com.catonix.wwe13.k14"/' "$G"
grep -q 'applicationId "com.catonix.wwe13.k14"' "$G" || { echo "FAILED: app id not changed in $G"; exit 1; }
S="$APKP/app/src/main/java/org/libsdl/app/SDLActivity.java"
# SDL3 lives inside librexruntime.so in this SDK: load it first, then the game.
sed -i '/protected String\[\] getLibraries() {/,/};/ s/"SDL3",/"rexruntime",/' "$S"
grep -q '"rexruntime",' "$S" || { echo "FAILED: SDLActivity library list not changed"; exit 1; }
python3 /workspaces/k14port/crashlog.py "$S" || exit 1
for f in $(grep -rl '<string name="app_name">' "$APKP/app/src/main/res" 2>/dev/null); do
  sed -i 's|<string name="app_name">\([^<]*\)</string>|<string name="app_name">\1 (2K14 b'$N')</string>|' "$f"
done
grep -rh '<string name="app_name">' "$APKP/app/src/main/res" | head -1

echo "== native libraries"
$T/llvm-strip --strip-unneeded -o "$J/libmain.so" "$MAIN" || exit 1
need() { readelf -d "$1" | sed -n 's/.*Shared library: \[\(.*\)\]/\1/p'; }
# librexgpu-xenos.so is the GPU plugin: loaded by name at startup, so no library lists it as a dependency.
todo="librexruntime.so librexgpu-xenos.so $(need "$MAIN")"
done_list=""
while [ -n "$todo" ]; do
  set -- $todo; l=$1; shift; todo="$*"
  case " $done_list " in *" $l "*) continue;; esac
  done_list="$done_list $l"
  [ -f "$LIB/$l" ] || continue          # a system library (libc, libvulkan, ...)
  $T/llvm-strip --strip-unneeded -o "$J/$l" "$LIB/$l" || exit 1
  todo="$todo $(need "$LIB/$l")"
done
ls -l "$J" | awk 'NR>1 {printf "   %-28s %6.1f MB\n", $9, $5/1048576}'

echo "== gradle"
cd "$APKP" && ANDROID_HOME=/workspaces/android-sdk ./gradlew assembleDebug --max-workers=2 > /workspaces/gradle-k14.log 2>&1 \
  || { echo "FAILED: gradle"; tail -20 /workspaces/gradle-k14.log; exit 1; }
grep -E "BUILD" /workspaces/gradle-k14.log
rm -f /workspaces/WWE13-2K14SDK-b*.apk /workspaces/WWE13-k14-test.apk
cp app/build/outputs/apk/debug/app-debug.apk /workspaces/$NAME.apk && ls -l /workspaces/$NAME.apk
df -h /workspaces | tail -1
echo
echo "TERMUX:"
echo "gh codespace ssh -c ubiquitous-system-g57w6vq79xvc9g9x -- 'cat /workspaces/$NAME.apk' > ~/storage/downloads/$NAME.apk"
