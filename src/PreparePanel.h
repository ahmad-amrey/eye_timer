#pragma once

#include <QPointer>
#include <QWidget>

class EyeTimerApp;
class ProgressLine;
class QLabel;
class QScreen;

// Glass card in the bottom-right corner warning that a break is near.
// Shown without taking focus from the app the user is working in.
class PreparePanel : public QWidget
{
    Q_OBJECT

public:
    PreparePanel(EyeTimerApp *app, QScreen *screen);

    void setCountdown(int secondsUntilBreak, int noticeSeconds);
    void reposition();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    EyeTimerApp *m_app;
    QPointer<QScreen> m_screen;
    QLabel *m_number;
    ProgressLine *m_progress;
};
