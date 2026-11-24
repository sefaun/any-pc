#include "core/Logging.h"

#include "BuildInfo.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextStream>

#include <cstdio>

namespace anypc::Logging {

namespace {

constexpr qint64 kMaxFileSize = 5 * 1024 * 1024;

QMutex g_mutex;
QFile *g_file = nullptr;
QtMessageHandler g_previous = nullptr;

const char *levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return "DEBUG";
    case QtInfoMsg: return "INFO ";
    case QtWarningMsg: return "WARN ";
    case QtCriticalMsg: return "ERROR";
    case QtFatalMsg: return "FATAL";
    }
    return "?????";
}

void rotateIfNeeded()
{
    if (!g_file || g_file->size() < kMaxFileSize)
        return;
    const QString path = g_file->fileName();
    g_file->close();
    QFile::remove(path + QStringLiteral(".1"));
    QFile::rename(path, path + QStringLiteral(".1"));
    g_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

void handler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    const QString line = QStringLiteral("%1 %2 [%3] %4\n")
                             .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                  QString::fromLatin1(levelName(type)),
                                  QString::fromLatin1(context.category ? context.category : "default"), message);
    {
        QMutexLocker lock(&g_mutex);
        if (g_file && g_file->isOpen()) {
            g_file->write(line.toUtf8());
            g_file->flush(); // Keep the tail intact if the process crashes.
            rotateIfNeeded();
        }
    }
    if (g_previous)
        g_previous(type, context, message);
    else
        std::fputs(line.toLocal8Bit().constData(), stderr);
}

} // namespace

QString directory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/logs");
}

QString currentFile()
{
    return directory() + QStringLiteral("/anypc.log");
}

void install()
{
    QDir().mkpath(directory());
    g_file = new QFile(currentFile());
    if (!g_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        delete g_file;
        g_file = nullptr;
    }
    g_previous = qInstallMessageHandler(handler);
    qInfo("==== AnyPC %s starting on %s (%s, Qt %s) ====", build::kVersion,
          qPrintable(QSysInfo::prettyProductName()), qPrintable(QSysInfo::currentCpuArchitecture()), qVersion());
}

} // namespace anypc::Logging
