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

- ✓ Clean links before showing them: redirectors (Outlook Safe Links, Google, Facebook,
  Proofpoint) unwrapped, tracking parameters (`utm_*`, `fbclid`, `gclid`, `mc_*`, …) taken out;
  the dialog opens the real target and can still use the original. `core/linkcleaner.cpp`.
- ✓ Images too: never loaded from known tracking services or mailing services' open trackers,
  and without tracking parameters when loaded.
- Not checked on real mail yet: Safe Links and newsletter links from the wild.
- ✓ HTML that came out empty: a zero font size between a layout's columns, `mso-hide:all` (which
  only hides from Outlook) and an unclosed head, title or style no longer hide the whole mail. If
  the HTML still has nothing to show, the reader shows the text part, says so, and logs it.
- Rows of small images (social media icons and the like) wrap messily when loaded. Possibly
  group them, or leave them out of "Load images".
- A coloured bar for quote levels, as FairEmail draws them, if indent and colour aren't enough
  (Qt's rich text can't draw block borders; it would need tables or a custom view).

### 3. Workflow

- ✓ Long-press menu per mail: mark read/unread, move, select, delete. (No swipe actions: they
  aren't a Silica pattern.)
- ✓ Selection mode, from the long-press menu or the pulley: tap to (un)select, a docked panel
  to mark read/unread, move or delete the selection; select all/none in the pulley.
- ✓ "Mark all as read" in the long-press menu of a folder and of the unified inbox, for all their
  mail (it used to be in the message list's pulley, and covered only what the list had loaded).
- ✓ Archive: from the long-press menu (at the top), the selection panel and the reader. QMF has
  no archive folder, so it's the one chosen per account in Settings, or else the folder called
  Archive (Archief, Archiv, …) or Gmail's All Mail (`core/archivefolder.cpp`). Each mail goes to
  its own account's, so it also works on a selection of several accounts.
- ✓ A Settings page: the unified inbox's folders, each account's archive folder. The message
  list's pulley is Sync (closest), New mail, Settings; selecting is done with a long-press.
- Configurable menu order, if the fixed order turns out not to fit.
- ✓ Starring (QMF's Important flag, IMAP's \Flagged, which the plugin can't set): from the
  long-press menu and the reader, shown in the list. Every list is now our own model, also a
  single folder, as the plugin's model doesn't tell whether mail is starred.
- ✓ Search through all mail: words in the subject, sender and recipients, optionally the text of
  mails too (QMF's local search, only mail whose text is downloaded), and filters for unread,
  starred and attachments, of all accounts or one. Trash and Junk are left out.
- ✓ Search on the server too: a button on the search page asks each account's server (QMF's
  remote search, up to 100 mails each), for text, unread and starred. What it finds is shown
  with the rest and removed from the phone again when the search page goes.
- ✓ Move to a folder of the same account, from the list and from an open mail.
- ✓ Favourite folders: added and removed from a folder's long-press menu, its pulley or the
  folder switcher, shown on the account overview under the unified inbox with their unread count.
- ✓ Switching folders from a folder's title (▾), without going back through the folder list.
- ✓ A unified inbox that can be configured: by default the inboxes of all accounts (also ones
  added later), with any folder of any account added or left out. Also there with a single
  account. Messages outside an inbox show their folder. Bulk moving works when the selection is
  in one account. Its own model (`UnifiedMessageListModel`, on QMF's `QMailMessageListModel`),
  as the plugin's combined inbox is inboxes only and its model's header isn't public.
- Moving mail between accounts: QMF only moves within one, so copy to the other account, then
  delete.
- ✓ Start where the app was left: the last folder (or account) opens again after a restart.
- ✓ With a single account, the start page has its name, and one "All folders" row instead of the
  account. (Its folders on the start page itself felt like too much.)
- ✓ A virtual Starred folder per account, above its folders: its starred mail, wherever it is.
  It can be a favourite like any folder.
- ✓ A New mail button on the cover, next to Sync.
- ✓ The cover shows what's waiting: the unified inbox's unread count and the newest unread
  senders and subjects, as many as fit above its buttons.
- ✓ Snooze, from the long-press menu and the reader: later today, this evening, tomorrow morning,
  this weekend, next week, or a date and time. Only on the phone (`Snoozes`, in state.ini): the
  mail is marked read and left out of the lists, then marked unread at its time, or when the app
  next starts. A virtual Snoozed folder shows what's waiting while there is any.
- Snoozing a selection, from the panel.
- ✓ Conversations, on unless switched off in Settings: a folder or the unified inbox has one row
  per conversation (QMF's threads), its newest mail with the conversation's size, unread when
  any of it is. It opens into all of its mail, in any folder, Sent included. Actions on the row
  (archive, read, move, delete, snooze) cover the conversation's mail in that list.
- A colour per account in the unified inbox, for when there's more than one.
- ✓ In place of Jolla's Mail, with the opt-in `posthoorn-default-mail` package: Posthoorn answers
  com.jolla.email.ui, so the system's new-mail notifications (from qmf-notifications-plugin) and
  mailto: links open it. Its sailjail profile allows the name; it conflicts with jolla-email.
  Removing jolla-email switches off the mail accounts, which then need switching on again.
- Checking for new mail on a schedule: Settings → Accounts already does this per account, peak
  hours included (Buteo's sync schedule, run by msyncd even with the app closed). Posthoorn's
  Settings could show each account's schedule and open that page.

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
- ✓ Undo send: after Send, a few seconds' countdown; cancelling reopens the mail in the composer.
  It's a draft meanwhile, so closing the app doesn't lose it.
- ✓ Our own message builder (`OutgoingMessage`, with QMF): the text goes out as
  `text/plain; charset=UTF-8; format=flowed`, quoted-printable, instead of the plugin's base64
  without flowed. Stored, sent and saved as drafts the way the plugin does.
- ✓ Recipient suggestions from Contacts (Silica's `RecipientField`, with the Contacts
  permission). The fields keep addresses only; names come back from the mail replied to or the
  contact when sending, quoted where needed (`MailAddresses::format`).
- ✓ Continuing drafts: a tap in Drafts opens the composer with everything back, the quote
  formatted again (`core/maildraft.cpp` splits the text, `FlowedText::decode` joins flowed lines).
  Unchanged drafts aren't saved again; saving replaces the old version, sending or "Delete draft"
  removes it.
- ✓ The "Binding loop detected for property canNavigateForward" warning from the compose Dialog
  is gone since the recipient fields changed; not seen again for a new mail or a reply.
- ✓ "Cannot pop while operation is in progress": going back now waits for page transitions.

### 5. PGP

- ✓ A current GnuPG: Sailfish OS has 2.0.4 from 2007, without ECC keys. `posthoorn-gnupg` is
  GnuPG 2.4 with its libraries, static, in `/usr/libexec/posthoorn/gnupg`, kept current by a weekly
  workflow that reads GnuPG's signed software list. QMF's crypto plugins use the system's GnuPG, so
  Posthoorn does PGP/MIME itself with this one.
- Reading: signature status in the reader, decrypting with the passphrase asked in the app,
  importing a public key from an attachment.
- Signing: a key per account, chosen in Settings; a switch in the composer.
- Encrypting to the recipients' keys, and a keys page: list, import, remove.
