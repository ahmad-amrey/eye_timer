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

} // namespace platform
