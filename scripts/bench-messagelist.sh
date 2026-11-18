#!/bin/sh
# Builds tests/bench (UnifiedMessageListModel on a large folder) in the community SDK image and
# runs it on the phone, in a store of its own: the phone's mail is never touched.
#
#   scripts/bench-messagelist.sh [ssh-host] [messages]     default: sailfish 15000
#
# The first run fills the store (~/.cache/posthoorn-bench on the phone); later runs reuse it.
set -eu

host=${1:-sailfish}
messages=${2:-15000}
release=5.1.0.11
arch=aarch64
source_dir=$(cd "$(dirname "$0")/.." && pwd)
output="$source_dir/build-docker/bench"
mkdir -p "$output"
chmod 777 "$output"

docker run --rm -v "$source_dir":/source:ro -v "$output":/output \
    "coderus/sailfishos-platform-sdk-$arch:$release" bash -ec "
        mkdir ~/src ~/build
        tar -C /source --exclude=./.git --exclude='./build*' -cf - . | tar -C ~/src -xf -
        cd ~/build
        sb2 -t SailfishOS-$release-$arch /usr/lib64/qt5/bin/qmake ~/src/tests/bench/bench.pro
        sb2 -t SailfishOS-$release-$arch make -j4
        cp bench-messagelist /output/
    "

scp -q "$output/bench-messagelist" "$host:"
# accounts-glib complains that it can't open the accounts database, which isn't needed.
ssh "$host" "mkdir -p ~/.cache/posthoorn-bench && QMF_DATA=\$HOME/.cache/posthoorn-bench ./bench-messagelist $messages 2>&1 | grep -v -E 'accounts-glib|GLib-CRITICAL|^\$'"
