#!/usr/bin/env bash
set -euo pipefail

HOST="${SUDEKIMP_SYNC_HOST:-100.95.93.91}"
PORT="${SUDEKIMP_SYNC_PORT:-18730}"
DEST_HOME="${SUDEKIMP_DEST_HOME:-$HOME}"
RSYNC=(rsync -aH --delete-delay --info=progress2 --human-readable --partial --timeout=30)

pull() {
    local module="$1" destination="$2"
    mkdir -p "$destination"
    echo "[sudekimp-sync] ${module} -> ${destination}"
    "${RSYNC[@]}" "rsync://${HOST}:${PORT}/${module}/" "$destination/"
}

pull sudeki-project "$DEST_HOME/Documents/Projects/sudeki-mp"
pull sudeki-game "$DEST_HOME/Games/SudekiMP"
pull sudeki-research-prefix "$DEST_HOME/Games/sudeki-research-prefix"
pull sudeki-prefix "$DEST_HOME/Games/sudeki-prefix"
pull sudeki-offline-prefix "$DEST_HOME/Games/sudeki-offline-prefix"
pull sudeki-bootstrap-prefix "$DEST_HOME/Games/sudeki-prefix-win32-bootstrap-attempt"
echo "[sudekimp-sync] complete"

if [[ "${1:-}" == "--watch" ]]; then
    while sleep 30; do
        "$0" || true
    done
fi
