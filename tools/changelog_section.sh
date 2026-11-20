#!/bin/sh
# Prints the body of one version's section from CHANGELOG.md (Keep a
# Changelog format), without its "## [X.Y.Z] - date" heading.
# Exits 1 when the section is missing or empty.
#
# Usage: tools/changelog_section.sh 0.10.0 [CHANGELOG.md]
set -eu

version="$1"
file="${2:-CHANGELOG.md}"

awk -v version="$version" '
    BEGIN { heading = "## [" version "]" }
    index($0, heading) == 1 { found = 1; next }
    found && /^## \[/ { exit }
    found { lines[++count] = $0 }
    END {
        # Trim blank lines at both ends.
        first = 1
        while (first <= count && lines[first] ~ /^[[:space:]]*$/) first++
        last = count
        while (last >= first && lines[last] ~ /^[[:space:]]*$/) last--
        if (!found || first > last) exit 1
        for (i = first; i <= last; i++) print lines[i]
    }
' "$file"
