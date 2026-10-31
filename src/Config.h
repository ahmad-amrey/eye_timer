#pragma once

#include <QString>
#include <QStringList>

struct AppConfig {
    int restIntervalSeconds = 20 * 60;
    int restDurationSeconds = 20;
    int prepareNoticeSeconds = 20;
    int idlePauseAfterSeconds = 60;
    int idleResetAfterSeconds = 10 * 60;
    int tickMs = 1000;
    double overlayAlpha = 0.86;
    double preparePanelAlpha = 0.96;
    double countdownAlpha = 0.88;
    bool startMinimized = false;
    bool startWithBoot = false;
    bool soundEnabled = true;
    QString palette = QStringLiteral("aurora");
    QStringList healthySuggestions = {
        QStringLiteral("Look at something at least 6 meters away."),
        QStringLiteral("Close your eyes and relax your shoulders."),
        QStringLiteral("Let your eyes wander around the room."),
        QStringLiteral("Soften your focus and breathe evenly."),
        QStringLiteral("Check your posture while your eyes rest."),
    };

    bool operator==(const AppConfig &) const = default;
};

// Returns one message per invalid field; empty when the config is valid.
QStringList validateConfig(const AppConfig &config);

QString settingsFilePath();

// Reads settings.json over the defaults. Falls back to the defaults when the
// file is missing, unreadable or fails validation.
AppConfig loadSavedConfig(const AppConfig &defaults = AppConfig{});

// Writes settings.json. On failure returns false and fills `error`.
bool saveConfig(const AppConfig &config, QString *savedPath, QString *error);
