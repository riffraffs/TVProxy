#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=tool/env.sh
source "$ROOT/tool/env.sh"
exec "$GRADLE_HOME/bin/gradle" "$@"
