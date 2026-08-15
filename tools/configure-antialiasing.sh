#!/usr/bin/env bash
set -euo pipefail

research_prefix="${SUDEKIMP_WINEPREFIX:-${WINEPREFIX:-${HOME}/Games/sudeki-research-prefix}}"
wine_user="${SUDEKIMP_WINE_USER:-steamuser}"
options="${research_prefix}/drive_c/users/${wine_user}/AppData/Roaming/Sudeki/PlayerOptions.xml"
mode="${1:---get}"
requested="${2:-}"

usage() {
    printf '%s\n' \
        'usage: tools/configure-antialiasing.sh [--get|--set value]' \
        '' \
        '  --get        Print only the current numeric AntiAliasing value.' \
        '  --set value  Atomically set AntiAliasing to an integer from 0 through 16.'
}

case "${mode}" in
    --get)
        if [[ -n "${requested}" ]]; then
            usage >&2
            exit 2
        fi
        ;;
    --set)
        if [[ ! "${requested}" =~ ^[0-9]+$ ]] || (( requested > 16 )); then
            printf 'Invalid AntiAliasing value: %s (expected 0 through 16)\n' \
                "${requested:-<missing>}" >&2
            exit 2
        fi
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
    "/<setting id='AntiAliasing'>/,/<\\/setting>/ s/.*value='([0-9]+)'.*/\\1/p" \
    "${utf8_source}")"
if [[ ! "${current}" =~ ^[0-9]+$ ]]; then
    printf 'Could not resolve one numeric AntiAliasing value in: %s\n' \
        "${options}" >&2
    exit 1
fi

if [[ "${mode}" == "--get" ]]; then
    printf '%s\n' "${current}"
    exit 0
fi
if [[ "${current}" == "${requested}" ]]; then
    exit 0
fi

sed -E \
    "/<setting id='AntiAliasing'>/,/<\\/setting>/ s/value='[0-9]+'/value='${requested}'/" \
    "${utf8_source}" >"${utf8_edited}"
iconv -f UTF-8 -t UTF-16 "${utf8_edited}" >"${utf16_edited}"
chmod --reference="${options}" "${utf16_edited}"
mv -- "${utf16_edited}" "${options}"

verified="$(iconv -f UTF-16 -t UTF-8 "${options}" | sed -n -E \
    "/<setting id='AntiAliasing'>/,/<\\/setting>/ s/.*value='([0-9]+)'.*/\\1/p")"
if [[ "${verified}" != "${requested}" ]]; then
    printf 'Failed to verify AntiAliasing=%s in: %s\n' \
        "${requested}" "${options}" >&2
    exit 1
fi
