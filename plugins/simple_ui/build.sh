#!/usr/bin/env bash
# Builds simple_ui plugin standalone from inside plugins/simple_ui/.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${HERE}/../build_simple_ui.sh" "$@"
