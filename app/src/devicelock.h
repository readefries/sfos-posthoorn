// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef DEVICELOCK_H
#define DEVICELOCK_H

#include <QObject>

// Tells when the phone locks: nemo-devicelock's state on the system bus, the lock behind the lock
// code rather than the screen's. Without a lock code it never locks.
class DeviceLock : public QObject
{
    Q_OBJECT

public:
    explicit DeviceLock(QObject *parent = nullptr);

signals:
    void locked();

private slots:
    void onStateChanged(int state);
};

#endif // DEVICELOCK_H
