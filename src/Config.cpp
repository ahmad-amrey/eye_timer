#include "Config.h"

#include "Platform.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

// Keys are shared with the settings.json files written by the Python version.
constexpr auto kRestInterval = "rest_interval_seconds";
constexpr auto kRestDuration = "rest_duration_seconds";
constexpr auto kPrepareNotice = "prepare_notice_seconds";
constexpr auto kIdlePause = "idle_pause_after_seconds";
constexpr auto kIdleReset = "idle_reset_after_seconds";
constexpr auto kOverlayAlpha = "overlay_alpha";
constexpr auto kPrepareAlpha = "prepare_panel_alpha";
constexpr auto kCountdownAlpha = "countdown_alpha";
constexpr auto kStartMinimized = "start_minimized";
constexpr auto kStartWithBoot = "start_with_boot";
constexpr auto kSoundEnabled = "sound_enabled";
constexpr auto kPalette = "palette";
constexpr auto kSuggestions = "healthy_suggestions";

void readInt(const QJsonObject &data, const char *key, int &target)
{
    const QJsonValue value = data.value(QLatin1String(key));
    if (value.isDouble())
        target = value.toInt(target);
}

void readDouble(const QJsonObject &data, const char *key, double &target)
{
    const QJsonValue value = data.value(QLatin1String(key));
    if (value.isDouble())
        target = value.toDouble(target);
}

void readBool(const QJsonObject &data, const char *key, bool &target)
{
    const QJsonValue value = data.value(QLatin1String(key));
    if (value.isBool())
        target = value.toBool(target);
}

} // namespace

QStringList validateConfig(const AppConfig &config)
{
    QStringList errors;
    if (config.restIntervalSeconds <= 0)
        errors << QStringLiteral("rest_interval_seconds must be positive");
    if (config.restDurationSeconds <= 0)
        errors << QStringLiteral("rest_duration_seconds must be positive");
    if (config.prepareNoticeSeconds < 0)
        errors << QStringLiteral("prepare_notice_seconds cannot be negative");
    if (config.idlePauseAfterSeconds < 0)
        errors << QStringLiteral("idle_pause_after_seconds cannot be negative");
    if (config.overlayAlpha < 0.0 || config.overlayAlpha > 1.0)
        errors << QStringLiteral("overlay_alpha must be between 0.0 and 1.0");
    if (config.preparePanelAlpha < 0.0 || config.preparePanelAlpha > 1.0)
        errors << QStringLiteral("prepare_panel_alpha must be between 0.0 and 1.0");
    if (config.countdownAlpha < 0.0 || config.countdownAlpha > 1.0)
        errors << QStringLiteral("countdown_alpha must be between 0.0 and 1.0");
    if (config.idleResetAfterSeconds < config.idlePauseAfterSeconds)
        errors << QStringLiteral("idle_reset_after_seconds must be >= idle_pause_after_seconds");
    if (config.healthySuggestions.isEmpty())
        errors << QStringLiteral("healthy_suggestions cannot be empty");
    if (config.tickMs <= 0)
        errors << QStringLiteral("tick_ms must be positive");
    return errors;
}

QString settingsFilePath()
{
    return QDir(platform::appDataDirectory()).filePath(QStringLiteral("settings.json"));
}

AppConfig loadSavedConfig(const AppConfig &defaults)
{
    QFile file(settingsFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return defaults;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return defaults;

    const QJsonObject data = document.object();
    AppConfig config = defaults;
    readInt(data, kRestInterval, config.restIntervalSeconds);
    readInt(data, kRestDuration, config.restDurationSeconds);
    readInt(data, kPrepareNotice, config.prepareNoticeSeconds);
    readInt(data, kIdlePause, config.idlePauseAfterSeconds);
    readInt(data, kIdleReset, config.idleResetAfterSeconds);
    readDouble(data, kOverlayAlpha, config.overlayAlpha);
    readDouble(data, kPrepareAlpha, config.preparePanelAlpha);
    readDouble(data, kCountdownAlpha, config.countdownAlpha);
    readBool(data, kStartMinimized, config.startMinimized);
    readBool(data, kStartWithBoot, config.startWithBoot);
    readBool(data, kSoundEnabled, config.soundEnabled);

    const QJsonValue palette = data.value(QLatin1String(kPalette));
    if (palette.isString())
        config.palette = palette.toString();

    const QJsonValue suggestions = data.value(QLatin1String(kSuggestions));
    if (suggestions.isArray()) {
        QStringList items;
        for (const QJsonValue &item : suggestions.toArray()) {
            const QString text = item.toVariant().toString().trimmed();
            if (!text.isEmpty())
                items << text;
        }
        config.healthySuggestions = items;
    }

    if (!validateConfig(config).isEmpty())
        return defaults;
    return config;
}

bool saveConfig(const AppConfig &config, QString *savedPath, QString *error)
{
    const QString path = settingsFilePath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (error)
            *error = QStringLiteral("Could not create %1").arg(QFileInfo(path).absolutePath());
        return false;
    }

    QJsonObject data;
    data.insert(QLatin1String(kRestInterval), config.restIntervalSeconds);
    data.insert(QLatin1String(kRestDuration), config.restDurationSeconds);
    data.insert(QLatin1String(kPrepareNotice), config.prepareNoticeSeconds);
    data.insert(QLatin1String(kIdlePause), config.idlePauseAfterSeconds);
    data.insert(QLatin1String(kIdleReset), config.idleResetAfterSeconds);
    data.insert(QLatin1String(kOverlayAlpha), config.overlayAlpha);
    data.insert(QLatin1String(kPrepareAlpha), config.preparePanelAlpha);
    data.insert(QLatin1String(kCountdownAlpha), config.countdownAlpha);
    data.insert(QLatin1String(kStartMinimized), config.startMinimized);
    data.insert(QLatin1String(kStartWithBoot), config.startWithBoot);
    data.insert(QLatin1String(kSoundEnabled), config.soundEnabled);
    data.insert(QLatin1String(kPalette), config.palette);
    data.insert(QLatin1String(kSuggestions), QJsonArray::fromStringList(config.healthySuggestions));

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(data).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    if (savedPath)
        *savedPath = QDir::toNativeSeparators(path);
    return true;
}
