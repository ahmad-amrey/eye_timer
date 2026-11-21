#include "Schedule.h"

#include <QtTest>

using schedule::Idle;

class ScheduleTest : public QObject
{
    Q_OBJECT

private:
    static AppConfig config()
    {
        AppConfig config;
        config.restIntervalSeconds = 20 * 60;
        config.prepareNoticeSeconds = 20;
        config.idlePauseAfterSeconds = 60;
        config.idleResetAfterSeconds = 10 * 60;
        return config;
    }

private slots:
    void noticeWindow()
    {
        const AppConfig c = config();
        QVERIFY(!schedule::inNotice(0, c));
        QVERIFY(!schedule::inNotice(c.restIntervalSeconds - 21, c));
        QVERIFY(schedule::inNotice(c.restIntervalSeconds - 20, c));
        QVERIFY(schedule::inNotice(c.restIntervalSeconds - 1, c));

        AppConfig off = c;
        off.prepareNoticeSeconds = 0;
        QVERIFY(!schedule::inNotice(off.restIntervalSeconds - 1, off));
    }

    void idleBeforeNotice_data()
    {
        QTest::addColumn<double>("idle");
        QTest::addColumn<Idle>("effect");
        QTest::newRow("active") << 5.0 << Idle::Counting;
        QTest::newRow("away a minute") << 60.0 << Idle::Paused;
        QTest::newRow("away five minutes") << 300.0 << Idle::Paused;
        QTest::newRow("away ten minutes") << 600.0 << Idle::Reset;
    }

    void idleBeforeNotice()
    {
        QFETCH(double, idle);
        QFETCH(Idle, effect);
        const AppConfig c = config();
        QCOMPARE(schedule::idleEffect(c.restIntervalSeconds - 300, idle, c), effect);
    }

    void noticeRunsToTheBreakWhileIdle_data()
    {
        QTest::addColumn<double>("idle");
        QTest::newRow("active") << 0.0;
        QTest::newRow("away five minutes") << 300.0;
        QTest::newRow("away an hour") << 3600.0;
    }

    // The notice used to close and reopen on every tick when the user was
    // idle; now the countdown keeps going once it has started.
    void noticeRunsToTheBreakWhileIdle()
    {
        QFETCH(double, idle);
        const AppConfig c = config();
        double active = c.restIntervalSeconds - c.prepareNoticeSeconds;
        int ticks = 0;
        while (active < c.restIntervalSeconds) {
            QCOMPARE(schedule::idleEffect(active, idle, c), Idle::Counting);
            active += 1.0;
            ++ticks;
        }
        QCOMPARE(ticks, c.prepareNoticeSeconds);
    }
};

QTEST_GUILESS_MAIN(ScheduleTest)
#include "tst_schedule.moc"
