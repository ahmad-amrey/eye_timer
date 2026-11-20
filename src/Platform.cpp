#include "Platform.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shobjidl.h>
#endif

namespace platform {

#if defined(Q_OS_LINUX)
namespace {
// Fire-and-forget helper whose output is discarded, so a missing service
// (e.g. no GNOME Shell) does not print an error every few seconds.
void startQuietly(const QString &program, const QStringList &arguments)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setStandardOutputFile(QProcess::nullDevice());
    process.setStandardErrorFile(QProcess::nullDevice());
    process.startDetached();
}
} // namespace
#endif

QString resourcePath(const QString &fileName)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates = {
        appDir,
        appDir + QStringLiteral("/../Resources"),
        appDir + QStringLiteral("/../share/eye-timer"),
    };
#ifdef EYETIMER_ASSET_DIR
    candidates << QStringLiteral(EYETIMER_ASSET_DIR);
#endif
    for (const QString &base : std::as_const(candidates)) {
        const QString path = QDir(base).filePath(fileName);
        if (QFileInfo::exists(path))
            return QDir::cleanPath(path);
    }
    return QDir(appDir).filePath(fileName);
}

QString appDataDirectory()
{
#if defined(Q_OS_WIN)
    QString base = qEnvironmentVariable("LOCALAPPDATA");
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/AppData/Local");
    return QDir(base).filePath(QStringLiteral("EyeTimer"));
#elif defined(Q_OS_MACOS)
    return QDir::homePath() + QStringLiteral("/Library/Application Support/EyeTimer");
#else
    QString base = qEnvironmentVariable("XDG_DATA_HOME");
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.local/share");
    return QDir(base).filePath(QStringLiteral("EyeTimer"));
#endif
}

void setWindowsAppUserModelId()
{
#ifdef Q_OS_WIN
    SetCurrentProcessExplicitAppUserModelID(L"EyeTimer.App");
#endif
}

QString startupMomentName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("Windows");
#else
    return QStringLiteral("login");
#endif
}

void reinforceAlwaysOnTop(const QString &windowTitle)
{
#if defined(Q_OS_LINUX)
    if (!QStandardPaths::findExecutable(QStringLiteral("wmctrl")).isEmpty()) {
        startQuietly(QStringLiteral("wmctrl"),
                                {QStringLiteral("-r"), windowTitle, QStringLiteral("-b"), QStringLiteral("add,above")});
        return;
    }
    if (!QStandardPaths::findExecutable(QStringLiteral("xdotool")).isEmpty()) {
        startQuietly(QStringLiteral("xdotool"),
                                {QStringLiteral("search"), QStringLiteral("--name"), windowTitle,
                                 QStringLiteral("set_window"), QStringLiteral("--overrideredirect"), QStringLiteral("1")});
        return;
    }
    if (!QStandardPaths::findExecutable(QStringLiteral("gdbus")).isEmpty()) {
        QString safeTitle = windowTitle;
        safeTitle.replace(QLatin1Char('\''), QStringLiteral("\\'"));
        const QString script = QStringLiteral(
                                   "global.get_window_actors().forEach(a => { let w = a.meta_window;"
                                   " if (w.title === '%1') w.make_above(); })")
                                   .arg(safeTitle);
        startQuietly(QStringLiteral("gdbus"),
                                {QStringLiteral("call"), QStringLiteral("--session"),
                                 QStringLiteral("--dest"), QStringLiteral("org.gnome.Shell"),
                                 QStringLiteral("--object-path"), QStringLiteral("/org/gnome/Shell"),
                                 QStringLiteral("--method"), QStringLiteral("org.gnome.Shell.Eval"), script});
    }
#else
    Q_UNUSED(windowTitle);
#endif
}

QString activeWindowId()
{
#if defined(Q_OS_LINUX)
    if (QStandardPaths::findExecutable(QStringLiteral("xdotool")).isEmpty())
        return {};
    QProcess process;
    process.start(QStringLiteral("xdotool"), {QStringLiteral("getactivewindow")});
    if (!process.waitForFinished(1000) || process.exitCode() != 0)
        return {};
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
#else
    return {};
#endif
}

void restoreActiveWindow(const QString &windowId)
{
#if defined(Q_OS_LINUX)
    if (windowId.isEmpty())
        return;
    startQuietly(QStringLiteral("xdotool"), {QStringLiteral("windowactivate"), QStringLiteral("--sync"), windowId});
#else
    Q_UNUSED(windowId);
#endif
}

} // namespace platform
