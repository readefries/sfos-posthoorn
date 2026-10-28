// SPDX-License-Identifier: AGPL-3.0-or-later
//
// posthoorn: the UI. Mail itself is handled by the Qt Messaging Framework's messageserver, which
// the app talks to through the Nemo.Email QML plugin.

#include "messagebody.h"

#include <sailfishapp.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQmlEngine>
#include <QQuickView>
#include <QScopedPointer>
#include <QStandardPaths>

namespace {

QFile *logFile = nullptr;
QtMessageHandler previousHandler = nullptr;

// Also writes everything to ~/.cache/nl.readefries/posthoorn/posthoorn.log: started from the
// launcher, the app's output ends up in a journal that's only readable as root.
void logToFile(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    if (logFile) {
        static const char *const levels[] = { "D", "W", "C", "F", "I" };
        const QString line = QStringLiteral("%1 [%2] %3: %4\n")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")),
                     QLatin1String(levels[type]),
                     QLatin1String(context.category),
                     message);
        logFile->write(line.toUtf8());
        logFile->flush();
    }
    if (previousHandler)
        previousHandler(type, context, message);
}

void startFileLog()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(dir);
    logFile = new QFile(dir + QStringLiteral("/posthoorn.log"));
    if (!logFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        delete logFile;
        logFile = nullptr;
        return;
    }
    previousHandler = qInstallMessageHandler(logToFile);
}

} // namespace

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    // Must match OrganizationName and ApplicationName in posthoorn.desktop.
    app->setOrganizationName(QStringLiteral("nl.readefries"));
    app->setApplicationName(QStringLiteral("posthoorn"));
    app->setApplicationVersion(QStringLiteral(POSTHOORN_VERSION));

    startFileLog();
    // The email plugin only logs warnings by default.
    QLoggingCategory::setFilterRules(QStringLiteral("org.nemomobile.email.debug=true"));

    qmlRegisterType<MessageBody>("Posthoorn", 1, 0, "MessageBody");

    QScopedPointer<QQuickView> view(SailfishApp::createView());
    view->setSource(SailfishApp::pathToMainQml());
    view->show();
    return app->exec();
}
