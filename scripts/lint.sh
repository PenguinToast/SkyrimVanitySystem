#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

MODE="releasedbg"
declare -a TARGETS=()
PROJECT_SOURCE_DIRS=("src")

while (($#)); do
    case "$1" in
        release|debug|releasedbg)
            MODE="$1"
            ;;
        *)
            TARGETS+=("$1")
            ;;
    esac
    shift
done

if ! command -v powershell.exe >/dev/null 2>&1; then
    echo "powershell.exe is required to generate compile_commands.json from WSL." >&2
    exit 1
fi

if ! command -v wslpath >/dev/null 2>&1; then
    echo "wslpath is required to convert repo paths for Windows tools." >&2
    exit 1
fi

find_clang_tidy() {
    local tool="/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/clang-tidy.exe"
    if [[ -x "$tool" ]]; then
        printf '%s\n' "$tool"
        return
    fi

    if command -v clang-tidy >/dev/null 2>&1; then
        command -v clang-tidy
        return
    fi

    tool="/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/bin/clang-tidy.exe"
    if [[ -x "$tool" ]]; then
        printf '%s\n' "$tool"
        return
    fi

    echo "clang-tidy was not found in PATH or the default Visual Studio LLVM install." >&2
    exit 1
}

to_tool_path() {
    local tool="$1"
    local path="$2"
    if [[ "$tool" == *.exe ]]; then
        wslpath -w "$path"
    else
        printf '%s\n' "$path"
    fi
}

collect_default_targets() {
    local source_dir
    for source_dir in "${PROJECT_SOURCE_DIRS[@]}"; do
        local abs="${REPO_ROOT}/${source_dir}"
        [[ -d "$abs" ]] || continue
        find "$abs" -type f -name '*.cpp' -print0
    done
}

is_project_path() {
    local abs="$1"
    local source_dir
    for source_dir in "${PROJECT_SOURCE_DIRS[@]}"; do
        local root="${REPO_ROOT}/${source_dir}"
        [[ -d "$root" ]] || continue
        root="$(realpath "$root")"
        if [[ "$abs" == "$root" || "$abs" == "$root"/* ]]; then
            return 0
        fi
    done

    return 1
}

collect_explicit_targets() {
    local target
    for target in "${TARGETS[@]}"; do
        local abs
        if [[ "$target" == /* ]]; then
            abs="$(realpath "$target")"
        else
            abs="$(realpath "${REPO_ROOT}/${target}")"
        fi
        if ! is_project_path "$abs"; then
            echo "Refusing to lint outside project source roots: ${target}" >&2
            echo "Allowed roots: ${PROJECT_SOURCE_DIRS[*]}" >&2
            exit 1
        fi
        if [[ -d "$abs" ]]; then
            find "$abs" -type f -name '*.cpp' -print0
        elif [[ -f "$abs" ]]; then
            printf '%s\0' "$abs"
        else
            echo "Path not found: ${target}" >&2
            exit 1
        fi
    done
}

regex_escape() {
    sed -e 's/[.[\*^$()+?{}|]/\\&/g' -e 's/\\/\\\\/g' <<<"$1"
}

build_header_filter() {
    local -a roots=()
    local source_dir
    for source_dir in "${PROJECT_SOURCE_DIRS[@]}"; do
        local abs="${REPO_ROOT}/${source_dir}"
        [[ -d "$abs" ]] || continue
        roots+=("$(regex_escape "$(to_tool_path "$CLANG_TIDY" "$abs")")([\\\\/].*)?")
    done

    local IFS='|'
    printf '^(%s)$' "${roots[*]}"
}

declare -a FILES=()
TARGET_LIST="$(mktemp)"
trap 'rm -f "$TARGET_LIST"' EXIT

if ((${#TARGETS[@]})); then
    collect_explicit_targets >"$TARGET_LIST"
else
    collect_default_targets >"$TARGET_LIST"
fi

while IFS= read -r -d '' file; do
    FILES+=("$file")
done <"$TARGET_LIST"

if ((${#FILES[@]} == 0)); then
    echo "No source files found." >&2
    exit 1
fi

WIN_REPO_ROOT="$(wslpath -w "$REPO_ROOT")"
POWERSHELL_CMD="
\$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath '$WIN_REPO_ROOT'
xmake f -y -c
xmake f -y -m '$MODE'
xmake project -k compile_commands
"
powershell.exe -NoProfile -Command "$POWERSHELL_CMD" >/dev/null

CLANG_TIDY="$(find_clang_tidy)"
PROJECT_PATH="$(to_tool_path "$CLANG_TIDY" "$REPO_ROOT")"
HEADER_FILTER="$(build_header_filter)"
for file in "${FILES[@]}"; do
    tool_file="$(to_tool_path "$CLANG_TIDY" "$file")"
    "$CLANG_TIDY" \
        --quiet \
        -p "$PROJECT_PATH" \
        --header-filter="$HEADER_FILTER" \
        --extra-arg-before=/Y- \
        "$tool_file"
done

echo "Processed ${#FILES[@]} translation unit(s) with clang-tidy"
