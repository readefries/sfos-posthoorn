// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MAILCONTENT_H
#define MAILCONTENT_H

#include <QString>
#include <QStringList>
#include <QVector>

// A run of text with one format. Line breaks within a block are '\n' in the text.
struct MailSpan
{
    QString text;
    bool bold = false;
    bool italic = false;
    // Only http(s), mailto and tel links are kept.
    QString href;
    // Index into MailContent::images for an image, which has no text.
    int image = -1;

    bool sameFormat(const MailSpan &other) const;
};

struct MailBlock
{
    enum Kind { Paragraph, Heading, ListItem, Rule, Preformatted };

    Kind kind = Paragraph;
    int quoteLevel = 0;
    // ListItem: 1 for an item of a top level list.
    int listDepth = 0;
    // ListItem: "•" or "3."
    QString marker;
    QVector<MailSpan> spans;

    QString text() const;
};

struct MailImage
{
    QString source;
    QString alt;
    // 0 when not given.
    int width = 0;
    int height = 0;
    // Fetched from the network when shown, so it tells the sender the mail was read.
    bool remote = false;
};

// A mail's text reduced to its structure: paragraphs, emphasis, links, lists and quote levels.
// What the sender's HTML looked like (fonts, sizes, colours, layout) is dropped, so every mail can
// be shown the same way.
struct MailContent
{
    QVector<MailBlock> blocks;
    QVector<MailImage> images;

    bool isEmpty() const { return blocks.isEmpty(); }
    int remoteImageCount() const;
    // Blocks separated by empty lines, quote levels as "> " prefixes.
    QString toPlainText() const;

    // Tolerant of broken HTML. Leaves out what a reader wouldn't see: scripts, styles, the head,
    // hidden elements (preheaders) and tracking pixels.
    static MailContent fromHtml(const QString &html);
    // Lines starting with '>' become quote levels, URLs and mail addresses become links.
    static MailContent fromPlainText(const QString &text);
};

struct MailStyle
{
    // CSS colours, e.g. "#ffffff" or "#80ffffff".
    QString textColor = QStringLiteral("#000000");
    QString linkColor = QStringLiteral("#0000ff");
    QString secondaryColor = QStringLiteral("#808080");
    // Text colour per quote level, repeating; textColor when empty.
    QStringList quoteColors;
    int paragraphSpacing = 12;
    int indent = 24;
    int maxImageWidth = 480;
    // Shown instead of an image: "%1" is the alt text. imageText is used when there is none.
    QString imagePlaceholder = QStringLiteral("[%1]");
    QString imageText = QStringLiteral("image");
    bool showImages = false;
};

// Qt rich text (the subset TextEdit understands), with only the style's colours and spacing.
QString toRichText(const MailContent &content, const MailStyle &style);

#endif // MAILCONTENT_H
