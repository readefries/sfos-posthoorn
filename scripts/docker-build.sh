#!/bin/sh
# Builds the RPM and runs the tests in a community Sailfish OS SDK image
# (https://hub.docker.com/r/coderus/sailfishos-platform-sdk), the same way CI does.
#
#   scripts/docker-build.sh [release] [arch] [gnupg]     default: 5.1.0.11 aarch64
#
# With "gnupg" it builds posthoorn-gnupg instead of the app, after downloading and checking its
# sources (scripts/fetch-gnupg.sh). RPMs end up in build-docker/<release>-<arch>[-gnupg]/. Extra `docker run` options can be given in
# DOCKER_RUN_ARGS; CI needs --privileged for the SDK's scratchbox2 (see .github/workflows).
# RPM_VERSION overrides the spec's placeholder version, for release builds.
set -eu

release=${1:-5.1.0.11}
version=${RPM_VERSION:-}
case "$version" in
    ''|[0-9]*.[0-9]*.[0-9]*) ;;
    *) echo "RPM_VERSION must look like 1.2.3, not '$version'" >&2; exit 2 ;;
esac
arch=${2:-aarch64}
project=${3:-}
source_dir=$(cd "$(dirname "$0")/.." && pwd)
output="$source_dir/build-docker/$release-$arch${project:+-$project}"
# The app is the repository; posthoorn-gnupg is gnupg/, with its own spec.
project_dir=src
if [ "$project" = gnupg ]; then
    "$source_dir/scripts/fetch-gnupg.sh"
    project_dir=src/gnupg
    version=
fi

mkdir -p "$output"
# Only this build's RPMs, so nothing stale ever gets picked up (e.g. attached to a release).
rm -f "$output"/*.rpm
# The image builds as mersdk (uid 100000).
chmod 777 "$output"

# shellcheck disable=SC2086 # DOCKER_RUN_ARGS is a list of options
docker run --rm ${DOCKER_RUN_ARGS:-} \
    -v "$source_dir":/source:ro \
    -v "$output":/output \
    "coderus/sailfishos-platform-sdk-$arch:$release" \
    bash -ec "
        mkdir ~/src ~/build
        tar -C /source --exclude=./.git --exclude='./build*' -cf - . | tar -C ~/src -xf -
        if [ -n '$version' ]; then
            sed -i 's/^Version:.*/Version:    $version/' ~/src/rpm/posthoorn.spec
        fi
        cd ~/build
        mb2 -t SailfishOS-$release-$arch build ~/$project_dir
        cp RPMS/*.rpm /output/
    "

ls "$output"
