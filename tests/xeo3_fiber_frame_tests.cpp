#include "xeo3_bridge/fiber_frame_registry.h"

#include <Windows.h>

#include <cstdint>

#define CHECK(expression)        \
    do                           \
    {                            \
        if (!(expression))       \
        {                        \
            return __LINE__;     \
        }                        \
    } while (false)

namespace
{
struct Frame
{
    std::uint32_t value;
    Frame* next;
};

struct FiberContext
{
    xeo3::FiberFrameRegistry<Frame>* registry;
    void* mainFiber;
    int failure;
    int stage;
};

void __stdcall TestFiber(void* parameter)
{
    auto& context = *static_cast<FiberContext*>(parameter);
    auto& registry = *context.registry;
    const auto anyFrame = [](const Frame&)
    {
        return true;
    };

    Frame first{1, nullptr};
    Frame second{2, nullptr};
    if (registry.find(anyFrame) != nullptr ||
        !registry.push(first) ||
        !registry.push(second) ||
        registry.find(anyFrame) != &second)
    {
        context.failure = __LINE__;
    }

    context.stage = 1;
    SwitchToFiber(context.mainFiber);

    if (registry.find(anyFrame) != &second ||
        !registry.remove(second) ||
        registry.find(anyFrame) != &first ||
        !registry.remove(first) ||
        registry.find(anyFrame) != nullptr)
    {
        context.failure = __LINE__;
    }

    context.stage = 2;
    SwitchToFiber(context.mainFiber);
    for (;;)
    {
        SwitchToFiber(context.mainFiber);
    }
}
}

int main()
{
    xeo3::FiberFrameRegistry<Frame> registry;
    CHECK(registry.initialize());

    void* const mainFiber = ConvertThreadToFiber(nullptr);
    CHECK(mainFiber != nullptr);

    Frame mainFrame{3, nullptr};
    CHECK(registry.push(mainFrame));

    FiberContext context{&registry, mainFiber, 0, 0};
    void* const testFiber = CreateFiber(0, &TestFiber, &context);
    CHECK(testFiber != nullptr);

    SwitchToFiber(testFiber);
    CHECK(context.failure == 0);
    CHECK(context.stage == 1);
    CHECK(registry.find([](const Frame&) { return true; }) == &mainFrame);

    SwitchToFiber(testFiber);
    CHECK(context.failure == 0);
    CHECK(context.stage == 2);
    CHECK(registry.find([](const Frame&) { return true; }) == &mainFrame);

    DeleteFiber(testFiber);
    CHECK(registry.remove(mainFrame));
    CHECK(registry.find([](const Frame&) { return true; }) == nullptr);
    CHECK(ConvertFiberToThread() != FALSE);
    registry.cleanup();
    return 0;
}
