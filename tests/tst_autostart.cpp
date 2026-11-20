#include "Autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// Points the login-item location at a temporary folder. On Windows the
// Startup shortcut follows APPDATA, but Task Manager's on/off switch lives in
// the real registry, so those tests run on CI machines only.
class TestAutostart : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
#ifdef Q_OS_WIN
        if (!qEnvironmentVariableIsSet("CI"))
            QSKIP("Changes Task Manager's startup switch for \"Eye Timer.lnk\" in the real registry; runs on CI only");
#endif
        m_home = std::make_unique<QTemporaryDir>();
        QVERIFY(m_home->isValid());
#if defined(Q_OS_WIN)
        qputenv("APPDATA", QDir::toNativeSeparators(m_home->path()).toLocal8Bit());
#elif defined(Q_OS_MACOS)
        qputenv("HOME", m_home->path().toLocal8Bit());
#else
        qputenv("XDG_CONFIG_HOME", m_home->path().toLocal8Bit());
#endif
    }

    void syncRegistersWhenMissing()
    {
        QVERIFY(!autostart::isRegistered());
        autostart::sync(false);
        QVERIFY(!autostart::isRegistered());
        autostart::sync(true);
        QVERIFY(autostart::isRegistered());
    }

    void syncRepointsAnEntryForAnotherCopy()
    {
        // The reported bug: the login item still launched an older copy of the
        // app from another folder, and was switched off in Task Manager.
        const QString oldCopy = QDir(m_home->path()).filePath(QStringLiteral("old/EyeTimer.exe"));
        QString error;
        QVERIFY2(autostart::setEnabled(true, &error, oldCopy), qPrintable(error));
        QVERIFY(!autostart::isRegistered());
#ifdef Q_OS_WIN
        setTaskManagerDisabled(true);
#endif
        autostart::sync(true);
        QVERIFY(autostart::isRegistered());
#ifdef Q_OS_WIN
        QVERIFY2(!taskManagerDisabled(), "the stale entry's Task Manager switch should be cleared");
#endif
    }

    void syncLeavesACurrentEntryAlone()
    {
        QVERIFY(autostart::setEnabled(true, nullptr));
#ifdef Q_OS_WIN
        // Switched off in Task Manager for this very copy: that choice stays.
        setTaskManagerDisabled(true);
        autostart::sync(true);
        QVERIFY(taskManagerDisabled());
        setTaskManagerDisabled(false);
#else
        autostart::sync(true);
#endif
        QVERIFY(autostart::isRegistered());
    }

    void disablingRemovesTheEntry()
    {
        QVERIFY(autostart::setEnabled(true, nullptr));
        QVERIFY(autostart::isRegistered());
        QVERIFY(autostart::setEnabled(false, nullptr));
        QVERIFY(!autostart::isRegistered());
        // Removing an entry that is already gone is fine.
        QVERIFY(autostart::setEnabled(false, nullptr));
    }

    void cleanup()
    {
        if (m_home)
            autostart::setEnabled(false, nullptr);
        m_home.reset();
    }

private:
#ifdef Q_OS_WIN
    static constexpr const wchar_t *kApprovedKey =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\StartupFolder";

    static void setTaskManagerDisabled(bool disabled)
    {
        if (!disabled) {
            RegDeleteKeyValueW(HKEY_CURRENT_USER, kApprovedKey, L"Eye Timer.lnk");
            return;
        }
        // 0x03 in the first byte is what Task Manager writes for "Disabled".
        const BYTE value[12] = {0x03};
        RegSetKeyValueW(HKEY_CURRENT_USER, kApprovedKey, L"Eye Timer.lnk", REG_BINARY, value, sizeof(value));
    }

    static bool taskManagerDisabled()
    {
        BYTE value[12] = {};
        DWORD size = sizeof(value);
        if (RegGetValueW(HKEY_CURRENT_USER, kApprovedKey, L"Eye Timer.lnk", RRF_RT_REG_BINARY, nullptr, value, &size)
            != ERROR_SUCCESS)
            return false;
        return size > 0 && (value[0] & 0x01);
    }
#endif

    std::unique_ptr<QTemporaryDir> m_home;
};

QTEST_GUILESS_MAIN(TestAutostart)
#include "tst_autostart.moc"
