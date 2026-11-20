#include "Config.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

// Points the per-user data folder at a temporary directory for each test.
class TestConfig : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_home.reset(new QTemporaryDir);
        QVERIFY(m_home->isValid());
#if defined(Q_OS_WIN)
        qputenv("LOCALAPPDATA", QDir::toNativeSeparators(m_home->path()).toLocal8Bit());
#elif defined(Q_OS_MACOS)
        qputenv("HOME", m_home->path().toLocal8Bit());
#else
        qputenv("XDG_DATA_HOME", m_home->path().toLocal8Bit());
#endif
        QVERIFY(settingsFilePath().startsWith(QDir::fromNativeSeparators(m_home->path())));
    }

    void missingFileGivesDefaults()
    {
        QCOMPARE(loadSavedConfig(), AppConfig{});
    }

    void roundTrip()
    {
        AppConfig config;
        config.restIntervalSeconds = 25 * 60;
        config.restDurationSeconds = 30;
        config.prepareNoticeSeconds = 0;
        config.idlePauseAfterSeconds = 120;
        config.idleResetAfterSeconds = 900;
        config.overlayAlpha = 0.5;
        config.preparePanelAlpha = 0.75;
        config.countdownAlpha = 0.25;
        config.startMinimized = true;
        config.startWithBoot = true;
        config.soundEnabled = false;
        config.checkForUpdates = false;
        config.palette = QStringLiteral("tide");
        config.healthySuggestions = {QStringLiteral("Blink slowly."), QStringLiteral("Look out of the window.")};

        QString savedPath;
        QString error;
        QVERIFY2(saveConfig(config, &savedPath, &error), qPrintable(error));
        QVERIFY(QFile::exists(settingsFilePath()));
        QCOMPARE(loadSavedConfig(), config);
    }

    void readsPythonEraFile()
    {
        // settings.json as written by the Python version: no palette or
        // update keys, suggestions with blank entries.
        writeSettings(R"({
            "rest_interval_seconds": 1200,
            "rest_duration_seconds": 20,
            "prepare_notice_seconds": 20,
            "idle_pause_after_seconds": 180,
            "idle_reset_after_seconds": 600,
            "overlay_alpha": 0.8,
            "prepare_panel_alpha": 0.8,
            "countdown_alpha": 0.8,
            "start_minimized": false,
            "start_with_boot": true,
            "sound_enabled": true,
            "healthy_suggestions": ["Look far away.", "  ", "Relax your shoulders."]
        })");
        const AppConfig config = loadSavedConfig();
        QCOMPARE(config.idlePauseAfterSeconds, 180);
        QCOMPARE(config.overlayAlpha, 0.8);
        QVERIFY(config.startWithBoot);
        QCOMPARE(config.palette, AppConfig{}.palette);
        QVERIFY(config.checkForUpdates);
        QCOMPARE(config.healthySuggestions,
                 QStringList({QStringLiteral("Look far away."), QStringLiteral("Relax your shoulders.")}));
    }

    void invalidFilesFallBackToDefaults_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::newRow("not json") << QByteArray("{ nope");
        QTest::newRow("array") << QByteArray("[1, 2, 3]");
        QTest::newRow("negative interval") << QByteArray(R"({"rest_interval_seconds": -5})");
        QTest::newRow("alpha out of range") << QByteArray(R"({"overlay_alpha": 1.5})");
        QTest::newRow("reset before pause") << QByteArray(R"({"idle_pause_after_seconds": 600, "idle_reset_after_seconds": 60})");
        QTest::newRow("no suggestions") << QByteArray(R"({"healthy_suggestions": []})");
    }

    void invalidFilesFallBackToDefaults()
    {
        QFETCH(QByteArray, json);
        writeSettings(json);
        QCOMPARE(loadSavedConfig(), AppConfig{});
    }

    void validation()
    {
        QVERIFY(validateConfig(AppConfig{}).isEmpty());
        AppConfig config;
        config.restDurationSeconds = 0;
        config.healthySuggestions.clear();
        QCOMPARE(validateConfig(config).size(), 2);
    }

private:
    void writeSettings(const QByteArray &json)
    {
        QDir().mkpath(QFileInfo(settingsFilePath()).absolutePath());
        QFile file(settingsFilePath());
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(json);
    }

    std::unique_ptr<QTemporaryDir> m_home;
};

QTEST_GUILESS_MAIN(TestConfig)
#include "tst_config.moc"
