#!/usr/bin/env bash
# Non-destructive committed-snapshot mirror; run inside WSL.
# --no-local uses Git transport, avoiding NTFS loose-object enumeration/hardlinks.
set -euo pipefail
if (( $# < 1 || $# > 2 )); then
    printf 'Usage: bash tools/SetupWslWorkspace.sh WINDOWS_REPO [NATIVE_DEST]\n' >&2
    exit 2
fi
source_repo=$(realpath "$1")
destination=$(realpath -m "${2:-$HOME/projects/Aion-Looper}")
case "$destination" in
    /mnt/*|/|"$source_repo") printf 'Destination must be a separate native Linux directory.\n' >&2; exit 2 ;;
esac
git_source() { git --no-optional-locks -c safe.directory="$source_repo" -C "$source_repo" "$@"; }
if [[ $(git_source rev-parse --show-toplevel) != "$source_repo" ]]; then
    printf 'Source must be the repository root.\n' >&2; exit 2
fi
source_commit=$(git_source rev-parse refs/heads/main)
if [[ -e "$destination" ]]; then
    if [[ ! -d "$destination/.git" ]] || [[ $(git -C "$destination" remote get-url windows) != "$source_repo" ]]; then
        printf 'Existing destination is not this managed mirror; no changes made.\n' >&2; exit 2
    fi
    if [[ -n $(git -C "$destination" status --porcelain) ]] || [[ $(git -C "$destination" branch --show-current) != main ]]; then
        printf 'Linux checkout is dirty or not on main; preserve it and resolve manually.\n' >&2; exit 2
    fi
    git -C "$destination" fetch --no-tags windows main
    git -C "$destination" merge --ff-only FETCH_HEAD
else
    mkdir -p "$(dirname "$destination")"
    git --no-optional-locks -c safe.directory="$source_repo" clone --no-local -c core.autocrlf=false --branch main --origin windows "$source_repo" "$destination"
    upstream=$(git_source remote get-url origin)
    git -C "$destination" remote add origin "$upstream"
fi
if [[ $(git -C "$destination" rev-parse HEAD) != "$source_commit" ]]; then
    printf 'Source changed during sync or mirror has local commits; no reset attempted.\n' >&2; exit 2
fi
# Reuse source repositories, not Windows build outputs/toolchain binaries.
for relative in .deps/JUCE .reference/Ascii-Looper; do
    cached="$source_repo/$relative"
    native="$destination/$relative"
    if [[ ! -d "$cached/.git" ]]; then
        printf 'Optional cache absent: %s (normal dependency fetching remains available).\n' "$relative"
        continue
    fi
    expected=$(git --no-optional-locks -c safe.directory="$cached" -C "$cached" rev-parse HEAD)
    # Windows Git may normalize CRLF via its global config, unavailable to WSL.
    # Accept only an actually clean index under one explicit mode; never discard edits.
    cache_autocrlf=false
    if [[ -n $(git --no-optional-locks -c safe.directory="$cached" -c core.autocrlf=false -C "$cached" status --porcelain) ]]; then
        if [[ -z $(git --no-optional-locks -c safe.directory="$cached" -c core.autocrlf=true -C "$cached" status --porcelain) ]]; then
            cache_autocrlf=true
        else
            printf 'Source cache is dirty: %s; no partial copy attempted.\n' "$cached" >&2; exit 2
        fi
    fi
    if [[ -e "$native" ]]; then
        if [[ ! -d "$native/.git" ]] || [[ $(git -C "$native" rev-parse HEAD) != "$expected" ]] || [[ -n $(git -C "$native" status --porcelain) ]]; then
            printf 'Existing native cache differs or is dirty: %s; no reset attempted.\n' "$native" >&2; exit 2
        fi
    else
        mkdir -p "$(dirname "$native")"
        git --no-optional-locks -c safe.directory="$cached" clone --no-local --no-checkout -c core.autocrlf="$cache_autocrlf" "$cached" "$native"
        git -C "$native" checkout --detach "$expected"
    fi
done
printf 'Native WSL checkout: %s\nCommitted snapshot: %s\n' "$destination" "$source_commit"
if [[ -n $(git_source status --porcelain) ]]; then
    printf 'Windows has uncommitted changes: they were NOT copied.\n'
fi
