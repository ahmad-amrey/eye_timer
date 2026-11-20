#pragma once

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QList>
#include <QRectF>
#include <QString>

#include <array>

class QMenu;
class QPainter;

// "Soft light" visual language: a near-black ground, large blurred colour
// fields, fine grain, hairline rings and thin type.
namespace theme {

struct Palette {
    QString id;
    QString label;
    std::array<QColor, 3> fields; // saturated, drawn heavily blurred
    std::array<QColor, 3> lines;  // pastel, used for rings and highlights
};

const QList<Palette> &palettes();
const Palette &palette(const QString &id);

QColor text(qreal opacity = 1.0);
QColor ground();
QColor overlayGround();
QColor glass(qreal opacity);
QColor warm();
QColor warmField();
QString css(const QColor &color);

// UI font with fallbacks (Geist if bundled or installed, then system fonts).
QFont font(qreal pixelSize, int weight = QFont::Normal, qreal letterSpacingPercent = 100.0);
QStringList fontFamilies();
void loadBundledFonts();

// A blurred colour blob filling `rect` (an ellipse with a soft edge).
void paintField(QPainter &painter, const QRectF &rect, const QColor &color, qreal opacity);
void paintGrain(QPainter &painter, const QRect &rect, qreal opacity);

// Progress ring that starts at 12 o'clock and runs clockwise.
void paintRing(QPainter &painter, const QRectF &rect, qreal progress, qreal trackWidth, qreal arcWidth,
               const QColor &track, const QList<QColor> &arcColors);

enum class Icon { Eye, EyeOff, Skip, Reset, Sliders, Power, Close, Play };
void paintIcon(QPainter &painter, Icon icon, const QRectF &rect, const QColor &color, qreal strokeWidth = 1.6);
QIcon makeIcon(Icon icon, const QColor &color, int size = 18);

// Rounded, translucent context menu matching the rest of the UI.
void styleMenu(QMenu *menu);

QString applicationStyleSheet();

} // namespace theme
