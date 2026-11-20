#pragma once

#include <QPoint>
#include <QTimer>
#include <QWidget>

class EyeTimerApp;
class IconButton;

// Small always-on-top pill with a progress ring and the time to the next
// break. Hovering reveals "take a break now" and "hide" buttons; it can be
// dragged anywhere and right-clicked for the full menu.
class CountdownWindow : public QWidget
{
    Q_OBJECT

public:
    static constexpr auto kWindowTitle = "Eye Timer countdown";

    explicit CountdownWindow(EyeTimerApp *app);

    void refresh();
    void setExpanded(bool expanded);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    QSize contentSize(bool expanded) const;
    void applyGeometry(bool keepRightEdge);
    void positionOnPrimaryScreen();
    void periodicRaise();

    EyeTimerApp *m_app;
    IconButton *m_breakButton;
    IconButton *m_hideButton;
    QTimer m_raiseTimer;
    QPoint m_dragOffset;
    bool m_manualDrag = false;
    bool m_expanded = false;
    int m_textWidth = 0;
};
