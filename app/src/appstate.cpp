// SPDX-License-Identifier: AGPL-3.0-or-later
#include "appstate.h"

#include <QStandardPaths>

QString AppState::fileName()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + QStringLiteral("/state.ini");
}

AppState::AppState(QObject *parent)
    : QObject(parent)
    , m_settings(fileName(), QSettings::IniFormat)
{
    m_page = m_settings.value(QStringLiteral("last/page"), QStringLiteral("accounts")).toString();
    m_accountId = m_settings.value(QStringLiteral("last/accountId"), 0).toInt();
    m_folderId = m_settings.value(QStringLiteral("last/folderId"), 0).toInt();
    m_unifiedConfigured = m_settings.contains(QStringLiteral("unified/folders"));
    m_unifiedFolders = m_settings.value(QStringLiteral("unified/folders")).toStringList();
    m_unifiedFolders.removeAll(QString());
    m_favourites = m_settings.value(QStringLiteral("favourites/folders")).toStringList();
    m_favourites.removeAll(QString());
    m_conversations = m_settings.value(QStringLiteral("lists/conversations"), true).toBool();
}

void AppState::setConversations(bool conversations)
{
    if (conversations == m_conversations)
        return;
    m_conversations = conversations;
    m_settings.setValue(QStringLiteral("lists/conversations"), conversations);
    m_settings.sync();
    emit conversationsChanged();
}

bool AppState::isFavourite(int accountId, int folderId) const
{
    return m_favourites.contains(QStringLiteral("%1/%2").arg(accountId).arg(folderId));
}

void AppState::setFavourite(int accountId, int folderId, bool favourite)
{
    const QString entry = QStringLiteral("%1/%2").arg(accountId).arg(folderId);
    if (favourite == m_favourites.contains(entry))
        return;
    if (favourite)
        m_favourites.append(entry);
    else
        m_favourites.removeAll(entry);
    if (m_favourites.isEmpty())
        m_settings.remove(QStringLiteral("favourites/folders"));
    else
        m_settings.setValue(QStringLiteral("favourites/folders"), m_favourites);
    m_settings.sync();
    emit favouritesChanged();
}

void AppState::setUnifiedFolders(const QStringList &folders)
{
    if (m_unifiedConfigured && folders == m_unifiedFolders)
        return;
    m_unifiedConfigured = true;
    m_unifiedFolders = folders;
    // An empty list must still count as chosen, which QSettings doesn't store as a list.
    m_settings.setValue(QStringLiteral("unified/folders"), folders.isEmpty() ? QVariant(QString()) : QVariant(folders));
    m_settings.sync();
    emit unifiedChanged();
}

void AppState::resetUnifiedFolders()
{
    if (!m_unifiedConfigured)
        return;
    m_unifiedConfigured = false;
    m_unifiedFolders.clear();
    m_settings.remove(QStringLiteral("unified/folders"));
    m_settings.sync();
    emit unifiedChanged();
}

void AppState::save(const QString &page, int accountId, int folderId)
{
    if (page == m_page && accountId == m_accountId && folderId == m_folderId)
        return;
    m_page = page;
    m_accountId = accountId;
    m_folderId = folderId;
    m_settings.setValue(QStringLiteral("last/page"), page);
    m_settings.setValue(QStringLiteral("last/accountId"), accountId);
    m_settings.setValue(QStringLiteral("last/folderId"), folderId);
    // Written right away: the app is usually closed from the switcher, without a clean exit.
    m_settings.sync();
    emit changed();
}
