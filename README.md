# Posthoorn

An email client for Sailfish OS with the workflow of clients like FairEmail: quick read/unread
handling, archiving and folders that are part of the flow, and a reader that follows the Silica
theme instead of showing mail on a white page.

Posthoorn builds on the system's Qt Messaging Framework (QMF, through the `Nemo.Email` QML plugin),
the same backend as Jolla's Mail. Accounts come from Settings → Accounts, and sync, push (IMAP IDLE)
and sending are done by the messageserver. It can be used next to Jolla's Mail, or
[in its place](#using-it-in-place-of-jollas-mail).

## What it does

**Reading**

- Every mail, HTML or plain text, is reduced to its structure (paragraphs, emphasis, links, lists,
  headings, quote levels) and drawn in the theme's font and colours. The sender's HTML is never
  shown; when an HTML part has nothing readable, the text part is shown instead.
- No remote images until asked for, per mail; tracking images never. Links are cleaned of
  redirectors (Outlook Safe Links, Google, Facebook, Proofpoint) and tracking parameters, and show
  where they go before opening.
- Attachments open and save; mail text is selectable, and replying to a selection quotes only that.

**Sorting**

- A unified inbox of any folders of any accounts (the inboxes of all accounts by default),
  favourite folders on the start page, and switching folders from a folder's title. The app opens
  where it was left.
- Conversations: a mail and its replies as one row, opening into all of them, from any folder,
  Sent included. Can be switched off in Settings.
- Per mail or conversation (long-press) or for a selection: archive, snooze, mark read or unread,
  star, move, delete. Archiving moves mail to its account's archive folder, found by name or
  chosen in Settings. Stars are the server's "flagged". "Mark all as read" for a folder or the
  unified inbox.
- Snoozing hides mail until later today, this evening, tomorrow morning, the weekend, next week
  or a chosen time, then brings it back unread. It's kept on the phone; the mail stays where it
  is on the server.
- Virtual folders per account, which can be favourites too: Starred, all starred mail wherever
  it is, and Snoozed, what's waiting to come back.
- Search through all mail on the phone, in the subject, sender and recipients and optionally the
  text, with filters for unread, starred and attachments, and on the servers too.

**Writing**

- New mail, reply, reply all and forward, from any of the accounts' addresses and aliases, with
  recipient suggestions from Contacts. Quotes above or below the reply, or none; the account's
  signature; attachments, also the original's when forwarding.
- Drafts are saved when leaving and can be continued. Mail goes out as `format=flowed` plain text.
- Sending can be undone for a few seconds; the mail then opens in the composer again.

**On the phone**

- The cover shows how much is unread in the unified inbox and the newest unread mails, with New
  mail and Sync buttons.
- With `posthoorn-default-mail`, new-mail notifications and `mailto:` links open Posthoorn.

Planned, among others: PGP and moving mail between accounts. See [docs/ROADMAP.md](docs/ROADMAP.md)
for those and the decisions made so far.

## Installing

Releases are on [GitHub](https://github.com/readefries/sfos-posthoorn/releases), see
[Installing a release from GitHub](#installing-a-release-from-github). Posthoorn needs Sailfish OS
5.1 or later.

It can't go into the Jolla Store (Harbour): QMF keeps mail accounts in the system accounts
database, which only Privileged apps can open. It's meant for SailfishOS:Chum.

## Layout

| Path    | What |
|---------|------|
| `core/` | Static library without UI, with unit tests: reducing HTML and plain text to structure (`mailcontent`), cleaning links (`linkcleaner`), `format=flowed` (`flowedtext`), mail addresses, splitting drafts (`maildraft`) and recognising archive folders. Qt Core only. |
| `app/src/` | Glue to QMF and QML: the message list model (folders, unified inbox, search), building outgoing mail, starring, archive folders, saved state, and `MailUiService`, the `com.jolla.email.ui` D-Bus service. |
| `app/qml/` | The Silica UI. |
| `app/defaultmail/` | The files of `posthoorn-default-mail`: the D-Bus service file, the sailjail profile allowing the name, and the `mailto:` handler. |
| `tests/` | Qt Test suite for `core`, run by the RPM build's `%check`. |

Sailfish OS ships Qt 5.6: don't use newer Qt APIs.

## Building

With the Sailfish SDK:

    scripts/deploy.sh [ssh-host]

builds the RPMs, running the tests, and copies them to the phone (default host `sailfish`). It
prints the line to install them with; each build gets its own release (`…dev<time>.g<commit>`), so
`pkcon install-local` never skips one as already installed.

`scripts/docker-build.sh [release] [arch]` builds in a community
[coderus/sailfishos-platform-sdk](https://hub.docker.com/r/coderus/sailfishos-platform-sdk) image,
with RPMs in `build-docker/<release>-<arch>/`. CI (`.github/workflows/build.yml`) runs it on every
push for 5.1.0.11 (aarch64, armv7hl, i486) and 5.2.0.15 (aarch64).

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
