#include "xeo3_bridge/xenonrecomp_overrides.h"

#include "ppc_recomp_shared.h"

#include <cstdint>

extern "C" PPC_FUNC(__imp__sub_82346648);

extern "C"
{
extern volatile std::uint64_t BridgeSynchronousQueueSubmitCount;
extern volatile std::uint64_t BridgeSynchronousQueueTaskCount;
extern volatile std::uint64_t BridgeSynchronousQueueFailureCount;
extern volatile std::uint64_t BridgeSynchronousQueueFallbackCount;
extern volatile std::uint32_t BridgeSynchronousQueueLastObject;
extern volatile std::uint32_t BridgeSynchronousQueueLastTask;
extern volatile std::uint32_t BridgeSynchronousQueueLastTarget;
extern volatile std::uint32_t BridgeSynchronousQueueLastPhase;
}

namespace
{
constexpr std::uint32_t kMissionQueue = 0x82865380U;
constexpr std::uint32_t kTaskVtable = 0x820114A0U;
constexpr std::uint32_t kGlobalResultStore = 0x82870700U;
constexpr std::uint32_t kQueueCriticalSectionOffset = 24U;
constexpr std::uint32_t kFreeTaskPoolOffset = 22748U;

void PublishGpr(PPCContext& ctx, const std::size_t index) noexcept
{
    switch (index)
    {
    case 3:
        PPC_PUBLISH_GPR(3);
        break;
    case 4:
        PPC_PUBLISH_GPR(4);
        break;
    default:
        break;
    }
}

void SetCall(
    PPCContext& ctx,
    const std::uint32_t r3,
    const std::uint64_t r4,
    const std::uint32_t returnIar) noexcept
{
    ctx.r3.u64 = r3;
    ctx.r4.u64 = r4;
    ctx.lr = returnIar;
    PublishGpr(ctx, 3);
    PublishGpr(ctx, 4);
    PPC_PUBLISH_LR();
}

bool ReturnTaskToPool(
    PPCContext& ctx,
    std::uint8_t* const base,
    const std::uint32_t queue,
    const std::uint32_t task)
{
    const std::uint32_t pool = queue + kFreeTaskPoolOffset;
    const std::uint32_t tailPointer = PPC_LOAD_U32(pool + 20U);
    if (tailPointer == 0)
    {
        return false;
    }

    const std::uint32_t node = PPC_LOAD_U32(tailPointer);
    if (node == 0)
    {
        return false;
    }

    const std::uint32_t next = PPC_LOAD_U32(node);
    const std::uint32_t availableNodes = PPC_LOAD_U32(pool + 12U);
    if (next == 0 || availableNodes == 0)
    {
        return false;
    }

    PPC_STORE_U32(tailPointer, next);
    PPC_STORE_U32(next + 4U, pool);
    PPC_STORE_U32(pool + 12U, availableNodes - 1U);
    PPC_STORE_U32(node + 8U, task);

    SetCall(ctx, pool + 24U, node, 0x82346518U);
    sub_82344270(ctx, base);
    return true;
}

void RunProducerSideEffects(
    PPCContext& ctx,
    std::uint8_t* const base,
    const std::uint64_t payload)
{
    ctx.lr = 0x82346684U;
    PPC_PUBLISH_LR();
    sub_8233AA98(ctx, base);
    const std::uint64_t allocationSize = ctx.r3.u64;

    SetCall(
        ctx,
        kGlobalResultStore + 4U,
        allocationSize,
        0x82346698U);
    sub_8233A488(ctx, base);
    const std::uint32_t result = ctx.r3.u32;

    SetCall(ctx, result, payload, 0x823466A0U);
    sub_8234E3D0(ctx, base);
    const std::uint32_t initializedResult = ctx.r3.u32;

    SetCall(
        ctx,
        kGlobalResultStore,
        initializedResult,
        0x823466ACU);
    sub_8233A630(ctx, base);
}
}

PPC_FUNC(sub_82346648)
{
    const std::uint32_t queue = ctx.r3.u32;
    const std::uint64_t payload = ctx.r4.u64;

    BridgeSynchronousQueueLastObject = queue;
    BridgeSynchronousQueueLastPhase = 1;

    if (queue != kMissionQueue)
    {
        BridgeSynchronousQueueFallbackCount =
            BridgeSynchronousQueueFallbackCount + 1;
        __imp__sub_82346648(ctx, base);
        return;
    }

    BridgeSynchronousQueueSubmitCount =
        BridgeSynchronousQueueSubmitCount + 1;

    const std::uint64_t savedR29 = ctx.r29.u64;
    const std::uint64_t savedR30 = ctx.r30.u64;
    const std::uint64_t savedR31 = ctx.r31.u64;
    const std::uint64_t savedLr = ctx.lr;
    const auto restoreContext = [&]() noexcept
    {
        ctx.r29.u64 = savedR29;
        ctx.r30.u64 = savedR30;
        ctx.r31.u64 = savedR31;
        ctx.lr = savedLr;
        PPC_PUBLISH_GPR(29);
        PPC_PUBLISH_GPR(30);
        PPC_PUBLISH_GPR(31);
        PPC_PUBLISH_LR();
        PPC_SET_GUEST_IAR(0x823466BCU);
    };

    PPC_SET_GUEST_IAR(0x82346648U);
    SetCall(ctx, queue, payload, 0x82346660U);
    sub_82346548(ctx, base);
    const std::uint32_t task = ctx.r3.u32;
    BridgeSynchronousQueueLastTask = task;
    BridgeSynchronousQueueLastPhase = 2;

    if (task == 0)
    {
        BridgeSynchronousQueueFailureCount =
            BridgeSynchronousQueueFailureCount + 1;
        restoreContext();
        return;
    }

    PPC_STORE_U64(task + 8U, payload);
    PPC_STORE_U32(task, kTaskVtable);

    const std::uint32_t callbackTarget =
        PPC_LOAD_U32(PPC_LOAD_U32(task) + 4U);
    BridgeSynchronousQueueLastTarget = callbackTarget;
    BridgeSynchronousQueueLastPhase = 3;
    if (callbackTarget == 0)
    {
        BridgeSynchronousQueueFailureCount =
            BridgeSynchronousQueueFailureCount + 1;
        restoreContext();
        return;
    }

    SetCall(ctx, task, ctx.r4.u64, 0x823464C0U);
    ctx.ctr.u64 = callbackTarget;
    PPC_PUBLISH_CTR();
    BridgeSynchronousQueueTaskCount =
        BridgeSynchronousQueueTaskCount + 1;
    PPC_CALL_INDIRECT_FUNC(callbackTarget);
    BridgeSynchronousQueueLastPhase = 4;

    SetCall(
        ctx,
        queue + kQueueCriticalSectionOffset,
        ctx.r4.u64,
        0x823464C8U);
    __imp__RtlEnterCriticalSection(ctx, base);

    {
        const std::uint32_t destructorTarget =
            PPC_LOAD_U32(PPC_LOAD_U32(task));
        if (destructorTarget == 0)
        {
            BridgeSynchronousQueueFailureCount =
                BridgeSynchronousQueueFailureCount + 1;
        }
        else
        {
            SetCall(ctx, task, 1U, 0x823464E0U);
            ctx.ctr.u64 = destructorTarget;
            PPC_PUBLISH_CTR();
            PPC_CALL_INDIRECT_FUNC(destructorTarget);
        }
    }

    BridgeSynchronousQueueLastPhase = 5;
    if (!ReturnTaskToPool(ctx, base, queue, task))
    {
        BridgeSynchronousQueueFailureCount =
            BridgeSynchronousQueueFailureCount + 1;
    }

    SetCall(
        ctx,
        queue + kQueueCriticalSectionOffset,
        ctx.r4.u64,
        0x82346464U);
    __imp__RtlLeaveCriticalSection(ctx, base);

    BridgeSynchronousQueueLastPhase = 6;
    RunProducerSideEffects(ctx, base, payload);
    BridgeSynchronousQueueLastPhase = 7;

    restoreContext();
}
