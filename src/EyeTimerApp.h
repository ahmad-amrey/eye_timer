#pragma once

#include "Config.h"
#include "IdleDetector.h"
#include "SoundPlayer.h"
#include "Theme.h"
#include "UpdateChecker.h"

#include <QDate>
#include <QElapsedTimer>
#include <QIcon>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <optional>

class BreakOverlay;
class CountdownWindow;
class PreparePanel;
class QAction;
class QApplication;
class QDialog;
class QMenu;
class QSystemTrayIcon;
class SettingsWindow;
class TrayHeader;

class EyeTimerApp : public QObject
{
    Q_OBJECT

public:
    enum class Phase { Counting, Soon, Paused, Break };

    struct Options {
        bool createTrayIcon = true;
        bool startTimer = true;
        bool checkForUpdates = true;
    };

    EyeTimerApp(QApplication *app, const AppConfig &config, Options options);
    ~EyeTimerApp() override;

    const AppConfig &config() const { return m_config; }
    const theme::Palette &palette() const;
    bool trayAvailable() const { return m_trayAvailable; }
    bool hasTrayIcon() const { return m_tray != nullptr; }
    bool isStopping() const { return m_stopping; }
    QIcon appIcon() const { return m_appIcon; }

    Phase phase() const;
    int secondsUntilBreak() const;
    bool isOnBreak() const { return m_breakStartedMs.has_value(); }
    int breakRemainingSeconds() const;
    qreal breakRemainingFraction() const; // 1.0 at the start of a break, 0.0 at its end
    qreal workProgress() const;           // 0.0 right after a break, 1.0 when the next one starts
    QString countdownText() const;        // "18:42"
    QString remainingText() const;        // "Next break in 18m 42s"

    // Context menu shared by the tray icon and the floating countdown.
    QMenu *createActionsMenu(QWidget *parent);

    bool applySettings(const AppConfig &config, QString *savedPath, QString *error);
    void previewTone(const QString &event);
    static QString tonePath(const QString &event); // "start" or "end"

    // Renders every window to PNG files in `directory` (used to review the UI).
    bool renderPreviews(const QString &directory);

public slots:
    void startBreak();
    void finishBreak(bool skipped);
    void skipBreak() { finishBreak(true); }
    // A key other than Esc was pressed during a break: shake every screen's
    // skip button to show that keys are held back and how to leave.
    void remindHowToSkip();
    void resetWorkTimer();
    void showSettingsWindow();
    void toggleCountdownWindow();
    void showCountdownWindow();
    void hideCountdownWindow();
    void dismissPreparePanel();
    void snoozeBreak();
    void checkForUpdatesNow();
    void showUpdateDialog();
    void stop();

private:
    void tick();
    bool handleResumeGap(double elapsed, double wallElapsed, qint64 now);
    bool isResumeGap(double wallElapsed) const;
    void updateWorkTimer(double elapsed, double idleSeconds);
    void updateBreakOverlay(qint64 now);
    void showOrUpdatePreparePanel(int secondsUntilBreak);
    void hidePreparePanel();
    void closeOverlays();
    void claimKeyboard();
    void playTone(const QString &event);

    void maybeCheckForUpdates();
    void handleUpdateResult(UpdateChecker::Result result, const ReleaseInfo &release, const QString &error);
    void presentUpdateResult(UpdateChecker::Result result, const ReleaseInfo &release, const QString &error);

    void setupTrayIcon();
    void applyStartupVisibility();
    void updateStatus();
    void updateMenus();
    void updateTrayIcon();

    QApplication *m_app;
    AppConfig m_config;
    IdleDetector m_idle;
    SoundPlayer m_sound;
    QTimer m_timer;
    QElapsedTimer m_clock;
    QIcon m_appIcon;

    qint64 m_lastTickMs = 0;
    qint64 m_lastWallMs = 0;
    std::optional<qint64> m_breakStartedMs;
    double m_activeSeconds = 0.0;
    double m_lastIdleSeconds = 0.0;
    int m_suggestionIndex = 0;
    int m_breaksToday = 0;
    QDate m_breakDay;
    bool m_prepareDismissed = false;
    bool m_stopping = false;
    bool m_trayAvailable = false;

    QList<QPointer<BreakOverlay>> m_overlays;
    QTimer m_keyboardTimer; // takes the keyboard back while a break is shown
    QPointer<PreparePanel> m_preparePanel;
    CountdownWindow *m_countdown = nullptr;
    QPointer<SettingsWindow> m_settings;

    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_trayMenu = nullptr;
    QList<QPointer<TrayHeader>> m_trayHeaders;
    QList<QPointer<QAction>> m_statusActions;
    QList<QPointer<QAction>> m_toggleActions;
    QList<QPointer<QAction>> m_updateActions;

    UpdateChecker *m_updates = nullptr;
    QTimer m_updateTimer;
    bool m_manualUpdateCheck = false;
    std::optional<ReleaseInfo> m_availableUpdate;
    QPointer<QDialog> m_updateDialog;
    QString m_trayIconKey;
};
