#pragma once

#include <QString>

// Compares dotted versions numerically, ignoring a leading "v":
// "0.10.0" > "0.9.0", "1.2" == "1.2.0", and a pre-release sorts before its
// release ("1.0.0-rc1" < "1.0.0"). Returns <0, 0 or >0.
int compareVersions(const QString &left, const QString &right);

// Strips a leading "v" or "V" from a tag such as "v0.10.0".
QString versionFromTag(const QString &tag);
