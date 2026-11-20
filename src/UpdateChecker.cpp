#include "UpdateChecker.h"

#include "Version.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QSysInfo>
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

// Whether this Linux system installs .deb packages, from /etc/os-release.
bool isDebianFamily()
{
    QFile file(QStringLiteral("/etc/os-release"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QStringList families;
    const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
    for (QString line : lines) {
        line = line.trimmed();
        if (line.startsWith(QLatin1String("ID=")) || line.startsWith(QLatin1String("ID_LIKE="))) {
            QString value = line.section(QLatin1Char('='), 1);
            value.remove(QLatin1Char('"'));
            value.remove(QLatin1Char('\''));
            families << value.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        }
    }
    return families.contains(QLatin1String("debian")) || families.contains(QLatin1String("ubuntu"));
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
    // Release files keep the same names in every release.
    const QString asset = installerAssetName();
    if (!asset.isEmpty())
        release.downloadUrl = assetDownloadUrl(tag, asset);
    finishWith(release);
}

void UpdateChecker::finishWith(const ReleaseInfo &release)
{
    if (compareVersions(release.version, m_currentVersion) > 0)
        emit finished(Result::UpdateAvailable, release, {});
    else
        emit finished(Result::UpToDate, release, {});
}

QUrl UpdateChecker::safeReleasePageUrl(const QString &url)
{
    const QUrl candidate(url, QUrl::StrictMode);
    const bool trusted = candidate.isValid() && candidate.scheme() == QLatin1String("https")
                         && candidate.host() == QLatin1String("github.com") && candidate.port() == -1
                         && candidate.userInfo().isEmpty()
                         && candidate.path().startsWith(QLatin1String("/ahmad-amrey/eye_timer/releases/"));
    return trusted ? candidate : QUrl(QString::fromLatin1(kReleasesPageUrl));
}

QString UpdateChecker::installerAssetName()
{
#if defined(Q_OS_WIN)
    return installerAssetName(QStringLiteral("windows"), QSysInfo::currentCpuArchitecture(), false);
#elif defined(Q_OS_MACOS)
    return installerAssetName(QStringLiteral("macos"), QSysInfo::currentCpuArchitecture(), false);
#else
    return installerAssetName(QStringLiteral("linux"), QSysInfo::currentCpuArchitecture(), isDebianFamily());
#endif
}

QString UpdateChecker::installerAssetName(const QString &os, const QString &cpuArchitecture, bool debianFamily)
{
    if (os == QLatin1String("windows"))
        return QStringLiteral("EyeTimer-windows-x64-setup.exe"); // also runs on Arm through emulation
    if (os == QLatin1String("macos"))
        return QStringLiteral("EyeTimer-macos.dmg"); // universal: Apple silicon and Intel
    if (os == QLatin1String("linux") && debianFamily && cpuArchitecture == QLatin1String("x86_64"))
        return QStringLiteral("EyeTimer-linux-amd64.deb");
    return {};
}

QString UpdateChecker::assetDownloadUrl(const QString &tag, const QString &asset)
{
    return QStringLiteral("https://github.com/ahmad-amrey/eye_timer/releases/download/%1/%2")
        .arg(QString::fromLatin1(QUrl::toPercentEncoding(tag)), QString::fromLatin1(QUrl::toPercentEncoding(asset)));
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
    const QString installer = installerAssetName();
    const QJsonArray assets = object.value(QLatin1String("assets")).toArray();
    for (const QJsonValue &asset : assets) {
        const QJsonObject file = asset.toObject();
        if (!installer.isEmpty() && file.value(QLatin1String("name")).toString() == installer)
            release.downloadUrl = file.value(QLatin1String("browser_download_url")).toString();
    }
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
