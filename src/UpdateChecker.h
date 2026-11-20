#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

#include <optional>

class QNetworkAccessManager;
class QNetworkReply;

struct ReleaseInfo {
    QString version; // "0.11.0"
    QString tag;     // "v0.11.0"
    QString name;
    QString pageUrl; // the release page on GitHub
    QString notes;   // Markdown body; empty when only the release page answered
};

// Asks GitHub for the latest published release and compares it with the
// running version. Notify-only: nothing is downloaded or installed.
//
// The API answers with notes, but allows 60 anonymous requests per hour per
// IP address, which a shared network can use up. When it refuses, the
// version is read from where the release page's /latest redirect points.
//
// EYETIMER_UPDATE_URL and EYETIMER_RELEASES_PAGE_URL override the two
// endpoints (file:// and local http:// URLs work, for tests).
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    enum class Result { UpdateAvailable, UpToDate, Failed };
    Q_ENUM(Result)

    static constexpr auto kReleasesUrl = "https://api.github.com/repos/ahmad-amrey/eye_timer/releases/latest";
    static constexpr auto kReleasesPageUrl = "https://github.com/ahmad-amrey/eye_timer/releases/latest";

    explicit UpdateChecker(const QString &currentVersion, QObject *parent = nullptr);

    void check();
    bool isChecking() const { return !m_reply.isNull(); }
    QString currentVersion() const { return m_currentVersion; }

    static QUrl endpoint();
    static QUrl releasesPageEndpoint();
    static std::optional<ReleaseInfo> parseRelease(const QByteArray &json, QString *error);
    // The tag a ".../releases/tag/<tag>" URL points at; empty for anything else.
    static QString tagFromReleasePageUrl(const QUrl &url);
    // First paragraph of release notes as plain text, cut to `maxChars`.
    static QString summarize(const QString &markdown, int maxChars = 220);

signals:
    void finished(UpdateChecker::Result result, const ReleaseInfo &release, const QString &error);

private:
    void handleApiReply();
    void checkReleasePage();
    void handlePageReply();
    void finishWith(const ReleaseInfo &release);

    QString m_currentVersion;
    QNetworkAccessManager *m_network;
    QPointer<QNetworkReply> m_reply;
    QString m_apiError;
};
