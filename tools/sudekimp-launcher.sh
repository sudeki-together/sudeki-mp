#!/usr/bin/env bash
set -euo pipefail

# Canonical public entry point. Keep the former beta filename as a compatible
# implementation path for existing desktop shortcuts and documentation links.
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec "${script_dir}/sudekimp-beta-launcher.sh" "$@"
