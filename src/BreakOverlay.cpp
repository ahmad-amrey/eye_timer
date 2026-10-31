#include "BreakOverlay.h"

#include "EyeTimerApp.h"
#include "Theme.h"
#include "Widgets.h"

#include <QKeyEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QScreen>

#include <cmath>
#include <numbers>

namespace {
// The layout is designed on a 1440×900 screen and scaled to the real one.
constexpr qreal kDesignWidth = 1440.0;
constexpr qreal kDesignHeight = 900.0;
// ~15 fps is plenty for fields that drift a few pixels per second, and keeps
// full-screen translucent repaints cheap.
constexpr int kFrameMs = 66;

struct Drift {
    QRectF rect; // in design coordinates
    qreal opacity;
    QPointF amplitude;
    qreal periodSeconds;
};

const std::array<Drift, 3> kFields = {{
    {QRectF(-220, 520, 900, 700), 0.55, QPointF(60, -40), 34},
    {QRectF(420, 700, 760, 620), 0.50, QPointF(-70, 30), 42},
    {QRectF(880, 480, 820, 680), 0.45, QPointF(40, 50), 38},
}};
} // namespace

BreakOverlay::BreakOverlay(EyeTimerApp *app, QScreen *screen, const QString &suggestion, int breakNumber)
    : QWidget(nullptr)
    , m_app(app)
    , m_suggestion(suggestion)
    , m_breakNumber(breakNumber)
    , m_remaining(app->config().restDurationSeconds)
    , m_skip(new KeyHintButton(QStringLiteral("Skip break"), QStringLiteral("Esc"), this))
{
    setWindowTitle(QStringLiteral("Eye Timer break"));
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::StrongFocus);
    setGeometry(screen->geometry());

    connect(m_skip, &KeyHintButton::clicked, app, &EyeTimerApp::skipBreak);
    connect(&m_frameTimer, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    m_clock.start();
}

void BreakOverlay::setRemaining(int seconds)
{
    if (m_remaining == seconds)
        return;
    m_remaining = seconds;
    update();
}

void BreakOverlay::setAnimated(bool animated)
{
    if (animated)
        m_frameTimer.start(kFrameMs);
    else
        m_frameTimer.stop();
}

qreal BreakOverlay::scale() const
{
    return qBound(0.7, qMin(width() / kDesignWidth, height() / kDesignHeight), 2.0);
}

void BreakOverlay::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    setAnimated(true);
    setFocus();
}

void BreakOverlay::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const qreal s = scale();
    m_skip->setScale(s);
    m_skip->move((width() - m_skip->width()) / 2, height() - qRound(56 * s) - m_skip->height());
}

void BreakOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Space) {
        m_app->finishBreak(true);
        return;
    }
    QWidget::keyPressEvent(event);
}

void BreakOverlay::paintBackground(QPainter &painter, qreal seconds)
{
    const qreal alpha = m_app->config().overlayAlpha;
    QColor base = theme::overlayGround();
    base.setAlphaF(static_cast<float>(alpha));
    painter.fillRect(rect(), base);

    const qreal sx = width() / kDesignWidth;
    const qreal sy = height() / kDesignHeight;
    const auto &fields = m_app->palette().fields;
    for (int i = 0; i < 3; ++i) {
        const Drift &drift = kFields[i];
        // Ease out and back, like a CSS ease-in-out alternate animation.
        const qreal phase = (1.0 - std::cos(2.0 * std::numbers::pi * seconds / drift.periodSeconds)) / 2.0;
        const QRectF designRect = drift.rect.translated(drift.amplitude * phase);
        const QRectF target(designRect.x() * sx, designRect.y() * sy, designRect.width() * sx, designRect.height() * sy);
        theme::paintField(painter, target, fields[i], drift.opacity * qMin(1.0, alpha / 0.86));
    }

    // Keep the middle dark so the countdown always reads clearly.
    const QPointF center(width() / 2.0, height() * 0.4);
    QRadialGradient vignette(center, 1.0);
    vignette.setColorAt(0.0, QColor(7, 8, 12, qRound(0.75 * 255 * alpha)));
    vignette.setColorAt(1.0, QColor(7, 8, 12, 0));
    painter.save();
    painter.translate(center);
    painter.scale(900 * sx * 0.7, 560 * sy * 0.7);
    painter.translate(-center);
    painter.fillRect(QRectF(center.x() - 1, center.y() - 1, 2, 2), vignette);
    painter.restore();

    theme::paintGrain(painter, rect(), 0.06);
}

void BreakOverlay::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const qreal s = scale();
    paintBackground(painter, m_clock.elapsed() / 1000.0);

    // Header: wordmark on the left, today's break count on the right.
    const qreal padX = 56 * s;
    const qreal headerY = 40 * s;
    const qreal headerH = 24 * s;
    theme::paintIcon(painter, theme::Icon::Eye, QRectF(padX, headerY + 2 * s, 20 * s, 20 * s), theme::text(0.78), 1.6);
    painter.setFont(theme::font(15 * s, QFont::Medium, 101));
    painter.setPen(theme::text(0.78));
    painter.drawText(QRectF(padX + 30 * s, headerY, 300 * s, headerH), Qt::AlignVCenter | Qt::AlignLeft,
                     QStringLiteral("Eye Timer"));
    painter.setFont(theme::font(14 * s, QFont::Normal));
    painter.setPen(theme::text(0.6));
    painter.drawText(QRectF(width() - padX - 400 * s, headerY, 400 * s, headerH), Qt::AlignVCenter | Qt::AlignRight,
                     QStringLiteral("Break %1 of today").arg(m_breakNumber));

    // Centre block: title, ring with the countdown, suggestion.
    const QFont titleFont = theme::font(15 * s, QFont::Medium, 124);
    const QFont suggestionFont = theme::font(24 * s, QFont::Light);
    const qreal titleH = QFontMetricsF(titleFont).height();
    const qreal ringSide = 360 * s;
    const qreal suggestionW = qMin<qreal>(720 * s, width() - 2 * padX);
    const QRectF suggestionBounds = QFontMetricsF(suggestionFont).boundingRect(
        QRectF(0, 0, suggestionW, 1000 * s), Qt::AlignHCenter | Qt::TextWordWrap, m_suggestion);
    const qreal gap = 36 * s;
    const qreal blockH = titleH + gap + ringSide + gap + suggestionBounds.height();
    qreal y = (height() - blockH) / 2.0 - 12 * s;

    painter.setFont(titleFont);
    painter.setPen(theme::text(0.7));
    painter.drawText(QRectF(0, y, width(), titleH), Qt::AlignCenter, QStringLiteral("REST YOUR EYES"));
    y += titleH + gap;

    const QRectF ring((width() - ringSide) / 2.0, y, ringSide, ringSide);
    const auto &lines = m_app->palette().lines;
    theme::paintRing(painter, ring, m_app->breakRemainingFraction(), 1.5 * s, 2.0 * s, QColor(255, 255, 255, 20),
                     {lines[0], lines[1], lines[2]});

    const QFont numberFont = theme::font(168 * s, QFont::Thin, 95);
    const QFont unitFont = theme::font(13 * s, QFont::Normal, 112);
    const qreal numberH = QFontMetricsF(numberFont).ascent() + QFontMetricsF(numberFont).descent() * 0.2;
    const qreal unitH = QFontMetricsF(unitFont).height();
    const qreal innerH = numberH + 2 * s + unitH;
    const qreal innerTop = ring.center().y() - innerH / 2.0;
    painter.setFont(numberFont);
    painter.setPen(theme::text());
    painter.drawText(QRectF(ring.left(), innerTop, ring.width(), numberH), Qt::AlignHCenter | Qt::AlignBottom,
                     QString::number(m_remaining));
    painter.setFont(unitFont);
    painter.setPen(theme::text(0.55));
    painter.drawText(QRectF(ring.left(), innerTop + numberH + 2 * s, ring.width(), unitH), Qt::AlignCenter,
                     m_remaining == 1 ? QStringLiteral("second") : QStringLiteral("seconds"));
    y += ringSide + gap;

    painter.setFont(suggestionFont);
    painter.setPen(theme::text(0.86));
    painter.drawText(QRectF((width() - suggestionW) / 2.0, y, suggestionW, suggestionBounds.height() + 4),
                     Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, m_suggestion);
}
