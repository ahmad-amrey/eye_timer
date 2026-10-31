#include "CountdownWindow.h"

#include "EyeTimerApp.h"
#include "Platform.h"
#include "Theme.h"
#include "Widgets.h"

#include <QApplication>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QWindow>

namespace {
constexpr int kHeight = 52;
constexpr int kRing = 20;
constexpr int kPaddingLeft = 14;
constexpr int kPaddingRight = 18;
constexpr int kGap = 12;
constexpr int kButton = 36;
constexpr int kButtonGap = 6;
constexpr int kButtonPaddingRight = 8;
constexpr int kScreenMargin = 24;
} // namespace

CountdownWindow::CountdownWindow(EyeTimerApp *app)
    : QWidget(nullptr)
    , m_app(app)
    , m_breakButton(new IconButton(theme::Icon::Skip, QStringLiteral("Take a break now"), this))
    , m_hideButton(new IconButton(theme::Icon::Close, QStringLiteral("Hide countdown"), this))
{
    setWindowTitle(QString::fromLatin1(kWindowTitle));
    setWindowIcon(app->appIcon());

#ifdef Q_OS_LINUX
    // WindowStaysOnTopHint is only reliable for normal windows on Linux; the
    // taskbar entry is the price of staying on top.
    const Qt::WindowType kind = Qt::Window;
#else
    const Qt::WindowType kind = app->trayAvailable() ? Qt::Tool : Qt::Window;
#endif
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | kind);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);

    connect(m_breakButton, &IconButton::clicked, app, &EyeTimerApp::startBreak);
    connect(m_hideButton, &IconButton::clicked, app, &EyeTimerApp::hideCountdownWindow);
    m_breakButton->hide();
    m_hideButton->hide();

    refresh();
    positionOnPrimaryScreen();

#ifdef Q_OS_LINUX
    connect(&m_raiseTimer, &QTimer::timeout, this, &CountdownWindow::periodicRaise);
    m_raiseTimer.start(3000);
    QTimer::singleShot(500, this, &CountdownWindow::periodicRaise);
#endif
}

QSize CountdownWindow::contentSize(bool expanded) const
{
    int width = kPaddingLeft + kRing + kGap + m_textWidth;
    width += expanded ? 10 + kButton + kButtonGap + kButton + kButtonPaddingRight : kPaddingRight;
    return {width, kHeight};
}

void CountdownWindow::refresh()
{
    const QFontMetrics metrics(theme::font(24, QFont::Light));
    const int textWidth = qMax(metrics.horizontalAdvance(QStringLiteral("00:00")),
                               metrics.horizontalAdvance(m_app->countdownText()));
    if (textWidth != m_textWidth || size() != contentSize(m_expanded)) {
        m_textWidth = textWidth;
        applyGeometry(true);
    }
    update();
}

void CountdownWindow::setExpanded(bool expanded)
{
    if (m_expanded == expanded)
        return;
    m_expanded = expanded;
    // Grow away from the nearest screen edge so the pill stays put under the
    // cursor and never slides off screen.
    bool keepRightEdge = true;
    if (const QScreen *current = screen())
        keepRightEdge = frameGeometry().center().x() > current->availableGeometry().center().x();
    applyGeometry(keepRightEdge);
}

void CountdownWindow::applyGeometry(bool keepRightEdge)
{
    const QSize target = contentSize(m_expanded);
    QRect geometry = this->geometry();
    if (keepRightEdge && geometry.width() > 0)
        geometry.setLeft(geometry.right() - target.width() + 1);
    geometry.setSize(target);
    setFixedSize(target);
    setGeometry(geometry);

    const int buttonY = (kHeight - kButton) / 2;
    const int firstButtonX = kPaddingLeft + kRing + kGap + m_textWidth + 10;
    m_breakButton->move(firstButtonX, buttonY);
    m_hideButton->move(firstButtonX + kButton + kButtonGap, buttonY);
    m_breakButton->setVisible(m_expanded);
    m_hideButton->setVisible(m_expanded);
}

void CountdownWindow::positionOnPrimaryScreen()
{
    const QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    const QRect available = screen->availableGeometry();
    move(available.right() - width() - kScreenMargin + 1, available.top() + kScreenMargin);
}

void CountdownWindow::periodicRaise()
{
    if (isVisible() && !m_app->isStopping()) {
        raise();
        platform::reinforceAlwaysOnTop(windowTitle());
    }
}

void CountdownWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const qreal alpha = m_app->config().countdownAlpha;
    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setBrush(theme::glass(qMin(1.0, alpha + (m_expanded ? 0.12 : 0.0))));
    painter.setPen(QPen(QColor(255, 255, 255, qRound((m_expanded ? 0.12 : 0.08) * 255 * qMax(alpha, 0.4))), 1));
    painter.drawRoundedRect(box, 14, 14);

    const EyeTimerApp::Phase phase = m_app->phase();
    const auto &lines = m_app->palette().lines;
    QColor accent = lines[0];
    qreal progress = m_app->workProgress();
    if (phase == EyeTimerApp::Phase::Break) {
        accent = lines[1];
        progress = m_app->breakRemainingFraction();
    } else if (m_app->secondsUntilBreak() <= 60) {
        accent = theme::warm();
    } else if (phase == EyeTimerApp::Phase::Paused) {
        accent = theme::text(0.45);
    }

    const QRectF ring(kPaddingLeft, (kHeight - kRing) / 2.0, kRing, kRing);
    theme::paintRing(painter, ring, progress, 1.5, 1.8, QColor(255, 255, 255, 31), {accent});

    painter.setFont(theme::font(24, QFont::Light, 99));
    const bool warmText = phase != EyeTimerApp::Phase::Break && m_app->secondsUntilBreak() <= 60;
    painter.setPen(warmText ? theme::warm() : theme::text());
    painter.drawText(QRectF(ring.right() + kGap, 0, m_textWidth + 4, kHeight), Qt::AlignVCenter | Qt::AlignLeft,
                     m_app->countdownText());
}

void CountdownWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Native moves work on Wayland and modern X11; fall back to a manual drag.
        if (windowHandle() && windowHandle()->startSystemMove()) {
            event->accept();
            return;
        }
        m_manualDrag = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void CountdownWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_manualDrag && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void CountdownWindow::mouseReleaseEvent(QMouseEvent *event)
{
    m_manualDrag = false;
    QWidget::mouseReleaseEvent(event);
}

void CountdownWindow::enterEvent(QEnterEvent *event)
{
    setExpanded(true);
    QWidget::enterEvent(event);
}

void CountdownWindow::leaveEvent(QEvent *event)
{
    setExpanded(false);
    QWidget::leaveEvent(event);
}

void CountdownWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = m_app->createActionsMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(event->globalPos());
}

void CountdownWindow::closeEvent(QCloseEvent *event)
{
    if (!m_app->isStopping()) {
        if (m_app->hasTrayIcon())
            m_app->hideCountdownWindow();
        else
            m_app->stop();
    }
    event->accept();
}
