// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef LINKS_H
#define LINKS_H

#include "linkcleaner.h"

#include <QObject>
#include <QVariantMap>

// LinkCleaner for QML, as the Links singleton.
class Links : public QObject
{
    Q_OBJECT

public:
    explicit Links(QObject *parent = nullptr) : QObject(parent) { }

    // { url, unwrapped, stripped, changed }
    Q_INVOKABLE QVariantMap clean(const QString &url) const
    {
        const LinkCleaner::Result result = LinkCleaner::clean(url);
        QVariantMap map;
        map.insert(QStringLiteral("url"), result.url);
        map.insert(QStringLiteral("unwrapped"), result.unwrapped);
        map.insert(QStringLiteral("stripped"), result.stripped);
        map.insert(QStringLiteral("changed"), result.changed());
        return map;
    }
};

#endif // LINKS_H
