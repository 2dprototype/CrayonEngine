#!/usr/bin/env bash
# Builds the assets plugin (assets.so / assets.dylib) standalone.
#
# Usage:
#   ./plugins/build_assets.sh
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$HERE/../build/plugins"
OUT="$HERE/bin"

echo "[*] Configuring plugins..."
GEN=()
command -v ninja >/dev/null 2>&1 && GEN=(-G Ninja)
cmake -S "$HERE" -B "$BUILD" "${GEN[@]}" -DCMAKE_BUILD_TYPE=Release -DCRAYON_PLUGIN_OUTPUT_DIR="$OUT"

JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
echo "[*] Building assets plugin..."
cmake --build "$BUILD" --target assets -j"$JOBS"

for d in "$HERE/../build" "$HERE/.."; do
    if [ -x "$d/crayon" ]; then
        cp -f "$OUT"/assets.so "$OUT"/assets.dylib "$d"/ 2>/dev/null || true
        echo "[OK] Copied assets plugin next to $d/crayon"
    fi
done
echo "[OK] assets in $OUT"
