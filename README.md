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

## License

AGPL-3.0-or-later, see [LICENSE](LICENSE).
