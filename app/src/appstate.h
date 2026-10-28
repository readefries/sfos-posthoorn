// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef APPSTATE_H
#define APPSTATE_H

#include <QObject>
#include <QSettings>

// Where the app was left, so it can open there again: the account overview, an account's
// folders, or a folder's messages. As the AppState singleton.
//
// Kept in ~/.config/nl.readefries/posthoorn/state.ini: QSettings' default file is one level up,
// outside what the sandbox lets the app write.
class AppState : public QObject
{
    Q_OBJECT
    // "accounts", "folders" or "messages".
    Q_PROPERTY(QString page READ page NOTIFY changed)
    // 0 with page "messages" for the combined inbox.
    Q_PROPERTY(int accountId READ accountId NOTIFY changed)
    Q_PROPERTY(int folderId READ folderId NOTIFY changed)

public:
    explicit AppState(QObject *parent = nullptr);

    QString page() const { return m_page; }
    int accountId() const { return m_accountId; }
    int folderId() const { return m_folderId; }

    Q_INVOKABLE void save(const QString &page, int accountId = 0, int folderId = 0);

signals:
    void changed();

private:
    QSettings m_settings;
    QString m_page;
    int m_accountId = 0;
    int m_folderId = 0;
};

#endif // APPSTATE_H
