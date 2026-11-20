#include "UpdateChecker.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

Q_DECLARE_METATYPE(ReleaseInfo)

// Minimal HTTP server standing in for api.github.com and github.com: each
// "METHOD /path" gets a canned raw response.
class FakeGitHub : public QObject
{
public:
    explicit FakeGitHub(QHash<QByteArray, QByteArray> routes)
        : m_routes(std::move(routes))
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = m_server.nextPendingConnection()) {
                // Each connection owns its buffer: keying a shared table by the
                // socket pointer let a new connection that reused a freed
                // address inherit an old request, which made the test flaky.
                auto request = std::make_shared<QByteArray>();
                auto answered = std::make_shared<bool>(false);
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket, request, answered] {
                    *request += socket->readAll();
                    if (*answered || !request->contains("\r\n\r\n"))
                        return;
                    *answered = true;
                    const QList<QByteArray> requestLine = request->left(request->indexOf("\r\n")).split(' ');
                    const QByteArray key = requestLine.value(0) + ' ' + requestLine.value(1);
                    socket->write(m_routes.value(key, "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"));
                    socket->disconnectFromHost();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
        m_server.listen(QHostAddress::LocalHost);
    }

    QString url(const char *path) const
    {
        return QStringLiteral("http://127.0.0.1:%1%2").arg(m_server.serverPort()).arg(QLatin1String(path));
    }

private:
    QTcpServer m_server;
    QHash<QByteArray, QByteArray> m_routes;
};

namespace {
const QByteArray kRateLimited = "HTTP/1.1 403 rate limit exceeded\r\nX-RateLimit-Limit: 60\r\nX-RateLimit-Remaining: 0\r\n"
                                "X-RateLimit-Reset: 4102444800\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
const QByteArray kRedirectToTag = "HTTP/1.1 302 Found\r\n"
                                  "Location: https://github.com/ahmad-amrey/eye_timer/releases/tag/v0.11.0\r\n"
                                  "Content-Length: 0\r\nConnection: close\r\n\r\n";
const QByteArray kServerError = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
} // namespace

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
        qunsetenv("EYETIMER_RELEASES_PAGE_URL");
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

    void installerAssetName_data()
    {
        QTest::addColumn<QString>("os");
        QTest::addColumn<QString>("cpu");
        QTest::addColumn<bool>("debian");
        QTest::addColumn<QString>("asset");
        QTest::newRow("windows x64") << "windows" << "x86_64" << false << "EyeTimer-windows-x64-setup.exe";
        QTest::newRow("windows arm (emulation)") << "windows" << "arm64" << false << "EyeTimer-windows-x64-setup.exe";
        QTest::newRow("macos apple silicon") << "macos" << "arm64" << false << "EyeTimer-macos.dmg";
        QTest::newRow("macos intel") << "macos" << "x86_64" << false << "EyeTimer-macos.dmg";
        QTest::newRow("ubuntu x64") << "linux" << "x86_64" << true << "EyeTimer-linux-amd64.deb";
        QTest::newRow("fedora x64") << "linux" << "x86_64" << false << "";
        QTest::newRow("debian arm") << "linux" << "arm64" << true << "";
    }

    void installerAssetName()
    {
        QFETCH(QString, os);
        QFETCH(QString, cpu);
        QFETCH(bool, debian);
        QFETCH(QString, asset);
        QCOMPARE(UpdateChecker::installerAssetName(os, cpu, debian), asset);
    }

    void picksThisPlatformsInstaller()
    {
        const auto release = UpdateChecker::parseRelease(releaseJson("v0.11.0"), nullptr);
        QVERIFY(release.has_value());
        const QString asset = UpdateChecker::installerAssetName();
        if (asset.isEmpty()) {
            QVERIFY(release->downloadUrl.isEmpty());
        } else {
            QCOMPARE(release->downloadUrl, UpdateChecker::assetDownloadUrl("v0.11.0", asset));
            QCOMPARE(UpdateChecker::safeReleasePageUrl(release->downloadUrl), QUrl(release->downloadUrl));
        }
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

    void tagFromReleasePageUrl_data()
    {
        QTest::addColumn<QString>("url");
        QTest::addColumn<QString>("tag");
        QTest::newRow("tag page") << "https://github.com/ahmad-amrey/eye_timer/releases/tag/v0.11.0" << "v0.11.0";
        QTest::newRow("trailing slash") << "https://github.com/a/b/releases/tag/v1.2.3/" << "v1.2.3";
        QTest::newRow("no release yet") << "https://github.com/ahmad-amrey/eye_timer/releases" << "";
        QTest::newRow("empty") << "" << "";
    }

    void tagFromReleasePageUrl()
    {
        QFETCH(QString, url);
        QFETCH(QString, tag);
        QCOMPARE(UpdateChecker::tagFromReleasePageUrl(QUrl(url)), tag);
    }

    void safeReleasePageUrl_data()
    {
        QTest::addColumn<QString>("url");
        QTest::addColumn<bool>("kept");
        QTest::newRow("release tag page") << "https://github.com/ahmad-amrey/eye_timer/releases/tag/v0.11.0" << true;
        QTest::newRow("latest page") << "https://github.com/ahmad-amrey/eye_timer/releases/latest" << true;
        QTest::newRow("installer download")
            << "https://github.com/ahmad-amrey/eye_timer/releases/download/v0.11.0/EyeTimer-macos.dmg" << true;
        QTest::newRow("other repository") << "https://github.com/someone/eye_timer/releases/tag/v9" << false;
        QTest::newRow("look-alike host") << "https://github.com.evil.example/ahmad-amrey/eye_timer/releases/" << false;
        QTest::newRow("user info trick") << "https://github.com@evil.example/ahmad-amrey/eye_timer/releases/" << false;
        QTest::newRow("plain http") << "http://github.com/ahmad-amrey/eye_timer/releases/latest" << false;
        QTest::newRow("other port") << "https://github.com:8443/ahmad-amrey/eye_timer/releases/latest" << false;
        QTest::newRow("not releases") << "https://github.com/ahmad-amrey/eye_timer/archive/main.zip" << false;
        QTest::newRow("file url") << "file:///C:/Windows/System32/calc.exe" << false;
        QTest::newRow("empty") << "" << false;
    }

    void safeReleasePageUrl()
    {
        QFETCH(QString, url);
        QFETCH(bool, kept);
        const QUrl expected = kept ? QUrl(url) : QUrl(QString::fromLatin1(UpdateChecker::kReleasesPageUrl));
        QCOMPARE(UpdateChecker::safeReleasePageUrl(url), expected);
    }

    void rateLimitedApiFallsBackToReleasePage()
    {
        FakeGitHub github({{"GET /api", kRateLimited}, {"HEAD /latest", kRedirectToTag}});
        qputenv("EYETIMER_UPDATE_URL", github.url("/api").toUtf8());
        qputenv("EYETIMER_RELEASES_PAGE_URL", github.url("/latest").toUtf8());

        UpdateChecker checker(QStringLiteral("0.10.0"));
        QSignalSpy spy(&checker, &UpdateChecker::finished);
        checker.check();
        QVERIFY(spy.wait(5000));
        QCOMPARE(spy.first().at(0).value<UpdateChecker::Result>(), UpdateChecker::Result::UpdateAvailable);
        const auto release = spy.first().at(1).value<ReleaseInfo>();
        QCOMPARE(release.version, QStringLiteral("0.11.0"));
        QCOMPARE(release.pageUrl, QStringLiteral("https://github.com/ahmad-amrey/eye_timer/releases/tag/v0.11.0"));
        QVERIFY(release.notes.isEmpty());
        // Without the API, the installer link is built from the tag.
        const QString asset = UpdateChecker::installerAssetName();
        QCOMPARE(release.downloadUrl, asset.isEmpty() ? QString() : UpdateChecker::assetDownloadUrl("v0.11.0", asset));
    }

    void rateLimitedEverywhereSaysSo()
    {
        FakeGitHub github({{"GET /api", kRateLimited}, {"HEAD /latest", kServerError}});
        qputenv("EYETIMER_UPDATE_URL", github.url("/api").toUtf8());
        qputenv("EYETIMER_RELEASES_PAGE_URL", github.url("/latest").toUtf8());

        UpdateChecker checker(QStringLiteral("0.10.0"));
        QSignalSpy spy(&checker, &UpdateChecker::finished);
        checker.check();
        QVERIFY(spy.wait(5000));
        QCOMPARE(spy.first().at(0).value<UpdateChecker::Result>(), UpdateChecker::Result::Failed);
        const QString error = spy.first().at(2).toString();
        QVERIFY2(error.contains(QStringLiteral("limiting update checks")), qPrintable(error));
        QVERIFY2(error.contains(QStringLiteral("Try again after")), qPrintable(error));
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
        QJsonArray assets;
        for (const char *file : {"EyeTimer-windows-x64-setup.exe", "EyeTimer-macos.dmg", "EyeTimer-linux-amd64.deb",
                                 "SHA256SUMS.txt"}) {
            QJsonObject asset;
            asset.insert(QStringLiteral("name"), QString::fromLatin1(file));
            asset.insert(QStringLiteral("browser_download_url"),
                         QStringLiteral("https://github.com/ahmad-amrey/eye_timer/releases/download/%1/%2")
                             .arg(name, QString::fromLatin1(file)));
            assets.append(asset);
        }
        object.insert(QStringLiteral("assets"), assets);
        return QJsonDocument(object).toJson();
    }

    QTemporaryDir m_dir;
};

QTEST_GUILESS_MAIN(TestUpdateChecker)
#include "tst_updatechecker.moc"
