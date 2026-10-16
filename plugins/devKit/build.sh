#!/usr/bin/env bash
# Builds devKit plugin standalone from inside plugins/devKit/.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${HERE}/../build_devKit.sh" "$@"
