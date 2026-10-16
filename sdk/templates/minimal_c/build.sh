#!/usr/bin/env bash
# Builds myplugin.so (or myplugin.dylib on macOS) standalone.
# No engine build or CMake needed.
set -euo pipefail

SDK="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if [[ "$OSTYPE" == "darwin"* ]]; then
    clang -bundle -undefined dynamic_lookup -O3 plugin.c -I"$SDK/include" -o myplugin.dylib
    echo "[OK] myplugin.dylib built successfully!"
else
    gcc -shared -fPIC -O3 plugin.c -I"$SDK/include" -s -o myplugin.so
    echo "[OK] myplugin.so built successfully!"
fi

echo "Put the library next to the crayon binary or in your project folder, and add:"
echo "  function crayon.config(t) t.plugins.myplugin = true end"
