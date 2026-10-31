#!/bin/sh
# Builds the RPM with the Sailfish SDK (tests included) and copies it to the phone.
#
#   scripts/deploy.sh [ssh-host]        default host: sailfish
#   SFOS_TARGET=SailfishOS-5.1.0.11-armv7hl scripts/deploy.sh
#
# Every build gets its own release (time and git commit), so the phone never mistakes it for one
# that's already installed, and `rpm -q posthoorn` there shows which build it runs.
# Works from the host and from inside the sailfish-sdk distrobox.
set -eu

host=${1:-sailfish}
target=${SFOS_TARGET:-SailfishOS-5.1.0.11-aarch64}
repo=$(cd "$(dirname "$0")/.." && pwd)
# sfdk only accepts paths as it sees them in its workspace: /home/..., not /var/home/...
repo=$(printf '%s' "$repo" | sed 's#^/var/home/#/home/#')
build="$repo/build-${target##*-}"
commit=$(git -C "$repo" rev-parse --short HEAD 2>/dev/null || echo unknown)
dirty=$(git -C "$repo" status --porcelain 2>/dev/null | grep -q . && echo .dirty || true)
suffix=".dev$(date +%Y%m%d%H%M%S).g$commit$dirty"

mkdir -p "$build"
rm -f "$build"/RPMS/posthoorn-*.rpm
sfdk_build="cd '$build' && ~/SailfishOS/bin/sfdk -c target=$target build .. -- --define 'dev_suffix $suffix'"
if [ -n "${CONTAINER_ID:-}" ]; then
    sh -c "$sfdk_build"
else
    distrobox enter sailfish-sdk -- sh -c "$sfdk_build"
fi

# posthoorn, and posthoorn-default-mail for those who use it in place of Jolla's Mail.
rpm=$(ls "$build"/RPMS/posthoorn-[0-9]*.rpm)
default_mail=$(ls "$build"/RPMS/posthoorn-default-mail-*.rpm)
scp -q "$rpm" "$default_mail" "$host:"
echo
echo "Copied $(basename "$rpm") and $(basename "$default_mail") to $host."
# Stopping the app afterwards, as a running one keeps the old version.
stop="pkill -f '^/usr/bin/posthoorn'"
# With posthoorn-default-mail installed both have to be updated together: it requires the exact
# same version of posthoorn.
if ssh "$host" rpm -q posthoorn-default-mail >/dev/null 2>&1; then
    echo "On the phone (posthoorn-default-mail is installed, so both):"
    echo "  devel-su pkcon install-local ./$(basename "$rpm") ./$(basename "$default_mail"); $stop"
else
    echo "On the phone:"
    echo "  devel-su pkcon install-local ./$(basename "$rpm"); $stop"
    echo "and, with Jolla's Mail uninstalled, to open mail notifications in Posthoorn:"
    echo "  devel-su pkcon install-local ./$(basename "$rpm") ./$(basename "$default_mail"); $stop"
fi
