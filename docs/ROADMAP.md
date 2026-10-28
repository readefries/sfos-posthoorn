# Roadmap

What Posthoorn should become, and the decisions made along the way. The model is FairEmail on
Android: lots of features, but reading and sorting mail stays quick.

## Decisions

- **Built on QMF.** The system's Qt Messaging Framework does IMAP/SMTP, sync, push and the mail
  store, through the `Nemo.Email` QML plugin. Accounts come from Settings → Accounts and are
  shared with the stock Email app.
- **Privileged app, so no Harbour.** QMF keeps mail accounts in the system accounts database,
  which only Privileged apps can open (`Permissions=Email;Accounts;Privileged`). Without it the app
  sees no accounts. Distribution is SailfishOS:Chum (and GitHub releases).
- **Never show the sender's HTML.** Every mail, HTML or plain text, is turned into the same
  structured text and drawn with the Silica theme: same font, same size, theme colours, so no white
  pages in the dark. There's no "show original HTML" view.
- **No images until asked.** Remote images are mostly tracking, so nothing is fetched by default.
  Images show as placeholders (with their alt text); loading them is an explicit action for that one
  mail. Inline (attached) images may get their own, separate switch later.
- **Mail text stays selectable**, for copying and for replies that quote only the selection.

## Milestones

### 1. Spike ✓

Accounts, folders and messages through `Nemo.Email`, message text in a selectable text area, a
first try at quoting the selection. Confirmed on a phone (SFOS 5.2) with an IMAP account.

### 2. Reader

- HTML → structured text: paragraphs, line breaks, bold/italic, links, lists, headings (as bold),
  quote levels. Fonts, sizes, colours, backgrounds and layout tables are dropped.
- Plain-text mail through the same path: clickable links, `>` lines as quote levels.
- Quote levels drawn the same for both (coloured bar or indent per level).
- Image placeholders, and a "Load images" action per mail.
- Written in C++ outside the UI, with unit tests run during the RPM build; odd newsletters become
  test cases.

### 3. Workflow

- Marking read/unread quickly: swipe actions in the list, bulk selection, mark read on open
  (already there).
- Folders as part of the flow: folder drawer per account, favourites, unread counts, unified
  inbox.
- Moving mail between folders, and between accounts (copy to the other account, then delete).

### 4. Compose

- Clean quoting: an attribution line ("On …, … wrote:"), `>` quoting of the original as plain
  text, the sender's signature and old nested quotes trimmed, reply above or below the quote,
  format=flowed.
- Replying with only the selected text quoted.
- Identities, attachments.

### 5. PGP

- Verify and sign through QMF's crypto service; `EmailMessage` also has `decrypt()`.
- Encrypting, and key management through the GnuPG keyring.
