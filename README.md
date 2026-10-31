# Posthoorn

An email client for Sailfish OS, aiming for the workflow of clients like FairEmail: quick
read/unread handling, folders that are part of the flow, a reader that follows the Silica theme
instead of showing mail on a white page, and PGP.

Posthoorn builds on the system's Qt Messaging Framework (through the `Nemo.Email` QML plugin), the
same backend as the stock Email app. Accounts come from Settings → Accounts, and sync, push (IMAP
IDLE) and sending are done by the messageserver.

Status: spike. It lists accounts, folders and messages and shows a message's body as-is. See
[docs/ROADMAP.md](docs/ROADMAP.md) for where it's going.

## Building

With the Sailfish SDK:

    scripts/deploy.sh [ssh-host]

builds the RPM and copies it to the phone (default host `sailfish`). Install it there with

    devel-su pkcon install-local ./posthoorn-*.rpm

Because the app uses the system's mail APIs it can't go into the Jolla Store (Harbour); it's meant
for SailfishOS:Chum.

`scripts/docker-build.sh [release] [arch]` builds in the community SDK image, as CI does.

## Releasing

Commits follow [Conventional Commits](https://www.conventionalcommits.org/) (`feat:`, `fix:`, `ci:`,
...). After every green build on `main`, CI runs [semantic-release](https://semantic-release.gitbook.io/):
it picks the next version from the commits since the last `vX.Y.Z` tag (`fix:` patch, `feat:` minor,
`BREAKING CHANGE:` major), builds the RPMs for aarch64, armv7hl and i486 with that version, and
creates the tag plus a GitHub release with the notes, the RPMs, `SHA256SUMS` and a build provenance
attestation. It commits nothing; the version in the spec is a placeholder that release builds
replace with the tag's.

### Installing a release from GitHub

Download `posthoorn-<version>-1.<arch>.rpm` for your device (`uname -m`: `aarch64` on recent
devices), and `posthoorn-default-mail-<version>-1.<arch>.rpm` too if Posthoorn takes over from
Jolla's Mail (see below). Check where they came from before installing them:

```sh
sha256sum -c --ignore-missing SHA256SUMS
gh attestation verify posthoorn-<version>-1.aarch64.rpm -R readefries/sfos-posthoorn  # built here, by CI
devel-su pkcon install-local ./posthoorn-<version>-1.aarch64.rpm
```

Packages installed this way don't update by themselves.

## Using it in place of Jolla's Mail

New-mail notifications and `mailto:` links open Jolla's Mail. With the `posthoorn-default-mail`
package they open Posthoorn instead. It conflicts with Jolla's Mail, so that has to go first:

    devel-su pkcon remove jolla-email
    devel-su pkcon install posthoorn-default-mail

Removing Jolla's Mail switches off the mail accounts: switch them on again in Settings → Accounts.
A Sailfish OS upgrade may want Jolla's Mail back; remove `posthoorn-default-mail` for the upgrade
and install it again afterwards. To go back: remove `posthoorn-default-mail`, install `jolla-email`.

## License

AGPL-3.0-or-later, see [LICENSE](LICENSE).
