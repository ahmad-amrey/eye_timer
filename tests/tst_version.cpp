#include "Version.h"

#include <QTest>

class TestVersion : public QObject
{
    Q_OBJECT

private slots:
    void compare_data()
    {
        QTest::addColumn<QString>("left");
        QTest::addColumn<QString>("right");
        QTest::addColumn<int>("expected");

        QTest::newRow("equal") << "0.10.0" << "0.10.0" << 0;
        QTest::newRow("numeric not lexical") << "0.10.0" << "0.9.0" << 1;
        QTest::newRow("older") << "0.9.0" << "0.10.0" << -1;
        QTest::newRow("leading v") << "v1.2.3" << "1.2.3" << 0;
        QTest::newRow("missing parts are zero") << "1.2" << "1.2.0" << 0;
        QTest::newRow("longer is newer") << "1.2.0.1" << "1.2" << 1;
        QTest::newRow("pre-release before release") << "1.0.0-rc1" << "1.0.0" << -1;
        QTest::newRow("release after pre-release") << "1.0.0" << "1.0.0-rc1" << 1;
        QTest::newRow("pre-releases ordered") << "1.0.0-rc2" << "1.0.0-rc1" << 1;
        QTest::newRow("major wins") << "2.0.0" << "1.99.99" << 1;
    }

    void compare()
    {
        QFETCH(QString, left);
        QFETCH(QString, right);
        QFETCH(int, expected);
        const int result = compareVersions(left, right);
        QCOMPARE((result > 0) - (result < 0), expected);
    }

    void versionFromTag()
    {
        QCOMPARE(::versionFromTag(QStringLiteral("v0.10.0")), QStringLiteral("0.10.0"));
        QCOMPARE(::versionFromTag(QStringLiteral("V2.0")), QStringLiteral("2.0"));
        QCOMPARE(::versionFromTag(QStringLiteral(" 1.0.0 ")), QStringLiteral("1.0.0"));
    }
};

QTEST_GUILESS_MAIN(TestVersion)
#include "tst_version.moc"
