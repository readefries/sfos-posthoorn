// SPDX-License-Identifier: AGPL-3.0-or-later
#include "imappartfetcher.h"

#include "imapsession.h"

#include <QDebug>

ImapPartFetcher::ImapPartFetcher(QObject *parent)
    : QObject(parent)
    , m_session(new ImapSession(this))
{
    connect(m_session, &ImapSession::failed, this, &ImapPartFetcher::fail);
    connect(m_session, &ImapSession::untagged, this, [this](const QByteArray &text,
                                                            const QList<QByteArray> &literals) {
        if (text.toUpper().contains(" FETCH ("))
            handleFetch(text, literals);
    });
}

void ImapPartFetcher::fetch(const QMailMessageId &id, const QString &part)
{
    m_part = part;
    m_mime.clear();
    m_body.clear();
    m_failed = false;

    const QMailMessageMetaData metaData(id);
    const QByteArray folder = ImapSession::folderPath(metaData).toUtf8();
    const QByteArray uid = ImapSession::uid(metaData);
    if (folder.isEmpty() || uid.isEmpty() || !m_session->open(metaData.parentAccountId())) {
        fail(tr("This mail isn't on an IMAP server"));
        return;
    }
    // Read-only: nothing changes on the server, not even the \Seen flag.
    m_session->command("EXAMINE " + ImapSession::quoted(folder), [this](bool ok, const QByteArray &text) {
        if (!ok) {
            qWarning() << "IMAP EXAMINE:" << text;
            fail(tr("The mail's folder isn't on the server any more"));
        }
    });
    m_session->command("UID FETCH " + uid + " (BODY.PEEK[" + m_part.toLatin1() + ".MIME] BODY.PEEK["
                       + m_part.toLatin1() + "])", [this](bool ok, const QByteArray &text) {
        if (m_failed)
            return;
        const bool fetched = !m_mime.isEmpty() && !m_body.isEmpty();
        if (!ok || !fetched) {
            qWarning() << "IMAP FETCH:" << text << "complete:" << fetched;
            fail(tr("The mail isn't on the server any more"));
            return;
        }
        m_session->logout();
        emit finished(m_mime + m_body);
    });
}

void ImapPartFetcher::handleFetch(const QByteArray &text, const QList<QByteArray> &literals)
{
    // Each literal follows its item's name: "BODY[1.MIME] {n}".
    const QList<QByteArray> before = text.split(ImapSession::literalMarker);
    const QByteArray mimeSection = m_part.toLatin1() + ".MIME";
    const QByteArray bodySection = m_part.toLatin1();
    for (int i = 0; i < literals.size() && i < before.size(); ++i) {
        const QByteArray item = before.at(i).toUpper();
        const int start = item.lastIndexOf("BODY[");
        const int end = item.indexOf(']', start);
        if (start < 0 || end < 0)
            continue;
        const QByteArray section = item.mid(start + 5, end - start - 5);
        if (section == mimeSection)
            m_mime = literals.at(i);
        else if (section == bodySection)
            m_body = literals.at(i);
    }
}

void ImapPartFetcher::fail(const QString &reason)
{
    if (m_failed)
        return;
    m_failed = true;
    m_session->logout();
    emit failed(reason);
}
