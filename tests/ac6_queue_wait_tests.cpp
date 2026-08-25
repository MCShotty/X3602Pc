#include "xeo3_bridge/ac6_queue_wait.h"

#include <cassert>

int main()
{
    using xeo3::QueueWaitAction;

    static_assert(!xeo3::IsQueueWaitActive(0U));
    static_assert(xeo3::IsQueueWaitActive(1U));
    static_assert(xeo3::IsQueueWaitActive(0x7FFFFFFFU));
    static_assert(!xeo3::IsQueueWaitActive(0x80000000U));
    static_assert(!xeo3::IsQueueWaitActive(0xFFFFFFFFU));

    assert(xeo3::SelectQueueWaitAction(0U, 1U) ==
           QueueWaitAction::reset);
    assert(xeo3::SelectQueueWaitAction(0xFFFFFFFFU, 1U) ==
           QueueWaitAction::reset);
    assert(xeo3::SelectQueueWaitAction(1U, 1U) ==
           QueueWaitAction::pause);
    assert(xeo3::SelectQueueWaitAction(
               1U,
               xeo3::kQueueWaitSwitchPeriod) ==
           QueueWaitAction::switchThread);
    assert(xeo3::SelectQueueWaitAction(
               1U,
               xeo3::kQueueWaitSleepPeriod) ==
           QueueWaitAction::sleepOneMillisecond);
    assert(xeo3::SelectQueueWaitAction(
               1U,
               xeo3::kQueueWaitSleepPeriod +
                   xeo3::kQueueWaitSwitchPeriod) ==
           QueueWaitAction::switchThread);

    return 0;
}
