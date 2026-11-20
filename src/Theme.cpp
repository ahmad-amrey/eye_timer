#include "Theme.h"

#include "Platform.h"

#include <QDir>
#include <QFontDatabase>
#include <QHash>
#include <QImage>
#include <QLinearGradient>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRadialGradient>
#include <QRandomGenerator>

namespace theme {

const QList<Palette> &palettes()
{
    static const QList<Palette> list = {
        {QStringLiteral("aurora"), QStringLiteral("Aurora"),
         {QColor(0x0E, 0x8F, 0x86), QColor(0x3B, 0x3F, 0xD8), QColor(0xB8, 0x3A, 0x8C)},
         {QColor(0x9A, 0xE6, 0xD8), QColor(0xA5, 0xB4, 0xFC), QColor(0xF0, 0xAB, 0xFC)}},
        {QStringLiteral("dawn"), QStringLiteral("Dawn"),
         {QColor(0xD9, 0x65, 0x3B), QColor(0xC2, 0x36, 0x6E), QColor(0x5B, 0x3F, 0xD1)},
         {QColor(0xFC, 0xD3, 0xA8), QColor(0xF9, 0xA8, 0xC9), QColor(0xC4, 0xB5, 0xFD)}},
        {QStringLiteral("tide"), QStringLiteral("Tide"),
         {QColor(0x0B, 0x6E, 0x99), QColor(0x1F, 0x8F, 0x7A), QColor(0x33, 0x46, 0xC9)},
         {QColor(0xA5, 0xE3, 0xF5), QColor(0x99, 0xF0, 0xD6), QColor(0xA5, 0xB4, 0xFC)}},
    };
    return list;
}

const Palette &palette(const QString &id)
{
    for (const Palette &candidate : palettes()) {
        if (candidate.id == id)
            return candidate;
    }
    return palettes().constFirst();
}

QColor text(qreal opacity)
{
    QColor color(0xF4, 0xF5, 0xF8);
    color.setAlphaF(static_cast<float>(qBound(0.0, opacity, 1.0)));
    return color;
}

QColor ground() { return QColor(0x0A, 0x0B, 0x10); }
QColor overlayGround() { return QColor(0x07, 0x08, 0x0C); }
QColor warm() { return QColor(0xF6, 0xC2, 0x8B); }
QColor warmField() { return QColor(0xE0, 0x68, 0x3A); }

QColor glass(qreal opacity)
{
    QColor color(0x12, 0x13, 0x1A);
    color.setAlphaF(static_cast<float>(qBound(0.0, opacity, 1.0)));
    return color;
}

QString css(const QColor &color)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(QString::number(color.alphaF(), 'f', 3));
}

QStringList fontFamilies()
{
    return {QStringLiteral("Geist"),
            QStringLiteral("Inter"),
            QStringLiteral("Segoe UI Variable Display"),
            QStringLiteral("Segoe UI"),
            QStringLiteral("SF Pro Display"),
            QStringLiteral(".AppleSystemUIFont"),
            QStringLiteral("Helvetica Neue"),
            QStringLiteral("Cantarell"),
            QStringLiteral("Noto Sans"),
            QStringLiteral("Ubuntu"),
            QStringLiteral("sans-serif")};
}

QFont font(qreal pixelSize, int weight, qreal letterSpacingPercent)
{
    QFont result;
    result.setFamilies(fontFamilies());
    result.setPixelSize(qMax(1, qRound(pixelSize)));
    result.setWeight(static_cast<QFont::Weight>(weight));
    result.setHintingPreference(QFont::PreferNoHinting);
    if (!qFuzzyCompare(letterSpacingPercent, 100.0))
        result.setLetterSpacing(QFont::PercentageSpacing, letterSpacingPercent);
    return result;
}

void loadBundledFonts()
{
    // Drop Geist (or any other) .ttf/.otf files into a "fonts" folder next to
    // the executable to use them without installing them system-wide.
    const QDir fontDir(platform::resourcePath(QStringLiteral("fonts")));
    const QStringList files = fontDir.entryList({QStringLiteral("*.ttf"), QStringLiteral("*.otf")}, QDir::Files);
    for (const QString &file : files)
        QFontDatabase::addApplicationFont(fontDir.filePath(file));
}

namespace {

// Blobs are rendered once at low resolution and scaled up; the content is
// blurry anyway, so scaling costs nothing visually and keeps repaints cheap.
const QImage &fieldImage(const QColor &color)
{
    static QHash<QRgb, QImage> cache;
    const QRgb key = color.rgb();
    auto it = cache.find(key);
    if (it != cache.end())
        return *it;

    constexpr int size = 160;
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    QRadialGradient gradient(QPointF(size / 2.0, size / 2.0), size / 2.0);
    // Roughly a Gaussian falloff, like a CSS blur over a solid ellipse.
    const std::array<std::pair<qreal, qreal>, 7> stops = {{
        {0.00, 1.00}, {0.20, 0.92}, {0.40, 0.70}, {0.60, 0.40}, {0.75, 0.20}, {0.90, 0.06}, {1.00, 0.00},
    }};
    for (const auto &[position, alpha] : stops) {
        QColor stop = color;
        stop.setAlphaF(static_cast<float>(alpha));
        gradient.setColorAt(position, stop);
    }
    painter.fillRect(image.rect(), gradient);
    painter.end();
    return *cache.insert(key, image);
}

const QPixmap &grainTile()
{
    static const QPixmap tile = [] {
        constexpr int size = 192;
        QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
        QRandomGenerator generator(0xE7E);
        for (int y = 0; y < size; ++y) {
            auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
            for (int x = 0; x < size; ++x) {
                const int value = generator.bounded(256);
                const int alpha = 255;
                line[x] = qPremultiply(qRgba(value, value, value, alpha));
            }
        }
        return QPixmap::fromImage(image);
    }();
    return tile;
}

} // namespace

void paintField(QPainter &painter, const QRectF &rect, const QColor &color, qreal opacity)
{
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setOpacity(painter.opacity() * opacity);
    painter.drawImage(rect, fieldImage(color));
    painter.restore();
}

void paintGrain(QPainter &painter, const QRect &rect, qreal opacity)
{
    painter.save();
    painter.setOpacity(painter.opacity() * opacity);
    painter.setCompositionMode(QPainter::CompositionMode_Overlay);
    painter.drawTiledPixmap(rect, grainTile());
    painter.restore();
}

void paintRing(QPainter &painter, const QRectF &rect, qreal progress, qreal trackWidth, qreal arcWidth,
               const QColor &track, const QList<QColor> &arcColors)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(Qt::NoBrush);

    const qreal inset = qMax(trackWidth, arcWidth) / 2.0;
    const QRectF ringRect = rect.adjusted(inset, inset, -inset, -inset);

    painter.setPen(QPen(track, trackWidth));
    painter.drawEllipse(ringRect);

    progress = qBound(0.0, progress, 1.0);
    if (progress > 0.0 && !arcColors.isEmpty()) {
        QBrush brush;
        if (arcColors.size() == 1) {
            brush = QBrush(arcColors.constFirst());
        } else {
            QLinearGradient gradient(ringRect.topLeft(), ringRect.bottomRight());
            for (qsizetype i = 0; i < arcColors.size(); ++i)
                gradient.setColorAt(static_cast<qreal>(i) / (arcColors.size() - 1), arcColors.at(i));
            brush = QBrush(gradient);
        }
        painter.setPen(QPen(brush, arcWidth, Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(ringRect, 90 * 16, -qRound(progress * 360 * 16));
    }
    painter.restore();
}

void paintIcon(QPainter &painter, Icon icon, const QRectF &rect, const QColor &color, qreal strokeWidth)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(rect.topLeft());
    const qreal scale = qMin(rect.width(), rect.height()) / 24.0;
    painter.scale(scale, scale);
    // The path is laid out on a 24-unit grid; keep the stroke at
    // `strokeWidth` pixels whatever size the icon is drawn at.
    painter.setPen(QPen(color, strokeWidth / scale, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    auto eyeShape = [&painter] {
        QPainterPath path;
        path.moveTo(2, 12);
        path.cubicTo(4.5, 7.2, 8, 5, 12, 5);
        path.cubicTo(16, 5, 19.5, 7.2, 22, 12);
        path.cubicTo(19.5, 16.8, 16, 19, 12, 19);
        path.cubicTo(8, 19, 4.5, 16.8, 2, 12);
        path.closeSubpath();
        painter.drawPath(path);
    };

    switch (icon) {
    case Icon::Eye:
        eyeShape();
        painter.drawEllipse(QPointF(12, 12), 3, 3);
        break;
    case Icon::EyeOff:
        eyeShape();
        painter.drawEllipse(QPointF(12, 12), 3, 3);
        painter.drawLine(QPointF(3.5, 3.5), QPointF(20.5, 20.5));
        break;
    case Icon::Skip: {
        QPainterPath triangle;
        triangle.moveTo(5, 4.5);
        triangle.lineTo(15, 12);
        triangle.lineTo(5, 19.5);
        triangle.closeSubpath();
        painter.drawPath(triangle);
        painter.drawLine(QPointF(19, 5), QPointF(19, 19));
        break;
    }
    case Icon::Play: {
        QPainterPath triangle;
        triangle.moveTo(7, 4.5);
        triangle.lineTo(19, 12);
        triangle.lineTo(7, 19.5);
        triangle.closeSubpath();
        painter.drawPath(triangle);
        break;
    }
    case Icon::Reset: {
        QPainterPath arc;
        const QRectF circle(3, 3, 18, 18);
        arc.arcMoveTo(circle, 150);
        arc.arcTo(circle, 150, -300);
        painter.drawPath(arc);
        painter.drawLine(QPointF(3.5, 3.5), QPointF(3.5, 8.5));
        painter.drawLine(QPointF(3.5, 8.5), QPointF(8.5, 8.5));
        break;
    }
    case Icon::Sliders:
        painter.drawLine(QPointF(4, 6), QPointF(20, 6));
        painter.drawLine(QPointF(4, 12), QPointF(20, 12));
        painter.drawLine(QPointF(4, 18), QPointF(20, 18));
        painter.drawLine(QPointF(15, 3.5), QPointF(15, 8.5));
        painter.drawLine(QPointF(8, 9.5), QPointF(8, 14.5));
        painter.drawLine(QPointF(17, 15.5), QPointF(17, 20.5));
        break;
    case Icon::Power: {
        painter.drawLine(QPointF(12, 3), QPointF(12, 11));
        QPainterPath arc;
        const QRectF circle(4, 5, 16, 16);
        arc.arcMoveTo(circle, 55);
        arc.arcTo(circle, 55, -290);
        painter.drawPath(arc);
        break;
    }
    case Icon::Close:
        painter.drawLine(QPointF(6, 6), QPointF(18, 18));
        painter.drawLine(QPointF(18, 6), QPointF(6, 18));
        break;
    }
    painter.restore();
}

QIcon makeIcon(Icon icon, const QColor &color, int size)
{
    QIcon result;
    for (const qreal ratio : {1.0, 1.5, 2.0}) {
        QPixmap pixmap(QSize(qRound(size * ratio), qRound(size * ratio)));
        pixmap.fill(Qt::transparent);
        pixmap.setDevicePixelRatio(ratio);
        QPainter painter(&pixmap);
        paintIcon(painter, icon, QRectF(0, 0, size, size), color, 1.6);
        painter.end();
        result.addPixmap(pixmap);
    }
    return result;
}

void styleMenu(QMenu *menu)
{
    menu->setWindowFlags(menu->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    menu->setAttribute(Qt::WA_TranslucentBackground);
}

QString applicationStyleSheet()
{
    const QString families = QStringLiteral("\"%1\"").arg(fontFamilies().join(QStringLiteral("\", \"")));
    return QStringLiteral(R"(
* { font-family: %1; }

QToolTip {
    background: #14151C;
    color: #F4F5F8;
    border: 1px solid rgba(255, 255, 255, 0.10);
    border-radius: 8px;
    padding: 6px 10px;
    font-size: 13px;
}

QMenu {
    background: rgba(20, 21, 28, 0.97);
    border: 1px solid rgba(255, 255, 255, 0.09);
    border-radius: 14px;
    padding: 6px;
}
QMenu::item {
    background: transparent;
    color: #F4F5F8;
    padding: 11px 22px 11px 10px;
    border-radius: 9px;
    font-size: 14px;
    min-width: 230px;
}
QMenu::item:selected { background: rgba(255, 255, 255, 0.07); }
QMenu::item:disabled { color: rgba(244, 245, 248, 0.6); }
QMenu::icon { padding-left: 10px; }
QMenu::separator {
    height: 1px;
    background: rgba(255, 255, 255, 0.07);
    margin: 5px 10px;
}

QPushButton#primaryButton {
    background: #F4F5F8;
    color: #0A0B10;
    border: 0;
    border-radius: 11px;
    padding: 0 20px;
    min-height: 44px;
    font-size: 15px;
    font-weight: 500;
}
QPushButton#primaryButton:hover { background: #FFFFFF; }
QPushButton#primaryButton:focus { outline: none; }
QPushButton#primaryButton:disabled {
    background: rgba(255, 255, 255, 0.06);
    color: rgba(244, 245, 248, 0.5);
}

QPushButton#secondaryButton {
    background: rgba(255, 255, 255, 0.04);
    color: #F4F5F8;
    border: 1px solid rgba(255, 255, 255, 0.12);
    border-radius: 11px;
    padding: 0 18px;
    min-height: 44px;
    font-size: 15px;
}
QPushButton#secondaryButton:hover {
    background: rgba(255, 255, 255, 0.08);
    border-color: rgba(255, 255, 255, 0.18);
}
)")
        .arg(families);
}

} // namespace theme
