#pragma once

#include "Config.h"

// The rules for counting active time toward the next break, kept free of
// windows and timers so they can be tested.
namespace schedule {

enum class Idle {
    Counting, // count this tick toward the break
    Paused,   // away for a while: stop counting
    Reset,    // away long enough to count as rest: start over
};

// True once the pre-break notice has started (the last
// `prepareNoticeSeconds` before the break).
bool inNotice(double activeSeconds, const AppConfig &config);

// What idle time does to the counter. Once the notice has started the
// countdown always runs to the break, whether or not anyone is at the computer.
Idle idleEffect(double activeSeconds, double idleSeconds, const AppConfig &config);

// Active seconds after "Snooze": the break moves to `snoozeMinutes` from now.
double snoozedActiveSeconds(const AppConfig &config);

} // namespace schedule
