#include "IdleDetector.h"

#include <QtGlobal>

#if defined(Q_OS_MACOS)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#elif defined(Q_OS_WIN)
#include <windows.h>
#else
#include <QProcess>
#include <QStandardPaths>
#include <dlfcn.h>
#ifdef EYETIMER_HAS_DBUS
#include <QDBusConnection>
#include <QDBusMessage>
#endif
#endif

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
namespace {

// Mirrors XScreenSaverInfo from <X11/extensions/scrnsaver.h>; libX11 and
// libXss are loaded at runtime so they are not build dependencies.
struct XssInfo {
    unsigned long window;
    int state;
    int kind;
    unsigned long til_or_since;
    unsigned long idle;
    unsigned long eventMask;
};

using XOpenDisplayFn = void *(*)(const char *);
using XCloseDisplayFn = int (*)(void *);
using XDefaultRootWindowFn = unsigned long (*)(void *);
using XssQueryInfoFn = int (*)(void *, unsigned long, XssInfo *);

} // namespace
#endif

struct IdleDetector::Backend {
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    enum class Kind { None, DBusMutter, DBusScreenSaver, XPrintIdle, XScreenSaver };
    Kind kind = Kind::None;

    void *x11 = nullptr;
    void *xss = nullptr;
    void *display = nullptr;
    unsigned long root = 0;
    XCloseDisplayFn closeDisplay = nullptr;
    XssQueryInfoFn queryInfo = nullptr;

    ~Backend()
    {
        if (display && closeDisplay)
            closeDisplay(display);
        if (xss)
            dlclose(xss);
        if (x11)
            dlclose(x11);
    }

#ifdef EYETIMER_HAS_DBUS
    static bool dbusIdle(Kind which, double *seconds)
    {
        QDBusMessage call;
        if (which == Kind::DBusMutter) {
            call = QDBusMessage::createMethodCall(QStringLiteral("org.gnome.Mutter.IdleMonitor"),
                                                  QStringLiteral("/org/gnome/Mutter/IdleMonitor/Core"),
                                                  QStringLiteral("org.gnome.Mutter.IdleMonitor"),
                                                  QStringLiteral("GetIdletime"));
        } else {
            call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.ScreenSaver"),
                                                  QStringLiteral("/org/freedesktop/ScreenSaver"),
                                                  QStringLiteral("org.freedesktop.ScreenSaver"),
                                                  QStringLiteral("GetSessionIdleTime"));
        }
        const QDBusMessage reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 1000);
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
            return false;
        bool ok = false;
        const qulonglong value = reply.arguments().constFirst().toULongLong(&ok);
        if (!ok)
            return false;
        // Mutter reports milliseconds, the freedesktop interface seconds.
        *seconds = which == Kind::DBusMutter ? value / 1000.0 : static_cast<double>(value);
        return true;
    }
#endif

    static bool xprintidle(double *seconds)
    {
        QProcess process;
        process.start(QStringLiteral("xprintidle"), {});
        if (!process.waitForFinished(1000) || process.exitCode() != 0)
            return false;
        bool ok = false;
        const qulonglong ms = QString::fromUtf8(process.readAllStandardOutput()).trimmed().toULongLong(&ok);
        if (!ok)
            return false;
        *seconds = ms / 1000.0;
        return true;
    }

    bool initXScreenSaver()
    {
        x11 = dlopen("libX11.so.6", RTLD_LAZY | RTLD_LOCAL);
        xss = dlopen("libXss.so.1", RTLD_LAZY | RTLD_LOCAL);
        if (!x11 || !xss)
            return false;
        auto openDisplay = reinterpret_cast<XOpenDisplayFn>(dlsym(x11, "XOpenDisplay"));
        auto defaultRoot = reinterpret_cast<XDefaultRootWindowFn>(dlsym(x11, "XDefaultRootWindow"));
        closeDisplay = reinterpret_cast<XCloseDisplayFn>(dlsym(x11, "XCloseDisplay"));
        queryInfo = reinterpret_cast<XssQueryInfoFn>(dlsym(xss, "XScreenSaverQueryInfo"));
        if (!openDisplay || !defaultRoot || !closeDisplay || !queryInfo)
            return false;
        display = openDisplay(nullptr);
        if (!display)
            return false;
        root = defaultRoot(display);
        XssInfo info{};
        return queryInfo(display, root, &info) != 0;
    }

    void select()
    {
#ifdef EYETIMER_HAS_DBUS
        double probe = 0;
        if (dbusIdle(Kind::DBusMutter, &probe)) {
            kind = Kind::DBusMutter;
            return;
        }
        if (dbusIdle(Kind::DBusScreenSaver, &probe)) {
            kind = Kind::DBusScreenSaver;
            return;
        }
#endif
        double probeSeconds = 0;
        if (!QStandardPaths::findExecutable(QStringLiteral("xprintidle")).isEmpty() && xprintidle(&probeSeconds)) {
            kind = Kind::XPrintIdle;
            return;
        }
        if (initXScreenSaver()) {
            kind = Kind::XScreenSaver;
            return;
        }
        kind = Kind::None;
    }

    double read()
    {
        double seconds = 0;
        switch (kind) {
#ifdef EYETIMER_HAS_DBUS
        case Kind::DBusMutter:
        case Kind::DBusScreenSaver:
            return dbusIdle(kind, &seconds) ? seconds : 0.0;
#else
        case Kind::DBusMutter:
        case Kind::DBusScreenSaver:
            return 0.0;
#endif
        case Kind::XPrintIdle:
            return xprintidle(&seconds) ? seconds : 0.0;
        case Kind::XScreenSaver: {
            XssInfo info{};
            if (!queryInfo(display, root, &info))
                return 0.0;
            return info.idle / 1000.0;
        }
        case Kind::None:
            break;
        }
        return 0.0;
    }
#elif defined(Q_OS_WIN)
    void select() {}

    double read()
    {
        LASTINPUTINFO info{};
        info.cbSize = sizeof(info);
        if (!GetLastInputInfo(&info))
            return 0.0;
        const DWORD elapsedMs = GetTickCount() - info.dwTime;
        return elapsedMs / 1000.0;
    }
#elif defined(Q_OS_MACOS)
    void select() {}

    double read()
    {
        io_service_t service = IOServiceGetMatchingService(MACH_PORT_NULL, IOServiceMatching("IOHIDSystem"));
        if (!service)
            return 0.0;
        CFTypeRef property = IORegistryEntryCreateCFProperty(service, CFSTR("HIDIdleTime"), kCFAllocatorDefault, 0);
        IOObjectRelease(service);
        if (!property)
            return 0.0;

        int64_t nanoseconds = 0;
        if (CFGetTypeID(property) == CFNumberGetTypeID()) {
            CFNumberGetValue(static_cast<CFNumberRef>(property), kCFNumberSInt64Type, &nanoseconds);
        } else if (CFGetTypeID(property) == CFDataGetTypeID()
                   && CFDataGetLength(static_cast<CFDataRef>(property)) >= static_cast<CFIndex>(sizeof(nanoseconds))) {
            CFDataGetBytes(static_cast<CFDataRef>(property), CFRangeMake(0, sizeof(nanoseconds)),
                           reinterpret_cast<UInt8 *>(&nanoseconds));
        }
        CFRelease(property);
        return nanoseconds / 1'000'000'000.0;
    }
#else
    void select() {}
    double read() { return 0.0; }
#endif
};

IdleDetector::IdleDetector()
    : m_backend(std::make_unique<Backend>())
{
    m_backend->select();
}

IdleDetector::~IdleDetector() = default;

double IdleDetector::seconds()
{
    const double value = m_backend->read();
    return value > 0.0 ? value : 0.0;
}
