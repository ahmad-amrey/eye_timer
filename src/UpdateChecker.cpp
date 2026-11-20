#include "UpdateChecker.h"

#include "Version.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>

namespace {
constexpr int kTimeoutMs = 15000;

QUrl urlFromEnvironment(const char *name, const char *fallback)
{
    const QString overridden = qEnvironmentVariable(name);
    return QUrl(overridden.isEmpty() ? QString::fromLatin1(fallback) : overridden);
}

// A readable message when GitHub refused because of its anonymous rate limit.
QString rateLimitMessage(const QNetworkReply *reply)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool limited = status == 429 || (status == 403 && reply->rawHeader("X-RateLimit-Remaining") == "0");
    if (!limited)
        return {};
    QString message = QStringLiteral("GitHub is limiting update checks from your network right now.");
    bool ok = false;
    const qint64 reset = reply->rawHeader("X-RateLimit-Reset").toLongLong(&ok);
    if (ok && reset > 0) {
        message += QStringLiteral(" Try again after %1.")
                       .arg(QDateTime::fromSecsSinceEpoch(reset).toLocalTime().toString(QStringLiteral("HH:mm")));
    }
    return message;
}

} // namespace

UpdateChecker::UpdateChecker(const QString &currentVersion, QObject *parent)
    : QObject(parent)
    , m_currentVersion(currentVersion)
    , m_network(new QNetworkAccessManager(this))
{
}

QUrl UpdateChecker::endpoint()
{
    return urlFromEnvironment("EYETIMER_UPDATE_URL", kReleasesUrl);
}

QUrl UpdateChecker::releasesPageEndpoint()
{
    return urlFromEnvironment("EYETIMER_RELEASES_PAGE_URL", kReleasesPageUrl);
}

void UpdateChecker::check()
{
    if (m_reply)
        return;
    m_apiError.clear();
    QNetworkRequest request(endpoint());
    // GitHub rejects API requests without a User-Agent.
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("EyeTimer/%1").arg(m_currentVersion));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setTransferTimeout(kTimeoutMs);
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, &UpdateChecker::handleApiReply);
}

void UpdateChecker::handleApiReply()
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
        if (status < 400) {
            emit finished(Result::Failed, {}, reply->errorString());
            return;
        }
        // GitHub refused, usually because the anonymous rate limit is used
        // up; the release page still says which version is the latest.
        m_apiError = rateLimitMessage(reply);
        if (m_apiError.isEmpty())
            m_apiError = reply->errorString();
        checkReleasePage();
        return;
    }

    QString error;
    const std::optional<ReleaseInfo> release = parseRelease(reply->readAll(), &error);
    if (!release) {
        emit finished(Result::Failed, {}, error);
        return;
    }
    finishWith(*release);
}

void UpdateChecker::checkReleasePage()
{
    QNetworkRequest request(releasesPageEndpoint());
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("EyeTimer/%1").arg(m_currentVersion));
    // Only the redirect matters: /releases/latest -> /releases/tag/vX.Y.Z.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(kTimeoutMs);
    m_reply = m_network->head(request);
    connect(m_reply, &QNetworkReply::finished, this, &UpdateChecker::handlePageReply);
}

void UpdateChecker::handlePageReply()
{
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    if (!reply)
        return;
    reply->deleteLater();

    const QUrl redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    const QUrl target = redirect.isEmpty() ? QUrl() : reply->url().resolved(redirect);
    const QString tag = tagFromReleasePageUrl(target);
    if (tag.isEmpty()) {
        emit finished(Result::Failed, {}, m_apiError.isEmpty() ? reply->errorString() : m_apiError);
        return;
    }

    ReleaseInfo release;
    release.tag = tag;
    release.version = versionFromTag(tag);
    release.pageUrl = target.toString();
    finishWith(release);
}

void UpdateChecker::finishWith(const ReleaseInfo &release)
{
    if (compareVersions(release.version, m_currentVersion) > 0)
        emit finished(Result::UpdateAvailable, release, {});
    else
        emit finished(Result::UpToDate, release, {});
}

QString UpdateChecker::tagFromReleasePageUrl(const QUrl &url)
{
    static const QRegularExpression pattern(QStringLiteral("/releases/tag/([^/]+)/?$"));
    const QRegularExpressionMatch match = pattern.match(url.path());
    return match.hasMatch() ? match.captured(1) : QString();
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
