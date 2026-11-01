#!/bin/sh
# Brings posthoorn-gnupg up to date with GnuPG's software list (swdb.lst): the versions in its
# spec and the sums in gnupg/SHA256SUMS. The list is only used when its signature is good and made
# by one of the keys in gnupg/release-keys.asc; a new release key has to be added there by hand.
# Prints what changed, nothing when it's current. Used by .github/workflows/gnupg-update.yml.
#
# The GnuPG branch followed is set below; 2.4 is the stable one until 2.6 is released.
set -eu

branch=gnupg24

repo=$(cd "$(dirname "$0")/.." && pwd)
spec="$repo/gnupg/rpm/posthoorn-gnupg.spec"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# versions.gnupg.org has the same list, but only over IPv6, which GitHub's runners lack.
curl -sSf -o "$work/swdb.lst" https://gnupg.org/swdb.lst
curl -sSf -o "$work/swdb.lst.sig" https://gnupg.org/swdb.lst.sig
gpg --batch --quiet --dearmor < "$repo/gnupg/release-keys.asc" > "$work/keys.gpg"
if ! gpgv --quiet --keyring "$work/keys.gpg" "$work/swdb.lst.sig" "$work/swdb.lst" 2> "$work/gpgv.log"; then
    cat "$work/gpgv.log" >&2
    echo "swdb.lst isn't signed by a known GnuPG release key" >&2
    exit 1
fi

value() {
    awk -v key="$1" '$1 == key { print $2 }' "$work/swdb.lst"
}

# Spec macro, swdb.lst name, tarball name.
changed_gnupg=
changed=
: > "$work/SHA256SUMS"
for entry in "libgpg_error_version libgpg_error libgpg-error" "libgcrypt_version libgcrypt libgcrypt" \
             "libassuan_version libassuan libassuan" "libksba_version libksba libksba" \
             "npth_version npth npth" "gnupg_version $branch gnupg"; do
    set -- $entry
    version=$(value "$2_ver")
    sum=$(value "$2_sha2")
    if [ -z "$version" ] || [ -z "$sum" ]; then
        echo "swdb.lst has no $2" >&2
        exit 1
    fi
    echo "$sum  $3-$version.tar.bz2" >> "$work/SHA256SUMS"
    current=$(awk -v macro="$1" '$1 == "%global" && $2 == macro { print $3 }' "$spec")
    if [ "$current" != "$version" ]; then
        echo "$3 $current -> $version"
        sed -i "s/^%global $1 .*/%global $1 $version/" "$spec"
        changed=yes
        [ "$3" = gnupg ] && changed_gnupg=yes
    fi
done

[ -n "$changed" ] || exit 0
cp "$work/SHA256SUMS" "$repo/gnupg/SHA256SUMS"
# A new GnuPG starts at release 1; new libraries alone make a new release of the same GnuPG.
if [ -n "$changed_gnupg" ]; then
    release=1
else
    release=$(( $(awk '$1 == "Release:" { print $2 }' "$spec") + 1 ))
fi
sed -i "s/^Release:.*/Release:    $release/" "$spec"
