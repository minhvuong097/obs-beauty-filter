#!/bin/sh
# Verify that every place carrying the plugin version agrees.
#
# Usage: check-version.sh [tag]
#
# CMakeLists.txt is the single source of truth.  The installer, both READMEs
# and CHANGELOG.md must match it; when a tag is given (v1.10.0) the tag must
# match as well.  Runs in CI on every push and pull request so a forgotten
# version bump fails before a release is published.
set -eu

tag=${1:-}
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$root"

version=$(sed -n 's/^set(BEAUTY_FILTER_VERSION "\(.*\)")$/\1/p' CMakeLists.txt)
if [ -z "$version" ]; then
	echo "CMakeLists.txt: BEAUTY_FILTER_VERSION not found" >&2
	exit 1
fi

if [ -n "$tag" ]; then
	expected=${tag#v}
	if [ "$expected" != "$version" ]; then
		echo "tag $tag does not match CMakeLists.txt version $version" >&2
		exit 1
	fi
fi

fail=0
check() {
	# file, fixed string, description
	if ! grep -Fq "$2" "$1"; then
		echo "MISSING: $1: $3" >&2
		fail=1
	fi
}

check installer/obs-beauty-filter.iss "#define PluginVersion \"$version\"" \
	"installer version $version"
check README.md "Phiên bản **$version**" "README version $version"
check README.en.md "Version **$version**" "README.en version $version"
check README.md "obs-beauty-filter-$version-windows-x64-setup.exe" \
	"README download name $version"
check README.en.md "obs-beauty-filter-$version-windows-x64-setup.exe" \
	"README.en download name $version"

if ! grep -Fq "## [$version]" CHANGELOG.md; then
	echo "MISSING: CHANGELOG.md: section ## [$version]" >&2
	fail=1
fi

if [ "$fail" -ne 0 ]; then
	exit 1
fi

if [ -n "$tag" ]; then
	echo "version $version matches $tag, installer, READMEs and CHANGELOG.md"
else
	echo "version $version is consistent across installer, READMEs and CHANGELOG.md"
fi
