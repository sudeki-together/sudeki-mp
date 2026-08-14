#!/usr/bin/env bash
set -euo pipefail

research_prefix="${SUDEKIMP_WINEPREFIX:-${WINEPREFIX:-${HOME}/Games/sudeki-research-prefix}}"
wine_user="${SUDEKIMP_WINE_USER:-steamuser}"
options="${research_prefix}/drive_c/users/${wine_user}/AppData/Roaming/Sudeki/PlayerOptions.xml"
backup="${options}.sudekimp-backup"
mode="${1:---windowed}"

usage() {
    printf '%s\n' \
        'usage: tools/configure-windowed.sh [--windowed|--fullscreen|--check]' \
        '' \
        '  --windowed   Set Sudeki native FullScreen option to False.' \
        '  --fullscreen Set Sudeki native FullScreen option to True.' \
        '  --check      Print the current native FullScreen option.'
}

case "${mode}" in
    --windowed)
        desired="False"
        ;;
    --fullscreen)
        desired="True"
        ;;
    --check)
        desired=""
        ;;
    --help|-h)
        usage
        exit 0
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac

if [[ ! -f "${options}" ]]; then
    printf 'Sudeki options file is missing: %s\n' "${options}" >&2
    exit 1
fi

utf8_source="$(mktemp "${options}.source.XXXXXX")"
utf8_edited="$(mktemp "${options}.edited.XXXXXX")"
utf16_edited="$(mktemp "${options}.encoded.XXXXXX")"
cleanup() {
    rm -f -- "${utf8_source}" "${utf8_edited}" "${utf16_edited}"
}
trap cleanup EXIT

iconv -f UTF-16 -t UTF-8 "${options}" >"${utf8_source}"
current="$(sed -n -E \
    "/<setting id='FullScreen'>/,/<\\/setting>/ s/.*value='(True|False)'.*/\\1/p" \
    "${utf8_source}")"
if [[ "${current}" != "True" && "${current}" != "False" ]]; then
    printf 'Could not resolve one valid FullScreen value in: %s\n' "${options}" >&2
    exit 1
fi

if [[ -z "${desired}" ]]; then
    printf 'Sudeki FullScreen=%s (%s)\n' "${current}" "${options}"
    exit 0
fi
if [[ "${current}" == "${desired}" ]]; then
    printf 'Sudeki FullScreen already equals %s (%s)\n' "${desired}" "${options}"
    exit 0
fi

if [[ ! -e "${backup}" ]]; then
    cp -p -- "${options}" "${backup}"
    printf 'Saved untouched options backup: %s\n' "${backup}"
fi

sed -E \
    "/<setting id='FullScreen'>/,/<\\/setting>/ s/value='(True|False)'/value='${desired}'/" \
    "${utf8_source}" >"${utf8_edited}"
iconv -f UTF-8 -t UTF-16 "${utf8_edited}" >"${utf16_edited}"
chmod --reference="${options}" "${utf16_edited}"
mv -- "${utf16_edited}" "${options}"

verified="$(iconv -f UTF-16 -t UTF-8 "${options}" | sed -n -E \
    "/<setting id='FullScreen'>/,/<\\/setting>/ s/.*value='(True|False)'.*/\\1/p")"
if [[ "${verified}" != "${desired}" ]]; then
    printf 'Failed to verify FullScreen=%s in: %s\n' "${desired}" "${options}" >&2
    exit 1
fi
printf 'Sudeki FullScreen changed: %s -> %s (%s)\n' \
    "${current}" "${verified}" "${options}"
