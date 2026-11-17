// SPDX-License-Identifier: AGPL-3.0-or-later
#include "devicelock.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDebug>

namespace {

const QString service = QStringLiteral("org.nemomobile.devicelock");
const QString path = QStringLiteral("/devicelock");
const QString interface = QStringLiteral("org.nemomobile.lipstick.devicelock");
// Its states: Unlocked, Locked, ManagerLockout, TemporaryLockout, PermanentLockout, Undefined.
const int unlocked = 0;
const int undefined = 5;

} // namespace

DeviceLock::DeviceLock(QObject *parent)
    : QObject(parent)
{
    if (!QDBusConnection::systemBus().connect(service, path, interface, QStringLiteral("stateChanged"),
                                              this, SLOT(onStateChanged(int)))) {
        qWarning() << "Can't follow the device lock:" << QDBusConnection::systemBus().lastError().message();
    }
}

void DeviceLock::onStateChanged(int state)
{
    if (state != unlocked && state != undefined)
        emit locked();
}
