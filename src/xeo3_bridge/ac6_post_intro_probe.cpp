#include "xeo3_bridge/xenonrecomp_overrides.h"

#include "ppc_recomp_shared.h"

#include <Windows.h>

#include <cstdint>
#include <limits>

extern "C" PPC_FUNC(__imp__sub_821E63F0);

extern "C"
{
__declspec(dllexport) volatile LONG64 BridgePostIntroCallbackCount = 0;
__declspec(dllexport) volatile LONG64 BridgePostIntroCallbackReturnCount = 0;
__declspec(dllexport) volatile LONG64 BridgePostIntroCallbackZeroCount = 0;
__declspec(dllexport) volatile LONG64 BridgePostIntroCallbackOneCount = 0;
__declspec(dllexport) volatile LONG64 BridgePostIntroCallbackOtherCount = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgePostIntroCallbackLastKind = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgePostIntroCallbackLastObject = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgePostIntroCallbackLastState = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgePostIntroCallbackLastFlags = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgePostIntroCallbackLastTarget = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgePostIntroCallbackLastTargetContext = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgePostIntroCallbackLastR13 = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgePostIntroCallbackLastEntryLr = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgePostIntroCallbackLastExitLr = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgePostIntroCallbackLastExitR3 = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgePostIntroCallbackLastThreadId = 0;
}

PPC_FUNC(sub_821E63F0)
{
    const auto kind = ctx.r3.u32;
    const auto object = ctx.r4.u32;

    InterlockedIncrement64(&BridgePostIntroCallbackCount);
    if (kind == 0)
    {
        InterlockedIncrement64(&BridgePostIntroCallbackZeroCount);
    }
    else if (kind == 1)
    {
        InterlockedIncrement64(&BridgePostIntroCallbackOneCount);
    }
    else
    {
        InterlockedIncrement64(&BridgePostIntroCallbackOtherCount);
    }

    BridgePostIntroCallbackLastKind = kind;
    BridgePostIntroCallbackLastObject = object;
    BridgePostIntroCallbackLastR13 = ctx.r13.u32;
    BridgePostIntroCallbackLastEntryLr =
        static_cast<std::uint32_t>(ctx.lr);
    BridgePostIntroCallbackLastThreadId = GetCurrentThreadId();
    BridgePostIntroCallbackLastState = 0;
    BridgePostIntroCallbackLastFlags = 0;
    BridgePostIntroCallbackLastTarget = 0;
    BridgePostIntroCallbackLastTargetContext = 0;

    if (kind == 1 &&
        object <= std::numeric_limits<std::uint32_t>::max() - 10900U)
    {
        const auto state = PPC_LOAD_U32(object + 10900U);
        BridgePostIntroCallbackLastState = state;
        if (state != 0 &&
            state <= std::numeric_limits<std::uint32_t>::max() - 20U)
        {
            BridgePostIntroCallbackLastFlags = PPC_LOAD_U32(state);
            BridgePostIntroCallbackLastTarget =
                PPC_LOAD_U32(state + 16U);
            BridgePostIntroCallbackLastTargetContext =
                PPC_LOAD_U32(state + 20U);
        }
    }

    __imp__sub_821E63F0(ctx, base);

    BridgePostIntroCallbackLastExitR3 = ctx.r3.u32;
    BridgePostIntroCallbackLastExitLr =
        static_cast<std::uint32_t>(ctx.lr);
    InterlockedIncrement64(&BridgePostIntroCallbackReturnCount);
}
