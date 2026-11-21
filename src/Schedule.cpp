#include "Schedule.h"

namespace schedule {

bool inNotice(double activeSeconds, const AppConfig &config)
{
    return config.prepareNoticeSeconds > 0 && config.restIntervalSeconds - activeSeconds <= config.prepareNoticeSeconds;
}

Idle idleEffect(double activeSeconds, double idleSeconds, const AppConfig &config)
{
    if (inNotice(activeSeconds, config))
        return Idle::Counting;
    if (idleSeconds >= config.idleResetAfterSeconds)
        return Idle::Reset;
    if (idleSeconds >= config.idlePauseAfterSeconds)
        return Idle::Paused;
    return Idle::Counting;
}

} // namespace schedule
