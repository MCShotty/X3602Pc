#include "xeo3_bridge/xenonrecomp_overrides.h"
#include "xeo3_bridge/xeo3_kernel_continuations.h"

#include "ppc_recomp_shared.h"

#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <cstring>
#include <thread>
#include <vector>

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
struct ExecutorObservation
{
    std::array<std::uint32_t, 4> sources{};
    std::array<std::uint32_t, 4> targets{};
    std::size_t callCount = 0;
    bool valid = true;
};

ExecutorObservation g_executorObservation;

void FakeMappedExecutor(
    PPCContext& context,
    std::uint8_t*,
    const std::uint32_t sourceIar,
    const std::uint32_t targetIar) noexcept
{
    if (g_executorObservation.callCount >=
        g_executorObservation.sources.size())
    {
        g_executorObservation.valid = false;
        return;
    }

    const auto index = g_executorObservation.callCount++;
    g_executorObservation.sources[index] = sourceIar;
    g_executorObservation.targets[index] = targetIar;
    g_executorObservation.valid =
        index == 0 &&
        sourceIar == 0x8005F0C8U &&
        targetIar == xeo3::kernel::kNativeBugCheckExTarget &&
        context.r4.u64 == 0 &&
        context.r5.u64 == 0 &&
        context.r6.u64 == 0 &&
        context.r7.u64 == 0;
}

void FakeThunk()
{
}

void OtherExecutableTarget()
{
}

xeo3::kernel::FingerprintResult MatchFingerprint(
    const std::uint8_t*,
    std::uint32_t& matchedMask) noexcept
{
    matchedMask = 3U;
    return xeo3::kernel::FingerprintResult::match;
}

xeo3::kernel::FingerprintResult RejectFingerprint(
    const std::uint8_t*,
    std::uint32_t& matchedMask) noexcept
{
    matchedMask = 3U;
    return xeo3::kernel::FingerprintResult::mismatch;
}

xeo3::kernel::FingerprintResult MatchNativeHost(
    void*& host,
    std::uint32_t& matchedMask) noexcept
{
    host = reinterpret_cast<void*>(&FakeThunk);
    matchedMask = 3U;
    return xeo3::kernel::FingerprintResult::match;
}

xeo3::kernel::FingerprintResult RejectNativeHost(
    void*& host,
    std::uint32_t& matchedMask) noexcept
{
    host = nullptr;
    matchedMask = 1U;
    return xeo3::kernel::FingerprintResult::mismatch;
}

struct HandlerCase
{
    std::uint64_t bugCheckCode;
    std::uint64_t linkRegister;
    std::uint64_t stackPointer;
};

int RunHandlerCase(const HandlerCase& testCase)
{
    xeo3::kernel::Shutdown();
    CHECK(xeo3::kernel::Initialize(
        &FakeMappedExecutor,
        reinterpret_cast<void*>(&FakeThunk),
        nullptr,
        &MatchFingerprint));

    alignas(32) std::array<std::uint8_t, 1> memory{};
    PPCContext context{};
    context.r1.u64 = testCase.stackPointer;
    context.r3.u64 = testCase.bugCheckCode;
    context.r4.u64 = 0x4444444444444444ULL;
    context.r5.u64 = 0x5555555555555555ULL;
    context.r6.u64 = 0x6666666666666666ULL;
    context.r7.u64 = 0x7777777777777777ULL;
    context.r8.u64 = 0x8888U;
    context.r9.u64 = 0x9999U;
    context.r10.u64 = 0xAAAAU;
    context.r11.u64 = 0xBBBBU;
    context.r27.u64 = 0x2727272727272727ULL;
    context.r31.u64 = 0x3131313131313131ULL;
    context.lr = testCase.linkRegister;
    context.cr6.lt = 0;
    context.cr6.gt = 0;
    context.cr6.eq = 1;
    context.cr6.so = 1;
    context.xer.so = 1;

    g_executorObservation = {};
    auto* const continuation = xeo3::kernel::FindContinuation(
        xeo3::kernel::kContinuation8005F0B4);
    CHECK(continuation != nullptr);
    continuation(context, memory.data());

    CHECK(g_executorObservation.valid);
    CHECK(g_executorObservation.callCount == 1);
    CHECK(BridgeKernelContinuationHitCount == 1);
    CHECK(BridgeKernelContinuationNativeCallCount == 1);
    CHECK(BridgeKernelContinuationNativeReturnCount == 1);
    CHECK(BridgeKernelContinuationEpilogueCallCount == 0);
    CHECK(BridgeKernelContinuationEpilogueReturnCount == 0);
    CHECK(BridgeKernelContinuationLastStage == 3);
    CHECK(BridgeKernelContinuationLastEntryR9 == 0x9999U);
    CHECK(BridgeKernelContinuationLastBugCheckCode ==
          testCase.bugCheckCode);
    CHECK(BridgeKernelContinuationLastPath == 3);
    CHECK(BridgeKernelContinuationLastResult ==
          static_cast<std::uint32_t>(testCase.bugCheckCode));
    CHECK(BridgeKernelContinuationLastStackPointer ==
          testCase.stackPointer);
    CHECK(BridgeKernelContinuationLastLr == testCase.linkRegister);
    CHECK(context.r1.u64 == testCase.stackPointer);
    CHECK(context.r3.u64 == testCase.bugCheckCode);
    CHECK(context.r4.u64 == 0);
    CHECK(context.r5.u64 == 0);
    CHECK(context.r6.u64 == 0);
    CHECK(context.r7.u64 == 0);
    CHECK(context.r8.u64 == 0x8888U);
    CHECK(context.r9.u64 == 0x9999U);
    CHECK(context.r10.u64 == 0xAAAAU);
    CHECK(context.r11.u64 == 0xBBBBU);
    CHECK(context.r27.u64 == 0x2727272727272727ULL);
    CHECK(context.r31.u64 == 0x3131313131313131ULL);
    CHECK(context.lr == testCase.linkRegister);
    CHECK(context.cr6.eq == 1);
    CHECK(context.cr6.so == 1);

    xeo3::kernel::Shutdown();
    return 0;
}

struct DispatchFixture
{
    static constexpr std::size_t kAllocationSize = 0x100000;
    static constexpr std::size_t kContinuationOffset = 0xC0000;

    std::uint8_t* allocation = nullptr;
    std::uintptr_t dispatchBase = 0;
    void** continuationSlot = nullptr;
    void** nativeSlot = nullptr;
    alignas(16) std::array<std::uint8_t, 0x100> cpuStateStorage{};
    std::array<std::uint8_t, 1> guestMemory{};

    bool initialize()
    {
        allocation = static_cast<std::uint8_t*>(VirtualAlloc(
            nullptr,
            kAllocationSize,
            MEM_RESERVE | MEM_COMMIT,
            PAGE_READWRITE));
        if (allocation == nullptr)
        {
            return false;
        }

        continuationSlot = reinterpret_cast<void**>(
            allocation + kContinuationOffset);
        dispatchBase =
            reinterpret_cast<std::uintptr_t>(continuationSlot) -
            static_cast<std::uintptr_t>(
                xeo3::kernel::kContinuation8005F0B4) * 2U;
        nativeSlot = reinterpret_cast<void**>(
            dispatchBase +
            static_cast<std::uintptr_t>(
                xeo3::kernel::kNativeBugCheckExTarget) * 2U);
        if (reinterpret_cast<std::uint8_t*>(nativeSlot) < allocation ||
            reinterpret_cast<std::uint8_t*>(nativeSlot) + sizeof(void*) >
                allocation + kAllocationSize)
        {
            cleanup();
            return false;
        }

        *nativeSlot = reinterpret_cast<void*>(&FakeThunk);
        auto* const cpuState = cpuStateStorage.data() + 0x40;
        std::memcpy(
            cpuState - 0x10,
            &dispatchBase,
            sizeof(dispatchBase));
        return true;
    }

    void* cpuState() noexcept
    {
        return cpuStateStorage.data() + 0x40;
    }

    bool protectContinuationPage(const DWORD protection)
    {
        DWORD ignored = 0;
        return VirtualProtect(
            continuationSlot,
            sizeof(*continuationSlot),
            protection,
            &ignored) != FALSE;
    }

    DWORD continuationProtection() const
    {
        MEMORY_BASIC_INFORMATION information{};
        if (VirtualQuery(
                continuationSlot,
                &information,
                sizeof(information)) != sizeof(information))
        {
            return 0;
        }
        return information.Protect;
    }

    void cleanup()
    {
        if (allocation != nullptr)
        {
            VirtualFree(allocation, 0, MEM_RELEASE);
            allocation = nullptr;
        }
    }

    ~DispatchFixture()
    {
        cleanup();
    }
};

int TestDispatchInstallation()
{
    DispatchFixture fixture;
    CHECK(fixture.initialize());
    *fixture.continuationSlot =
        reinterpret_cast<void*>(&OtherExecutableTarget);
    CHECK(fixture.protectContinuationPage(PAGE_READONLY));
    CHECK(xeo3::kernel::Initialize(
        &FakeMappedExecutor,
        reinterpret_cast<void*>(&FakeThunk),
        reinterpret_cast<void*>(&OtherExecutableTarget),
        &MatchFingerprint));

    const auto first = xeo3::kernel::EnsureInstalled(
        fixture.cpuState(),
        fixture.guestMemory.data());
    CHECK(first == xeo3::kernel::InstallResult::installed);
    CHECK(*fixture.continuationSlot ==
          reinterpret_cast<void*>(&FakeThunk));
    CHECK((fixture.continuationProtection() & 0xFFU) == PAGE_READONLY);
    CHECK(BridgeKernelContinuationInstallCount == 1);
    CHECK(BridgeKernelContinuationFingerprintMask == 3U);

    const auto second = xeo3::kernel::EnsureInstalled(
        fixture.cpuState(),
        fixture.guestMemory.data());
    CHECK(second == xeo3::kernel::InstallResult::alreadyInstalled);
    CHECK(BridgeKernelContinuationInstallCount == 1);

    xeo3::kernel::Shutdown();
    CHECK(*fixture.continuationSlot ==
          reinterpret_cast<void*>(&OtherExecutableTarget));
    CHECK((fixture.continuationProtection() & 0xFFU) == PAGE_READONLY);
    CHECK(BridgeKernelContinuationRestoreCount == 1);
    return 0;
}

int TestConcurrentInstallation()
{
    DispatchFixture fixture;
    CHECK(fixture.initialize());
    CHECK(xeo3::kernel::Initialize(
        &FakeMappedExecutor,
        reinterpret_cast<void*>(&FakeThunk),
        nullptr,
        &MatchFingerprint));

    std::atomic<std::uint32_t> installed{0};
    std::atomic<std::uint32_t> alreadyInstalled{0};
    std::atomic<std::uint32_t> pending{0};
    std::atomic<std::uint32_t> fatal{0};
    std::vector<std::thread> threads;
    for (std::size_t index = 0; index < 8; ++index)
    {
        threads.emplace_back(
            [&]()
            {
                const auto result = xeo3::kernel::EnsureInstalled(
                    fixture.cpuState(),
                    fixture.guestMemory.data());
                if (result == xeo3::kernel::InstallResult::installed)
                {
                    ++installed;
                }
                else if (
                    result ==
                    xeo3::kernel::InstallResult::alreadyInstalled)
                {
                    ++alreadyInstalled;
                }
                else if (
                    result ==
                    xeo3::kernel::InstallResult::pending)
                {
                    ++pending;
                }
                else
                {
                    ++fatal;
                }
            });
    }
    for (auto& thread : threads)
    {
        thread.join();
    }

    CHECK(installed == 1);
    CHECK(installed + alreadyInstalled + pending + fatal == 8);
    CHECK(fatal == 0);
    CHECK(BridgeKernelContinuationInstallFailureCount == 0);
    xeo3::kernel::Shutdown();
    CHECK(*fixture.continuationSlot == nullptr);
    return 0;
}

int TestPreexistingBridgeThunkIsIdempotent()
{
    DispatchFixture fixture;
    CHECK(fixture.initialize());
    *fixture.continuationSlot =
        reinterpret_cast<void*>(&FakeThunk);
    CHECK(xeo3::kernel::Initialize(
        &FakeMappedExecutor,
        reinterpret_cast<void*>(&FakeThunk),
        nullptr,
        &MatchFingerprint));

    CHECK(xeo3::kernel::EnsureInstalled(
              fixture.cpuState(),
              fixture.guestMemory.data()) ==
          xeo3::kernel::InstallResult::alreadyInstalled);
    CHECK(*fixture.continuationSlot ==
          reinterpret_cast<void*>(&FakeThunk));

    xeo3::kernel::Shutdown();
    CHECK(*fixture.continuationSlot == nullptr);
    CHECK(BridgeKernelContinuationRestoreCount == 1);
    return 0;
}

int TestTwoDispatchBases()
{
    DispatchFixture first;
    DispatchFixture second;
    CHECK(first.initialize());
    CHECK(second.initialize());
    CHECK(xeo3::kernel::Initialize(
        &FakeMappedExecutor,
        reinterpret_cast<void*>(&FakeThunk),
        nullptr,
        &MatchFingerprint));

    CHECK(xeo3::kernel::EnsureInstalled(
              first.cpuState(),
              first.guestMemory.data()) ==
          xeo3::kernel::InstallResult::installed);
    CHECK(xeo3::kernel::EnsureInstalled(
              second.cpuState(),
              second.guestMemory.data()) ==
          xeo3::kernel::InstallResult::installed);
    CHECK(BridgeKernelContinuationInstallCount == 2);
    xeo3::kernel::Shutdown();
    CHECK(*first.continuationSlot == nullptr);
    CHECK(*second.continuationSlot == nullptr);
    CHECK(BridgeKernelContinuationRestoreCount == 2);
    return 0;
}

int TestFingerprintAndSlotRejection()
{
    DispatchFixture fixture;
    CHECK(fixture.initialize());
    CHECK(xeo3::kernel::Initialize(
        &FakeMappedExecutor,
        reinterpret_cast<void*>(&FakeThunk),
        nullptr,
        &RejectFingerprint));
    CHECK(xeo3::kernel::EnsureInstalled(
              fixture.cpuState(),
              fixture.guestMemory.data()) ==
          xeo3::kernel::InstallResult::fatal);
    CHECK(
        BridgeKernelContinuationLastFailure ==
        static_cast<std::uint32_t>(
            xeo3::kernel::FailureReason::kernelFingerprintMismatch));
    CHECK(*fixture.continuationSlot == nullptr);
    xeo3::kernel::Shutdown();

    *fixture.continuationSlot =
        reinterpret_cast<void*>(&OtherExecutableTarget);
    CHECK(xeo3::kernel::Initialize(
        &FakeMappedExecutor,
        reinterpret_cast<void*>(&FakeThunk),
        nullptr,
        &MatchFingerprint));
    CHECK(xeo3::kernel::EnsureInstalled(
              fixture.cpuState(),
              fixture.guestMemory.data()) ==
          xeo3::kernel::InstallResult::fatal);
    CHECK(
        BridgeKernelContinuationLastFailure ==
        static_cast<std::uint32_t>(
            xeo3::kernel::FailureReason::unexpectedSlotTarget));
    CHECK(*fixture.continuationSlot ==
          reinterpret_cast<void*>(&OtherExecutableTarget));
    xeo3::kernel::Shutdown();
    return 0;
}

int TestNativeTargetValidation()
{
    DispatchFixture fixture;
    CHECK(fixture.initialize());
    *fixture.nativeSlot = nullptr;
    CHECK(xeo3::kernel::Initialize(
        &FakeMappedExecutor,
        reinterpret_cast<void*>(&FakeThunk),
        nullptr,
        &MatchFingerprint));
    CHECK(xeo3::kernel::EnsureInstalled(
              fixture.cpuState(),
              fixture.guestMemory.data()) ==
          xeo3::kernel::InstallResult::pending);
    CHECK(
        BridgeKernelContinuationLastFailure ==
        static_cast<std::uint32_t>(
            xeo3::kernel::FailureReason::nativeTargetMissing));
    CHECK(*fixture.continuationSlot == nullptr);
    xeo3::kernel::Shutdown();

    *fixture.nativeSlot = fixture.allocation;
    CHECK(xeo3::kernel::Initialize(
        &FakeMappedExecutor,
        reinterpret_cast<void*>(&FakeThunk),
        nullptr,
        &MatchFingerprint));
    CHECK(xeo3::kernel::EnsureInstalled(
              fixture.cpuState(),
              fixture.guestMemory.data()) ==
          xeo3::kernel::InstallResult::fatal);
    CHECK(
        BridgeKernelContinuationLastFailure ==
        static_cast<std::uint32_t>(
            xeo3::kernel::FailureReason::nativeTargetNotExecutable));
    xeo3::kernel::Shutdown();
    return 0;
}

int TestDirectNativeHostFallback()
{
    DispatchFixture fixture;
    CHECK(fixture.initialize());
    *fixture.nativeSlot = nullptr;
    CHECK(xeo3::kernel::Initialize(
        &FakeMappedExecutor,
        reinterpret_cast<void*>(&FakeThunk),
        nullptr,
        &MatchFingerprint,
        &MatchNativeHost));

    CHECK(xeo3::kernel::EnsureInstalled(
              fixture.cpuState(),
              fixture.guestMemory.data()) ==
          xeo3::kernel::InstallResult::installed);
    CHECK(BridgeKernelContinuationNativeDispatchMask == 0);
    CHECK(BridgeKernelContinuationNativeDirectMask == 1U);
    CHECK(BridgeKernelContinuationHostFingerprintMask == 3U);
    CHECK(
        xeo3::kernel::NativeHostTarget(
            xeo3::kernel::kNativeBugCheckExTarget) ==
        reinterpret_cast<void*>(&FakeThunk));

    xeo3::kernel::Shutdown();
    CHECK(*fixture.continuationSlot == nullptr);

    CHECK(xeo3::kernel::Initialize(
        &FakeMappedExecutor,
        reinterpret_cast<void*>(&FakeThunk),
        nullptr,
        &MatchFingerprint,
        &RejectNativeHost));
    CHECK(xeo3::kernel::EnsureInstalled(
              fixture.cpuState(),
              fixture.guestMemory.data()) ==
          xeo3::kernel::InstallResult::fatal);
    CHECK(
        BridgeKernelContinuationLastFailure ==
        static_cast<std::uint32_t>(
            xeo3::kernel::FailureReason::nativeHostProfileMismatch));
    CHECK(BridgeKernelContinuationHostFingerprintMask == 1U);
    xeo3::kernel::Shutdown();
    return 0;
}

int TestPendingInstallationIsThrottled()
{
    DispatchFixture fixture;
    CHECK(fixture.initialize());
    *fixture.nativeSlot = nullptr;
    CHECK(xeo3::kernel::Initialize(
        &FakeMappedExecutor,
        reinterpret_cast<void*>(&FakeThunk),
        nullptr,
        &MatchFingerprint));

    CHECK(xeo3::kernel::EnsureInstalled(
              fixture.cpuState(),
              fixture.guestMemory.data()) ==
          xeo3::kernel::InstallResult::pending);
    CHECK(BridgeKernelContinuationAttemptCount == 1);
    for (std::size_t index = 0; index < 64; ++index)
    {
        CHECK(xeo3::kernel::EnsureInstalled(
                  fixture.cpuState(),
                  fixture.guestMemory.data()) ==
              xeo3::kernel::InstallResult::pending);
    }
    CHECK(BridgeKernelContinuationAttemptCount == 1);
    CHECK(BridgeKernelContinuationThrottleCount == 64);
    CHECK(BridgeKernelContinuationInstallFailureCount == 0);

    xeo3::kernel::Shutdown();
    return 0;
}

int TestCleanupDoesNotOverwriteChangedSlot()
{
    DispatchFixture fixture;
    CHECK(fixture.initialize());
    CHECK(xeo3::kernel::Initialize(
        &FakeMappedExecutor,
        reinterpret_cast<void*>(&FakeThunk),
        nullptr,
        &MatchFingerprint));
    CHECK(xeo3::kernel::EnsureInstalled(
              fixture.cpuState(),
              fixture.guestMemory.data()) ==
          xeo3::kernel::InstallResult::installed);

    *fixture.continuationSlot =
        reinterpret_cast<void*>(&OtherExecutableTarget);
    CHECK(xeo3::kernel::EnsureInstalled(
              fixture.cpuState(),
              fixture.guestMemory.data()) ==
          xeo3::kernel::InstallResult::fatal);
    CHECK(
        BridgeKernelContinuationLastFailure ==
        static_cast<std::uint32_t>(
            xeo3::kernel::FailureReason::unexpectedSlotTarget));
    xeo3::kernel::Shutdown();
    CHECK(*fixture.continuationSlot ==
          reinterpret_cast<void*>(&OtherExecutableTarget));
    CHECK(BridgeKernelContinuationRestoreFailureCount == 1);
    return 0;
}

int RunPinnedHostProfileCase(const wchar_t* const modulePath)
{
    const auto module = LoadLibraryW(modulePath);
    CHECK(module != nullptr);

    void* host = nullptr;
    std::uint32_t matchedMask = 0;
    CHECK(xeo3::kernel::detail::ResolvePinnedNativeHost(
              host,
              matchedMask) ==
          xeo3::kernel::FingerprintResult::match);
    CHECK(matchedMask == 3U);
    const auto imageBase =
        reinterpret_cast<std::uintptr_t>(module);
    CHECK(
        reinterpret_cast<std::uintptr_t>(host) ==
        imageBase + 0x00021899U);
    CHECK(FreeLibrary(module) != FALSE);
    return 0;
}

int RunPinnedKernelFileCase(const wchar_t* const kernelPath)
{
    std::uint32_t matchedMask = 0;
    CHECK(xeo3::kernel::detail::ValidatePinnedKernelFile(
              kernelPath,
              matchedMask) ==
          xeo3::kernel::FingerprintResult::match);
    CHECK(matchedMask == 3U);
    return 0;
}
}

int wmain(const int argumentCount, wchar_t** arguments)
{
    if (argumentCount == 3 &&
        std::wcscmp(arguments[1], L"--kernel-file") == 0)
    {
        return RunPinnedKernelFileCase(arguments[2]);
    }
    if (argumentCount == 2)
    {
        return RunPinnedHostProfileCase(arguments[1]);
    }
    CHECK(argumentCount == 1);

    for (const HandlerCase testCase : {
             HandlerCase{0, 0x82383348U, 0x30071270U},
             HandlerCase{0xDEAD0000C000000DULL,
                         0x82123456U,
                         0x30002000U},
         })
    {
        const auto result = RunHandlerCase(testCase);
        if (result != 0)
        {
            return result;
        }
    }

    for (const auto test : {
             &TestDispatchInstallation,
             &TestConcurrentInstallation,
             &TestPreexistingBridgeThunkIsIdempotent,
             &TestTwoDispatchBases,
             &TestFingerprintAndSlotRejection,
             &TestNativeTargetValidation,
             &TestDirectNativeHostFallback,
             &TestPendingInstallationIsThrottled,
             &TestCleanupDoesNotOverwriteChangedSlot,
         })
    {
        const auto result = test();
        if (result != 0)
        {
            return result;
        }
    }
    return 0;
}
