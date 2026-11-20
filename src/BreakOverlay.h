#pragma once

#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

class EyeTimerApp;
class KeyHintButton;
class QScreen;

// Full-screen rest overlay shown on every screen during a break.
class BreakOverlay : public QWidget
{
    Q_OBJECT

public:
    BreakOverlay(EyeTimerApp *app, QScreen *screen, const QString &suggestion, int breakNumber);

    void setRemaining(int seconds);
    void setAnimated(bool animated);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    qreal scale() const;
    void paintBackground(QPainter &painter, qreal seconds);

    EyeTimerApp *m_app;
    QString m_suggestion;
    int m_breakNumber;
    int m_remaining = 0;
    KeyHintButton *m_skip;
    QTimer m_frameTimer;
    QElapsedTimer m_clock;
};
