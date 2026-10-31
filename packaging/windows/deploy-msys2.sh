#!/usr/bin/env bash
# Builds a self-contained Eye Timer folder from an MSYS2 (MinGW-w64) build.
#
# windeployqt copies Qt's own DLLs and plugins but not the MinGW runtime and
# third-party libraries Qt links against (libstdc++, ICU, zlib, ...), so those
# are collected with ldd.
#
# Usage, from an MSYS2 MINGW64 shell in the repository root:
#   packaging/windows/deploy-msys2.sh [build-dir] [output-dir]
set -euo pipefail

build_dir="${1:-build}"
out_dir="${2:-dist/EyeTimer}"
mingw_bin="${MINGW_PREFIX:-/mingw64}/bin"

rm -rf "$out_dir"
cmake --install "$build_dir" --prefix "$out_dir"
windeployqt6 --no-translations "$out_dir/EyeTimer.exe"

# Repeat until no new DLLs appear, since copied DLLs bring their own deps.
while true; do
    added=0
    while read -r dll; do
        name="$(basename "$dll")"
        if [ ! -e "$out_dir/$name" ]; then
            cp "$dll" "$out_dir/"
            added=1
        fi
    done < <(find "$out_dir" -iname '*.dll' -o -iname '*.exe' | xargs -d '\n' ldd 2>/dev/null \
                 | awk '{print $3}' | grep -i "^${mingw_bin}/" | sort -u)
    [ "$added" -eq 0 ] && break
done

echo "Self-contained build in $out_dir"
