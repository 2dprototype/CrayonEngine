#!/usr/bin/env bash
# Builds the Crayon native plugins (plugins/*/) on their own, without building the engine.
#
#   plugins/build.sh               build every plugin
#   plugins/build.sh physics4D     build one plugin
#
# Output: plugins/bin/<name>.so (.dylib on macOS), copied next to the crayon binary
# (build/ and the repo root) when it exists there.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$HERE/../build/plugins"
OUT="$HERE/bin"

if [ ! -f "$BUILD/CMakeCache.txt" ]; then
    echo "[*] Configuring plugins..."
    GEN=()
    command -v ninja >/dev/null 2>&1 && GEN=(-G Ninja)
    cmake -S "$HERE" -B "$BUILD" "${GEN[@]}" -DCMAKE_BUILD_TYPE=Release -DCRAYON_PLUGIN_OUTPUT_DIR="$OUT"
fi

JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
if [ $# -eq 0 ]; then
    echo "[*] Building all plugins..."
    cmake --build "$BUILD" -j"$JOBS"
else
    echo "[*] Building plugin $1..."
    cmake --build "$BUILD" --target "$1" -j"$JOBS"
fi

for d in "$HERE/../build" "$HERE/.."; do
    if [ -x "$d/crayon" ]; then
        cp -f "$OUT"/*.so "$OUT"/*.dylib "$d"/ 2>/dev/null || true
        echo "[OK] Copied plugins next to $d/crayon"
    fi
done
echo "[OK] Plugins in $OUT"
