#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
linux_launcher="${project_dir}/tools/sudekimp-beta-launcher.sh"
windows_launcher="${project_dir}/src/launcher/beta_launcher.c"
manifest="${project_dir}/public/launcher-manifest.txt"
license="${project_dir}/LICENSE"
notice="${project_dir}/NOTICE"

for required in \
    'Local co-op — 2 players' \
    'Local co-op — 3 players' \
    'LAN arena — Host as Tal' \
    'LAN arena — Join as Ailish' \
    'Cleanroom' \
    'Enable cleanroom sandbox tools (F8)' \
    'Stop Sudeki' \
    'View recent log' \
    'Export support logs' \
    'Check for updates'; do
    grep -Fq -- "${required}" "${linux_launcher}"
done

for required in \
    'Local co-op (2 players)' \
    'LAN arena host — Tal' \
    'LAN arena client — Ailish' \
    'Cleanroom' \
    'Enable cleanroom sandbox tools (F8)' \
    'stop_tracked_sudeki' \
    'view_runtime_log' \
    'export_support_logs' \
    'check_for_launcher_update'; do
    grep -Fq -- "${required}" "${windows_launcher}"
done

if grep -Fq -- '--talos' "${linux_launcher}"; then
    printf '%s\n' 'Linux public launcher unexpectedly exposes a Talos profile.' >&2
    exit 1
fi
if grep -Fq -- 'EnableTalos' "${windows_launcher}"; then
    printf '%s\n' 'Windows public launcher unexpectedly exposes a Talos profile.' >&2
    exit 1
fi

grep -Fxq 'version=0.4.0' "${manifest}"
grep -Fxq 'policy=prompt_only_never_silent' "${manifest}"
grep -Fq 'automatic_upload=false' "${windows_launcher}"
grep -Fq 'Automatic upload is intentionally not enabled yet' "${linux_launcher}"
grep -Fq 'SUDEKIMP_CLEANROOM_TOOLS' "${linux_launcher}"
grep -Fq 'EnableCleanroomMenu' "${windows_launcher}"
grep -Fq 'GNU AFFERO GENERAL PUBLIC LICENSE' "${license}"
grep -Fq 'Version 3, 19 November 2007' "${license}"
grep -Fq 'SudekiMP was originally created by wander.' "${notice}"
grep -Fq 'AGPL-3.0-or-later' "${notice}"
grep -Fq '"${project_dir}/LICENSE" "${project_dir}/NOTICE"' \
    "${project_dir}/tools/build-launcher-packages.sh"
grep -Fq "(Join-Path \$env:GITHUB_WORKSPACE 'LICENSE')" \
    "${project_dir}/.gitea/workflows/windows-build.yml"

printf '%s\n' 'launcher surface checks passed'
