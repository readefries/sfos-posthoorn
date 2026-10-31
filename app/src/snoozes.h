// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef SNOOZES_H
#define SNOOZES_H

#include <QDateTime>
#include <QObject>
#include <QSettings>
#include <QTimer>
#include <QVariantList>

// Snoozed mail, as the Snoozes singleton: which messages are hidden from the lists until when.
// Only on the phone: the mail stays where it is on the server. Kept in state.ini.
//
// When a message's time comes, due() is emitted, and the app wakes the messages up with
// takeDue(), marking them unread. If the app isn't running then, that happens when it starts.
class Snoozes : public QObject
{
    Q_OBJECT
    // The snoozed messages' ids, for the lists to leave out.
    Q_PROPERTY(QVariantList messageIds READ messageIds NOTIFY changed)

public:
    explicit Snoozes(QObject *parent = nullptr);

    QVariantList messageIds() const;

    Q_INVOKABLE void snooze(int messageId, const QDateTime &until);
    Q_INVOKABLE void unsnooze(int messageId);
    // Invalid when the message isn't snoozed.
    Q_INVOKABLE QDateTime until(int messageId) const;
    // The messages whose time has come, which are no longer snoozed then.
    Q_INVOKABLE QVariantList takeDue();

signals:
    void changed();
    void due();

private:
    void scheduleNext();

    QSettings m_settings;
    QTimer m_timer;
};

#endif // SNOOZES_H
