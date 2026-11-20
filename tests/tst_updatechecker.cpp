#include "UpdateChecker.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

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
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                    m_buffer[socket] += socket->readAll();
                    if (!m_buffer[socket].contains("\r\n\r\n"))
                        return;
                    const QList<QByteArray> requestLine = m_buffer[socket].left(m_buffer[socket].indexOf("\r\n")).split(' ');
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
    QHash<QTcpSocket *, QByteArray> m_buffer;
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
        return QJsonDocument(object).toJson();
    }

    QTemporaryDir m_dir;
};

QTEST_GUILESS_MAIN(TestUpdateChecker)
#include "tst_updatechecker.moc"
