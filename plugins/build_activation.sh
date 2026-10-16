#!/usr/bin/env bash
# Builds the activation plugin (activation.so / activation.dylib) and tools/crayon_keygen standalone.
#
# Usage:
#   ./plugins/build_activation.sh
#   ./plugins/build_activation.sh "your-secret-key"
#   CRAYON_ACTIVATION_SECRET="your-secret-key" ./plugins/build_activation.sh
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$HERE/../build/plugins"
OUT="$HERE/bin"

SECRET="${1:-${CRAYON_ACTIVATION_SECRET:-}}"
EXTRA_FLAGS=()
if [ -n "$SECRET" ]; then
    EXTRA_FLAGS+=("-DCRAYON_ACTIVATION_SECRET=$SECRET")
fi

echo "[*] Configuring plugins..."
GEN=()
command -v ninja >/dev/null 2>&1 && GEN=(-G Ninja)
cmake -S "$HERE" -B "$BUILD" "${GEN[@]}" -DCMAKE_BUILD_TYPE=Release -DCRAYON_PLUGIN_OUTPUT_DIR="$OUT" "${EXTRA_FLAGS[@]}"

JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
echo "[*] Building activation plugin..."
cmake --build "$BUILD" --target activation -j"$JOBS"

for d in "$HERE/../build" "$HERE/.."; do
    if [ -x "$d/crayon" ]; then
        cp -f "$OUT"/activation.so "$OUT"/activation.dylib "$d"/ 2>/dev/null || true
        echo "[OK] Copied activation plugin next to $d/crayon"
    fi
done
echo "[OK] activation in $OUT"
if [ -f "$OUT/tools/crayon_keygen" ]; then
    echo "[OK] crayon_keygen in $OUT/tools/crayon_keygen"
fi
