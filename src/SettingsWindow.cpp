#include "SettingsWindow.h"

#include "EyeTimerApp.h"
#include "Platform.h"
#include "Theme.h"
#include "Widgets.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

QString settingsStyleSheet(const QColor &accent)
{
    return QStringLiteral(R"(
QLabel { background: transparent; color: rgba(244, 245, 248, 0.92); }
QLabel#settingsTitle { color: #F4F5F8; }
QLabel#rowTitle { color: #F4F5F8; }
QLabel#rowHint, QLabel#mutedText, QLabel#statusText { color: rgba(244, 245, 248, 0.62); }
QLabel#messageNumber { color: rgba(244, 245, 248, 0.45); }
QLabel#sliderValue { color: rgba(244, 245, 248, 0.85); }

QWidget#tabBar { border-bottom: 1px solid rgba(255, 255, 255, 0.08); background: transparent; }
QPushButton#tab {
    background: transparent;
    border: 0;
    border-bottom: 1px solid transparent;
    padding: 12px 0 11px 0;
    color: rgba(244, 245, 248, 0.55);
    font-size: 14px;
}
QPushButton#tab:hover { color: rgba(244, 245, 248, 0.85); }
QPushButton#tab:checked { color: #F4F5F8; border-bottom-color: #F4F5F8; font-weight: 500; }

QFrame#row { background: transparent; border: 0; border-bottom: 1px solid rgba(255, 255, 255, 0.06); }

QFrame#stepper {
    border: 1px solid rgba(255, 255, 255, 0.10);
    border-radius: 11px;
    background: rgba(255, 255, 255, 0.03);
}
QToolButton#stepperButton {
    border: 0;
    border-radius: 10px;
    background: transparent;
    color: rgba(244, 245, 248, 0.75);
    font-size: 20px;
    font-weight: 300;
    min-width: 42px;
    min-height: 42px;
}
QToolButton#stepperButton:hover { color: #F4F5F8; background: rgba(255, 255, 255, 0.06); }
QToolButton#stepperButton:disabled { color: rgba(244, 245, 248, 0.22); }
QSpinBox#stepperValue {
    border: 0;
    background: transparent;
    color: #F4F5F8;
    font-size: 16px;
    selection-background-color: rgba(165, 180, 252, 0.35);
}

QSlider { min-height: 24px; background: transparent; }
QSlider::groove:horizontal { height: 2px; background: rgba(255, 255, 255, 0.12); border-radius: 1px; }
QSlider::sub-page:horizontal { height: 2px; background: %1; border-radius: 1px; }
QSlider::handle:horizontal {
    background: #F4F5F8;
    width: 16px;
    height: 16px;
    margin: -7px 0;
    border-radius: 8px;
}

QLineEdit {
    border: 1px solid rgba(255, 255, 255, 0.10);
    border-radius: 11px;
    background: rgba(0, 0, 0, 0.25);
    color: #F4F5F8;
    padding: 0 14px;
    min-height: 42px;
    font-size: 15px;
    font-weight: 300;
    selection-background-color: rgba(165, 180, 252, 0.35);
}
QLineEdit:focus { border-color: rgba(255, 255, 255, 0.28); }
QLineEdit#messageEdit {
    border: 0;
    border-radius: 0;
    background: transparent;
    padding: 0 2px;
    min-height: 48px;
}

QScrollArea, QScrollArea > QWidget > QWidget, QStackedWidget { background: transparent; border: 0; }
QScrollBar:vertical { background: transparent; width: 8px; margin: 2px; }
QScrollBar::handle:vertical { background: rgba(255, 255, 255, 0.16); border-radius: 3px; min-height: 36px; }
QScrollBar::handle:vertical:hover { background: rgba(255, 255, 255, 0.28); }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; border: 0; height: 0; }
)")
        .arg(theme::css(accent));
}

QLabel *makeLabel(const QString &text, const char *objectName, const QFont &font, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QLatin1String(objectName));
    label->setFont(font);
    return label;
}

QWidget *scrollable(QWidget *content)
{
    auto *area = new QScrollArea;
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    area->setWidget(content);
    return area;
}

} // namespace

SettingsWindow::SettingsWindow(EyeTimerApp *app)
    : QWidget(nullptr)
    , m_app(app)
{
    setObjectName(QStringLiteral("settingsWindow"));
    setWindowTitle(QStringLiteral("Eye Timer settings"));
    setWindowIcon(app->appIcon());
    setMinimumSize(560, 640);
    resize(640, 820);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(32, 30, 32, 0);
    root->setSpacing(0);

    auto *header = new QHBoxLayout;
    header->setSpacing(12);
    auto *icon = new QLabel(this);
    icon->setPixmap(theme::makeIcon(theme::Icon::Eye, theme::text(0.85), 22).pixmap(22, 22));
    header->addWidget(icon);
    header->addWidget(makeLabel(QStringLiteral("Settings"), "settingsTitle", theme::font(20, QFont::Medium), this));
    header->addStretch(1);
    root->addLayout(header);
    root->addSpacing(24);

    m_summary = makeLabel(QString(), "summary", theme::font(26, QFont::ExtraLight, 99), this);
    m_summary->setWordWrap(true);
    m_summary->setTextFormat(Qt::RichText);
    root->addWidget(m_summary);
    root->addSpacing(24);

    auto *tabBar = new QWidget(this);
    tabBar->setObjectName(QStringLiteral("tabBar"));
    tabBar->setAttribute(Qt::WA_StyledBackground);
    auto *tabLayout = new QHBoxLayout(tabBar);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(22);
    const QStringList tabNames = {QStringLiteral("Rhythm"), QStringLiteral("Idle"), QStringLiteral("Appearance"),
                                  QStringLiteral("System"), QStringLiteral("Messages")};
    for (int i = 0; i < tabNames.size(); ++i) {
        auto *tab = new QPushButton(tabNames.at(i), tabBar);
        tab->setObjectName(QStringLiteral("tab"));
        tab->setCheckable(true);
        tab->setCursor(Qt::PointingHandCursor);
        connect(tab, &QPushButton::clicked, this, [this, i] { selectTab(i); });
        tabLayout->addWidget(tab);
        m_tabs << tab;
    }
    tabLayout->addStretch(1);
    root->addWidget(tabBar);

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(scrollable(buildRhythmPage()));
    m_pages->addWidget(scrollable(buildIdlePage()));
    m_pages->addWidget(scrollable(buildAppearancePage()));
    m_pages->addWidget(scrollable(buildSystemPage()));
    m_pages->addWidget(scrollable(buildMessagesPage()));
    root->addWidget(m_pages, 1);

    auto *footer = new QFrame(this);
    footer->setObjectName(QStringLiteral("footer"));
    footer->setStyleSheet(QStringLiteral("QFrame#footer { border-top: 1px solid rgba(255, 255, 255, 0.07); }"));
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(0, 16, 0, 24);
    footerLayout->setSpacing(10);
    m_status = makeLabel(QString(), "statusText", theme::font(13, QFont::Light), footer);
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *close = new QPushButton(QStringLiteral("Close"), footer);
    close->setObjectName(QStringLiteral("secondaryButton"));
    close->setCursor(Qt::PointingHandCursor);
    m_save = new QPushButton(QStringLiteral("Saved"), footer);
    m_save->setObjectName(QStringLiteral("primaryButton"));
    m_save->setCursor(Qt::PointingHandCursor);
    m_save->setDefault(true);
    footerLayout->addWidget(m_status, 1);
    footerLayout->addWidget(close);
    footerLayout->addWidget(m_save);
    root->addWidget(footer);

    connect(close, &QPushButton::clicked, this, &QWidget::close);
    connect(m_save, &QPushButton::clicked, this, &SettingsWindow::saveSettings);

    loadFromConfig();
    selectTab(0);
}

int SettingsWindow::tabCount() const
{
    return static_cast<int>(m_tabs.size());
}

void SettingsWindow::selectTab(int index)
{
    for (int i = 0; i < m_tabs.size(); ++i)
        m_tabs.at(i)->setChecked(i == index);
    m_pages->setCurrentIndex(index);
}

QWidget *SettingsWindow::makeRow(const QString &title, const QString &hint, QWidget *control)
{
    auto *row = new QFrame;
    row->setObjectName(QStringLiteral("row"));
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 18, 0, 18);
    layout->setSpacing(20);

    auto *text = new QVBoxLayout;
    text->setSpacing(4);
    text->addWidget(makeLabel(title, "rowTitle", theme::font(15, QFont::Medium), row));
    QLabel *hintLabel = makeLabel(hint, "rowHint", theme::font(13, QFont::Light), row);
    hintLabel->setWordWrap(true);
    text->addWidget(hintLabel);
    layout->addLayout(text, 1);
    layout->addWidget(control, 0, Qt::AlignVCenter);
    return row;
}

QWidget *SettingsWindow::makeSliderRow(const QString &title, const QString &hint, QSlider **slider)
{
    auto *row = new QFrame;
    row->setObjectName(QStringLiteral("row"));
    auto *layout = new QVBoxLayout(row);
    layout->setContentsMargins(0, 18, 0, 18);
    layout->setSpacing(10);

    auto *top = new QHBoxLayout;
    top->addWidget(makeLabel(title, "rowTitle", theme::font(15, QFont::Medium), row), 1);
    QLabel *value = makeLabel(QString(), "sliderValue", theme::font(15, QFont::Light), row);
    top->addWidget(value);
    layout->addLayout(top);

    *slider = new QSlider(Qt::Horizontal, row);
    (*slider)->setRange(20, 100);
    (*slider)->setSingleStep(1);
    (*slider)->setPageStep(5);
    (*slider)->setCursor(Qt::PointingHandCursor);
    (*slider)->setAccessibleName(title);
    layout->addWidget(*slider);
    connect(*slider, &QSlider::valueChanged, value, [value](int percent) {
        value->setText(QStringLiteral("%1%").arg(percent));
    });
    connect(*slider, &QSlider::valueChanged, this, &SettingsWindow::formChanged);

    QLabel *hintLabel = makeLabel(hint, "rowHint", theme::font(13, QFont::Light), row);
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);
    return row;
}

QWidget *SettingsWindow::buildRhythmPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 4, 8, 20);
    layout->setSpacing(0);

    m_workMinutes = new Stepper(1, 480, 1, QStringLiteral("min"));
    m_restSeconds = new Stepper(5, 600, 5, QStringLiteral("sec"));
    m_noticeSeconds = new Stepper(0, 600, 5, QStringLiteral("sec"));
    layout->addWidget(makeRow(QStringLiteral("Work interval"),
                              QStringLiteral("Active computer time before the rest overlay appears."), m_workMinutes));
    layout->addWidget(makeRow(QStringLiteral("Rest duration"),
                              QStringLiteral("How long the overlay stays before closing by itself."), m_restSeconds));
    layout->addWidget(makeRow(QStringLiteral("Pre-break notice"),
                              QStringLiteral("How early the small warning panel appears. 0 turns it off."),
                              m_noticeSeconds));
    layout->addStretch(1);

    for (Stepper *stepper : {m_workMinutes, m_restSeconds, m_noticeSeconds})
        connect(stepper, &Stepper::valueChanged, this, &SettingsWindow::formChanged);
    return page;
}

QWidget *SettingsWindow::buildIdlePage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 22, 8, 20);
    layout->setSpacing(0);

    m_timeline = new IdleTimeline(page);
    layout->addWidget(m_timeline);
    layout->addSpacing(6);

    m_idlePauseMinutes = new Stepper(0, 60, 1, QStringLiteral("min"));
    m_idleRestMinutes = new Stepper(1, 240, 1, QStringLiteral("min"));
    layout->addWidget(makeRow(QStringLiteral("Pause when idle after"),
                              QStringLiteral("Stop counting toward a break while you are away."), m_idlePauseMinutes));
    layout->addWidget(makeRow(QStringLiteral("Count idle as rest after"),
                              QStringLiteral("A long absence counts as a full rest and restarts the interval."),
                              m_idleRestMinutes));
    layout->addStretch(1);

    connect(m_idlePauseMinutes, &Stepper::valueChanged, this, [this](int minutes) {
        // Idle time must pause counting before it can count as rest.
        m_idleRestMinutes->setMinimum(qMax(1, minutes));
        formChanged();
    });
    connect(m_idleRestMinutes, &Stepper::valueChanged, this, &SettingsWindow::formChanged);
    return page;
}

QWidget *SettingsWindow::buildAppearancePage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 4, 8, 20);
    layout->setSpacing(0);

    auto *paletteRow = new QWidget(page);
    auto *paletteLayout = new QHBoxLayout(paletteRow);
    paletteLayout->setContentsMargins(0, 0, 0, 0);
    paletteLayout->setSpacing(8);
    m_paletteGroup = new QButtonGroup(this);
    m_paletteGroup->setExclusive(true);
    for (const theme::Palette &palette : theme::palettes()) {
        auto *button = new PaletteButton(palette, paletteRow);
        m_paletteGroup->addButton(button);
        paletteLayout->addWidget(button);
    }
    connect(m_paletteGroup, &QButtonGroup::buttonClicked, this, &SettingsWindow::formChanged);

    auto *colourRow = new QFrame(page);
    colourRow->setObjectName(QStringLiteral("row"));
    auto *colourLayout = new QVBoxLayout(colourRow);
    colourLayout->setContentsMargins(0, 18, 0, 18);
    colourLayout->setSpacing(10);
    colourLayout->addWidget(makeLabel(QStringLiteral("Colours"), "rowTitle", theme::font(15, QFont::Medium), colourRow));
    colourLayout->addWidget(paletteRow, 0, Qt::AlignLeft);
    QLabel *colourHint = makeLabel(QStringLiteral("The soft light behind the rest overlay and the ring colours."),
                                   "rowHint", theme::font(13, QFont::Light), colourRow);
    colourHint->setWordWrap(true);
    colourLayout->addWidget(colourHint);
    layout->addWidget(colourRow);

    layout->addWidget(makeSliderRow(QStringLiteral("Rest overlay opacity"),
                                    QStringLiteral("Higher values hide more of the desktop during a break."),
                                    &m_overlayOpacity));
    layout->addWidget(makeSliderRow(QStringLiteral("Pre-break panel opacity"),
                                    QStringLiteral("Higher values make the warning panel less see-through."),
                                    &m_panelOpacity));
    layout->addWidget(makeSliderRow(QStringLiteral("Floating countdown opacity"),
                                    QStringLiteral("Lower values let the countdown fade into the background."),
                                    &m_countdownOpacity));
    layout->addStretch(1);
    return page;
}

QWidget *SettingsWindow::buildSystemPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 4, 8, 20);
    layout->setSpacing(0);

    m_startWithBoot = new ToggleSwitch;
    m_startHidden = new ToggleSwitch;
    m_tones = new ToggleSwitch;
    const QString moment = platform::startupMomentName();
    m_startWithBoot->setAccessibleName(QStringLiteral("Start with %1").arg(moment));
    m_startHidden->setAccessibleName(QStringLiteral("Start hidden in tray"));
    m_tones->setAccessibleName(QStringLiteral("Notification tones"));

    layout->addWidget(makeRow(QStringLiteral("Start with %1").arg(moment),
                              QStringLiteral("Launch Eye Timer automatically when you sign in."), m_startWithBoot));
    layout->addWidget(makeRow(QStringLiteral("Start hidden in tray"),
                              QStringLiteral("Takes effect next launch. The countdown starts visible when off."),
                              m_startHidden));
    layout->addWidget(makeRow(QStringLiteral("Notification tones"),
                              QStringLiteral("A short tone when a rest starts and another when it completes."), m_tones));

    auto *previewRow = new QHBoxLayout;
    previewRow->setContentsMargins(0, 18, 0, 0);
    previewRow->setSpacing(10);
    auto *startTone = new QPushButton(QStringLiteral("Play rest-start tone"), page);
    auto *endTone = new QPushButton(QStringLiteral("Play rest-complete tone"), page);
    for (QPushButton *button : {startTone, endTone}) {
        button->setObjectName(QStringLiteral("secondaryButton"));
        button->setCursor(Qt::PointingHandCursor);
        previewRow->addWidget(button);
    }
    previewRow->addStretch(1);
    layout->addLayout(previewRow);
    layout->addStretch(1);

    connect(startTone, &QPushButton::clicked, this, [this] { m_app->previewTone(QStringLiteral("start")); });
    connect(endTone, &QPushButton::clicked, this, [this] { m_app->previewTone(QStringLiteral("end")); });
    for (ToggleSwitch *toggle : {m_startWithBoot, m_startHidden, m_tones})
        connect(toggle, &ToggleSwitch::toggled, this, &SettingsWindow::formChanged);
    return page;
}

QWidget *SettingsWindow::buildMessagesPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 18, 8, 20);
    layout->setSpacing(14);

    QLabel *hint = makeLabel(QStringLiteral("The rest overlay cycles through these, one per break. Click a line to edit it."),
                             "mutedText", theme::font(13, QFont::Light), page);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *list = new QWidget(page);
    m_messagesLayout = new QVBoxLayout(list);
    m_messagesLayout->setContentsMargins(0, 0, 0, 0);
    m_messagesLayout->setSpacing(0);
    layout->addWidget(list);

    auto *addRow = new QHBoxLayout;
    addRow->setSpacing(8);
    m_newMessage = new QLineEdit(page);
    m_newMessage->setPlaceholderText(QStringLiteral("Add a suggestion…"));
    m_newMessage->setAccessibleName(QStringLiteral("New suggestion"));
    auto *add = new QPushButton(QStringLiteral("Add"), page);
    add->setObjectName(QStringLiteral("secondaryButton"));
    add->setCursor(Qt::PointingHandCursor);
    addRow->addWidget(m_newMessage, 1);
    addRow->addWidget(add);
    layout->addLayout(addRow);
    layout->addStretch(1);

    connect(add, &QPushButton::clicked, this, &SettingsWindow::addMessage);
    connect(m_newMessage, &QLineEdit::returnPressed, this, &SettingsWindow::addMessage);
    return page;
}

QStringList SettingsWindow::currentMessages() const
{
    QStringList messages;
    for (const QLineEdit *edit : m_messageEdits) {
        const QString text = edit->text().trimmed();
        if (!text.isEmpty())
            messages << text;
    }
    return messages;
}

void SettingsWindow::rebuildMessages(const QStringList &messages)
{
    while (QLayoutItem *item = m_messagesLayout->takeAt(0)) {
        if (QWidget *widget = item->widget())
            widget->deleteLater();
        delete item;
    }
    m_messageEdits.clear();

    for (int i = 0; i < messages.size(); ++i) {
        auto *row = new QFrame;
        row->setObjectName(QStringLiteral("row"));
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(12);

        QLabel *number = makeLabel(QString::number(i + 1), "messageNumber", theme::font(13, QFont::Light), row);
        number->setFixedWidth(20);
        auto *edit = new QLineEdit(messages.at(i), row);
        edit->setObjectName(QStringLiteral("messageEdit"));
        edit->setAccessibleName(QStringLiteral("Suggestion %1").arg(i + 1));
        auto *remove = new IconButton(theme::Icon::Close, QStringLiteral("Remove suggestion %1").arg(i + 1), row);
        remove->setFilled(false);
        remove->setIconColor(theme::text(0.55));
        remove->setRadius(8);

        layout->addWidget(number);
        layout->addWidget(edit, 1);
        layout->addWidget(remove);
        m_messagesLayout->addWidget(row);
        m_messageEdits << edit;

        connect(edit, &QLineEdit::textChanged, this, &SettingsWindow::formChanged);
        connect(remove, &IconButton::clicked, this, [this, edit] {
            QStringList messages;
            for (QLineEdit *other : std::as_const(m_messageEdits)) {
                if (other != edit && !other->text().trimmed().isEmpty())
                    messages << other->text().trimmed();
            }
            rebuildMessages(messages);
            formChanged();
        });
    }
}

void SettingsWindow::addMessage()
{
    const QString text = m_newMessage->text().trimmed();
    if (text.isEmpty())
        return;
    rebuildMessages(currentMessages() << text);
    m_newMessage->clear();
    formChanged();
}

AppConfig SettingsWindow::currentFormConfig() const
{
    AppConfig config = m_app->config();
    config.restIntervalSeconds = m_workMinutes->value() * 60;
    config.restDurationSeconds = m_restSeconds->value();
    config.prepareNoticeSeconds = m_noticeSeconds->value();
    config.idlePauseAfterSeconds = m_idlePauseMinutes->value() * 60;
    config.idleResetAfterSeconds = m_idleRestMinutes->value() * 60;
    config.overlayAlpha = m_overlayOpacity->value() / 100.0;
    config.preparePanelAlpha = m_panelOpacity->value() / 100.0;
    config.countdownAlpha = m_countdownOpacity->value() / 100.0;
    config.startWithBoot = m_startWithBoot->isChecked();
    config.startMinimized = m_startHidden->isChecked();
    config.soundEnabled = m_tones->isChecked();
    if (auto *button = qobject_cast<PaletteButton *>(m_paletteGroup->checkedButton()))
        config.palette = button->paletteId();
    config.healthySuggestions = currentMessages();
    return config;
}

void SettingsWindow::loadFromConfig()
{
    m_loading = true;
    const AppConfig &config = m_app->config();
    m_workMinutes->setValue(qMax(1, qRound(config.restIntervalSeconds / 60.0)));
    m_restSeconds->setValue(config.restDurationSeconds);
    m_noticeSeconds->setValue(config.prepareNoticeSeconds);
    m_idlePauseMinutes->setValue(qMax(0, qRound(config.idlePauseAfterSeconds / 60.0)));
    m_idleRestMinutes->setMinimum(qMax(1, m_idlePauseMinutes->value()));
    m_idleRestMinutes->setValue(qMax(1, qRound(config.idleResetAfterSeconds / 60.0)));
    m_overlayOpacity->setValue(qRound(config.overlayAlpha * 100));
    m_panelOpacity->setValue(qRound(config.preparePanelAlpha * 100));
    m_countdownOpacity->setValue(qRound(config.countdownAlpha * 100));
    m_startWithBoot->setChecked(config.startWithBoot);
    m_startHidden->setChecked(config.startMinimized);
    m_tones->setChecked(config.soundEnabled);
    for (QAbstractButton *button : m_paletteGroup->buttons()) {
        auto *paletteButton = static_cast<PaletteButton *>(button);
        paletteButton->setChecked(paletteButton->paletteId() == m_app->palette().id);
    }
    rebuildMessages(config.healthySuggestions);
    m_status->clear();
    m_loading = false;
    formChanged();
}

void SettingsWindow::formChanged()
{
    if (m_loading)
        return;
    const AppConfig form = currentFormConfig();
    const bool dirty = !(form == m_app->config());
    m_save->setEnabled(dirty);
    m_save->setText(dirty ? QStringLiteral("Save changes") : QStringLiteral("Saved"));
    if (dirty)
        m_status->setText(QStringLiteral("Unsaved changes"));
    else if (m_status->text() == QStringLiteral("Unsaved changes"))
        m_status->clear();
    m_timeline->setMinutes(m_idlePauseMinutes->value(), m_idleRestMinutes->value());
    updateSummary();
    applyPaletteStyle();
}

void SettingsWindow::updateSummary()
{
    const theme::Palette &palette = theme::palette(currentFormConfig().palette);
    const int work = m_workMinutes->value();
    const int rest = m_restSeconds->value();
    m_summary->setText(
        QStringLiteral("<span style=\"color:%1\">Every </span>"
                       "<span style=\"color:%2; font-weight:400\">%3 %4</span>"
                       "<span style=\"color:%1\"> of screen time, rest your eyes for </span>"
                       "<span style=\"color:%5; font-weight:400\">%6 %7</span>"
                       "<span style=\"color:%1\">.</span>")
            .arg(theme::text(0.82).name(QColor::HexArgb), palette.lines[0].name(), QString::number(work),
                 work == 1 ? QStringLiteral("minute") : QStringLiteral("minutes"), palette.lines[2].name(),
                 QString::number(rest), rest == 1 ? QStringLiteral("second") : QStringLiteral("seconds")));
}

void SettingsWindow::applyPaletteStyle()
{
    const QString paletteId = currentFormConfig().palette;
    if (property("appliedPalette").toString() == paletteId)
        return;
    setProperty("appliedPalette", paletteId);
    const theme::Palette &palette = theme::palette(paletteId);
    setStyleSheet(settingsStyleSheet(palette.lines[1]));
    m_timeline->setColors(palette.lines[0], palette.lines[2]);
    update();
}

void SettingsWindow::saveSettings()
{
    QString savedPath;
    QString error;
    if (!m_app->applySettings(currentFormConfig(), &savedPath, &error)) {
        m_status->setText(QStringLiteral("Settings were not saved: %1").arg(error));
        return;
    }
    m_status->setText(QStringLiteral("Saved to %1").arg(savedPath));
    formChanged();
}

void SettingsWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), theme::ground());
    const theme::Palette &palette = theme::palette(property("appliedPalette").toString());
    const qreal sx = width() / 640.0;
    theme::paintField(painter, QRectF(-160 * sx, -300, 520 * sx, 460), palette.fields[0], 0.55);
    theme::paintField(painter, QRectF(180 * sx, -340, 480 * sx, 440), palette.fields[1], 0.5);
    theme::paintField(painter, QRectF(380 * sx, -260, 460 * sx, 420), palette.fields[2], 0.45);
    theme::paintGrain(painter, rect(), 0.05);
}
