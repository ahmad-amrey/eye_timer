#pragma once

#include "Config.h"
#include "IdleDetector.h"
#include "SoundPlayer.h"
#include "Theme.h"

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
    int breakRemainingSeconds() const;
    qreal breakRemainingFraction() const; // 1.0 at the start of a break, 0.0 at its end
    qreal workProgress() const;           // 0.0 right after a break, 1.0 when the next one starts
    QString countdownText() const;        // "18:42"
    QString remainingText() const;        // "Next break in 18m 42s"

    // Context menu shared by the tray icon and the floating countdown.
    QMenu *createActionsMenu(QWidget *parent);

    bool applySettings(const AppConfig &config, QString *savedPath, QString *error);
    void previewTone(const QString &event);

    // Renders every window to PNG files in `directory` (used to review the UI).
    bool renderPreviews(const QString &directory);

public slots:
    void startBreak();
    void finishBreak(bool skipped);
    void skipBreak() { finishBreak(true); }
    void resetWorkTimer();
    void showSettingsWindow();
    void toggleCountdownWindow();
    void showCountdownWindow();
    void hideCountdownWindow();
    void dismissPreparePanel();
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
    void playTone(const QString &event);
    QString tonePath(const QString &event) const;

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
    QPointer<PreparePanel> m_preparePanel;
    CountdownWindow *m_countdown = nullptr;
    QPointer<SettingsWindow> m_settings;

    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_trayMenu = nullptr;
    QList<QPointer<TrayHeader>> m_trayHeaders;
    QList<QPointer<QAction>> m_statusActions;
    QList<QPointer<QAction>> m_toggleActions;
    QString m_trayIconKey;
};
