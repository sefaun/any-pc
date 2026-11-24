#include "BuildInfo.h"
#include "app/Application.h"
#include "core/Logging.h"
#include "rtc/WebEngineEnvironment.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>

namespace {

constexpr auto kInstanceKeyPrefix = "anypc-desktop-single-instance";

/** Returns true if another instance is running (and asks it to show itself). */
bool notifyRunningInstance(const QString &instanceKey)
{
    QLocalSocket socket;
    socket.connectToServer(instanceKey);
    if (!socket.waitForConnected(300))
        return false;
    socket.write("show");
    socket.waitForBytesWritten(300);
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    // Must be decided before QApplication exists (Chromium flags, GL backend).
    bool safeMode = qEnvironmentVariableIsSet("ANYPC_SAFE_MODE");
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--safe-mode") == 0)
            safeMode = true;
    }
    anypc::WebEngineEnvironment::prepare(safeMode);

    QApplication::setApplicationName(QString::fromLatin1(anypc::build::kAppName));
    QApplication::setApplicationDisplayName(QString::fromLatin1(anypc::build::kAppName));
    QApplication::setApplicationVersion(QString::fromLatin1(anypc::build::kVersion));
    QApplication::setOrganizationName(QString::fromLatin1(anypc::build::kOrganization));
    QApplication::setOrganizationDomain(QString::fromLatin1(anypc::build::kOrganizationDomain));

    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/anypc.svg")));
    app.setQuitOnLastWindowClosed(false);
    anypc::Theme::apply(app);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("AnyPC remote desktop"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption profileOption(
        QStringLiteral("profile"),
        QStringLiteral("Runs an isolated instance with its own settings and device identity."),
        QStringLiteral("name"));
    parser.addOption(profileOption);
    parser.addOption(QCommandLineOption(QStringLiteral("safe-mode"),
                                        QStringLiteral("Disables GPU acceleration (for faulty graphics drivers).")));
    parser.process(app);

    QString instanceKey = QString::fromLatin1(kInstanceKeyPrefix);
    const QString profile = parser.value(profileOption).trimmed();
    if (!profile.isEmpty()) {
        // QSettings picks the application name up when AppSettings is created.
        QApplication::setApplicationName(QApplication::applicationName() + QLatin1Char('-') + profile);
        instanceKey += QLatin1Char('-') + profile;
    }

    if (notifyRunningInstance(instanceKey))
        return 0;

    anypc::Logging::install();
    if (safeMode)
        qInfo("Safe mode: GPU acceleration disabled");

    QLocalServer::removeServer(instanceKey);
    QLocalServer instanceServer;
    instanceServer.listen(instanceKey);

    anypc::Application application;
    QObject::connect(&instanceServer, &QLocalServer::newConnection, &application, [&instanceServer]() {
        while (QLocalSocket *client = instanceServer.nextPendingConnection()) {
            client->deleteLater();
            for (QWidget *widget : QApplication::topLevelWidgets()) {
                if (widget->inherits("anypc::MainWindow")) {
                    widget->showNormal();
                    widget->raise();
                    widget->activateWindow();
                }
            }
        }
    });
    if (!application.start())
        return 0;
    return app.exec();
}
