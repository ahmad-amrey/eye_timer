#include "Autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

#ifdef Q_OS_WIN
#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>
#endif

namespace autostart {
namespace {

QString executablePath()
{
    return QCoreApplication::applicationFilePath();
}

bool writeTextFile(const QString &path, const QByteArray &content, QString *error)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (error)
            *error = QStringLiteral("Could not create %1").arg(QFileInfo(path).absolutePath());
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(content);
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

bool removeFile(const QString &path, QString *error)
{
    if (!QFileInfo::exists(path) || QFile::remove(path))
        return true;
    if (error)
        *error = QStringLiteral("Could not remove %1").arg(QDir::toNativeSeparators(path));
    return false;
}

#ifdef Q_OS_WIN
QString startupShortcutPath()
{
    QString appData = qEnvironmentVariable("APPDATA");
    if (appData.isEmpty())
        appData = QDir::homePath() + QStringLiteral("/AppData/Roaming");
    return QDir(appData).filePath(
        QStringLiteral("Microsoft/Windows/Start Menu/Programs/Startup/Eye Timer.lnk"));
}

bool samePath(const QString &left, const QString &right)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(left)).compare(QDir::cleanPath(QDir::fromNativeSeparators(right)),
                                                                    Qt::CaseInsensitive)
           == 0;
}

// Task Manager's Startup tab keeps its on/off switch per shortcut name under
// StartupApproved. Removing the value means "enabled" again.
void clearStartupApproval()
{
    const std::wstring name = QFileInfo(startupShortcutPath()).fileName().toStdWString();
    RegDeleteKeyValueW(HKEY_CURRENT_USER,
                       L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\StartupFolder",
                       name.c_str());
}

QString shortcutTarget(const QString &shortcutPath)
{
    if (!QFileInfo::exists(shortcutPath))
        return {};
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    QString target;
    IShellLinkW *link = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                                   reinterpret_cast<void **>(&link)))) {
        IPersistFile *file = nullptr;
        if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&file)))) {
            const std::wstring path = QDir::toNativeSeparators(shortcutPath).toStdWString();
            wchar_t buffer[MAX_PATH] = {};
            if (SUCCEEDED(file->Load(path.c_str(), STGM_READ))
                && SUCCEEDED(link->GetPath(buffer, MAX_PATH, nullptr, SLGP_RAWPATH)))
                target = QString::fromWCharArray(buffer);
            file->Release();
        }
        link->Release();
    }
    if (SUCCEEDED(init))
        CoUninitialize();
    return target;
}

bool createShortcut(const QString &shortcutPath, const QString &target, QString *error)
{
    if (!QDir().mkpath(QFileInfo(shortcutPath).absolutePath())) {
        if (error)
            *error = QStringLiteral("Could not create the Startup folder");
        return false;
    }

    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool uninitialize = SUCCEEDED(init);

    IShellLinkW *link = nullptr;
    HRESULT result = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                                      reinterpret_cast<void **>(&link));
    if (SUCCEEDED(result)) {
        const std::wstring targetPath = QDir::toNativeSeparators(target).toStdWString();
        const std::wstring workingDir = QDir::toNativeSeparators(QFileInfo(target).absolutePath()).toStdWString();
        link->SetPath(targetPath.c_str());
        link->SetWorkingDirectory(workingDir.c_str());
        link->SetDescription(L"Eye Timer");

        IPersistFile *file = nullptr;
        result = link->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&file));
        if (SUCCEEDED(result)) {
            const std::wstring path = QDir::toNativeSeparators(shortcutPath).toStdWString();
            result = file->Save(path.c_str(), TRUE);
            file->Release();
        }
        link->Release();
    }

    if (uninitialize)
        CoUninitialize();

    if (FAILED(result)) {
        if (error)
            *error = QStringLiteral("Could not create the startup shortcut (0x%1)")
                         .arg(static_cast<quint32>(result), 8, 16, QLatin1Char('0'));
        return false;
    }
    return true;
}
#endif

#ifdef Q_OS_MACOS
QString launchAgentPath()
{
    return QDir::homePath() + QStringLiteral("/Library/LaunchAgents/com.eyetimer.app.plist");
}

QString xmlEscape(QString value)
{
    return value.replace(QLatin1Char('&'), QStringLiteral("&amp;"))
        .replace(QLatin1Char('<'), QStringLiteral("&lt;"))
        .replace(QLatin1Char('>'), QStringLiteral("&gt;"));
}
#endif

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
QString desktopEntryPath()
{
    QString configHome = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (configHome.isEmpty())
        configHome = QDir::homePath() + QStringLiteral("/.config");
    return QDir(configHome).filePath(QStringLiteral("autostart/eye-timer.desktop"));
}

// Quotes an Exec= argument per the Desktop Entry spec.
QString desktopExecArg(const QString &value)
{
    bool needsQuotes = value.isEmpty();
    for (const QChar ch : value) {
        if (ch.isSpace() || ch == QLatin1Char('"') || ch == QLatin1Char('\\') || ch == QLatin1Char('$')
            || ch == QLatin1Char('`')) {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes)
        return value;
    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"))
        .replace(QLatin1Char('"'), QStringLiteral("\\\""))
        .replace(QLatin1Char('$'), QStringLiteral("\\$"))
        .replace(QLatin1Char('`'), QStringLiteral("\\`"));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}
#endif

} // namespace

bool setEnabled(bool enabled, QString *error, const QString &executable)
{
    const QString target = executable.isEmpty() ? executablePath() : executable;
#if defined(Q_OS_WIN)
    const QString shortcut = startupShortcutPath();
    // Turning it on or off here is an explicit choice, so it also replaces
    // whatever was set for this entry in Task Manager.
    clearStartupApproval();
    if (!enabled)
        return removeFile(shortcut, error);
    return createShortcut(shortcut, target, error);
#elif defined(Q_OS_MACOS)
    const QString path = launchAgentPath();
    if (!enabled)
        return removeFile(path, error);
    const QString plist = QStringLiteral(
                              "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                              "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
                              "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
                              "<plist version=\"1.0\">\n<dict>\n"
                              "  <key>Label</key>\n  <string>com.eyetimer.app</string>\n"
                              "  <key>ProgramArguments</key>\n  <array>\n    <string>%1</string>\n  </array>\n"
                              "  <key>RunAtLoad</key>\n  <true/>\n"
                              "  <key>KeepAlive</key>\n  <false/>\n"
                              "</dict>\n</plist>\n")
                              .arg(xmlEscape(target));
    return writeTextFile(path, plist.toUtf8(), error);
#else
    const QString path = desktopEntryPath();
    if (!enabled)
        return removeFile(path, error);
    const QString entry = QStringLiteral(
                              "[Desktop Entry]\n"
                              "Type=Application\n"
                              "Name=Eye Timer\n"
                              "Exec=%1\n"
                              "Hidden=false\n"
                              "X-GNOME-Autostart-enabled=true\n")
                              .arg(desktopExecArg(target));
    return writeTextFile(path, entry.toUtf8(), error);
#endif
}

bool isRegistered()
{
#if defined(Q_OS_WIN)
    const QString target = shortcutTarget(startupShortcutPath());
    return !target.isEmpty() && samePath(target, executablePath());
#elif defined(Q_OS_MACOS)
    QFile file(launchAgentPath());
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QString needle = QStringLiteral("<string>%1</string>").arg(xmlEscape(executablePath()));
    return QString::fromUtf8(file.readAll()).contains(needle);
#else
    QFile file(desktopEntryPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    const QString exec = QStringLiteral("Exec=") + desktopExecArg(executablePath());
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        if (stream.readLine().trimmed() == exec)
            return true;
    }
    return false;
#endif
}

void sync(bool wanted)
{
    if (wanted && !isRegistered())
        setEnabled(true, nullptr);
}

} // namespace autostart
