#!/bin/sh
# Print the release notes for a tag on stdout.
#
# Usage: release-notes.sh <tag> [changelog]
#
# The notes come from .github/release-notes/<tag>.md when that file exists,
# otherwise from the matching "## [x.y.z]" section of CHANGELOG.md.  Nothing
# is printed when neither source has content, so the caller can fall back to
# GitHub's generated notes.
set -eu

tag=${1:-}
changelog=${2:-CHANGELOG.md}

if [ -z "$tag" ]; then
	echo "usage: $0 <tag> [changelog]" >&2
	exit 2
fi

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
notes_file="$script_dir/../release-notes/$tag.md"

if [ -f "$notes_file" ]; then
	notes=$(cat "$notes_file")
else
	version=${tag#v}
	notes=$(
		awk -v ver="$version" '
			index($0, "## [" ver "]") == 1 { inside = 1; next }
			inside && index($0, "## [") == 1 { exit }
			inside { print }
		' "$changelog" 2>/dev/null || true
	)
fi

# Command substitution already stripped the trailing newlines; drop the
# leading ones too so the Markdown starts with a heading.
notes=$(printf '%s\n' "$notes" | sed -e '/./,$!d')

if [ -n "$notes" ]; then
	printf '%s\n' "$notes"
fi
