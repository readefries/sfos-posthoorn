// SPDX-License-Identifier: AGPL-3.0-or-later
#include "appstate.h"

#include <QStandardPaths>

AppState::AppState(QObject *parent)
    : QObject(parent)
    , m_settings(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                 + QStringLiteral("/state.ini"), QSettings::IniFormat)
{
    m_page = m_settings.value(QStringLiteral("last/page"), QStringLiteral("accounts")).toString();
    m_accountId = m_settings.value(QStringLiteral("last/accountId"), 0).toInt();
    m_folderId = m_settings.value(QStringLiteral("last/folderId"), 0).toInt();
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
