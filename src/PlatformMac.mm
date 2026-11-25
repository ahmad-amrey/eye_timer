#include "Platform.h"

#include <QGuiApplication>
#include <QWidget>

#import <AppKit/AppKit.h>

namespace platform {

void keepFloatingEverywhere(QWidget *widget)
{
    // Must be set before the native window exists: Qt reads it when it
    // creates the NSPanel for a Qt::Tool window.
    widget->setAttribute(Qt::WA_MacAlwaysShowToolWindow);

    // winId() is an NSView only on the Cocoa platform; under offscreen (CI
    // smoke tests, preview rendering) it is a plain number.
    if (QGuiApplication::platformName() != QLatin1String("cocoa"))
        return;

    auto *view = reinterpret_cast<NSView *>(widget->winId());
    NSWindow *window = view.window;
    if (!window)
        return;
    NSWindowCollectionBehavior behavior = window.collectionBehavior;
    behavior &= ~NSWindowCollectionBehaviorMoveToActiveSpace;
    behavior |= NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorFullScreenAuxiliary;
    window.collectionBehavior = behavior;
}

void takeKeyboardFocus(QWidget *widget)
{
    if (!widget || !widget->isVisible())
        return;
    if (QGuiApplication::platformName() == QLatin1String("cocoa") && !widget->isActiveWindow()) {
        // Eye Timer is a menu bar (agent) app that is never active on its
        // own, and key events only reach the key window of the active app.
        // The newer -activate is cooperative and does nothing while another
        // app is in use, which is exactly when a break starts.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        [NSApp activateIgnoringOtherApps:YES];
#pragma clang diagnostic pop
        auto *view = reinterpret_cast<NSView *>(widget->winId());
        [view.window makeKeyAndOrderFront:nil];
    }
    if (!widget->isActiveWindow())
        widget->activateWindow();
    widget->setFocus(Qt::ActiveWindowFocusReason);
}

void releaseKeyboardFocus(QWidget *widget)
{
    Q_UNUSED(widget);
}

} // namespace platform
