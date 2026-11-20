#include "UpdateChecker.h"

#include "Version.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>

namespace {
constexpr int kTimeoutMs = 15000;
} // namespace

UpdateChecker::UpdateChecker(const QString &currentVersion, QObject *parent)
    : QObject(parent)
    , m_currentVersion(currentVersion)
    , m_network(new QNetworkAccessManager(this))
{
}

QUrl UpdateChecker::endpoint()
{
    const QString overridden = qEnvironmentVariable("EYETIMER_UPDATE_URL");
    return QUrl(overridden.isEmpty() ? QString::fromLatin1(kReleasesUrl) : overridden);
}

void UpdateChecker::check()
{
    if (m_reply)
        return;
    QNetworkRequest request(endpoint());
    // GitHub rejects API requests without a User-Agent.
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("EyeTimer/%1").arg(m_currentVersion));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setTransferTimeout(kTimeoutMs);
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, &UpdateChecker::handleReply);
}

void UpdateChecker::handleReply()
{
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    if (!reply)
        return;
    reply->deleteLater();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 404) {
        // No published release yet.
        emit finished(Result::UpToDate, {}, {});
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        emit finished(Result::Failed, {}, reply->errorString());
        return;
    }

    QString error;
    const std::optional<ReleaseInfo> release = parseRelease(reply->readAll(), &error);
    if (!release) {
        emit finished(Result::Failed, {}, error);
        return;
    }
    if (compareVersions(release->version, m_currentVersion) > 0)
        emit finished(Result::UpdateAvailable, *release, {});
    else
        emit finished(Result::UpToDate, *release, {});
}

std::optional<ReleaseInfo> UpdateChecker::parseRelease(const QByteArray &json, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = QStringLiteral("Unexpected response from GitHub");
        return std::nullopt;
    }
    const QJsonObject object = document.object();
    if (object.value(QLatin1String("draft")).toBool() || object.value(QLatin1String("prerelease")).toBool()) {
        if (error)
            *error = QStringLiteral("The latest release is not published yet");
        return std::nullopt;
    }

    ReleaseInfo release;
    release.tag = object.value(QLatin1String("tag_name")).toString();
    release.version = versionFromTag(release.tag);
    release.name = object.value(QLatin1String("name")).toString();
    release.pageUrl = object.value(QLatin1String("html_url")).toString();
    release.notes = object.value(QLatin1String("body")).toString();
    if (release.version.isEmpty()) {
        if (error)
            *error = QStringLiteral("The release has no version tag");
        return std::nullopt;
    }
    return release;
}

QString UpdateChecker::summarize(const QString &markdown, int maxChars)
{
    // Skip headings and blank lines, then take the first paragraph or, for a
    // list, its first few items.
    QStringList pieces;
    bool inList = false;
    const QStringList lines = markdown.split(QLatin1Char('\n'));
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty()) {
            if (!pieces.isEmpty())
                break;
            continue;
        }
        if (line.startsWith(QLatin1Char('#')))
            continue;
        static const QRegularExpression bullet(QStringLiteral("^([-*+]|\\d+\\.)\\s+"));
        const bool isBullet = bullet.match(line).hasMatch();
        if (!pieces.isEmpty() && isBullet != inList)
            break;
        inList = isBullet;
        line.remove(bullet);
        pieces << line;
        if (inList && pieces.size() == 3)
            break;
    }

    QString text = pieces.join(inList ? QStringLiteral(" · ") : QStringLiteral(" "));
    static const QRegularExpression link(QStringLiteral("\\[([^\\]]*)\\]\\([^)]*\\)"));
    text.replace(link, QStringLiteral("\\1"));
    static const QRegularExpression emphasis(QStringLiteral("[*_`]+"));
    text.remove(emphasis);
    text = text.simplified();
    if (text.size() > maxChars) {
        text.truncate(maxChars);
        const qsizetype space = text.lastIndexOf(QLatin1Char(' '));
        if (space > maxChars / 2)
            text.truncate(space);
        text += QStringLiteral("…");
    }
    return text;
}
