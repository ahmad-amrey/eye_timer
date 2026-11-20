#include "Platform.h"

#include <QWidget>

#import <AppKit/AppKit.h>

namespace platform {

void keepFloatingEverywhere(QWidget *widget)
{
    // Must be set before the native window exists: Qt reads it when it
    // creates the NSPanel for a Qt::Tool window.
    widget->setAttribute(Qt::WA_MacAlwaysShowToolWindow);

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
