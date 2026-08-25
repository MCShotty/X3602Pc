#pragma once

#include <cstdint>

namespace xeo3
{
enum class QueueWaitAction : std::uint8_t
{
    reset,
    pause,
    switchThread,
    sleepOneMillisecond,
};

constexpr std::uint32_t kQueueWaitSwitchPeriod = 256U;
constexpr std::uint32_t kQueueWaitSleepPeriod = 16384U;

constexpr bool IsQueueWaitActive(const std::uint32_t count) noexcept
{
    return static_cast<std::int32_t>(count) > 0;
}

constexpr QueueWaitAction SelectQueueWaitAction(
    const std::uint32_t count,
    const std::uint32_t iteration) noexcept
{
    if (!IsQueueWaitActive(count))
    {
        return QueueWaitAction::reset;
    }

    if (iteration != 0U && iteration % kQueueWaitSleepPeriod == 0U)
    {
        return QueueWaitAction::sleepOneMillisecond;
    }

    if (iteration != 0U && iteration % kQueueWaitSwitchPeriod == 0U)
    {
        return QueueWaitAction::switchThread;
    }

    return QueueWaitAction::pause;
}
}
