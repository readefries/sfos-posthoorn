// SPDX-License-Identifier: AGPL-3.0-or-later
#include "mailcontent.h"

#include "linkcleaner.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>

bool MailSpan::sameFormat(const MailSpan &other) const
{
    return image < 0 && other.image < 0 && bold == other.bold && italic == other.italic
            && href == other.href;
}

QString MailBlock::text() const
{
    QString result;
    for (const MailSpan &span : spans)
        result += span.text;
    return result;
}

int MailContent::remoteImageCount() const
{
    int count = 0;
    for (const MailImage &image : images) {
        if (image.remote)
            ++count;
    }
    return count;
}

QString MailContent::toPlainText() const
{
    QStringList paragraphs;
    for (const MailBlock &block : blocks) {
        QString text;
        if (block.kind == MailBlock::Rule) {
            text = QStringLiteral("----");
        } else {
            for (const MailSpan &span : block.spans) {
                if (span.image >= 0) {
                    const QString &alt = images.at(span.image).alt;
                    if (!alt.isEmpty())
                        text += QLatin1Char('[') + alt + QLatin1Char(']');
                } else {
                    text += span.text;
                }
            }
            if (block.kind == MailBlock::ListItem) {
                text = QString(2 * (block.listDepth - 1), QLatin1Char(' ')) + block.marker
                        + QLatin1Char(' ') + text;
            }
        }
        if (block.quoteLevel > 0) {
            const QString prefix = QString(block.quoteLevel, QLatin1Char('>')) + QLatin1Char(' ');
            text = prefix + text.replace(QLatin1Char('\n'), QLatin1Char('\n') + prefix);
        }
        paragraphs.append(text);
    }
    return paragraphs.join(QStringLiteral("\n\n"));
}

QString MailContent::quoted(bool keepEarlierQuotes) const
{
    MailContent kept;
    kept.images = images;
    for (const MailBlock &block : blocks) {
        if (!block.signature && (keepEarlierQuotes || block.quoteLevel == 0))
            kept.blocks.append(block);
    }
    return quoteLines(kept.toPlainText());
}

QString MailContent::quoteLines(const QString &text)
{
    QString normalized = text;
    normalized.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    normalized.replace(QChar(0x2028), QLatin1Char('\n'));
    normalized.replace(QChar(0x2029), QLatin1Char('\n'));
    QStringList lines = normalized.split(QLatin1Char('\n'));
    while (!lines.isEmpty() && lines.last().trimmed().isEmpty())
        lines.removeLast();
    while (!lines.isEmpty() && lines.first().trimmed().isEmpty())
        lines.removeFirst();
    for (QString &line : lines) {
        if (line.isEmpty())
            line = QStringLiteral(">");
        else if (line.startsWith(QLatin1Char('>')))
            line.prepend(QLatin1Char('>'));
        else
            line.prepend(QLatin1String("> "));
    }
    return lines.join(QLatin1Char('\n'));
}

namespace {

// Characters mail senders use to pad preheaders, or that only matter for hyphenation and joining.
bool isInvisible(QChar c)
{
    switch (c.unicode()) {
    case 0x00ad: // soft hyphen
    case 0x034f: // combining grapheme joiner
    case 0x200b: // zero width space
    case 0x200c: // zero width non-joiner
    case 0x200d: // zero width joiner
    case 0x2060: // word joiner
    case 0xfeff: // byte order mark
        return true;
    default:
        return false;
    }
}

QString sanitizedHref(const QString &href)
{
    const QString trimmed = href.trimmed();
    static const QRegularExpression allowed(QStringLiteral("^(https?|mailto|tel):"),
                                            QRegularExpression::CaseInsensitiveOption);
    return allowed.match(trimmed).hasMatch() ? trimmed : QString();
}

void appendSpan(QVector<MailSpan> &spans, const MailSpan &span)
{
    if (span.image < 0 && span.text.isEmpty())
        return;
    if (!spans.isEmpty() && spans.last().sameFormat(span))
        spans.last().text += span.text;
    else
        spans.append(span);
}

// Appends text with format, making links of the URLs and mail addresses in it unless it is a
// link already.
void appendLinkified(QVector<MailSpan> &spans, const QString &text, const MailSpan &format)
{
    if (!format.href.isEmpty()) {
        MailSpan span = format;
        span.text = text;
        appendSpan(spans, span);
        return;
    }

    static const QRegularExpression links(QStringLiteral(
            "(?:https?://|www\\.)[^\\s<>\"]+"
            "|mailto:[^\\s<>\"]+"
            "|[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(?:\\.[A-Za-z0-9-]+)+"),
            QRegularExpression::CaseInsensitiveOption);

    int position = 0;
    QRegularExpressionMatchIterator it = links.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        QString link = match.captured();
        // Punctuation after a link usually belongs to the sentence, and so does a closing
        // parenthesis that wasn't opened in the link.
        while (!link.isEmpty()) {
            const QChar last = link.at(link.size() - 1);
            if (QStringLiteral(".,;:!?'\"").contains(last)
                    || (last == QLatin1Char(')') && link.count(QLatin1Char('(')) < link.count(QLatin1Char(')')))) {
                link.chop(1);
            } else {
                break;
            }
        }
        if (link.isEmpty())
            continue;

        MailSpan before = format;
        before.text = text.mid(position, match.capturedStart() - position);
        appendSpan(spans, before);

        MailSpan linked = format;
        linked.text = link;
        if (link.startsWith(QLatin1String("www."), Qt::CaseInsensitive))
            linked.href = QStringLiteral("https://") + link;
        else if (link.contains(QLatin1Char('@')) && !link.contains(QLatin1Char(':')))
            linked.href = QStringLiteral("mailto:") + link;
        else
            linked.href = link;
        appendSpan(spans, linked);

        position = match.capturedStart() + link.size();
    }
    MailSpan rest = format;
    rest.text = text.mid(position);
    appendSpan(spans, rest);
}

// Removes trailing white space and line breaks, and turns runs of empty lines into one.
// Returns false when nothing visible is left.
bool tidyBlock(MailBlock &block)
{
    while (!block.spans.isEmpty()) {
        MailSpan &last = block.spans.last();
        if (last.image >= 0)
            break;
        int end = last.text.size();
        while (end > 0 && last.text.at(end - 1).isSpace())
            --end;
        last.text.truncate(end);
        if (!last.text.isEmpty())
            break;
        block.spans.removeLast();
    }
    if (block.kind != MailBlock::Preformatted) {
        static const QRegularExpression emptyLines(QStringLiteral(" *\n[ \n]*\n *"));
        static const QRegularExpression lineStart(QStringLiteral(" *\n *"));
        for (MailSpan &span : block.spans) {
            span.text.replace(emptyLines, QStringLiteral("\n\n"));
            span.text.replace(lineStart, QStringLiteral("\n"));
        }
        // Leading line breaks.
        while (!block.spans.isEmpty() && block.spans.first().image < 0) {
            MailSpan &first = block.spans.first();
            int start = 0;
            while (start < first.text.size() && first.text.at(start).isSpace())
                ++start;
            first.text.remove(0, start);
            if (!first.text.isEmpty())
                break;
            block.spans.removeFirst();
        }
    }
    return block.kind == MailBlock::Rule || !block.spans.isEmpty();
}

// Named character references beyond Latin-1.
const QHash<QString, uint> &entities()
{
    static const QHash<QString, uint> table = []() {
        QHash<QString, uint> t;
        static const char *const latin1[] = {
            "nbsp", "iexcl", "cent", "pound", "curren", "yen", "brvbar", "sect", "uml", "copy",
            "ordf", "laquo", "not", "shy", "reg", "macr", "deg", "plusmn", "sup2", "sup3", "acute",
            "micro", "para", "middot", "cedil", "sup1", "ordm", "raquo", "frac14", "frac12",
            "frac34", "iquest", "Agrave", "Aacute", "Acirc", "Atilde", "Auml", "Aring", "AElig",
            "Ccedil", "Egrave", "Eacute", "Ecirc", "Euml", "Igrave", "Iacute", "Icirc", "Iuml",
            "ETH", "Ntilde", "Ograve", "Oacute", "Ocirc", "Otilde", "Ouml", "times", "Oslash",
            "Ugrave", "Uacute", "Ucirc", "Uuml", "Yacute", "THORN", "szlig", "agrave", "aacute",
            "acirc", "atilde", "auml", "aring", "aelig", "ccedil", "egrave", "eacute", "ecirc",
            "euml", "igrave", "iacute", "icirc", "iuml", "eth", "ntilde", "ograve", "oacute",
            "ocirc", "otilde", "ouml", "divide", "oslash", "ugrave", "uacute", "ucirc", "uuml",
            "yacute", "thorn", "yuml"
        };
        for (uint i = 0; i < sizeof(latin1) / sizeof(latin1[0]); ++i)
            t.insert(QLatin1String(latin1[i]), 0xa0 + i);
        static const struct { const char *name; uint code; } others[] = {
            { "amp", '&' }, { "lt", '<' }, { "gt", '>' }, { "quot", '"' }, { "apos", '\'' },
            { "OElig", 0x152 }, { "oelig", 0x153 }, { "Scaron", 0x160 }, { "scaron", 0x161 },
            { "Yuml", 0x178 }, { "fnof", 0x192 }, { "circ", 0x2c6 }, { "tilde", 0x2dc },
            { "ensp", 0x2002 }, { "emsp", 0x2003 }, { "thinsp", 0x2009 }, { "zwnj", 0x200c },
            { "zwj", 0x200d }, { "lrm", 0x200e }, { "rlm", 0x200f }, { "ndash", 0x2013 },
            { "mdash", 0x2014 }, { "lsquo", 0x2018 }, { "rsquo", 0x2019 }, { "sbquo", 0x201a },
            { "ldquo", 0x201c }, { "rdquo", 0x201d }, { "bdquo", 0x201e }, { "dagger", 0x2020 },
            { "Dagger", 0x2021 }, { "bull", 0x2022 }, { "hellip", 0x2026 }, { "permil", 0x2030 },
            { "prime", 0x2032 }, { "lsaquo", 0x2039 }, { "rsaquo", 0x203a }, { "euro", 0x20ac },
            { "trade", 0x2122 }, { "larr", 0x2190 }, { "uarr", 0x2191 }, { "rarr", 0x2192 },
            { "darr", 0x2193 }, { "minus", 0x2212 }, { "check", 0x2713 }
        };
        for (const auto &entity : others)
            t.insert(QLatin1String(entity.name), entity.code);
        return t;
    }();
    return table;
}

QString decodeEntities(const QString &text)
{
    if (!text.contains(QLatin1Char('&')))
        return text;

    QString result;
    result.reserve(text.size());
    int i = 0;
    while (i < text.size()) {
        const int amp = text.indexOf(QLatin1Char('&'), i);
        if (amp < 0) {
            result += text.midRef(i);
            break;
        }
        result += text.midRef(i, amp - i);
        const int semicolon = text.indexOf(QLatin1Char(';'), amp + 1);
        bool decoded = false;
        if (semicolon > amp + 1 && semicolon - amp <= 32) {
            const QString name = text.mid(amp + 1, semicolon - amp - 1);
            uint code = 0;
            if (name.startsWith(QLatin1Char('#'))) {
                bool ok = false;
                if (name.size() > 1 && (name.at(1) == QLatin1Char('x') || name.at(1) == QLatin1Char('X')))
                    code = name.mid(2).toUInt(&ok, 16);
                else
                    code = name.mid(1).toUInt(&ok, 10);
                if (!ok || code == 0 || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
                    code = 0;
            } else {
                code = entities().value(name);
            }
            if (code) {
                result += QString::fromUcs4(&code, 1);
                i = semicolon + 1;
                decoded = true;
            }
        }
        if (!decoded) {
            result += QLatin1Char('&');
            i = amp + 1;
        }
    }
    return result;
}

bool isBlockElement(const QString &name)
{
    static const QSet<QString> names = {
        QStringLiteral("address"), QStringLiteral("article"), QStringLiteral("aside"),
        QStringLiteral("blockquote"), QStringLiteral("caption"), QStringLiteral("center"),
        QStringLiteral("dd"), QStringLiteral("details"), QStringLiteral("div"), QStringLiteral("dl"),
        QStringLiteral("dt"), QStringLiteral("fieldset"), QStringLiteral("figcaption"),
        QStringLiteral("figure"), QStringLiteral("footer"), QStringLiteral("form"),
        QStringLiteral("h1"), QStringLiteral("h2"), QStringLiteral("h3"), QStringLiteral("h4"),
        QStringLiteral("h5"), QStringLiteral("h6"), QStringLiteral("header"), QStringLiteral("li"),
        QStringLiteral("main"), QStringLiteral("nav"), QStringLiteral("ol"), QStringLiteral("p"),
        QStringLiteral("pre"), QStringLiteral("section"), QStringLiteral("summary"),
        QStringLiteral("table"), QStringLiteral("tbody"), QStringLiteral("td"),
        QStringLiteral("tfoot"), QStringLiteral("th"), QStringLiteral("thead"),
        QStringLiteral("tr"), QStringLiteral("ul")
    };
    return names.contains(name);
}

bool isVoidElement(const QString &name)
{
    static const QSet<QString> names = {
        QStringLiteral("area"), QStringLiteral("base"), QStringLiteral("br"), QStringLiteral("col"),
        QStringLiteral("embed"), QStringLiteral("hr"), QStringLiteral("img"), QStringLiteral("input"),
        QStringLiteral("link"), QStringLiteral("meta"), QStringLiteral("source"),
        QStringLiteral("track"), QStringLiteral("wbr")
    };
    return names.contains(name);
}

// Elements whose content is never text for the reader.
bool isHiddenElement(const QString &name)
{
    static const QSet<QString> names = {
        QStringLiteral("head"), QStringLiteral("iframe"), QStringLiteral("noscript"),
        QStringLiteral("object"), QStringLiteral("script"), QStringLiteral("select"),
        QStringLiteral("style"), QStringLiteral("svg"), QStringLiteral("template"),
        QStringLiteral("title")
    };
    return names.contains(name);
}

// Elements whose content isn't HTML, so must be skipped without looking for tags in it.
bool isRawTextElement(const QString &name)
{
    return name == QLatin1String("script") || name == QLatin1String("style")
            || name == QLatin1String("title") || name == QLatin1String("textarea")
            || name == QLatin1String("xmp");
}

// Styles that hide an element, as used for preheaders (the preview text some mail apps show).
bool isHiddenStyle(const QString &style)
{
    if (style.isEmpty())
        return false;
    QString css = style.toLower();
    css.remove(QRegularExpression(QStringLiteral("\\s+")));
    static const QRegularExpression hidden(QStringLiteral(
            "(^|;)(display:none|visibility:hidden|mso-hide:all|opacity:0(\\.0*)?(;|!|$)"
            "|font-size:0(px|pt|em|rem|%)?(;|!|$))"));
    if (hidden.match(css).hasMatch())
        return true;
    static const QRegularExpression zeroHeight(QStringLiteral(
            "(^|;)(max-)?height:0(px)?(;|!|$)"));
    return zeroHeight.match(css).hasMatch() && css.contains(QLatin1String("overflow:hidden"));
}

int pixels(const QString &value)
{
    static const QRegularExpression number(QStringLiteral("^\\s*(\\d+)\\s*(px)?\\s*$"));
    const QRegularExpressionMatch match = number.match(value);
    return match.hasMatch() ? match.captured(1).toInt() : 0;
}

int stylePixels(const QString &style, const QString &property)
{
    const QRegularExpression re(QStringLiteral("(?:^|;)\\s*") + property
                                + QStringLiteral("\\s*:\\s*(\\d+)\\s*px"),
                                QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(style);
    return match.hasMatch() ? match.captured(1).toInt() : 0;
}

typedef QHash<QString, QString> Attributes;

class HtmlReader
{
public:
    MailContent read(const QString &html);

private:
    struct Element
    {
        QString name;
        bool hidden = false;
        QString href;
        bool ordered = false;
        int counter = 1;
        bool signature = false;
    };

    void startTag(const QString &name, const Attributes &attributes);
    void endTag(const QString &name);
    void text(const QString &text);
    void lineBreak();
    void image(const Attributes &attributes);
    void openBlock();
    void closeBlock();

    bool hidden() const;
    MailSpan format() const;
    int count(const QString &name) const;
    int indexOf(const QString &name, const QString &stopAt = QString()) const;

    QVector<Element> m_stack;
    MailContent m_content;
    MailBlock m_block;
    bool m_blockOpen = false;
    QString m_marker;
};

MailContent HtmlReader::read(const QString &html)
{
    QString pending;
    auto flush = [&]() {
        if (!pending.isEmpty()) {
            text(decodeEntities(pending));
            pending.clear();
        }
    };

    const int n = html.size();
    int i = 0;
    while (i < n) {
        const int lt = html.indexOf(QLatin1Char('<'), i);
        if (lt < 0) {
            pending += html.midRef(i);
            break;
        }
        pending += html.midRef(i, lt - i);
        i = lt;

        const QChar next = lt + 1 < n ? html.at(lt + 1) : QChar();
        if (html.midRef(lt, 4) == QLatin1String("<!--")) {
            const int end = html.indexOf(QLatin1String("-->"), lt + 4);
            i = end < 0 ? n : end + 3;
        } else if (next == QLatin1Char('!') || next == QLatin1Char('?')) {
            const int end = html.indexOf(QLatin1Char('>'), lt);
            i = end < 0 ? n : end + 1;
        } else if (next == QLatin1Char('/') || next.isLetter()) {
            const bool closing = next == QLatin1Char('/');
            int j = lt + (closing ? 2 : 1);
            const int nameStart = j;
            while (j < n && (html.at(j).isLetterOrNumber() || html.at(j) == QLatin1Char('-')
                             || html.at(j) == QLatin1Char(':')))
                ++j;
            const QString name = html.mid(nameStart, j - nameStart).toLower();

            Attributes attributes;
            while (j < n && html.at(j) != QLatin1Char('>')) {
                const QChar c = html.at(j);
                if (c.isSpace() || c == QLatin1Char('/')) {
                    ++j;
                    continue;
                }
                const int attrStart = j;
                while (j < n && !html.at(j).isSpace() && html.at(j) != QLatin1Char('=')
                       && html.at(j) != QLatin1Char('>') && html.at(j) != QLatin1Char('/'))
                    ++j;
                const QString attrName = html.mid(attrStart, j - attrStart).toLower();
                while (j < n && html.at(j).isSpace())
                    ++j;
                QString value;
                if (j < n && html.at(j) == QLatin1Char('=')) {
                    ++j;
                    while (j < n && html.at(j).isSpace())
                        ++j;
                    if (j < n && (html.at(j) == QLatin1Char('"') || html.at(j) == QLatin1Char('\''))) {
                        const QChar quote = html.at(j);
                        const int end = html.indexOf(quote, j + 1);
                        const int stop = end < 0 ? n : end;
                        value = html.mid(j + 1, stop - j - 1);
                        j = end < 0 ? n : end + 1;
                    } else {
                        const int valueStart = j;
                        while (j < n && !html.at(j).isSpace() && html.at(j) != QLatin1Char('>'))
                            ++j;
                        value = html.mid(valueStart, j - valueStart);
                    }
                }
                if (!attrName.isEmpty() && !attributes.contains(attrName))
                    attributes.insert(attrName, decodeEntities(value));
            }
            i = j < n ? j + 1 : n;

            flush();
            if (closing) {
                endTag(name);
            } else {
                startTag(name, attributes);
                if (isRawTextElement(name)) {
                    const int end = html.indexOf(QLatin1String("</") + name, i, Qt::CaseInsensitive);
                    if (name == QLatin1String("textarea") || name == QLatin1String("xmp")) {
                        const QString raw = html.mid(i, (end < 0 ? n : end) - i);
                        text(name == QLatin1String("textarea") ? decodeEntities(raw) : raw);
                    }
                    i = end < 0 ? n : end;
                }
            }
        } else {
            // A '<' that doesn't start a tag.
            pending += QLatin1Char('<');
            i = lt + 1;
        }
    }
    flush();
    closeBlock();
    return m_content;
}

bool HtmlReader::hidden() const
{
    for (const Element &element : m_stack) {
        if (element.hidden)
            return true;
    }
    return false;
}

int HtmlReader::count(const QString &name) const
{
    int result = 0;
    for (const Element &element : m_stack) {
        if (element.name == name)
            ++result;
    }
    return result;
}

// Innermost open element called name, not looking past stopAt. -1 if there isn't one.
int HtmlReader::indexOf(const QString &name, const QString &stopAt) const
{
    for (int i = m_stack.size() - 1; i >= 0; --i) {
        if (m_stack.at(i).name == name)
            return i;
        if (!stopAt.isEmpty() && stopAt.split(QLatin1Char(',')).contains(m_stack.at(i).name))
            return -1;
    }
    return -1;
}

MailSpan HtmlReader::format() const
{
    MailSpan span;
    for (const Element &element : m_stack) {
        const QString &name = element.name;
        if (name == QLatin1String("b") || name == QLatin1String("strong") || name == QLatin1String("th")
                || (name.size() == 2 && name.at(0) == QLatin1Char('h') && name.at(1).isDigit()))
            span.bold = true;
        else if (name == QLatin1String("i") || name == QLatin1String("em") || name == QLatin1String("cite"))
            span.italic = true;
        else if (name == QLatin1String("a"))
            span.href = element.href;
    }
    return span;
}

void HtmlReader::openBlock()
{
    if (m_blockOpen)
        return;
    m_block = MailBlock();
    m_block.quoteLevel = count(QStringLiteral("blockquote"));
    bool heading = false;
    for (const Element &element : m_stack) {
        const QString &name = element.name;
        if (name.size() == 2 && name.at(0) == QLatin1Char('h') && name.at(1).isDigit())
            heading = true;
        if (element.signature)
            m_block.signature = true;
    }
    if (count(QStringLiteral("pre")) > 0) {
        m_block.kind = MailBlock::Preformatted;
    } else if (!m_marker.isEmpty()) {
        m_block.kind = MailBlock::ListItem;
        m_block.marker = m_marker;
        m_block.listDepth = count(QStringLiteral("ul")) + count(QStringLiteral("ol"));
    } else if (heading) {
        m_block.kind = MailBlock::Heading;
    }
    m_marker.clear();
    m_blockOpen = true;
}

void HtmlReader::closeBlock()
{
    if (!m_blockOpen)
        return;
    m_blockOpen = false;
    if (tidyBlock(m_block))
        m_content.blocks.append(m_block);
}

void HtmlReader::text(const QString &raw)
{
    if (hidden())
        return;

    if (count(QStringLiteral("pre")) > 0) {
        QString text = raw;
        text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
        if (!m_blockOpen) {
            // A line break right after <pre> isn't part of the content.
            if (text.startsWith(QLatin1Char('\n')))
                text.remove(0, 1);
            if (text.isEmpty())
                return;
        }
        openBlock();
        MailSpan span = format();
        span.text = text;
        appendSpan(m_block.spans, span);
        return;
    }

    QString text;
    text.reserve(raw.size());
    bool space = false;
    for (const QChar c : raw) {
        if (isInvisible(c))
            continue;
        if (c.isSpace()) {
            space = true;
            continue;
        }
        if (space)
            text += QLatin1Char(' ');
        space = false;
        text += c;
    }
    if (space)
        text += QLatin1Char(' ');
    if (text.isEmpty())
        return;

    bool atStart = !m_blockOpen || m_block.spans.isEmpty();
    if (!atStart) {
        const MailSpan &last = m_block.spans.last();
        atStart = last.image < 0 && (last.text.endsWith(QLatin1Char(' '))
                                     || last.text.endsWith(QLatin1Char('\n')));
    }
    if (atStart && text.startsWith(QLatin1Char(' ')))
        text.remove(0, 1);
    if (text.isEmpty())
        return;

    openBlock();
    appendLinkified(m_block.spans, text, format());
}

void HtmlReader::lineBreak()
{
    if (hidden() || !m_blockOpen)
        return;
    MailSpan span = format();
    span.text = QStringLiteral("\n");
    appendSpan(m_block.spans, span);
}

void HtmlReader::image(const Attributes &attributes)
{
    if (hidden())
        return;
    const QString style = attributes.value(QStringLiteral("style"));
    if (isHiddenStyle(style))
        return;

    MailImage image;
    image.source = attributes.value(QStringLiteral("src")).trimmed();
    image.alt = attributes.value(QStringLiteral("alt")).simplified();
    image.width = pixels(attributes.value(QStringLiteral("width")));
    if (!image.width)
        image.width = stylePixels(style, QStringLiteral("width"));
    image.height = pixels(attributes.value(QStringLiteral("height")));
    if (!image.height)
        image.height = stylePixels(style, QStringLiteral("height"));
    // Tracking pixels and spacers.
    if ((image.width > 0 && image.width <= 2) || (image.height > 0 && image.height <= 2))
        return;
    if (image.source.isEmpty() && image.alt.isEmpty())
        return;

    static const QRegularExpression remote(QStringLiteral("^(https?:)?//"),
                                           QRegularExpression::CaseInsensitiveOption);
    image.remote = remote.match(image.source).hasMatch();
    if (image.source.startsWith(QLatin1String("//")))
        image.source.prepend(QLatin1String("https:"));
    if (image.remote) {
        // Never loaded, not even when images are, so not worth a placeholder either.
        if (LinkCleaner::isTrackingImage(image.source))
            return;
        image.source = LinkCleaner::clean(image.source).url;
    }
    // The email plugin puts cid: in while an inline image is still downloading.
    if (attributes.value(QStringLiteral("nemo-inline-image-loading")) == QLatin1String("yes")
            || (!image.remote && !image.source.startsWith(QLatin1String("data:image/"), Qt::CaseInsensitive)))
        image.source.clear();

    openBlock();
    MailSpan span = format();
    span.image = m_content.images.size();
    m_content.images.append(image);
    appendSpan(m_block.spans, span);
}

void HtmlReader::startTag(const QString &name, const Attributes &attributes)
{
    if (name == QLatin1String("br")) {
        lineBreak();
        return;
    }
    if (name == QLatin1String("hr")) {
        closeBlock();
        if (!hidden()) {
            MailBlock rule;
            rule.kind = MailBlock::Rule;
            rule.quoteLevel = count(QStringLiteral("blockquote"));
            m_content.blocks.append(rule);
        }
        return;
    }
    if (name == QLatin1String("img")) {
        image(attributes);
        return;
    }
    if (isVoidElement(name))
        return;

    // End tags HTML lets senders leave out.
    if (name == QLatin1String("p")) {
        const int open = indexOf(name, QStringLiteral("div,td,th,li,blockquote"));
        if (open >= 0)
            endTag(name);
    } else if (name == QLatin1String("li")) {
        if (indexOf(name, QStringLiteral("ul,ol")) >= 0)
            endTag(name);
    } else if (name == QLatin1String("td") || name == QLatin1String("th")) {
        const int td = indexOf(QStringLiteral("td"), QStringLiteral("tr,table"));
        const int th = indexOf(QStringLiteral("th"), QStringLiteral("tr,table"));
        if (td >= 0 || th >= 0)
            endTag(td > th ? QStringLiteral("td") : QStringLiteral("th"));
    } else if (name == QLatin1String("tr")) {
        if (indexOf(name, QStringLiteral("table")) >= 0)
            endTag(name);
    }

    if (isBlockElement(name))
        closeBlock();

    Element element;
    element.name = name;
    element.hidden = isHiddenElement(name) || attributes.contains(QStringLiteral("hidden"))
            || isHiddenStyle(attributes.value(QStringLiteral("style")));
    // How Gmail, Thunderbird and Outlook mark the signatures they add.
    const QStringList classes = attributes.value(QStringLiteral("class")).split(QLatin1Char(' '));
    element.signature = classes.contains(QStringLiteral("gmail_signature"))
            || classes.contains(QStringLiteral("moz-signature"))
            || attributes.value(QStringLiteral("id")).compare(QLatin1String("signature"), Qt::CaseInsensitive) == 0;
    if (name == QLatin1String("a")) {
        element.href = sanitizedHref(attributes.value(QStringLiteral("href")));
    } else if (name == QLatin1String("ol")) {
        element.ordered = true;
        bool ok = false;
        const int start = attributes.value(QStringLiteral("start")).toInt(&ok);
        if (ok)
            element.counter = start;
    } else if (name == QLatin1String("li")) {
        for (int i = m_stack.size() - 1; i >= 0; --i) {
            Element &list = m_stack[i];
            if (list.name == QLatin1String("ol")) {
                m_marker = QString::number(list.counter++) + QLatin1Char('.');
                break;
            }
            if (list.name == QLatin1String("ul")) {
                m_marker = QStringLiteral("•");
                break;
            }
        }
        if (m_marker.isEmpty())
            m_marker = QStringLiteral("•");
    }
    m_stack.append(element);
}

void HtmlReader::endTag(const QString &name)
{
    // Browsers read </br> as <br>, and some mail is written for them.
    if (name == QLatin1String("br")) {
        lineBreak();
        return;
    }
    const int open = indexOf(name);
    if (open < 0)
        return;
    bool block = false;
    for (int i = open; i < m_stack.size(); ++i)
        block = block || isBlockElement(m_stack.at(i).name);
    if (block)
        closeBlock();
    m_stack.resize(open);
    if (name == QLatin1String("li"))
        m_marker.clear();
}

// Marks the blocks from a "-- " line on as signature, until the quote level changes: a quote
// after a signature is what the sender replied to.
void markSignatures(MailContent &content)
{
    int level = -1;
    for (MailBlock &block : content.blocks) {
        if (level >= 0 && block.quoteLevel != level)
            level = -1;
        const QString text = block.text();
        if (level < 0 && (text == QLatin1String("--") || text.startsWith(QLatin1String("--\n"))))
            level = block.quoteLevel;
        if (level >= 0)
            block.signature = true;
    }
}

} // namespace

MailContent MailContent::fromHtml(const QString &html)
{
    MailContent content = HtmlReader().read(html);
    markSignatures(content);
    return content;
}

MailContent MailContent::fromPlainText(const QString &text)
{
    MailContent content;
    MailBlock block;
    bool open = false;

    auto close = [&]() {
        if (open && tidyBlock(block))
            content.blocks.append(block);
        open = false;
    };

    QString normalized = text;
    normalized.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    const QStringList lines = normalized.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        int level = 0;
        int i = 0;
        // "> > text" and ">> text" are both level 2.
        while (true) {
            int j = i;
            while (j < line.size() && line.at(j) == QLatin1Char(' '))
                ++j;
            if (j < line.size() && line.at(j) == QLatin1Char('>')) {
                ++level;
                i = j + 1;
            } else {
                break;
            }
        }
        if (level > 0 && i < line.size() && line.at(i) == QLatin1Char(' '))
            ++i;
        QString rest = line.mid(i);
        for (int k = rest.size() - 1; k >= 0; --k) {
            if (isInvisible(rest.at(k)))
                rest.remove(k, 1);
        }

        if (rest.trimmed().isEmpty()) {
            close();
            continue;
        }
        if (open && block.quoteLevel != level)
            close();
        if (!open) {
            block = MailBlock();
            block.quoteLevel = level;
            open = true;
        } else {
            MailSpan lineBreak;
            lineBreak.text = QStringLiteral("\n");
            appendSpan(block.spans, lineBreak);
        }
        appendLinkified(block.spans, rest, MailSpan());
    }
    close();
    markSignatures(content);
    return content;
}

QString toRichText(const MailContent &content, const MailStyle &style)
{
    QString html;
    for (const MailBlock &block : content.blocks) {
        const QString color = block.quoteLevel > 0 && !style.quoteColors.isEmpty()
                ? style.quoteColors.at((block.quoteLevel - 1) % style.quoteColors.size())
                : style.textColor;
        int indent = block.quoteLevel * style.indent;
        if (block.kind == MailBlock::ListItem)
            indent += (block.listDepth - 1) * style.indent;

        if (block.kind == MailBlock::Rule) {
            html += QStringLiteral("<p style=\"margin-top:0px; margin-bottom:%1px; margin-left:%2px; color:%3;\">"
                                   "──────</p>")
                    .arg(style.paragraphSpacing).arg(indent).arg(style.secondaryColor);
            continue;
        }

        const bool pre = block.kind == MailBlock::Preformatted;
        html += QStringLiteral("<p style=\"margin-top:0px; margin-bottom:%1px; margin-left:%2px; color:%3;%4\">")
                .arg(style.paragraphSpacing).arg(indent).arg(color,
                     pre ? QStringLiteral(" white-space:pre-wrap; font-family:monospace;") : QString());
        if (block.kind == MailBlock::ListItem)
            html += block.marker.toHtmlEscaped() + QStringLiteral("&nbsp;");
        if (block.kind == MailBlock::Heading)
            html += QStringLiteral("<b>");

        for (const MailSpan &span : block.spans) {
            QString part;
            if (span.image >= 0) {
                const MailImage &image = content.images.at(span.image);
                if (style.showImages && !image.source.isEmpty()) {
                    part = QStringLiteral("<img src=\"%1\"").arg(image.source.toHtmlEscaped());
                    if (image.width > 0) {
                        const int width = qMin(image.width, style.maxImageWidth);
                        part += QStringLiteral(" width=\"%1\"").arg(width);
                        if (image.height > 0)
                            part += QStringLiteral(" height=\"%1\"").arg(image.height * width / image.width);
                    }
                    part += QStringLiteral(" />");
                } else {
                    const QString label = style.imagePlaceholder
                            .arg(image.alt.isEmpty() ? style.imageText : image.alt);
                    part = QStringLiteral("<span style=\"color:%1;\">%2</span>")
                            .arg(span.href.isEmpty() ? style.secondaryColor : style.linkColor,
                                 label.toHtmlEscaped());
                }
            } else {
                part = span.text.toHtmlEscaped();
                if (!pre)
                    part.replace(QLatin1Char('\n'), QLatin1String("<br/>"));
            }
            if (span.bold && block.kind != MailBlock::Heading)
                part = QStringLiteral("<b>") + part + QStringLiteral("</b>");
            if (span.italic)
                part = QStringLiteral("<i>") + part + QStringLiteral("</i>");
            if (!span.href.isEmpty()) {
                part = QStringLiteral("<a href=\"%1\" style=\"color:%2;\">%3</a>")
                        .arg(span.href.toHtmlEscaped(), style.linkColor, part);
            }
            html += part;
        }

        if (block.kind == MailBlock::Heading)
            html += QStringLiteral("</b>");
        html += QStringLiteral("</p>");
    }
    return html;
}
