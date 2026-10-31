#pragma once

#include <memory>

// Reports how long the user has not touched the keyboard or mouse.
//   Windows: GetLastInputInfo
//   macOS:   IOKit HIDIdleTime
//   Linux:   GNOME Mutter or freedesktop ScreenSaver over D-Bus, then
//            xprintidle, then the XScreenSaver extension. Without any of
//            them the user always counts as active.
class IdleDetector
{
public:
    IdleDetector();
    ~IdleDetector();
    IdleDetector(const IdleDetector &) = delete;
    IdleDetector &operator=(const IdleDetector &) = delete;

    double seconds();

private:
    struct Backend;
    std::unique_ptr<Backend> m_backend;
};
