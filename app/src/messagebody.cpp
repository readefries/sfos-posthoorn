// SPDX-License-Identifier: AGPL-3.0-or-later
#include "messagebody.h"

namespace {

QString cssColor(const QColor &color)
{
    return color.name(color.alpha() == 255 ? QColor::HexRgb : QColor::HexArgb);
}

} // namespace

MessageBody::MessageBody(QObject *parent)
    : QObject(parent)
{
    connect(this, &MessageBody::sourceChanged, this, &MessageBody::parse);
    connect(this, &MessageBody::styleChanged, this, &MessageBody::restyle);
}

void MessageBody::setHtml(const QString &html)
{
    if (html != m_html) {
        m_html = html;
        emit sourceChanged();
    }
}

void MessageBody::setPlainText(const QString &text)
{
    if (text != m_plainText) {
        m_plainText = text;
        emit sourceChanged();
    }
}

void MessageBody::setShowImages(bool show)
{
    if (show != m_style.showImages) {
        m_style.showImages = show;
        emit styleChanged();
    }
}

void MessageBody::setTextColor(const QColor &color)
{
    if (color != m_textColor) {
        m_textColor = color;
        m_style.textColor = cssColor(color);
        emit styleChanged();
    }
}

void MessageBody::setLinkColor(const QColor &color)
{
    if (color != m_linkColor) {
        m_linkColor = color;
        m_style.linkColor = cssColor(color);
        emit styleChanged();
    }
}

void MessageBody::setSecondaryColor(const QColor &color)
{
    if (color != m_secondaryColor) {
        m_secondaryColor = color;
        m_style.secondaryColor = cssColor(color);
        emit styleChanged();
    }
}

void MessageBody::setQuoteColors(const QVariantList &colors)
{
    if (colors != m_quoteColors) {
        m_quoteColors = colors;
        m_style.quoteColors.clear();
        for (const QVariant &color : colors)
            m_style.quoteColors.append(cssColor(color.value<QColor>()));
        emit styleChanged();
    }
}

void MessageBody::setParagraphSpacing(int spacing)
{
    if (spacing != m_style.paragraphSpacing) {
        m_style.paragraphSpacing = spacing;
        emit styleChanged();
    }
}

void MessageBody::setIndent(int indent)
{
    if (indent != m_style.indent) {
        m_style.indent = indent;
        emit styleChanged();
    }
}

void MessageBody::setMaxImageWidth(int width)
{
    if (width != m_style.maxImageWidth) {
        m_style.maxImageWidth = width;
        emit styleChanged();
    }
}

void MessageBody::setImagePlaceholder(const QString &placeholder)
{
    if (placeholder != m_style.imagePlaceholder) {
        m_style.imagePlaceholder = placeholder;
        emit styleChanged();
    }
}

void MessageBody::setImageText(const QString &text)
{
    if (text != m_style.imageText) {
        m_style.imageText = text;
        emit styleChanged();
    }
}

QString MessageBody::text() const
{
    // Rendered on demand: QML sets the style properties one by one while it creates the object.
    if (!m_textValid) {
        m_text = toRichText(m_content, m_style);
        m_textValid = true;
    }
    return m_text;
}

void MessageBody::parse()
{
    m_content = m_html.isEmpty() ? MailContent::fromPlainText(m_plainText)
                                 : MailContent::fromHtml(m_html);
    restyle();
}

void MessageBody::restyle()
{
    m_textValid = false;
    emit textChanged();
}
