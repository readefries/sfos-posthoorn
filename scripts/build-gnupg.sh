#!/bin/sh
# Builds posthoorn-gnupg with the Sailfish SDK, after downloading and checking its sources.
# It's separate from the app, so app builds don't rebuild GnuPG.
#
#   scripts/build-gnupg.sh [target]      default target: SailfishOS-5.1.0.11-aarch64
#
# RPMs end up in build-gnupg-<arch>/RPMS/. Works from the host and inside the sailfish-sdk distrobox.
set -eu

target=${1:-SailfishOS-5.1.0.11-aarch64}
repo=$(cd "$(dirname "$0")/.." && pwd)
"$repo/scripts/fetch-gnupg.sh"
# sfdk only accepts paths as it sees them in its workspace: /home/..., not /var/home/...
repo=$(printf '%s' "$repo" | sed 's#^/var/home/#/home/#')
build="$repo/build-gnupg-${target##*-}"

mkdir -p "$build"
rm -f "$build"/RPMS/posthoorn-gnupg-*.rpm
# no-fix-version: the version is GnuPG's, not one from this repository's tags.
sfdk_build="cd '$build' && ~/SailfishOS/bin/sfdk -c target=$target -c no-fix-version build ../gnupg"
if [ -n "${CONTAINER_ID:-}" ]; then
    sh -c "$sfdk_build"
else
    distrobox enter sailfish-sdk -- sh -c "$sfdk_build"
fi
ls "$build"/RPMS/posthoorn-gnupg-*.rpm
