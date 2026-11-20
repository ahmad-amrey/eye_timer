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
    QString notes;   // Markdown body
};

// Asks GitHub for the latest published release and compares it with the
// running version. Notify-only: nothing is downloaded or installed.
//
// EYETIMER_UPDATE_URL overrides the endpoint (file:// URLs work, for tests).
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    enum class Result { UpdateAvailable, UpToDate, Failed };
    Q_ENUM(Result)

    static constexpr auto kReleasesUrl = "https://api.github.com/repos/ahmad-amrey/eye_timer/releases/latest";

    explicit UpdateChecker(const QString &currentVersion, QObject *parent = nullptr);

    void check();
    bool isChecking() const { return !m_reply.isNull(); }
    QString currentVersion() const { return m_currentVersion; }

    static QUrl endpoint();
    static std::optional<ReleaseInfo> parseRelease(const QByteArray &json, QString *error);
    // First paragraph of release notes as plain text, cut to `maxChars`.
    static QString summarize(const QString &markdown, int maxChars = 220);

signals:
    void finished(UpdateChecker::Result result, const ReleaseInfo &release, const QString &error);

private:
    void handleReply();

    QString m_currentVersion;
    QNetworkAccessManager *m_network;
    QPointer<QNetworkReply> m_reply;
};
