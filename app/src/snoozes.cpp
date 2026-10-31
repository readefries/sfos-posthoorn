// SPDX-License-Identifier: AGPL-3.0-or-later
#include "snoozes.h"

#include "appstate.h"

#include <limits>

namespace {

const QString group = QStringLiteral("snoozed");

} // namespace

Snoozes::Snoozes(QObject *parent)
    : QObject(parent)
    , m_settings(AppState::fileName(), QSettings::IniFormat)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &Snoozes::due);
    scheduleNext();
}

QVariantList Snoozes::messageIds() const
{
    QVariantList ids;
    const QString prefix = group + QLatin1Char('/');
    for (const QString &key : m_settings.allKeys()) {
        if (key.startsWith(prefix))
            ids.append(key.mid(prefix.size()).toInt());
    }
    return ids;
}

void Snoozes::snooze(int messageId, const QDateTime &until)
{
    if (messageId <= 0 || !until.isValid())
        return;
    m_settings.setValue(group + QLatin1Char('/') + QString::number(messageId), until.toUTC().toString(Qt::ISODate));
    m_settings.sync();
    scheduleNext();
    emit changed();
}

void Snoozes::unsnooze(int messageId)
{
    const QString key = group + QLatin1Char('/') + QString::number(messageId);
    if (!m_settings.contains(key))
        return;
    m_settings.remove(key);
    m_settings.sync();
    scheduleNext();
    emit changed();
}

QDateTime Snoozes::until(int messageId) const
{
    const QString value = m_settings.value(group + QLatin1Char('/') + QString::number(messageId)).toString();
    if (value.isEmpty())
        return QDateTime();
    QDateTime time = QDateTime::fromString(value, Qt::ISODate);
    time.setTimeSpec(Qt::UTC);
    return time.toLocalTime();
}

QVariantList Snoozes::takeDue()
{
    const QDateTime now = QDateTime::currentDateTime();
    QVariantList due;
    for (const QVariant &id : messageIds()) {
        if (until(id.toInt()) <= now) {
            due.append(id);
            m_settings.remove(group + QLatin1Char('/') + id.toString());
        }
    }
    if (!due.isEmpty()) {
        m_settings.sync();
        emit changed();
    }
    scheduleNext();
    return due;
}

void Snoozes::scheduleNext()
{
    m_timer.stop();
    QDateTime next;
    for (const QVariant &id : messageIds()) {
        const QDateTime time = until(id.toInt());
        if (!next.isValid() || time < next)
            next = time;
    }
    if (!next.isValid())
        return;
    // Right away when it has passed; QTimer takes an int, so far ahead it checks again later.
    const qint64 ms = qMax<qint64>(0, QDateTime::currentDateTime().msecsTo(next));
    m_timer.start(int(qMin<qint64>(ms, std::numeric_limits<int>::max())));
}
