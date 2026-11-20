#include "UpdateChecker.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

Q_DECLARE_METATYPE(ReleaseInfo)

// Exercises the full check against file:// responses, so no test touches GitHub.
class TestUpdateChecker : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<ReleaseInfo>();
        qRegisterMetaType<UpdateChecker::Result>();
        QVERIFY(m_dir.isValid());
    }

    void cleanup()
    {
        qunsetenv("EYETIMER_UPDATE_URL");
    }

    void defaultEndpoint()
    {
        QCOMPARE(UpdateChecker::endpoint(), QUrl(QString::fromLatin1(UpdateChecker::kReleasesUrl)));
    }

    void parsesRelease()
    {
        QString error;
        const auto release = UpdateChecker::parseRelease(releaseJson("v0.11.0"), &error);
        QVERIFY2(release.has_value(), qPrintable(error));
        QCOMPARE(release->version, QStringLiteral("0.11.0"));
        QCOMPARE(release->tag, QStringLiteral("v0.11.0"));
        QCOMPARE(release->pageUrl, QStringLiteral("https://github.com/ahmad-amrey/eye_timer/releases/tag/v0.11.0"));
        QVERIFY(release->notes.contains(QStringLiteral("Smoother tones")));
    }

    void ignoresDraftsAndPrereleases()
    {
        QVERIFY(!UpdateChecker::parseRelease(releaseJson("v0.11.0", true, false), nullptr));
        QVERIFY(!UpdateChecker::parseRelease(releaseJson("v0.11.0", false, true), nullptr));
        QVERIFY(!UpdateChecker::parseRelease("not json", nullptr));
        QVERIFY(!UpdateChecker::parseRelease("{\"name\": \"no tag\"}", nullptr));
    }

    void summarize_data()
    {
        QTest::addColumn<QString>("markdown");
        QTest::addColumn<QString>("summary");
        QTest::newRow("first paragraph") << "## [0.11.0]\n\nSmoother **tones** and a [new](https://x) icon.\n\nMore text."
                                         << "Smoother tones and a new icon.";
        QTest::newRow("list items") << "### Fixed\n- One\n- Two\n- Three\n- Four" << "One · Two · Three";
        QTest::newRow("empty") << "" << "";
    }

    void summarize()
    {
        QFETCH(QString, markdown);
        QFETCH(QString, summary);
        QCOMPARE(UpdateChecker::summarize(markdown), summary);
    }

    void summarizeTruncates()
    {
        const QString longText = QString(QStringLiteral("word ")).repeated(100);
        const QString summary = UpdateChecker::summarize(longText, 50);
        QVERIFY(summary.size() <= 51);
        QVERIFY(summary.endsWith(QChar(0x2026)));
    }

    void check_data()
    {
        QTest::addColumn<QByteArray>("response");
        QTest::addColumn<QString>("current");
        QTest::addColumn<UpdateChecker::Result>("expected");
        QTest::newRow("newer") << releaseJson("v0.11.0") << "0.10.0" << UpdateChecker::Result::UpdateAvailable;
        QTest::newRow("same") << releaseJson("v0.10.0") << "0.10.0" << UpdateChecker::Result::UpToDate;
        QTest::newRow("older") << releaseJson("v0.9.0") << "0.10.0" << UpdateChecker::Result::UpToDate;
        QTest::newRow("garbage") << QByteArray("<html>") << "0.10.0" << UpdateChecker::Result::Failed;
    }

    void check()
    {
        QFETCH(QByteArray, response);
        QFETCH(QString, current);
        QFETCH(UpdateChecker::Result, expected);

        const QString path = m_dir.filePath(QStringLiteral("%1.json").arg(QTest::currentDataTag()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(response);
        file.close();
        qputenv("EYETIMER_UPDATE_URL", QUrl::fromLocalFile(path).toString().toUtf8());

        UpdateChecker checker(current);
        QSignalSpy spy(&checker, &UpdateChecker::finished);
        checker.check();
        QVERIFY(checker.isChecking());
        QVERIFY(spy.wait(5000));
        QCOMPARE(spy.first().at(0).value<UpdateChecker::Result>(), expected);
        QVERIFY(!checker.isChecking());
    }

    void missingEndpointFails()
    {
        qputenv("EYETIMER_UPDATE_URL",
                QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("absent.json"))).toString().toUtf8());
        UpdateChecker checker(QStringLiteral("0.10.0"));
        QSignalSpy spy(&checker, &UpdateChecker::finished);
        checker.check();
        QVERIFY(spy.wait(5000));
        QCOMPARE(spy.first().at(0).value<UpdateChecker::Result>(), UpdateChecker::Result::Failed);
    }

private:
    static QByteArray releaseJson(const char *tag, bool draft = false, bool prerelease = false)
    {
        const QString name = QString::fromLatin1(tag);
        QJsonObject object;
        object.insert(QStringLiteral("tag_name"), name);
        object.insert(QStringLiteral("name"), QStringLiteral("Eye Timer ") + name);
        object.insert(QStringLiteral("draft"), draft);
        object.insert(QStringLiteral("prerelease"), prerelease);
        object.insert(QStringLiteral("html_url"), QStringLiteral("https://github.com/ahmad-amrey/eye_timer/releases/tag/") + name);
        object.insert(QStringLiteral("body"), QStringLiteral("Smoother tones on every platform.\n\n### Fixed\n- Stutter"));
        return QJsonDocument(object).toJson();
    }

    QTemporaryDir m_dir;
};

QTEST_GUILESS_MAIN(TestUpdateChecker)
#include "tst_updatechecker.moc"
