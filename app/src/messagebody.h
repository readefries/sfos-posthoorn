// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MESSAGEBODY_H
#define MESSAGEBODY_H

#include "mailcontent.h"

#include <QColor>
#include <QObject>
#include <QVariantList>

// A mail's text as the reader shows it: the HTML or plain text part, reduced to its structure by
// MailContent and drawn with the theme's colours. Images stay placeholders until showImages.
class MessageBody : public QObject
{
    Q_OBJECT
    // Used when set, otherwise plainText.
    Q_PROPERTY(QString html READ html WRITE setHtml NOTIFY sourceChanged)
    Q_PROPERTY(QString plainText READ plainText WRITE setPlainText NOTIFY sourceChanged)
    Q_PROPERTY(bool showImages READ showImages WRITE setShowImages NOTIFY styleChanged)

    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY styleChanged)
    Q_PROPERTY(QColor linkColor READ linkColor WRITE setLinkColor NOTIFY styleChanged)
    Q_PROPERTY(QColor secondaryColor READ secondaryColor WRITE setSecondaryColor NOTIFY styleChanged)
    // Text colour per quote level, repeating.
    Q_PROPERTY(QVariantList quoteColors READ quoteColors WRITE setQuoteColors NOTIFY styleChanged)
    Q_PROPERTY(int paragraphSpacing READ paragraphSpacing WRITE setParagraphSpacing NOTIFY styleChanged)
    Q_PROPERTY(int indent READ indent WRITE setIndent NOTIFY styleChanged)
    Q_PROPERTY(int maxImageWidth READ maxImageWidth WRITE setMaxImageWidth NOTIFY styleChanged)
    // "%1" is the image's description, imageText the description of one without.
    Q_PROPERTY(QString imagePlaceholder READ imagePlaceholder WRITE setImagePlaceholder NOTIFY styleChanged)
    Q_PROPERTY(QString imageText READ imageText WRITE setImageText NOTIFY styleChanged)

    // Rich text for a TextEdit.
    Q_PROPERTY(QString text READ text NOTIFY textChanged)
    Q_PROPERTY(int imageCount READ imageCount NOTIFY textChanged)
    Q_PROPERTY(int remoteImageCount READ remoteImageCount NOTIFY textChanged)

public:
    explicit MessageBody(QObject *parent = nullptr);

    QString html() const { return m_html; }
    void setHtml(const QString &html);
    QString plainText() const { return m_plainText; }
    void setPlainText(const QString &text);
    bool showImages() const { return m_style.showImages; }
    void setShowImages(bool show);

    QColor textColor() const { return m_textColor; }
    void setTextColor(const QColor &color);
    QColor linkColor() const { return m_linkColor; }
    void setLinkColor(const QColor &color);
    QColor secondaryColor() const { return m_secondaryColor; }
    void setSecondaryColor(const QColor &color);
    QVariantList quoteColors() const { return m_quoteColors; }
    void setQuoteColors(const QVariantList &colors);
    int paragraphSpacing() const { return m_style.paragraphSpacing; }
    void setParagraphSpacing(int spacing);
    int indent() const { return m_style.indent; }
    void setIndent(int indent);
    int maxImageWidth() const { return m_style.maxImageWidth; }
    void setMaxImageWidth(int width);
    QString imagePlaceholder() const { return m_style.imagePlaceholder; }
    void setImagePlaceholder(const QString &placeholder);
    QString imageText() const { return m_style.imageText; }
    void setImageText(const QString &text);

    QString text() const;
    int imageCount() const { return m_content.images.size(); }
    int remoteImageCount() const { return m_content.remoteImageCount(); }

signals:
    void sourceChanged();
    void styleChanged();
    void textChanged();

private:
    void parse();
    void restyle();

    QString m_html;
    QString m_plainText;
    MailContent m_content;
    MailStyle m_style;
    QColor m_textColor;
    QColor m_linkColor;
    QColor m_secondaryColor;
    QVariantList m_quoteColors;
    mutable QString m_text;
    mutable bool m_textValid = false;
};

#endif // MESSAGEBODY_H
