#!/usr/bin/env bash
# One-time setup of the two Android dependencies that can't ship inside the repo:
#   1. SDL3 source   -> vendor/sdl3-src            (libSDL3.so + Java glue are built from it)
#   2. LuaJIT static -> vendor/android/<abi>/lib/libluajit-5.1.a
# Everything else (Jolt, box2d, hypervis4d, GLES loader) is compiled by CMake / already in vendor/.
#
# Run from Linux, macOS or WSL (LuaJIT's cross build needs a POSIX shell + host gcc + make).
#   ABIS="arm64-v8a" ./android/setup_deps.sh        # default
#   ABIS="arm64-v8a x86_64" ./android/setup_deps.sh # emulator too
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ABIS="${ABIS:-arm64-v8a}"
API="${API:-24}"
SDL_TAG="${SDL_TAG:-release-3.4.16}"     # keep equal to vendor/sdl3 (desktop) version
LUAJIT_REF="${LUAJIT_REF:-v2.1}"

# ---- 1. SDL3 source ---------------------------------------------------------
if [ ! -d "$ROOT/vendor/sdl3-src" ]; then
    echo ">> cloning SDL3 ($SDL_TAG)"
    git clone --depth 1 --branch "$SDL_TAG" https://github.com/libsdl-org/SDL "$ROOT/vendor/sdl3-src"
else
    echo ">> vendor/sdl3-src already present"
fi

# ---- 2. Locate the NDK ------------------------------------------------------
NDK="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}"
SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
if [ -z "$NDK" ] && [ -n "$SDK" ] && ls "$SDK"/ndk/* >/dev/null 2>&1; then
    NDK="$(ls -d "$SDK"/ndk/* | sort -V | tail -1)"
fi
if [ -z "$NDK" ] || [ ! -d "$NDK" ]; then
    echo "Android NDK not found. Install NDK r26+ and set ANDROID_NDK_HOME." >&2
    exit 1
fi
case "$(uname -s)" in
    Linux)  HOST_TAG=linux-x86_64 ;;
    Darwin) HOST_TAG=darwin-x86_64 ;;
    MINGW*|MSYS*) HOST_TAG=windows-x86_64 ;;
    *) echo "Run this script from Linux, macOS or WSL." >&2; exit 1 ;;
esac
TC="$NDK/toolchains/llvm/prebuilt/$HOST_TAG/bin"
echo ">> NDK: $NDK"

MAKE_CMD="make"
if ! command -v make >/dev/null 2>&1; then
    if command -v mingw32-make >/dev/null 2>&1; then
        MAKE_CMD="mingw32-make"
    fi
fi


# ---- 3. LuaJIT (static, position-independent) per ABI -----------------------
LJ="$ROOT/vendor/luajit-src"
if [ ! -d "$LJ" ]; then
    echo ">> cloning LuaJIT ($LUAJIT_REF)"
    git clone --depth 1 --branch "$LUAJIT_REF" https://github.com/LuaJIT/LuaJIT "$LJ"
fi

for ABI in $ABIS; do
    case "$ABI" in
        arm64-v8a)   TRIPLE=aarch64-linux-android;    HOST_CC="gcc" ;;
        x86_64)      TRIPLE=x86_64-linux-android;     HOST_CC="gcc" ;;
        armeabi-v7a) TRIPLE=armv7a-linux-androideabi; HOST_CC="gcc -m32" ;;  # needs gcc-multilib
        *) echo "Unsupported ABI: $ABI" >&2; exit 1 ;;
    esac
    CLANG="$TC/${TRIPLE}${API}-clang"
    OUT="$ROOT/vendor/android/$ABI/lib"
    mkdir -p "$OUT"
    echo ">> building LuaJIT for $ABI"
    CORES=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo "${NUMBER_OF_PROCESSORS:-4}")
    AR_BIN="$TC/llvm-ar"
    [ -f "$TC/llvm-ar.exe" ] && AR_BIN="$TC/llvm-ar.exe"
    STRIP_BIN="$TC/llvm-strip"
    [ -f "$TC/llvm-strip.exe" ] && STRIP_BIN="$TC/llvm-strip.exe"

    ( cd "$LJ" && "$MAKE_CMD" clean >/dev/null 2>&1 || true
      "$MAKE_CMD" -j"$CORES" BUILDMODE=static TARGET_SYS=Linux \
          HOST_CC="$HOST_CC" \
          STATIC_CC="$CLANG -fPIC" DYNAMIC_CC="$CLANG -fPIC" TARGET_LD="$CLANG" \
          TARGET_AR="$AR_BIN rcus" TARGET_STRIP="$STRIP_BIN" )
    cp "$LJ/src/libluajit.a" "$OUT/libluajit-5.1.a"
    echo "   -> $OUT/libluajit-5.1.a"
done

echo
echo "Done. Next:  cd android && gradle wrapper && ./gradlew installDebug"
