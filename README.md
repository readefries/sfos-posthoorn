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
- Lists newest or oldest first, by sender (A to Z or Z to A) or largest first, optionally with
  starred and unread mail on top, all chosen at the top left of a list; starred mail stands out
  with a tinted background. "Mark all as read" in the pulley of any list.
- Per mail or conversation (long-press) or for a selection: archive, snooze, mark read or unread,
  star, move, delete. Archiving moves mail to its account's archive folder, found by name or
  chosen in Settings. Deleting moves mail to the account's Trash; in Trash or Spam (or for an
  account without Trash) it deletes it for good. Stars are the server's "flagged". "Mark all as read" for a folder or the
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
  recipient suggestions from Contacts. Quotes above or below the reply, or none; attachments,
  also the original's when forwarding.
- Reply addresses by pattern, per account in Settings → Reply addresses: with `*@example.com` or
  `me+*@example.com` (`*` is anything but `@`), a reply goes out from the address the mail was
  delivered to (`Delivered-To`, `X-Original-To`, …, then To and Cc) when it matches, as
  Thunderbird's "reply from this identity when delivery headers match". For catch-all domains and
  `+` addresses; the account's mail server has to accept sending from them.
- Signatures: the account's (from Settings → Accounts) or signatures of one's own, kept in
  Posthoorn, with one chosen per sending address and changeable per mail.
- Drafts are saved when leaving and can be continued. Mail goes out as `format=flowed` plain text.
- Sending can be undone for a few seconds; the mail then opens in the composer again.

**PGP**

- Signed mail shows who signed it and whether the signature holds; encrypted mail decrypts with
  your key's passphrase. PGP/MIME and inline PGP.
- The passphrase is remembered in memory for 10 minutes after its last use, or as long as chosen
  in Settings (never, up to 4 hours, or until Posthoorn closes); optionally forgotten when the
  phone locks, and optionally kept by GnuPG's gpg-agent rather than by Posthoorn. That agent,
  `posthoorn-gpg-agent`, is a systemd user service outside the app's sandbox, so it keeps the
  passphrase after Posthoorn closes; it listens from the next login after installing, or right
  away after `systemctl --user start posthoorn-gpg-agent.socket`.
- New mail is signed when one of your keys is for its From address, optionally with your public
  key attached, and can be encrypted to the recipients' keys (and yours, for Sent). Replies to
  encrypted mail are encrypted.
- Keys are imported from a file, from an attachment, or with one tap when a signed mail brings
  the sender's key along; listed and removed under Settings → Keys.
- With a GnuPG of its own, `posthoorn-gnupg` (see [GnuPG](#gnupg)), and a keyring of its own.

**On the phone**

- The cover shows how much is unread in the unified inbox and the newest unread mails, with New
  mail and Sync buttons.
- With `posthoorn-default-mail`, new-mail notifications and `mailto:` links open Posthoorn.
- In English and French; see [Translating](#translating).
- With `posthoorn-notify-filter`, accounts can be kept quiet: their new mail comes in as before,
  without a notification (see [Notifications per account](#notifications-per-account)).

Planned, among others: moving mail between accounts. See [docs/ROADMAP.md](docs/ROADMAP.md)
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
| `core/` | Static library without UI, with unit tests: reducing HTML and plain text to structure (`mailcontent`), cleaning links (`linkcleaner`), `format=flowed` (`flowedtext`), mail addresses and patterns of them (`addresspatterns`), splitting drafts (`maildraft`) and recognising archive folders. Qt Core only. |
| `app/src/` | Glue to QMF and QML: the message list model (folders, unified inbox, search), building outgoing mail, the addresses to send from (`sendingaddresses`), starring, archive folders, saved state, and `MailUiService`, the `com.jolla.email.ui` D-Bus service. |
| `app/qml/` | The Silica UI. |
| `app/translations/` | `posthoorn.ts`, the texts to translate (made by the build), and a `posthoorn-<language>.ts` per translation. |
| `notifyfilter/` | `posthoorn-notify-filter`, the messageserver plugin that keeps the accounts switched off in Settings → Notifications quiet. |
| `app/defaultmail/` | The files of `posthoorn-default-mail`: the D-Bus service file, the sailjail profile allowing the name, and the `mailto:` handler. |
| `tests/` | Qt Test suite for `core`, run by the RPM build's `%check`. |
| `gnupg/` | `posthoorn-gnupg`, a current GnuPG for Posthoorn alone (see below): its spec, the pinned sums of its sources, and GnuPG's release keys. |

Sailfish OS ships Qt 5.6: don't use newer Qt APIs.

## Building

With the Sailfish SDK:

    scripts/deploy.sh [ssh-host]

builds the RPMs, running the tests, and copies them to the phone (default host `sailfish`). It
prints the line to install them with: `posthoorn` and `posthoorn-notify-filter`, with
`posthoorn-default-mail` when the phone has it, then stopping the app and restarting the
messageserver so both run the new version. Each build gets its own release
(`…dev<time>.g<commit>`), so `pkcon install-local` never skips one as already installed.

`scripts/build-gnupg.sh [target]` builds `posthoorn-gnupg` the same way; it's separate, so app
builds don't rebuild GnuPG.

`scripts/docker-build.sh [release] [arch] [gnupg]` builds in a community
[coderus/sailfishos-platform-sdk](https://hub.docker.com/r/coderus/sailfishos-platform-sdk) image,
with RPMs in `build-docker/<release>-<arch>/`. CI (`.github/workflows/build.yml`) runs it on every
push for 5.1.0.11 (aarch64, armv7hl, i486) and 5.2.0.15 (aarch64).

## GnuPG

Sailfish OS ships GnuPG 2.0, which knows no ECC keys (ed25519, cv25519). `posthoorn-gnupg` is
GnuPG 2.4 with its own libgpg-error, libgcrypt, libassuan, libksba and npth, linked statically
and installed in `/usr/libexec/posthoorn/gnupg`, so the system's GnuPG and what uses it aren't
touched. It leaves out smartcards, keyservers (dirmngr), S/MIME and the rest Posthoorn doesn't use.

Its sources aren't in the repository: `gnupg/SHA256SUMS` pins them, and `scripts/fetch-gnupg.sh`
downloads them and refuses any that don't match. Every Monday `.github/workflows/gnupg-update.yml`
runs `scripts/update-gnupg.sh`, which reads GnuPG's software list (`swdb.lst`) once its signature
checks out against the release keys in `gnupg/release-keys.asc`, and opens a pull request with any
newer versions. Merging it releases them. A new GnuPG release key has to be added to that file by
hand; until then the update fails. To follow GnuPG 2.6 once it's out, change the branch at the top
of `scripts/update-gnupg.sh`.

## Releasing

Commits follow [Conventional Commits](https://www.conventionalcommits.org/) (`feat:`, `fix:`, `ci:`,
...). After every green build on `main`, CI runs [semantic-release](https://semantic-release.gitbook.io/):
it picks the next version from the commits since the last `vX.Y.Z` tag (`fix:` patch, `feat:` minor,
`BREAKING CHANGE:` major), builds the RPMs for aarch64, armv7hl and i486 with that version, and
creates the tag plus a GitHub release with the notes, the RPMs, `SHA256SUMS` and a build provenance
attestation. It commits nothing; the version in the spec is a placeholder that release builds
replace with the tag's.

### Installing a release from GitHub

Download `posthoorn-<version>-1.<arch>.rpm` and `posthoorn-gnupg-<gnupg-version>-1.<arch>.rpm`,
which it requires, for your device (`uname -m`: `aarch64` on recent devices), and
`posthoorn-default-mail-<version>-1.<arch>.rpm` too if Posthoorn takes over from Jolla's Mail
(see below). Check where they came from before installing them:

```sh
sha256sum -c --ignore-missing SHA256SUMS
for rpm in posthoorn-*.aarch64.rpm; do   # built here, by CI
    gh attestation verify "$rpm" -R readefries/sfos-posthoorn
done
devel-su pkcon install-local ./posthoorn-<version>-1.aarch64.rpm ./posthoorn-gnupg-<gnupg-version>-1.aarch64.rpm
```

`posthoorn-gnupg` only changes with GnuPG: once installed, it can be left out of later updates
until a release brings a newer one. Packages installed this way don't update by themselves.

## Using it in place of Jolla's Mail

New-mail notifications and `mailto:` links open Jolla's Mail. With the `posthoorn-default-mail`
package they open Posthoorn instead. It conflicts with Jolla's Mail, so that has to go first:

    devel-su pkcon remove jolla-email
    devel-su pkcon install-local ./posthoorn-default-mail-<version>-1.aarch64.rpm

It requires the same version of `posthoorn`, so the two are updated together.

Removing Jolla's Mail switches off the mail accounts: switch them on again in Settings → Accounts.
A Sailfish OS upgrade may want Jolla's Mail back; remove `posthoorn-default-mail` for the upgrade
and install it again afterwards. To go back: remove `posthoorn-default-mail`, install `jolla-email`.

## Translating

The texts are `qsTr()` in `app/qml` and `tr()` in `app/src`. The build collects them in
`app/translations/posthoorn.ts`. For a new language, copy that file to
`app/translations/posthoorn-<language>.ts` (`de`, `pt_BR`, …), add it to `TRANSLATIONS` in
`app/app.pro`, and translate it, for instance with Qt Linguist. Posthoorn uses the phone's language
when there's a translation for it.

## Notifications per account

New-mail notifications aren't the app's: the system's mail service (QMF's messageserver) shows
them, through its notifications plugin, also when Posthoorn isn't running. The
`posthoorn-notify-filter` package adds a plugin to the messageserver which marks new mail of the
accounts switched off under Settings → Notifications as not to be announced (QMF's
`NoNotification`), before the notifications plugin sees it. Nothing else about the mail changes.
The messageserver loads plugins when it starts, so after installing:

    devel-su pkcon install-local ./posthoorn-notify-filter-<version>-1.aarch64.rpm
    systemctl --user restart messageserver5

Like `posthoorn-default-mail`, it requires the same version of `posthoorn`.

## License

AGPL-3.0-or-later, see [LICENSE](LICENSE).
