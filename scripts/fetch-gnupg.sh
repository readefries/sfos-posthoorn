#!/bin/sh
# Downloads the sources of posthoorn-gnupg pinned in gnupg/SHA256SUMS into gnupg/rpm/, next to its
# spec where the SDK looks for them, and checks them against those sums. Files already there are
# only checked.
#
# scripts/update-gnupg.sh changes the pins; this never trusts anything it downloads otherwise.
set -eu

dir=$(cd "$(dirname "$0")/../gnupg" && pwd)
cd "$dir/rpm"

while read -r sum file; do
    [ -n "$file" ] || continue
    [ -f "$file" ] && continue
    # libgpg-error-1.61.tar.bz2 lives in .../libgpg-error/
    project=${file%-*}
    echo "Downloading $file"
    curl -sSfL -o "$file.part" "https://gnupg.org/ftp/gcrypt/$project/$file"
    mv "$file.part" "$file"
done < ../SHA256SUMS

sha256sum -c --quiet ../SHA256SUMS
