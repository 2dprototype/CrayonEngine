#!/usr/bin/env bash
# Builds the simple_ui plugin (libsimple_ui.so / simple_ui.dylib) standalone.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="${HERE}/../build/plugins"
OUT="${HERE}/bin"

echo "[*] Configuring plugins..."
cmake -S "${HERE}" -B "${BUILD}" -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCRAYON_PLUGIN_OUTPUT_DIR="${OUT}"

echo "[*] Building simpleUI plugin..."
cmake --build "${BUILD}" --target simpleUI -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

for D in "${HERE}/../build" "${HERE}/.."; do
    if [ -f "${D}/crayon" ]; then
        cp -f "${OUT}"/*simpleUI* "${D}/" 2>/dev/null || true
        echo "[OK] Copied simpleUI plugin next to ${D}/crayon"
    fi
done

echo "[OK] simpleUI plugin built in ${OUT}"
