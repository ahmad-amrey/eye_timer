#include "EyeTimerApp.h"

#include "Autostart.h"
#include "BreakOverlay.h"
#include "CountdownWindow.h"
#include "Platform.h"
#include "PreparePanel.h"
#include "SettingsWindow.h"
#include "Widgets.h"

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QPainter>
#include <QScreen>
#include <QSystemTrayIcon>
#include <QWidgetAction>
#include <QWindow>

namespace {
constexpr double kResumeGapMinSeconds = 5.0;
constexpr double kResumeGapTickMultiplier = 5.0;
constexpr auto kRestStartTone = "universfield-new-notification-017-352293.mp3";
constexpr auto kRestEndTone = "universfield-new-notification-017-352293-pitch-changed.mp3";

QString formatCountdown(int seconds)
{
    seconds = qMax(0, seconds);
    return QStringLiteral("%1:%2").arg(seconds / 60, 2, 10, QLatin1Char('0')).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

QString formatNextBreak(int seconds)
{
    seconds = qMax(0, seconds);
    const int minutes = seconds / 60;
    const int remainder = seconds % 60;
    if (minutes > 0)
        return QStringLiteral("Next break in %1m %2s").arg(minutes).arg(remainder);
    return QStringLiteral("Next break in %1s").arg(remainder);
}

QIcon loadAppIcon()
{
#ifdef Q_OS_WIN
    const QStringList names = {QStringLiteral("icon.ico"), QStringLiteral("icon.png")};
#else
    const QStringList names = {QStringLiteral("icon.png"), QStringLiteral("icon.ico")};
#endif
    for (const QString &name : names) {
        const QString path = platform::resourcePath(name);
        if (QFileInfo::exists(path))
            return QIcon(path);
    }
    return {};
}

// Tray icon: the same progress ring as the countdown, on a dark tile so it
// reads on both light and dark taskbars.
QIcon renderTrayIcon(qreal progress, const QColor &color)
{
    QIcon icon;
    for (const int size : {16, 20, 24, 32, 40, 48, 64}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0x12, 0x13, 0x1A));
        painter.drawRoundedRect(QRectF(0, 0, size, size), size * 0.25, size * 0.25);
        const qreal stroke = qMax(1.6, size / 11.0);
        const qreal inset = size * 0.18;
        theme::paintRing(painter, QRectF(inset, inset, size - 2 * inset, size - 2 * inset), progress, stroke * 0.8,
                         stroke, QColor(255, 255, 255, 60), {color});
        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

} // namespace

EyeTimerApp::EyeTimerApp(QApplication *app, const AppConfig &config, Options options)
    : QObject(nullptr)
    , m_app(app)
    , m_config(config)
    , m_appIcon(loadAppIcon())
{
    m_clock.start();
    m_lastTickMs = m_clock.elapsed();
    m_lastWallMs = QDateTime::currentMSecsSinceEpoch();
    m_trayAvailable = options.createTrayIcon && QSystemTrayIcon::isSystemTrayAvailable();

    m_countdown = new CountdownWindow(this);
    if (m_trayAvailable)
        setupTrayIcon();
    if (options.startTimer) {
        applyStartupVisibility();
        connect(&m_timer, &QTimer::timeout, this, &EyeTimerApp::tick);
        m_timer.start(m_config.tickMs);
    }
}

EyeTimerApp::~EyeTimerApp()
{
    closeOverlays();
    hidePreparePanel();
    delete m_settings;
    delete m_countdown;
    delete m_trayMenu;
}

const theme::Palette &EyeTimerApp::palette() const
{
    return theme::palette(m_config.palette);
}

// ------------------------------------------------------------------ state

EyeTimerApp::Phase EyeTimerApp::phase() const
{
    if (m_breakStartedMs)
        return Phase::Break;
    if (m_lastIdleSeconds >= m_config.idlePauseAfterSeconds)
        return Phase::Paused;
    if (secondsUntilBreak() <= m_config.prepareNoticeSeconds)
        return Phase::Soon;
    return Phase::Counting;
}

int EyeTimerApp::secondsUntilBreak() const
{
    return qMax(0, static_cast<int>(m_config.restIntervalSeconds - m_activeSeconds));
}

int EyeTimerApp::breakRemainingSeconds() const
{
    if (!m_breakStartedMs)
        return m_config.restDurationSeconds;
    const int elapsed = static_cast<int>((m_clock.elapsed() - *m_breakStartedMs) / 1000);
    return qMax(0, m_config.restDurationSeconds - elapsed);
}

qreal EyeTimerApp::breakRemainingFraction() const
{
    if (!m_breakStartedMs)
        return 1.0;
    const qreal elapsed = (m_clock.elapsed() - *m_breakStartedMs) / 1000.0;
    return qBound(0.0, 1.0 - elapsed / qMax(1, m_config.restDurationSeconds), 1.0);
}

qreal EyeTimerApp::workProgress() const
{
    return qBound(0.0, m_activeSeconds / qMax(1, m_config.restIntervalSeconds), 1.0);
}

QString EyeTimerApp::countdownText() const
{
    return formatCountdown(m_breakStartedMs ? breakRemainingSeconds() : secondsUntilBreak());
}

QString EyeTimerApp::remainingText() const
{
    if (m_breakStartedMs)
        return QStringLiteral("Break remaining: %1s").arg(breakRemainingSeconds());
    return formatNextBreak(secondsUntilBreak());
}

// ------------------------------------------------------------------ timer

void EyeTimerApp::tick()
{
    const qint64 now = m_clock.elapsed();
    const qint64 wallNow = QDateTime::currentMSecsSinceEpoch();
    const double elapsed = qMax<qint64>(0, now - m_lastTickMs) / 1000.0;
    const double wallElapsed = qMax<qint64>(0, wallNow - m_lastWallMs) / 1000.0;
    m_lastTickMs = now;
    m_lastWallMs = wallNow;

    if (handleResumeGap(elapsed, wallElapsed, now)) {
        updateStatus();
        return;
    }

    m_lastIdleSeconds = m_idle.seconds();
    if (!m_breakStartedMs)
        updateWorkTimer(elapsed, m_lastIdleSeconds);
    else
        updateBreakOverlay(now);
    updateStatus();
}

bool EyeTimerApp::handleResumeGap(double elapsed, double wallElapsed, qint64 now)
{
    if (!isResumeGap(wallElapsed))
        return false;

    if (m_breakStartedMs) {
        // The monotonic clock may not have advanced while the machine slept.
        const double missed = qMax(0.0, wallElapsed - elapsed);
        *m_breakStartedMs -= static_cast<qint64>(missed * 1000.0);
        updateBreakOverlay(now);
        return true;
    }

    m_prepareDismissed = false;
    hidePreparePanel();
    if (wallElapsed >= m_config.restDurationSeconds)
        m_activeSeconds = 0.0;
    return true;
}

bool EyeTimerApp::isResumeGap(double wallElapsed) const
{
    const double expectedTick = qMax(0.001, m_config.tickMs / 1000.0);
    return wallElapsed >= qMax(kResumeGapMinSeconds, expectedTick * kResumeGapTickMultiplier);
}

void EyeTimerApp::updateWorkTimer(double elapsed, double idleSeconds)
{
    if (idleSeconds >= m_config.idleResetAfterSeconds) {
        m_activeSeconds = 0.0;
        m_prepareDismissed = false;
        hidePreparePanel();
        return;
    }

    if (idleSeconds < m_config.idlePauseAfterSeconds) {
        m_activeSeconds += elapsed;
    } else {
        m_prepareDismissed = false;
        hidePreparePanel();
    }

    const double untilBreak = m_config.restIntervalSeconds - m_activeSeconds;
    if (untilBreak <= 0) {
        startBreak();
    } else if (untilBreak <= m_config.prepareNoticeSeconds) {
        if (!m_prepareDismissed)
            showOrUpdatePreparePanel(qMax(1, static_cast<int>(untilBreak)));
    } else {
        m_prepareDismissed = false;
        hidePreparePanel();
    }
}

void EyeTimerApp::startBreak()
{
    if (m_breakStartedMs)
        return;

    m_prepareDismissed = false;
    hidePreparePanel();
    m_breakStartedMs = m_clock.elapsed();
    playTone(QStringLiteral("start"));

    const QDate today = QDate::currentDate();
    if (m_breakDay != today) {
        m_breakDay = today;
        m_breaksToday = 0;
    }
    ++m_breaksToday;

    const QStringList &suggestions = m_config.healthySuggestions;
    const QString suggestion = suggestions.at(m_suggestionIndex % suggestions.size());
    ++m_suggestionIndex;

    closeOverlays();
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        auto *overlay = new BreakOverlay(this, screen, suggestion, m_breaksToday);
        // Tie the native window to its screen before showing it so the
        // compositor puts the full-screen window on the right output.
        overlay->winId();
        if (QWindow *window = overlay->windowHandle())
            window->setScreen(screen);
        overlay->showFullScreen();
        overlay->raise();
        m_overlays << overlay;
    }
    if (!m_overlays.isEmpty() && m_overlays.constFirst()) {
        m_overlays.constFirst()->activateWindow();
        m_overlays.constFirst()->setFocus();
    }
    updateBreakOverlay(m_clock.elapsed());
    updateStatus();
}

void EyeTimerApp::updateBreakOverlay(qint64 now)
{
    if (!m_breakStartedMs)
        return;
    const double elapsed = (now - *m_breakStartedMs) / 1000.0;
    const int remaining = qMax(0, m_config.restDurationSeconds - static_cast<int>(elapsed));
    for (const QPointer<BreakOverlay> &overlay : std::as_const(m_overlays)) {
        if (overlay)
            overlay->setRemaining(remaining);
    }
    if (elapsed >= m_config.restDurationSeconds)
        finishBreak(false);
}

void EyeTimerApp::finishBreak(bool skipped)
{
    if (!m_breakStartedMs)
        return;
    closeOverlays();
    m_breakStartedMs.reset();
    m_activeSeconds = 0.0;
    m_prepareDismissed = false;
    m_lastTickMs = m_clock.elapsed();
    m_lastWallMs = QDateTime::currentMSecsSinceEpoch();
    if (!skipped)
        playTone(QStringLiteral("end"));
    updateStatus();
}

void EyeTimerApp::closeOverlays()
{
    for (const QPointer<BreakOverlay> &overlay : std::as_const(m_overlays)) {
        if (overlay) {
            overlay->setAnimated(false);
            overlay->close();
            overlay->deleteLater();
        }
    }
    m_overlays.clear();
}

void EyeTimerApp::resetWorkTimer()
{
    if (m_breakStartedMs)
        return;
    m_activeSeconds = 0.0;
    m_prepareDismissed = false;
    hidePreparePanel();
    updateStatus();
}

// ------------------------------------------------------------ prepare panel

void EyeTimerApp::showOrUpdatePreparePanel(int secondsUntilBreak)
{
    if (m_prepareDismissed)
        return;

    const bool created = !m_preparePanel;
    if (created)
        m_preparePanel = new PreparePanel(this, QGuiApplication::primaryScreen());
    m_preparePanel->setCountdown(secondsUntilBreak, m_config.prepareNoticeSeconds);
    if (!created && m_preparePanel->isVisible())
        return;

    const QString focused = platform::activeWindowId();
    m_preparePanel->show();
    // Position again once the window manager has mapped the window.
    QTimer::singleShot(50, m_preparePanel.data(), &PreparePanel::reposition);
    platform::restoreActiveWindow(focused);
}

void EyeTimerApp::hidePreparePanel()
{
    if (m_preparePanel) {
        m_preparePanel->close();
        m_preparePanel->deleteLater();
        m_preparePanel = nullptr;
    }
}

void EyeTimerApp::dismissPreparePanel()
{
    m_prepareDismissed = true;
    hidePreparePanel();
}

// ------------------------------------------------------------------ sound

QString EyeTimerApp::tonePath(const QString &event) const
{
    return platform::resourcePath(QString::fromLatin1(event == QStringLiteral("start") ? kRestStartTone : kRestEndTone));
}

void EyeTimerApp::playTone(const QString &event)
{
    if (m_config.soundEnabled)
        m_sound.play(tonePath(event));
}

void EyeTimerApp::previewTone(const QString &event)
{
    m_sound.play(tonePath(event));
}

// ---------------------------------------------------------------- windows

void EyeTimerApp::applyStartupVisibility()
{
    if (m_config.startMinimized && m_tray) {
        m_countdown->hide();
        return;
    }
    showCountdownWindow();
}

void EyeTimerApp::toggleCountdownWindow()
{
    if (m_countdown->isVisible())
        hideCountdownWindow();
    else
        showCountdownWindow();
}

void EyeTimerApp::showCountdownWindow()
{
    m_countdown->show();
    m_countdown->raise();
    m_countdown->activateWindow();
    platform::reinforceAlwaysOnTop(m_countdown->windowTitle());
    updateMenus();
}

void EyeTimerApp::hideCountdownWindow()
{
    if (!m_tray) {
        m_countdown->showMinimized();
        return;
    }
    m_countdown->setExpanded(false);
    m_countdown->hide();
    updateMenus();
}

void EyeTimerApp::showSettingsWindow()
{
    if (!m_settings)
        m_settings = new SettingsWindow(this);
    else
        m_settings->loadFromConfig();
    m_settings->show();
    m_settings->raise();
    m_settings->activateWindow();
}

bool EyeTimerApp::applySettings(const AppConfig &config, QString *savedPath, QString *error)
{
    const QStringList problems = validateConfig(config);
    if (!problems.isEmpty()) {
        if (error)
            *error = problems.join(QStringLiteral("; "));
        return false;
    }

    if (config.startWithBoot != m_config.startWithBoot && !autostart::setEnabled(config.startWithBoot, error))
        return false;
    if (!saveConfig(config, savedPath, error)) {
        if (config.startWithBoot != m_config.startWithBoot)
            autostart::setEnabled(m_config.startWithBoot, nullptr);
        return false;
    }

    m_config = config;
    m_timer.setInterval(m_config.tickMs);
    m_countdown->update();
    if (m_preparePanel)
        m_preparePanel->update();
    for (const QPointer<BreakOverlay> &overlay : std::as_const(m_overlays)) {
        if (overlay)
            overlay->update();
    }
    if (m_breakStartedMs)
        updateBreakOverlay(m_clock.elapsed());
    m_trayIconKey.clear();
    updateStatus();
    return true;
}

void EyeTimerApp::stop()
{
    if (m_stopping)
        return;
    m_stopping = true;
    m_timer.stop();
    hidePreparePanel();
    closeOverlays();
    m_sound.stopAll();
    if (m_tray) {
        m_tray->hide();
        m_tray->deleteLater();
        m_tray = nullptr;
    }
    m_app->quit();
}

// ------------------------------------------------------------ tray & menus

QMenu *EyeTimerApp::createActionsMenu(QWidget *parent)
{
    auto *menu = new QMenu(parent);
    theme::styleMenu(menu);

#ifdef Q_OS_MACOS
    // macOS draws status-item menus natively and ignores embedded widgets.
    QAction *status = menu->addAction(remainingText());
    status->setEnabled(false);
    m_statusActions << status;
#else
    auto *headerAction = new QWidgetAction(menu);
    auto *header = new TrayHeader(menu);
    headerAction->setDefaultWidget(header);
    menu->addAction(headerAction);
    m_trayHeaders << header;
#endif
    menu->addSeparator();

    const QColor iconColor = theme::text(0.7);
    QAction *toggle = menu->addAction(theme::makeIcon(theme::Icon::EyeOff, iconColor), QString());
    connect(toggle, &QAction::triggered, this, &EyeTimerApp::toggleCountdownWindow);
    m_toggleActions << toggle;

    QAction *takeBreak = menu->addAction(theme::makeIcon(theme::Icon::Skip, iconColor), QStringLiteral("Take a break now"));
    connect(takeBreak, &QAction::triggered, this, &EyeTimerApp::startBreak);
    QAction *reset = menu->addAction(theme::makeIcon(theme::Icon::Reset, iconColor), QStringLiteral("Reset timer"));
    connect(reset, &QAction::triggered, this, &EyeTimerApp::resetWorkTimer);
    QAction *settings = menu->addAction(theme::makeIcon(theme::Icon::Sliders, iconColor), QStringLiteral("Settings…"));
    connect(settings, &QAction::triggered, this, &EyeTimerApp::showSettingsWindow);
    menu->addSeparator();
    QAction *quit = menu->addAction(theme::makeIcon(theme::Icon::Power, iconColor), QStringLiteral("Quit Eye Timer"));
    connect(quit, &QAction::triggered, this, &EyeTimerApp::stop);

    connect(menu, &QMenu::aboutToShow, this, [this, takeBreak, reset] {
        takeBreak->setEnabled(!m_breakStartedMs);
        reset->setEnabled(!m_breakStartedMs);
        updateMenus();
    });
    updateMenus();
    return menu;
}

void EyeTimerApp::setupTrayIcon()
{
    m_tray = new QSystemTrayIcon(this);
    m_trayMenu = createActionsMenu(nullptr);
    m_tray->setContextMenu(m_trayMenu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick
            || reason == QSystemTrayIcon::MiddleClick)
            toggleCountdownWindow();
    });
    updateTrayIcon();
    m_tray->show();
    updateStatus();
}

void EyeTimerApp::updateStatus()
{
    if (m_tray)
        m_tray->setToolTip(QStringLiteral("Eye Timer\n%1").arg(remainingText()));
    updateTrayIcon();
    updateMenus();
    m_countdown->refresh();
}

void EyeTimerApp::updateMenus()
{
    const bool inBreak = m_breakStartedMs.has_value();
    const QString caption = inBreak ? QStringLiteral("Break remaining") : QStringLiteral("Next break in");
    const qreal progress = inBreak ? breakRemainingFraction() : workProgress();
    const auto &lines = palette().lines;
    const QList<QColor> colors = {lines[0], lines[1], lines[2]};

    for (const QPointer<TrayHeader> &header : std::as_const(m_trayHeaders)) {
        if (header)
            header->setState(caption, countdownText(), progress, colors);
    }
    for (const QPointer<QAction> &action : std::as_const(m_statusActions)) {
        if (action)
            action->setText(remainingText());
    }
    const bool visible = m_countdown && m_countdown->isVisible();
    for (const QPointer<QAction> &action : std::as_const(m_toggleActions)) {
        if (!action)
            continue;
        action->setText(visible ? QStringLiteral("Hide countdown") : QStringLiteral("Show countdown"));
        action->setIcon(theme::makeIcon(visible ? theme::Icon::EyeOff : theme::Icon::Eye, theme::text(0.7)));
    }
    m_trayHeaders.removeAll(nullptr);
    m_statusActions.removeAll(nullptr);
    m_toggleActions.removeAll(nullptr);
}

void EyeTimerApp::updateTrayIcon()
{
    if (!m_tray)
        return;
    QColor color = palette().lines[0];
    qreal progress = workProgress();
    switch (phase()) {
    case Phase::Break:
        color = palette().lines[1];
        progress = breakRemainingFraction();
        break;
    case Phase::Soon:
        color = theme::warm();
        break;
    case Phase::Paused:
        color = theme::text(0.45);
        break;
    case Phase::Counting:
        break;
    }
    // Re-render only when the ring visibly changes.
    const QString key = QStringLiteral("%1:%2").arg(color.rgba()).arg(qRound(progress * 48));
    if (key == m_trayIconKey)
        return;
    m_trayIconKey = key;
    m_tray->setIcon(renderTrayIcon(progress, color));
}

// ---------------------------------------------------------------- previews

bool EyeTimerApp::renderPreviews(const QString &directory)
{
    if (!QDir().mkpath(directory))
        return false;
    const QDir dir(directory);
    bool ok = true;

    // Pretend we are partway through a work interval.
    m_activeSeconds = m_config.restIntervalSeconds * 0.07;
    m_countdown->refresh();
    ok &= m_countdown->grab().save(dir.filePath(QStringLiteral("countdown.png")));
    m_countdown->setExpanded(true);
    ok &= m_countdown->grab().save(dir.filePath(QStringLiteral("countdown-hover.png")));
    m_countdown->setExpanded(false);
    m_activeSeconds = m_config.restIntervalSeconds - 45;
    m_countdown->refresh();
    ok &= m_countdown->grab().save(dir.filePath(QStringLiteral("countdown-final-minute.png")));

    PreparePanel panel(this, QGuiApplication::primaryScreen());
    panel.setCountdown(12, qMax(1, m_config.prepareNoticeSeconds));
    panel.adjustSize();
    ok &= panel.grab().save(dir.filePath(QStringLiteral("prepare-panel.png")));

    m_activeSeconds = m_config.restIntervalSeconds * 0.07;
    QMenu *menu = createActionsMenu(nullptr);
    menu->adjustSize();
    ok &= menu->grab().save(dir.filePath(QStringLiteral("tray-menu.png")));
    delete menu;

    m_breakStartedMs = m_clock.elapsed() - qint64(m_config.restDurationSeconds * 0.3 * 1000);
    m_breaksToday = 4;
    BreakOverlay overlay(this, QGuiApplication::primaryScreen(), m_config.healthySuggestions.constFirst(), 4);
    overlay.setGeometry(0, 0, 1440, 900);
    overlay.setRemaining(breakRemainingSeconds());
    ok &= overlay.grab().save(dir.filePath(QStringLiteral("rest-overlay.png")));
    m_breakStartedMs.reset();

    SettingsWindow settings(this);
    settings.resize(640, 820);
    for (int i = 0; i < settings.tabCount(); ++i) {
        settings.selectTab(i);
        ok &= settings.grab().save(dir.filePath(QStringLiteral("settings-%1.png").arg(i + 1)));
    }

    const QIcon tray = renderTrayIcon(0.35, palette().lines[0]);
    ok &= tray.pixmap(64, 64).save(dir.filePath(QStringLiteral("tray-icon.png")));
    return ok;
}
