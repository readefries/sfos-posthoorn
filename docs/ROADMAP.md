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

### 2. Reader ✓

- HTML and plain text reduced to the same structure (paragraphs, emphasis, links, lists, headings,
  quote levels) and drawn in the theme. Hidden preheaders, scripts and tracking pixels are left out.
- Images as placeholders, "Load images" per mail.
- Links show where they go before opening. Only http(s), mailto and tel.
- `core/mailcontent.cpp`, with unit tests run during the RPM build.

### 2b. Reader follow-ups

- **Clean links before showing them:** strip tracking parameters (`utm_*`, `fbclid`, `gclid`,
  `mc_eid`, …) and unwrap redirectors such as Outlook Safe Links
  (`*.safelinks.protection.outlook.com/?url=…`), so the dialog shows and opens the real target.
  Keep a way to open the original link.
- **Images too:** never load images from known tracking hosts, even after "Load images", and drop
  tracking parameters from image URLs.
- Rows of small images (social media icons and the like) wrap messily when loaded. Possibly
  group them, or leave them out of "Load images".
- A coloured bar for quote levels, as FairEmail draws them, if indent and colour aren't enough
  (Qt's rich text can't draw block borders; it would need tables or a custom view).

### 3. Workflow

- ✓ Long-press menu per mail: mark read/unread, move, select, delete. (No swipe actions: they
  aren't a Silica pattern.)
- ✓ Selection mode, from the long-press menu or the pulley: tap to (un)select, a docked panel
  to mark read/unread, move or delete the selection; select all/none in the pulley.
- ✓ "Mark all as read" in a folder's pulley (covers the messages loaded in the list).
- ✓ Move to a folder of the same account, from the list and from an open mail.
- Folders as part of the flow: folder drawer per account, favourites, unified inbox with a way
  to move mail from it in bulk.
- Moving mail between accounts: QMF only moves within one, so copy to the other account, then
  delete.
- ✓ Start where the app was left: the last folder (or account) opens again after a restart.
- With a single account, the account overview could be skipped altogether.

### 4. Compose

- ✓ New mail, reply, reply all, forward (text only), from the message page's action bar and the
  pulleys. From picks among the accounts' addresses and aliases; a reply defaults to the address
  the mail was sent to. Reply all leaves out one's own addresses.
- ✓ Quoting: an attribution line, the original as `>` lines without the sender's signature and,
  unless asked for, without what it quoted itself; shown the way the reader shows quotes. Reply
  above (default) or below the quote, or without it.
- ✓ Replying to a selection quotes only that.
- ✓ The account's signature, drafts when leaving with text, a notice when sending finished.
- ✓ Attachments: adding files with the content picker, sending the original's along when
  forwarding (downloaded into the mail store first, not into Downloads), removing them again.
- ✓ Opening and saving the attachments of received mail: listed in the reader, downloaded on a
  tap (into Downloads, where the apps that open them can read them) and opened.
- ✓ Our own message builder (`OutgoingMessage`, with QMF): the text goes out as
  `text/plain; charset=UTF-8; format=flowed`, quoted-printable, instead of the plugin's base64
  without flowed. Stored, sent and saved as drafts the way the plugin does.
- Recipient suggestions from Contacts (needs the Contacts permission).
- Opening a draft in the composer again.
- A "Binding loop detected for property canNavigateForward" warning from the compose Dialog.

### 5. PGP

- Verify and sign through QMF's crypto service; `EmailMessage` also has `decrypt()`.
- Encrypting, and key management through the GnuPG keyring.
