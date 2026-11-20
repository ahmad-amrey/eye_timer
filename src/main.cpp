#include "Config.h"
#include "EyeTimerApp.h"
#include "Platform.h"
#include "SoundPlayer.h"
#include "Theme.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QLockFile>
#include <QMessageBox>
#include <QTimer>

int main(int argc, char *argv[])
{
#ifdef Q_OS_LINUX
    // Always-on-top, positioning and focus control are unreliable for
    // frameless windows on native Wayland; XWayland behaves.
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "xcb");
#endif
    platform::setWindowsAppUserModelId();

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Eye Timer"));
    QApplication::setApplicationDisplayName(QStringLiteral("Eye Timer"));
    QApplication::setApplicationVersion(QStringLiteral(EYETIMER_VERSION));
    QApplication::setOrganizationName(QStringLiteral("EyeTimer"));
    QApplication::setDesktopFileName(QString::fromLatin1(platform::kAppUserModelId));
    QApplication::setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Eye-rest reminder"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption previewOption(QStringLiteral("render-previews"),
                                           QStringLiteral("Render every window to PNG files in <dir> and exit."),
                                           QStringLiteral("dir"));
    parser.addOption(previewOption);
    const QCommandLineOption toneOption(QStringLiteral("play-tone"),
                                        QStringLiteral("Play the \"start\" or \"end\" tone and exit."),
                                        QStringLiteral("tone"));
    parser.addOption(toneOption);
    parser.process(app);
    const bool previewMode = parser.isSet(previewOption);

    if (parser.isSet(toneOption)) {
        SoundPlayer player;
        player.play(EyeTimerApp::tonePath(parser.value(toneOption)));
        QTimer::singleShot(4000, &app, &QApplication::quit);
        return app.exec();
    }

    // One running copy per user. QLockFile cleans up after crashes.
    QLockFile lock(QDir::temp().filePath(QStringLiteral("eye_timer.lock")));
    if (!previewMode) {
        lock.setStaleLockTime(0);
        if (!lock.tryLock(100)) {
            QMessageBox::information(nullptr, QStringLiteral("Eye Timer"),
                                     QStringLiteral("Eye Timer is already running in the system tray."));
            return 0;
        }
    }

    theme::loadBundledFonts();
    QApplication::setFont(theme::font(14));
    app.setStyleSheet(theme::applicationStyleSheet());

    const AppConfig config = loadSavedConfig();
    EyeTimerApp controller(&app, config,
                           {.createTrayIcon = !previewMode, .startTimer = !previewMode, .checkForUpdates = !previewMode});
    QApplication::setWindowIcon(controller.appIcon());

    if (previewMode)
        return controller.renderPreviews(parser.value(previewOption)) ? 0 : 1;
    return app.exec();
}
