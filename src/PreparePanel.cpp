#include "PreparePanel.h"

#include "EyeTimerApp.h"
#include "Theme.h"
#include "Widgets.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

namespace {
constexpr int kRadius = 20;
constexpr int kScreenMargin = 24;
} // namespace

PreparePanel::PreparePanel(EyeTimerApp *app, QScreen *screen)
    : QWidget(nullptr)
    , m_app(app)
    , m_screen(screen)
    , m_number(new QLabel(this))
    , m_progress(new ProgressLine(this))
{
    setWindowTitle(QStringLiteral("Eye Timer break soon"));
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus | Qt::Tool);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 18, 18, 22);
    layout->setSpacing(16);

    auto *header = new QHBoxLayout;
    header->setSpacing(10);
    auto *title = new QLabel(QStringLiteral("BREAK SOON"), this);
    title->setFont(theme::font(13, QFont::Medium, 114));
    title->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(theme::css(theme::text(0.7))));
    auto *close = new IconButton(theme::Icon::Close, QStringLiteral("Hide until the break"), this);
    close->setSide(32);
    close->setRadius(8);
    close->setFilled(false);
    close->setIconColor(theme::text(0.6));
    header->addWidget(title, 1);
    header->addWidget(close);
    layout->addLayout(header);

    auto *body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 4, 0);
    body->setSpacing(18);
    m_number->setFont(theme::font(56, QFont::Thin, 96));
    m_number->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(theme::css(theme::warm())));
    m_number->setMinimumWidth(QFontMetrics(m_number->font()).horizontalAdvance(QStringLiteral("00:00")) / 1);
    auto *message = new QLabel(QStringLiteral("Finish what you are doing and look away."), this);
    message->setWordWrap(true);
    message->setFont(theme::font(15, QFont::Light));
    message->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(theme::css(theme::text(0.82))));
    body->addWidget(m_number);
    body->addWidget(message, 1);
    layout->addLayout(body);

    auto *progressRow = new QHBoxLayout;
    progressRow->setContentsMargins(0, 0, 4, 0);
    progressRow->addWidget(m_progress);
    layout->addLayout(progressRow);

    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 4, 0);
    buttons->setSpacing(10);
    auto *startNow = new QPushButton(QStringLiteral("Start break now"), this);
    startNow->setObjectName(QStringLiteral("primaryButton"));
    startNow->setCursor(Qt::PointingHandCursor);
    startNow->setFocusPolicy(Qt::NoFocus);
    auto *hide = new QPushButton(QStringLiteral("Hide"), this);
    hide->setObjectName(QStringLiteral("secondaryButton"));
    hide->setCursor(Qt::PointingHandCursor);
    hide->setFocusPolicy(Qt::NoFocus);
    buttons->addWidget(startNow, 1);
    buttons->addWidget(hide, 1);
    layout->addLayout(buttons);

    connect(close, &IconButton::clicked, app, &EyeTimerApp::dismissPreparePanel);
    connect(hide, &QPushButton::clicked, app, &EyeTimerApp::dismissPreparePanel);
    connect(startNow, &QPushButton::clicked, app, &EyeTimerApp::startBreak);

    const int available = screen ? screen->availableGeometry().width() : 1280;
    setFixedWidth(qMin(420, qMax(280, available - 2 * kScreenMargin)));
    adjustSize();
}

void PreparePanel::setCountdown(int secondsUntilBreak, int noticeSeconds)
{
    const int seconds = qMax(0, secondsUntilBreak);
    m_number->setText(QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0')));
    const qreal span = qMax(1, noticeSeconds);
    m_progress->setProgress(1.0 - qBound(0.0, seconds / span, 1.0));
    adjustSize();
    reposition();
    update();
}

void PreparePanel::reposition()
{
    QScreen *screen = m_screen ? m_screen.data() : QGuiApplication::primaryScreen();
    if (!screen)
        return;
    const QRect available = screen->availableGeometry();
    move(available.right() - width() - kScreenMargin + 1, available.bottom() - height() - kScreenMargin + 1);
}

void PreparePanel::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath shape;
    shape.addRoundedRect(box, kRadius, kRadius);

    painter.fillPath(shape, theme::glass(m_app->config().preparePanelAlpha));
    painter.save();
    painter.setClipPath(shape);
    theme::paintField(painter, QRectF(width() - 200, -140, 320, 280), theme::warmField(),
                      0.35 * m_app->config().preparePanelAlpha);
    theme::paintGrain(painter, rect(), 0.05);
    painter.restore();
    painter.setPen(QPen(QColor(255, 255, 255, 20), 1));
    painter.drawPath(shape);
}
