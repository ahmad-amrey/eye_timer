#include "BreakOverlay.h"
#include "Config.h"
#include "EyeTimerApp.h"
#include "Widgets.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QPointer>
#include <QTest>
#include <QtTest/qtestspontaneevent.h>

#include <memory>

// During a break the overlay holds the keyboard: Esc skips, and every other
// key is swallowed (so it never reaches the app underneath) and shakes the
// skip button instead.
class TestBreakOverlay : public QObject
{
    Q_OBJECT

public:
    static void initMain()
    {
        // Never take the developer's real keyboard while the tests run.
        if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
            qputenv("QT_QPA_PLATFORM", "offscreen");
    }

private slots:
    void init()
    {
        AppConfig config;
        config.soundEnabled = false;
        config.restDurationSeconds = 600;
        m_app = std::make_unique<EyeTimerApp>(
            qApp, config, EyeTimerApp::Options{.createTrayIcon = false, .startTimer = false, .checkForUpdates = false});
        m_app->startBreak();
        QVERIFY(m_app->isOnBreak());

        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (auto *overlay = qobject_cast<BreakOverlay *>(widget); overlay && overlay->isVisible()) {
                m_overlay = overlay;
                break;
            }
        }
        QVERIFY(m_overlay);
        QVERIFY(QTest::qWaitForWindowExposed(m_overlay));
        m_skip = m_overlay->findChild<KeyHintButton *>();
        QVERIFY(m_skip);
    }

    void cleanup()
    {
        m_app.reset();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void overlayTakesTheKeyboard()
    {
        QTRY_VERIFY(m_overlay->isActiveWindow());
        QVERIFY(m_overlay->hasFocus());
        QCOMPARE(m_skip->focusPolicy(), Qt::NoFocus);
    }

    void typingIsHeldBackAndShakesTheSkipButton()
    {
        const int restingX = centeredSkipX();
        QCOMPARE(m_skip->x(), restingX);
        QVERIFY(!m_overlay->isShakingSkipButton());

        QTest::keyClicks(m_overlay, QStringLiteral("hello"));
        QVERIFY(m_app->isOnBreak());
        QVERIFY(m_overlay->isShakingSkipButton());
        QTRY_VERIFY_WITH_TIMEOUT(m_skip->x() != restingX, 400);

        // It settles back in the middle.
        QTRY_VERIFY_WITH_TIMEOUT(!m_overlay->isShakingSkipButton(), 2000);
        QCOMPARE(m_skip->x(), restingX);
    }

    void onlyEscapeSkips_data()
    {
        QTest::addColumn<int>("key");
        QTest::addColumn<int>("modifiers");
        QTest::newRow("Space") << int(Qt::Key_Space) << int(Qt::NoModifier);
        QTest::newRow("Return") << int(Qt::Key_Return) << int(Qt::NoModifier);
        QTest::newRow("Enter") << int(Qt::Key_Enter) << int(Qt::KeypadModifier);
        QTest::newRow("Tab") << int(Qt::Key_Tab) << int(Qt::NoModifier);
        QTest::newRow("Shift+Tab") << int(Qt::Key_Backtab) << int(Qt::ShiftModifier);
        QTest::newRow("Shift") << int(Qt::Key_Shift) << int(Qt::NoModifier);
        QTest::newRow("Alt+F4") << int(Qt::Key_F4) << int(Qt::AltModifier);
    }

    void onlyEscapeSkips()
    {
        QFETCH(int, key);
        QFETCH(int, modifiers);
        QTest::keyClick(m_overlay, Qt::Key(key), Qt::KeyboardModifiers(modifiers));
        QVERIFY(m_app->isOnBreak());
        QVERIFY(m_overlay->isVisible());
        QVERIFY(m_overlay->isShakingSkipButton());
        QVERIFY(!m_skip->hasFocus());
    }

    void escapeSkips()
    {
        QPointer<BreakOverlay> overlay = m_overlay;
        QTest::keyClick(m_overlay, Qt::Key_Escape);
        QVERIFY(!m_app->isOnBreak());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!overlay);
    }

    void skipButtonSkips()
    {
        QTest::mouseClick(m_skip, Qt::LeftButton);
        QVERIFY(!m_app->isOnBreak());
    }

    void applicationShortcutsDoNotFire()
    {
        QWidget host;
        auto *save = new QAction(&host);
        save->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
        save->setShortcutContext(Qt::ApplicationShortcut);
        host.addAction(save);
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        int fired = 0;
        connect(save, &QAction::triggered, this, [&fired] { ++fired; });

        QTest::keyClick(&host, Qt::Key_S, Qt::ControlModifier);
        QCOMPARE(fired, 1); // the shortcut works outside the overlay

        QTest::keyClick(m_overlay, Qt::Key_S, Qt::ControlModifier);
        QCOMPARE(fired, 1);
        QVERIFY(m_overlay->isShakingSkipButton());
    }

    void keyboardIsTakenBackFromOtherWindows()
    {
        QWidget other;
        other.show();
        QVERIFY(QTest::qWaitForWindowExposed(&other));
        other.activateWindow();
        QTRY_VERIFY(other.isActiveWindow());
        QTRY_VERIFY_WITH_TIMEOUT(m_overlay->isActiveWindow(), 2000);
    }

    void windowManagerCloseIsRefused()
    {
        QCloseEvent close;
        QSpontaneKeyEvent::setSpontaneous(&close);
        QApplication::instance()->notify(m_overlay, &close);
        QVERIFY(!close.isAccepted());
        QVERIFY(m_overlay->isVisible());
        QVERIFY(m_app->isOnBreak());
        QVERIFY(m_overlay->isShakingSkipButton());
    }

private:
    int centeredSkipX() const { return (m_overlay->width() - m_skip->width()) / 2; }

    std::unique_ptr<EyeTimerApp> m_app;
    BreakOverlay *m_overlay = nullptr;
    KeyHintButton *m_skip = nullptr;
};

QTEST_MAIN(TestBreakOverlay)
#include "tst_breakoverlay.moc"
