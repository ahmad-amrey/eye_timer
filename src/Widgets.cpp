#include "Widgets.h"

#include <QEnterEvent>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QSpinBox>
#include <QToolButton>

// ---------------------------------------------------------------- IconButton

IconButton::IconButton(theme::Icon icon, const QString &accessibleName, QWidget *parent)
    : QAbstractButton(parent)
    , m_icon(icon)
{
    setAccessibleName(accessibleName);
    setToolTip(accessibleName);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setFixedSize(sizeHint());
}

void IconButton::setSide(int side)
{
    m_side = side;
    setFixedSize(sizeHint());
}

void IconButton::setRadius(qreal radius)
{
    m_radius = radius;
    update();
}

void IconButton::setIconColor(const QColor &color)
{
    m_iconColor = color;
    update();
}

void IconButton::setFilled(bool filled)
{
    m_filled = filled;
    update();
}

QSize IconButton::sizeHint() const
{
    return {m_side, m_side};
}

void IconButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal fill = isDown() ? 0.14 : (m_hovered ? 0.11 : (m_filled ? 0.07 : 0.0));
    if (fill > 0) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, qRound(fill * 255)));
        painter.drawRoundedRect(rect(), m_radius, m_radius);
    }
    if (hasFocus()) {
        painter.setPen(QPen(theme::text(0.5), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), m_radius, m_radius);
    }
    const qreal iconSide = m_side * 0.42;
    const QRectF iconRect((width() - iconSide) / 2.0, (height() - iconSide) / 2.0, iconSide, iconSide);
    theme::paintIcon(painter, m_icon, iconRect, m_hovered ? theme::text() : m_iconColor, 1.6);
}

void IconButton::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QAbstractButton::enterEvent(event);
}

void IconButton::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QAbstractButton::leaveEvent(event);
}

// ------------------------------------------------------------- KeyHintButton

KeyHintButton::KeyHintButton(const QString &label, const QString &hint, QWidget *parent)
    : QAbstractButton(parent)
    , m_hint(hint)
{
    setText(label);
    setAccessibleName(label);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
}

void KeyHintButton::setScale(qreal scale)
{
    m_scale = scale;
    setFixedSize(sizeHint());
    update();
}

QSize KeyHintButton::sizeHint() const
{
    const QFontMetricsF labelMetrics(theme::font(15 * m_scale, QFont::Normal));
    const QFontMetricsF hintMetrics(theme::font(12 * m_scale, QFont::Normal));
    const qreal width = 20 * m_scale + labelMetrics.horizontalAdvance(text()) + 12 * m_scale
                        + hintMetrics.horizontalAdvance(m_hint) + 20 * m_scale;
    return {qCeil(width), qRound(46 * m_scale)};
}

void KeyHintButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal radius = 12 * m_scale;
    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setBrush(QColor(255, 255, 255, isDown() ? 26 : (m_hovered ? 20 : 10)));
    painter.setPen(QPen(QColor(255, 255, 255, hasFocus() ? 90 : (m_hovered ? 56 : 36)), 1));
    painter.drawRoundedRect(box, radius, radius);

    const QFont labelFont = theme::font(15 * m_scale, QFont::Normal);
    const QFont hintFont = theme::font(12 * m_scale, QFont::Normal);
    const qreal labelWidth = QFontMetricsF(labelFont).horizontalAdvance(text());
    qreal x = 20 * m_scale;
    painter.setFont(labelFont);
    painter.setPen(theme::text(0.92));
    painter.drawText(QRectF(x, 0, labelWidth + 2, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
    x += labelWidth + 12 * m_scale;
    painter.setFont(hintFont);
    painter.setPen(theme::text(0.5));
    painter.drawText(QRectF(x, 0, width() - x, height()), Qt::AlignVCenter | Qt::AlignLeft, m_hint);
}

void KeyHintButton::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QAbstractButton::enterEvent(event);
}

void KeyHintButton::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QAbstractButton::leaveEvent(event);
}

// -------------------------------------------------------------- ToggleSwitch

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setFixedSize(sizeHint());
}

QSize ToggleSwitch::sizeHint() const
{
    return {46, 28};
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF track = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    if (isChecked()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(isEnabled() ? theme::text() : theme::text(0.3));
    } else {
        painter.setPen(QPen(QColor(255, 255, 255, 41), 1));
        painter.setBrush(QColor(255, 255, 255, 10));
    }
    painter.drawRoundedRect(track, track.height() / 2, track.height() / 2);

    if (hasFocus()) {
        painter.setPen(QPen(theme::text(0.45), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(track.adjusted(-0.0, -0.0, 0, 0), track.height() / 2, track.height() / 2);
    }

    const qreal knob = 20;
    const qreal x = isChecked() ? width() - knob - 4 : 4;
    painter.setPen(Qt::NoPen);
    painter.setBrush(isChecked() ? theme::ground() : theme::text(isEnabled() ? 0.55 : 0.25));
    painter.drawEllipse(QRectF(x, (height() - knob) / 2.0, knob, knob));
}

// ------------------------------------------------------------------- Stepper

Stepper::Stepper(int minimum, int maximum, int step, const QString &unit, QWidget *parent)
    : QFrame(parent)
    , m_spin(new QSpinBox(this))
    , m_minus(new QToolButton(this))
    , m_plus(new QToolButton(this))
{
    setObjectName(QStringLiteral("stepper"));
    setFixedSize(164, 44);

    m_spin->setObjectName(QStringLiteral("stepperValue"));
    m_spin->setRange(minimum, maximum);
    m_spin->setSingleStep(step);
    m_spin->setSuffix(QLatin1Char(' ') + unit);
    m_spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_spin->setAlignment(Qt::AlignCenter);
    m_spin->setAccelerated(true);
    m_spin->setKeyboardTracking(false);

    m_minus->setObjectName(QStringLiteral("stepperButton"));
    m_minus->setText(QStringLiteral("−"));
    m_minus->setAutoRepeat(true);
    m_minus->setCursor(Qt::PointingHandCursor);
    m_minus->setAccessibleName(QStringLiteral("Decrease"));
    m_plus->setObjectName(QStringLiteral("stepperButton"));
    m_plus->setText(QStringLiteral("+"));
    m_plus->setAutoRepeat(true);
    m_plus->setCursor(Qt::PointingHandCursor);
    m_plus->setAccessibleName(QStringLiteral("Increase"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);
    layout->addWidget(m_minus);
    layout->addWidget(m_spin, 1);
    layout->addWidget(m_plus);

    connect(m_minus, &QToolButton::clicked, m_spin, &QSpinBox::stepDown);
    connect(m_plus, &QToolButton::clicked, m_spin, &QSpinBox::stepUp);
    connect(m_spin, &QSpinBox::valueChanged, this, [this](int value) {
        updateButtons();
        emit valueChanged(value);
    });
    updateButtons();
}

int Stepper::value() const
{
    return m_spin->value();
}

void Stepper::setValue(int value)
{
    m_spin->setValue(value);
    updateButtons();
}

void Stepper::setMinimum(int minimum)
{
    m_spin->setMinimum(minimum);
    updateButtons();
}

void Stepper::updateButtons()
{
    m_minus->setEnabled(m_spin->value() > m_spin->minimum());
    m_plus->setEnabled(m_spin->value() < m_spin->maximum());
}

// ------------------------------------------------------------- PaletteButton

PaletteButton::PaletteButton(const theme::Palette &palette, QWidget *parent)
    : QAbstractButton(parent)
    , m_palette(palette)
{
    setCheckable(true);
    setText(palette.label);
    setAccessibleName(palette.label + QStringLiteral(" colours"));
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setFixedSize(sizeHint());
}

QString PaletteButton::paletteId() const
{
    return m_palette.id;
}

QSize PaletteButton::sizeHint() const
{
    return {112, 44};
}

void PaletteButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setBrush(QColor(255, 255, 255, isChecked() ? 18 : 8));
    painter.setPen(QPen(isChecked() ? theme::text(0.55) : QColor(255, 255, 255, hasFocus() ? 70 : 26), 1));
    painter.drawRoundedRect(box, 11, 11);

    const QRectF swatch(12, (height() - 18) / 2.0, 18, 18);
    QLinearGradient gradient(swatch.topLeft(), swatch.bottomRight());
    gradient.setColorAt(0.0, m_palette.lines[0]);
    gradient.setColorAt(0.5, m_palette.lines[1]);
    gradient.setColorAt(1.0, m_palette.lines[2]);
    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawEllipse(swatch);

    painter.setFont(theme::font(14, isChecked() ? QFont::Medium : QFont::Normal));
    painter.setPen(isChecked() ? theme::text() : theme::text(0.7));
    painter.drawText(QRectF(swatch.right() + 10, 0, width() - swatch.right() - 14, height()),
                     Qt::AlignVCenter | Qt::AlignLeft, text());
}

// -------------------------------------------------------------- ProgressLine

ProgressLine::ProgressLine(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(2);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void ProgressLine::setProgress(qreal progress)
{
    m_progress = qBound(0.0, progress, 1.0);
    update();
}

void ProgressLine::setColor(const QColor &color)
{
    m_color = color;
    update();
}

QSize ProgressLine::sizeHint() const
{
    return {200, 2};
}

void ProgressLine::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 20));
    painter.drawRoundedRect(rect(), 1, 1);
    painter.setBrush(m_color);
    painter.drawRoundedRect(QRectF(0, 0, width() * m_progress, height()), 1, 1);
}

// -------------------------------------------------------------- IdleTimeline

IdleTimeline::IdleTimeline(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(sizeHint().height());
}

void IdleTimeline::setMinutes(int pauseMinutes, int restMinutes)
{
    m_pause = pauseMinutes;
    m_rest = restMinutes;
    update();
}

void IdleTimeline::setColors(const QColor &active, const QColor &rested)
{
    m_active = active;
    m_rested = rested;
    update();
}

QSize IdleTimeline::sizeHint() const
{
    return {400, 36};
}

void IdleTimeline::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    constexpr qreal gap = 3;
    const qreal unit = (width() - 2 * gap) / 7.0;
    const std::array<qreal, 3> widths = {unit * 3, unit * 2, unit * 2};
    const std::array<QColor, 3> colors = {m_active, theme::text(0.25), m_rested};
    const std::array<QString, 3> labels = {
        QStringLiteral("Active · counting"),
        QStringLiteral("Idle %1 min · paused").arg(m_pause),
        QStringLiteral("Idle %1 min · rested").arg(m_rest),
    };

    painter.setFont(theme::font(12, QFont::Light));
    qreal x = 0;
    for (int i = 0; i < 3; ++i) {
        painter.fillRect(QRectF(x, 0, widths[i], 2), colors[i]);
        painter.setPen(theme::text(0.62));
        painter.drawText(QRectF(x, 10, widths[i] - 4, height() - 10), Qt::AlignLeft | Qt::AlignTop, labels[i]);
        x += widths[i] + gap;
    }
}

// ---------------------------------------------------------------- TrayHeader

TrayHeader::TrayHeader(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(sizeHint());
}

void TrayHeader::setState(const QString &caption, const QString &time, qreal progress, const QList<QColor> &colors)
{
    m_caption = caption;
    m_time = time;
    m_progress = progress;
    m_colors = colors;
    update();
}

QSize TrayHeader::sizeHint() const
{
    return {290, 82};
}

void TrayHeader::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF ring(14, (height() - 52) / 2.0, 52, 52);
    theme::paintRing(painter, ring, m_progress, 1.5, 2.0, QColor(255, 255, 255, 26), m_colors);

    const qreal x = ring.right() + 16;
    painter.setFont(theme::font(12, QFont::Normal));
    painter.setPen(theme::text(0.6));
    painter.drawText(QRectF(x, ring.top() + 4, width() - x, 18), Qt::AlignLeft | Qt::AlignVCenter, m_caption);
    painter.setFont(theme::font(30, QFont::ExtraLight, 98));
    painter.setPen(theme::text());
    painter.drawText(QRectF(x, ring.top() + 18, width() - x, 36), Qt::AlignLeft | Qt::AlignVCenter, m_time);
}
