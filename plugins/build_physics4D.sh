#!/usr/bin/env bash
# Builds the physics4D plugin (physics4D.so / physics4D.dylib) standalone.
#
# Usage:
#   ./plugins/build_physics4D.sh
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
"$HERE/build.sh" physics4D "$@"
