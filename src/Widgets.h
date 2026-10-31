#pragma once

#include "Theme.h"

#include <QAbstractButton>
#include <QFrame>
#include <QWidget>

class QSpinBox;
class QToolButton;

// Small square button that draws one of the theme's line icons.
class IconButton : public QAbstractButton
{
    Q_OBJECT

public:
    IconButton(theme::Icon icon, const QString &accessibleName, QWidget *parent = nullptr);

    void setSide(int side);
    void setRadius(qreal radius);
    void setIconColor(const QColor &color);
    void setFilled(bool filled);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    theme::Icon m_icon;
    int m_side = 36;
    qreal m_radius = 9;
    QColor m_iconColor = theme::text(0.85);
    bool m_filled = true;
    bool m_hovered = false;
};

// Button with a label followed by a muted keyboard hint, e.g. "Skip break  Esc".
class KeyHintButton : public QAbstractButton
{
    Q_OBJECT

public:
    KeyHintButton(const QString &label, const QString &hint, QWidget *parent = nullptr);

    void setScale(qreal scale);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QString m_hint;
    qreal m_scale = 1.0;
    bool m_hovered = false;
};

class ToggleSwitch : public QAbstractButton
{
    Q_OBJECT

public:
    explicit ToggleSwitch(QWidget *parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
};

// Editable number with − / + buttons on either side.
class Stepper : public QFrame
{
    Q_OBJECT

public:
    Stepper(int minimum, int maximum, int step, const QString &unit, QWidget *parent = nullptr);

    int value() const;
    void setValue(int value);
    void setMinimum(int minimum);

signals:
    void valueChanged(int value);

private:
    void updateButtons();

    QSpinBox *m_spin;
    QToolButton *m_minus;
    QToolButton *m_plus;
};

// Round swatch showing a palette's three line colours, plus its name.
class PaletteButton : public QAbstractButton
{
    Q_OBJECT

public:
    PaletteButton(const theme::Palette &palette, QWidget *parent = nullptr);
    QString paletteId() const;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    theme::Palette m_palette;
};

// Thin progress line, 2 px tall.
class ProgressLine : public QWidget
{
    Q_OBJECT

public:
    explicit ProgressLine(QWidget *parent = nullptr);
    void setProgress(qreal progress);
    void setColor(const QColor &color);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qreal m_progress = 0;
    QColor m_color = theme::warm();
};

// Three-segment line explaining when idle time pauses and when it counts as rest.
class IdleTimeline : public QWidget
{
    Q_OBJECT

public:
    explicit IdleTimeline(QWidget *parent = nullptr);
    void setMinutes(int pauseMinutes, int restMinutes);
    void setColors(const QColor &active, const QColor &rested);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_pause = 1;
    int m_rest = 10;
    QColor m_active;
    QColor m_rested;
};

// Header row of the tray menu: progress ring with the time to the next break.
class TrayHeader : public QWidget
{
    Q_OBJECT

public:
    explicit TrayHeader(QWidget *parent = nullptr);
    void setState(const QString &caption, const QString &time, qreal progress, const QList<QColor> &colors);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_caption;
    QString m_time;
    qreal m_progress = 0;
    QList<QColor> m_colors;
};
