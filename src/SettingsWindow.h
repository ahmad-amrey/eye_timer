#pragma once

#include "Config.h"

#include <QList>
#include <QWidget>

class EyeTimerApp;
class IdleTimeline;
class QButtonGroup;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QStackedWidget;
class QVBoxLayout;
class Stepper;
class ToggleSwitch;

class SettingsWindow : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsWindow(EyeTimerApp *app);

    void loadFromConfig();
    void selectTab(int index);
    int tabCount() const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QWidget *buildRhythmPage();
    QWidget *buildIdlePage();
    QWidget *buildAppearancePage();
    QWidget *buildSystemPage();
    QWidget *buildMessagesPage();
    QWidget *makeRow(const QString &title, const QString &hint, QWidget *control);
    QWidget *makeSliderRow(const QString &title, const QString &hint, QSlider **slider);

    QStringList currentMessages() const;
    void rebuildMessages(const QStringList &messages);
    void addMessage();

    AppConfig currentFormConfig() const;
    void formChanged();
    void updateSummary();
    void applyPaletteStyle();
    void saveSettings();

    EyeTimerApp *m_app;
    bool m_loading = false;

    QLabel *m_summary = nullptr;
    QList<QPushButton *> m_tabs;
    QStackedWidget *m_pages = nullptr;

    Stepper *m_workMinutes = nullptr;
    Stepper *m_restSeconds = nullptr;
    Stepper *m_noticeSeconds = nullptr;
    Stepper *m_idlePauseMinutes = nullptr;
    Stepper *m_idleRestMinutes = nullptr;
    IdleTimeline *m_timeline = nullptr;

    QSlider *m_overlayOpacity = nullptr;
    QSlider *m_panelOpacity = nullptr;
    QSlider *m_countdownOpacity = nullptr;
    QButtonGroup *m_paletteGroup = nullptr;

    ToggleSwitch *m_startWithBoot = nullptr;
    ToggleSwitch *m_startHidden = nullptr;
    ToggleSwitch *m_tones = nullptr;

    QVBoxLayout *m_messagesLayout = nullptr;
    QList<QLineEdit *> m_messageEdits;
    QLineEdit *m_newMessage = nullptr;

    QLabel *m_status = nullptr;
    QPushButton *m_save = nullptr;
};
