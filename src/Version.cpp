#include "Version.h"

#include <QStringList>

QString versionFromTag(const QString &tag)
{
    const QString trimmed = tag.trimmed();
    if (trimmed.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        return trimmed.mid(1);
    return trimmed;
}

int compareVersions(const QString &left, const QString &right)
{
    auto split = [](const QString &version, QString *preRelease) {
        const QString clean = versionFromTag(version);
        const qsizetype dash = clean.indexOf(QLatin1Char('-'));
        *preRelease = dash >= 0 ? clean.mid(dash + 1) : QString();
        return (dash >= 0 ? clean.left(dash) : clean).split(QLatin1Char('.'));
    };

    QString leftPre;
    QString rightPre;
    const QStringList leftParts = split(left, &leftPre);
    const QStringList rightParts = split(right, &rightPre);
    const qsizetype count = qMax(leftParts.size(), rightParts.size());
    for (qsizetype i = 0; i < count; ++i) {
        const qulonglong a = i < leftParts.size() ? leftParts.at(i).toULongLong() : 0;
        const qulonglong b = i < rightParts.size() ? rightParts.at(i).toULongLong() : 0;
        if (a != b)
            return a < b ? -1 : 1;
    }

    if (leftPre.isEmpty() != rightPre.isEmpty())
        return leftPre.isEmpty() ? 1 : -1;
    return QString::compare(leftPre, rightPre, Qt::CaseInsensitive);
}
