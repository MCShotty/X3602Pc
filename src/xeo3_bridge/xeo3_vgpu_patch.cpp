#include "xeo3_bridge/xeo3_vgpu_patch.h"
#include "xeo3_bridge/ac6_constant_descriptor_shadow.h"
#include "xeo3_bridge/ac6_g2h_trace.h"

#include <Windows.h>
#include <intrin.h>
#include <bcrypt.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxcapi.h>

#include "ac6_edram_load_fix.dxil.inc"
#include "ac6_edram_scale_fix.dxil.inc"
#include "ac6_edram_transfer_vs.dxil.inc"
#include "ac6_pso341_width_fix.dxil.inc"

#include <algorithm>
#include <array>
#include <atomic>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <limits>
#include <mutex>
#include <string_view>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "d3dcompiler.lib")

static_assert(xeo3::vgpu::generated::kAc6EdramScaleFixPixelShaderSize == 4460);
static_assert(xeo3::vgpu::generated::kAc6EdramLoadFixPixelShaderSize == 4200);
static_assert(xeo3::vgpu::generated::kAc6EdramTransferVertexShaderSize == 2224);
static_assert(xeo3::vgpu::generated::kAc6Pso341WidthFixComputeShaderSize ==
              8708);

#if defined(XEO3_CONTRACT_BUILD)
#define XEO3_VGPU_EXPORT __declspec(dllexport)
#else
#define XEO3_VGPU_EXPORT
#endif

extern "C" void VgpuCreatePlacedResourceLegacyColdThunk();
extern "C" void VgpuNullPipelineStateGuardThunk();

extern "C" {
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuPatchStatus = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuExtendedFetchCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuLastFetchCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuZeroedHeapCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuLastHeapFlags = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuLastResourceFlags = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuDiscardedResourceCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuDiscardFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuPlacedResourceCallCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuLastPlacedResourceSite = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuLastPlacedResourceFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuLegacyFallbackCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuLegacyFallbackFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuCommittedOverflowFallbackCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuCommittedOverflowFallbackFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuLastCommittedHeapFlags = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuLastHeapType = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuLastHeapCpuPageProperty = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuLastHeapMemoryPoolPreference =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuLastHeapOffset = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuLastHeapSize = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuLastAllocationSize = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuVertexShaderCompileCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuLastVertexShaderSize = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuVertexShaderCaptureFailure =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuShaderCompileCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuLastCompiledShaderSize = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuShaderCaptureFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuXenosTranslateHookInstalled =
    0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuXenosTranslateHookFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuXenosTranslateCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuXenosUcodeCaptureCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuXenosUcodeCaptureFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuXenosShaderMapFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuXenosLastStage = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuXenosLastUcodeSize = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuXenosLastUcodeHash0 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuXenosLastUcodeHash1 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuXenosLastUcodeHash2 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuXenosLastUcodeHash3 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuGroundFixShaderCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuGroundFixFetchCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuGroundFixFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuGroundFixEnabled = 1;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuIndexFixShaderCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuIndexFixSiteCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuIndexFixFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuAircraftRestartShaderCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuShadowRestartShaderCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuReciprocalFixShaderCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuReciprocalFixInstructionCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuReciprocalFixFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuReciprocalFixEnabled = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuWaveBallotFingerprintMatchCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuWaveBallotFixShaderCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuWaveBallotFixSiteCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuWaveBallotFixFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuWaveBallotFixEnabled = 1;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuVposFixShaderCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuVposFixSiteCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuVposFixFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuVposScaleFixEnabled = 1;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuVposSceneHalfWidthUvEnabled =
    1;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuToneMapFixEnabled = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuExposureFixEnabled = 1;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuExposureFingerprintMatchCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuExposureFixShaderCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuExposureFixSiteCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuExposureFixFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuTextureEndianFixEnabled = 1;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuTextureEndianFixBudget =
    UINT32_MAX;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuTextureEndianReplacement = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTextureEndianFixCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTexturePreviewEndianFixCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTextureEndianSignatureMatchCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTextureEndianLastMatchCall =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTextureEndianLastPatchedCall =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTextureTransferCallCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuConstantUploadCallCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuConstantUpload128Count = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuConstantUploadContextCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuConstantUploadContextFailureCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuConstantUploadLastContext = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuConstantUploadLastCpuBase = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuConstantUploadLastGpuBase = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuConstantUploadLastStride = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuConstantUploadLastSlotCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuConstantUploadLastMappedSpan = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuTransfer341WidthFixEnabled =
    1;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341WidthCandidateCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341WidthSignatureMatchCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTransfer341WidthPatchCount =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTransfer341WidthFailureCount =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTransfer341MappedBufferCount =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341ArenaCandidateCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341CommittedArenaCandidateCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuTransfer341LastArenaHeapType =
    0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuTransfer341LastArenaMapResult = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341UploadContextCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341UploadContextHitCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTransfer341LastUploadContext =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTransfer341LastUploadCpuBase =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTransfer341LastUploadGpuBase =
    0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuTransfer341LastUploadStride =
    0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuTransfer341LastUploadSlotCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341LastUploadMappedSpan = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341LastResolvedCpuAddress = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTransfer341DescriptorCount =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTransfer341BindCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341DescriptorMissCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341GpuAddressMissCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTransfer341LastGpuAddress = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTransfer341LastMappedGpuBase =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341LastMappedBufferSize = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTransfer341WidthLastCall = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTransfer341WidthLastContext =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341WidthLastSourceSize = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuTransfer341WidthLastOriginalPackedDimensions = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuTransfer341WidthLastReplacementPackedDimensions = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramConstantCandidateCount =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramLoadConstantCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramScaleConstantCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramConstantSnapshotSequence = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramConstantLastCall = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramConstantLastContext = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramConstantLastSize = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuEdramConstantLastKind = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramConstantData0 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramConstantData1 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramConstantData2 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramConstantData3 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramConstantData4 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramConstantData5 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramConstantData6 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramConstantData7 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTextureEndian2CallCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuLastTextureEndianOriginal = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuLastTextureEndianReplacement =
    0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuLastTextureEndian2Parameter9 =
    0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuLastTextureEndian2Parameter13 = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuLastTextureEndian2Parameter14 = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuLastTextureEndian2Parameter15 = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuLastTextureEndian2Parameter16 = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuDeviceRemovedReason = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuDredDeviceState = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuDredPageFaultAddress = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuDredBreadcrumbNodeCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuDredLastBreadcrumbOp = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuD3d12MessageCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuDredCaptureFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuPipelineStateHookInstalled =
    0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuPipelineStateHookFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuPipelineStateCreateCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuComputePipelineStateHookInstalled = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuComputePipelineStateHookFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuComputePipelineStateCreateCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuComputePipelineStateFingerprintMatchCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuComputePipelineStateLastShaderSize = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuComputePipelineStateLastShaderHash0 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuComputePipelineStateLastShaderHash1 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuComputePipelineStateLastShaderHash2 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuComputePipelineStateLastShaderHash3 = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuComputePipelineStateLastCreateResult = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuComputePipelineStateLastCreateOutput = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTransfer341PipelineState = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTransfer341PipelineBindCount =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341CachedPsoQueryCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341CachedPsoCacheHitCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341CachedPsoMatchCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTransfer341LastCachedPso = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341LastCachedBlobSize = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341LastCachedBlobHash0 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341LastCachedBlobHash1 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341LastCachedBlobHash2 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341LastCachedBlobHash3 = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuTransfer341LastCachedBlobResult = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuTransfer341FingerprintDumpFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuTransfer341PipelineReplacementEnabled = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341ReplacementPipelineState = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341ReplacementCreateCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341ReplacementSubstitutionCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuTransfer341ReplacementFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341LastComputeRootSignature = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTransfer341LastCpuDescriptor =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTransfer341LastDescriptor0 =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTransfer341LastDescriptor1 =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTransfer341LastDescriptor2 =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuTransfer341LastDescriptor3 =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341LastTaskCpuDescriptor = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341LastTaskDescriptor0 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341LastTaskDescriptor1 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341LastTaskDescriptor2 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341LastTaskDescriptor3 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuTransfer341LastTaskGpuAddress = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuPipelineStreamHookInstalled =
    0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuPipelineStreamHookFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuPipelineStreamCreateCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuPipelineStreamParseCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuPipelineStreamParseFailureCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuPipelineStreamLastCreateResult = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuPipelineStreamLastCreateOutput = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuSuppressedEdramRestorePsoCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramRestoreFingerprintCount =
    0;
// Primitive-restart parity is corrected in the generated vertex shader. Keep
// the cull override available for diagnostics, but do not mask winding bugs in
// normal runs.
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuPso535CullFixEnabled = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuPso535CullCandidateCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuPso535CullFingerprintMatchCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuPso535CullFixPipelineCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuPso535CullFixFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuPso535CullLastOriginalMode = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuPso535CullLastReplacementMode = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuEdramScaleFixEnabled = 1;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramScaleCandidateCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramScaleFingerprintMatchCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramScaleFixPipelineCount =
    0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuEdramScaleFixFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuEdramScaleReplacementShaderSize = static_cast<std::uint32_t>(
        xeo3::vgpu::generated::kAc6EdramScaleFixPixelShaderSize);
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramScaleCandidateVsHash0 =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramScaleCandidateVsHash1 =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramScaleCandidateVsHash2 =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramScaleCandidateVsHash3 =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramScaleCandidatePsHash0 =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramScaleCandidatePsHash1 =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramScaleCandidatePsHash2 =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramScaleCandidatePsHash3 =
    0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuEdramLoadFixEnabled = 1;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramLoadCandidateCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramLoadFingerprintMatchCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramLoadFixPipelineCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuEdramLoadFixFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuEdramLoadReplacementShaderSize = static_cast<std::uint32_t>(
        xeo3::vgpu::generated::kAc6EdramLoadFixPixelShaderSize);
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramLoadCandidateVsHash0 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramLoadCandidateVsHash1 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramLoadCandidateVsHash2 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramLoadCandidateVsHash3 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramLoadCandidatePsHash0 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramLoadCandidatePsHash1 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramLoadCandidatePsHash2 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramLoadCandidatePsHash3 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramScaleDrawMatchCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramScaleDrawSubstitutionCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramScaleDrawOriginalPipelineState = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramScaleDrawReplacementPipelineState = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramLoadDrawMatchCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramLoadDrawSubstitutionCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramLoadScissorOverrideCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramLoadDrawOriginalPipelineState = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramLoadDrawReplacementPipelineState = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramDrawRootSignature = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuPixEdramBoundMatchCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuPixEdramBoundRejectCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuPixEdramResolveFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuRtvHookInstalled = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuRtvHookFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuPixDescriptorCopyCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuPixConstantCopyCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuPixDescriptorHookFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramDrawRootSignatureMismatchCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramDrawFingerprintCandidateCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramDrawFingerprintQueryCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramDrawFingerprintCacheHitCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramDrawFingerprintCacheOverflowCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramDrawLastFingerprintPipelineState = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramDrawLastFingerprintBlobSize = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramDrawLastFingerprintHash0 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramDrawLastFingerprintHash1 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramDrawLastFingerprintHash2 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramDrawLastFingerprintHash3 = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuEdramDrawLastFingerprintResult = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuEdramDrawLastFingerprintClassification = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuEdramDrawFingerprintDumpEnabled = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuEdramDrawFingerprintDumpFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuHostCommandListHookCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuHostCommandListHookFailure =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuHostCommandListResetCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuHostCommandListResetLastInitialPipelineState = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuHostCommandListResetLastResult = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuHostDrawCallCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuRestartTerrainPipelineCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuRestartSkyPipelineCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuRestartPipelineOverflowCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuRestartTerrainDrawCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuRestartSkyDrawCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuRestartTerrainStartVertexZeroCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuRestartTerrainStartVertexNonZeroCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuRestartSkyStartVertexZeroCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuRestartSkyStartVertexNonZeroCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuRestartTerrainLastVertexCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuRestartTerrainLastStartVertex = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuRestartTerrainMaxStartVertex = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuRestartSkyLastVertexCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuRestartSkyLastStartVertex = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuRestartSkyMaxStartVertex = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuRestartLastClassification = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuRestartLastPipelineState = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuRestartLastInstanceCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuRestartLastStartInstance = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuRestartLastThreadId = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuRestartLastDrawCall = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuRestartConstantResolveCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuRestartConstantResolveFailureCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuRestartLastResolveFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuRestartLastRootDescriptorTable = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuRestartLastDescriptorHeapGpuStart = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuRestartLastDescriptorHeapCpuStart = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuRestartLastDescriptorHeapByteSpan = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuRestartLastDescriptorHeapIncrement = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuRestartLastCpuDescriptor = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuRestartLastDescriptorWord0 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuRestartLastDescriptorWord1 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuRestartLastDecodedGpuAddress = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuRestartLastUploadContextKind = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuRestartStartMatchesVertexOffsetCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuRestartStartMismatchesVertexOffsetCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuRestartLastConstantGpuAddress = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuRestartLastConstantCpuAddress = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuRestartLastVertexOffsetBits = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuRestartLastUseIndexBuffer = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuRestartLastIndexCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuRestartLastVfetchEndianness = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuRestartLastPackedIbDesc = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuRestartLastResetIndex = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuRestartLastIbBase = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuRestartTerrainLastVertexOffsetBits = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuRestartTerrainLastIndexCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuRestartTerrainLastPackedIbDesc = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuRestartSkyLastVertexOffsetBits = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuRestartSkyLastIndexCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuRestartSkyLastPackedIbDesc = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuHostTransferDrawCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuHostTransferExperimentSelector = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuHostTransferLastCandidate = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuHostTransferLastClassification = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuHostTransferLastPipelineState = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuHostTransferLastRootSignature = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuHostSetDescriptorHeapsCount =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuHostSetGraphicsRootDescriptorTableCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuHostTransferLastDescriptorHeapCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuHostTransferLastDescriptorHeap0 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuHostTransferLastDescriptorHeap1 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuHostTransferLastDescriptorHeap0GpuStart = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuHostTransferLastDescriptorHeap1GpuStart = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuHostTransferLastRootDescriptorTableMask = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuHostTransferLastRootDescriptorTable0 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuHostTransferLastRootDescriptorTable1 = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuHostEdramRestoreDrawSkipEnabled = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuHostEdramRestoreDrawCandidateCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuHostEdramRestoreDrawSkipCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuHostEdramRestoreDrawLastDrawCall = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuHostEdramRestoreDrawLastPipelineState = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuHostEdramRestoreDrawLastRootSignature = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuFullscreenScissorFixEnabled =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuFullscreenScissorFixCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuFullscreenScissorFixFailure =
    0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuMsaaViewportFixEnabled = 1;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuMsaaViewportCandidateCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuMsaaViewportFixCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuMsaaViewportFixFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuMsaaViewportLastPipelineState = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuMsaaViewportLastOriginalWidthBits = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuMsaaViewportLastReplacementWidthBits = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuMsaaViewportLastVertexCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramRestoreDrawCandidateCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramRestoreDrawSkipCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramRestoreDrawHashMismatchCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuEdramRestoreDrawGuardFailure =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuEdramRestoreDrawLastRecord =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramRestoreDrawLastPipelineState = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramRestoreDrawLastCachedBlobSize = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramRestoreDrawLastCachedBlobHash0 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramRestoreDrawLastCachedBlobHash1 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramRestoreDrawLastCachedBlobHash2 = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramRestoreDrawLastCachedBlobHash3 = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuEdramRestoreExperimentSelector = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuEdramRestoreExperimentCandidateCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuEdramRestoreExperimentLastCandidate = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramRestoreExperimentHitCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramRestoreExperimentSkipCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuEdramRestoreExperimentLastCreateCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuDrawRecordCallCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuDrawRecordInterestingCount =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuDrawRecordUniqueCount = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuDrawRecordLastRecord = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuDrawRecordLastCommandList = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuDrawRecordLastCommandContext =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuDrawRecordLastRootSignature =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuDrawRecordLastPipelineState =
    0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuDrawRecordLastViewportWidthBits = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuDrawRecordLastViewportHeightBits = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuDrawRecordLastViewportMinDepthBits = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuDrawRecordLastViewportMaxDepthBits = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuDrawRecordLastViewportTopLeftXBits = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuDrawRecordLastViewportTopLeftYBits = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuDrawRecordLastScissorRight =
    0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuDrawRecordLastScissorBottom =
    0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuDrawRecordLastRecordKind = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuDrawRecordLastVertexCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuDrawRecordLastStartVertex = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuPipelineLastVertexShaderSize =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuPipelineLastPixelShaderSize =
    0;
XEO3_VGPU_EXPORT volatile std::uint32_t BridgeVgpuPipelineLastSampleCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuPipelineLastRenderTargetFormat = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuPipelineLastInputElementCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuPipelineLastExpectedInputLayout = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuPipelineLastExpectedFixedState = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuPipelineStateCreateFailureCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuPipelineStateLastCreateResult = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuPipelineStateLastCreateOutput = 0;
// Unlike LastCreateResult, these survive successful pipeline creation.
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuPipelineStateLastFailureResult = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuPipelineStateLastFailureCreateSequence = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuNullPipelineStateGuardInstalled = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuNullPipelineStateGuardFailure = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuNullPipelineStateSkipCount =
    0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuNullPipelineStateLastThreadId = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t BridgeVgpuNullPipelineStateLastRecord =
    0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuNullPipelineStateLastCommandList = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuNullPipelineStateLastCommandContext = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuNullPipelineStateLastCachedPso = 0;
XEO3_VGPU_EXPORT volatile std::uint64_t
    BridgeVgpuNullPipelineStateLastRootSignature = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuNullPipelineStateLastRecordKind = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuNullPipelineStateLastVertexCount = 0;
XEO3_VGPU_EXPORT volatile std::uint32_t
    BridgeVgpuNullPipelineStateLastStartVertex = 0;

std::uintptr_t VgpuNullPipelineStateNormalTarget = 0;
std::uintptr_t VgpuNullPipelineStateSkipTarget = 0;
}

namespace {
// The package resolver supplies 2608.3123.1.0 even when the executable is
// launched from the lab. Hook locations are validated against that exact
// image; earlier profiles remain historical references under profiles/xeo3.
constexpr DWORD kExpectedTimestamp = 0x6A8FB92D;
constexpr DWORD kExpectedImageSize = 0x0073E000;
constexpr std::uintptr_t kFetchTableRva = 0x00014178;
constexpr std::uintptr_t kShaderCompileRva = 0x0002C3BC;
constexpr std::uintptr_t kXenosTranslateRva = 0x0003C970;
constexpr std::uintptr_t kTextureTransferRva = 0x000756B0;
constexpr std::uintptr_t kStructuredTextureTransferRva = 0x00075838;
constexpr std::uintptr_t kConstantUploadRva = 0x00047BE4;
constexpr std::size_t kUploadContextCpuBaseOffset = 0x10;
constexpr std::size_t kUploadContextGpuBaseOffset = 0x18;
constexpr std::size_t kUploadContextStrideOffset = 0x20;
constexpr std::size_t kUploadContextSlotCountOffset = 0x24;
constexpr std::size_t kUploadContextPrefixSize = 0x28;
constexpr std::uint64_t kMaximumUploadContextLinearSpan = 64ULL << 20;
constexpr std::uintptr_t kNullPipelineStateGuardRva = 0x0000E926;
constexpr std::uintptr_t kNullPipelineStateNormalRva = 0x0000E938;
constexpr std::uintptr_t kNullPipelineStateSkipRva = 0x0000E90E;
constexpr std::uintptr_t kSamplerAddressModeTableRva = 0x00414BE0;
constexpr std::uintptr_t kTightAlignmentGateRva = 0x00017D90;
constexpr std::uintptr_t kPlacedResourceGateRva = 0x00017DF1;
constexpr std::uintptr_t kCreateHeapCallRva = 0x0004435B;
constexpr std::uintptr_t kModernPlacedResourceCallRva = 0x00017E35;
constexpr std::uintptr_t kLegacyPlacedResourceCallRva = 0x0005C8B0;
constexpr std::uintptr_t kSecondaryModernPlacedResourceCallRva = 0x0004E0A2;
constexpr std::uintptr_t kColdLegacyPlacedResourceSequenceRva = 0x000686C7;
constexpr std::uintptr_t kColdLegacyPlacedResourceCallRva = 0x000686CA;
// Confirmed from the native fetch-table call target and PE unwind boundary.
constexpr std::uintptr_t kAllocatorRva = 0x00047450;
constexpr std::uintptr_t kUploadInterfacePointerRva = 0x005F79D8;
constexpr std::size_t kUploadMethodVtableOffset = 0xB8;
constexpr std::size_t kCallRelayCount = 5;
constexpr std::size_t kCallRelayRegionSize =
    kCallRelayCount * xeo3::vgpu::kCallRelaySize;
constexpr std::uintptr_t kCallRelayMaximumDistance = 0x70000000;
// ID3D12Device inherits the three IUnknown slots plus four ID3D12Object
// slots before GetNodeCount, CreateCommandQueue, CreateCommandAllocator, and
// CreateGraphicsPipelineState.
constexpr std::size_t kCreateGraphicsPipelineStateVtableIndex = 10;
constexpr std::size_t kCreateComputePipelineStateVtableIndex = 11;
// ID3D12Device::CreateConstantBufferView follows CreateDescriptorHeap and
// GetDescriptorHandleIncrementSize in the inherited COM vtable.
constexpr std::size_t kCreateConstantBufferViewVtableIndex = 17;
constexpr std::size_t kCopyDescriptorsVtableIndex = 23;
constexpr std::size_t kCopyDescriptorsSimpleVtableIndex = 24;
constexpr std::size_t kCreateRenderTargetViewVtableIndex = 20;
// ID3D12Device2 appends CreatePipelineState after the three ID3D12Device1
// methods, at slot 47 of the inherited COM vtable.
constexpr std::size_t kCreatePipelineStateVtableIndex = 47;
// ID3D12GraphicsCommandList inherits ID3D12CommandList. These slots are
// stable across later command-list interface revisions.
constexpr std::size_t kReleaseVtableIndex = 2;
constexpr std::size_t kResetVtableIndex = 10;
constexpr std::size_t kDrawInstancedVtableIndex = 12;
constexpr std::size_t kRsSetViewportsVtableIndex = 21;
constexpr std::size_t kRsSetScissorRectsVtableIndex = 22;
constexpr std::size_t kSetPipelineStateVtableIndex = 25;
constexpr std::size_t kSetDescriptorHeapsVtableIndex = 28;
constexpr std::size_t kSetComputeRootSignatureVtableIndex = 29;
constexpr std::size_t kSetGraphicsRootSignatureVtableIndex = 30;
constexpr std::size_t kSetComputeRootDescriptorTableVtableIndex = 31;
constexpr std::size_t kSetGraphicsRootDescriptorTableVtableIndex = 32;
constexpr std::size_t kOmSetRenderTargetsVtableIndex = 46;
constexpr std::size_t kGraphicsCommandListVtableEntryCount = 96;
// XeO3 creates its persistent host-texture heaps before the title AOT DLL is
// loaded, so the CreateHeap call-site hook cannot clear CREATE_NOT_ZEROED for
// them. Initialize each later RT/DS placed resource before XeO3 submits it.
constexpr bool kEnableIndependentDiscardQueue = true;
constexpr std::array<std::uint8_t, 32> kExpectedSha256{
    0x83, 0x06, 0xB4, 0xC0, 0x6B, 0x10, 0x0C, 0xAE, 0x18, 0xF9, 0x1D,
    0xCC, 0xD0, 0x46, 0x8C, 0x22, 0x10, 0xCC, 0xA1, 0x1A, 0x02, 0xD9,
    0x28, 0x02, 0x5D, 0xE5, 0x9B, 0xD8, 0x27, 0x61, 0x02, 0x47,
};
constexpr std::size_t kAc6ExposureShaderSourceSize = 3317;
constexpr std::array<std::uint8_t, 32> kAc6ExposureShaderSourceSha256{
    0x12, 0x97, 0x02, 0x7B, 0x92, 0x53, 0x1D, 0x70, 0xF6, 0x1A, 0x5E,
    0xFD, 0x10, 0xF1, 0xC3, 0x87, 0x67, 0x51, 0x00, 0xE4, 0xB1, 0x0A,
    0x1B, 0x0B, 0x59, 0x0F, 0xDB, 0x41, 0x32, 0x42, 0x7E, 0x09,
};
constexpr std::size_t kAc6SkyRestartShaderSourceSize = 2581;
constexpr std::array<std::uint8_t, 32> kAc6SkyRestartShaderSourceSha256{
    0xEB, 0x95, 0x4D, 0xCD, 0x9B, 0xDB, 0x98, 0xBC, 0xCE, 0xDC, 0x6E,
    0xCC, 0xF8, 0x53, 0xD3, 0xB8, 0x4F, 0xA2, 0x7A, 0x13, 0xDA, 0x93,
    0xBE, 0x12, 0xEE, 0x6C, 0x1A, 0xFA, 0x41, 0xD1, 0x70, 0xE9,
};
constexpr std::size_t kAc6TerrainFanRestartShaderSourceSize = 5398;
constexpr std::array<std::uint8_t, 32> kAc6TerrainFanRestartShaderSourceSha256{
    0xBB, 0x1C, 0xBA, 0x01, 0x5B, 0x7F, 0x47, 0xA0, 0x78, 0x05, 0xE5,
    0xB7, 0xD8, 0x36, 0x7A, 0xCF, 0x84, 0xDA, 0xC5, 0xDA, 0x1C, 0xDC,
    0xED, 0x80, 0xAF, 0xB4, 0xCE, 0x32, 0xAB, 0x22, 0xF0, 0x5F,
};
constexpr std::size_t kAc6TerrainFanRestartDxilSize = 11008;
constexpr std::size_t kAc6AircraftRestartShaderSourceSize = 4807;
constexpr std::array<std::uint8_t, 32> kAc6AircraftRestartShaderSourceSha256{
    0x50, 0x68, 0x0B, 0x1F, 0xA8, 0x45, 0x87, 0x9F, 0xD6, 0x8E, 0xB3,
    0x9A, 0x34, 0xB8, 0x48, 0x20, 0x36, 0x62, 0x5E, 0x13, 0xB8, 0xD3,
    0x06, 0xBA, 0xBF, 0x94, 0xCD, 0xC4, 0x2D, 0x8E, 0x2A, 0x4A,
};
constexpr std::size_t kAc6ShadowRestartShaderSourceSize = 4299;
constexpr std::array<std::uint8_t, 32> kAc6ShadowRestartShaderSourceSha256{
    0x6C, 0x0E, 0xDF, 0x6C, 0xA9, 0x47, 0xE4, 0x7A, 0x84, 0x9C, 0x87,
    0xFB, 0xD6, 0x4C, 0x52, 0xC1, 0x94, 0xCB, 0x13, 0x4D, 0x7D, 0x2B,
    0xDB, 0xEF, 0x02, 0xA7, 0x90, 0x4F, 0xAD, 0x56, 0xFE, 0x42,
};
constexpr std::array<std::uint8_t, 32> kAc6TerrainFanRestartDxilSha256{
    0x50, 0x9E, 0xC0, 0xF2, 0x86, 0xCD, 0xD9, 0x9B, 0x35, 0x50, 0x26,
    0x0B, 0x5F, 0x2F, 0x0D, 0xD1, 0x6B, 0x38, 0x02, 0x95, 0xC1, 0x81,
    0x5B, 0x93, 0xBD, 0x5B, 0xEF, 0x0D, 0x99, 0x54, 0x63, 0xA7,
};
// Current source includes the restart-window's first index in its clip test.
constexpr std::size_t kAc6TerrainFanRestartWindowDxilSize = 11192;
constexpr std::array<std::uint8_t, 32> kAc6TerrainFanRestartWindowDxilSha256{
    0xA6, 0xD4, 0x21, 0xB7, 0xF2, 0xFF, 0xEC, 0x26, 0x09, 0x7D, 0x50,
    0x70, 0x4A, 0x58, 0x0F, 0x9A, 0x10, 0x5B, 0x80, 0xC8, 0x20, 0xD5,
    0x1D, 0x26, 0x37, 0xD3, 0x18, 0xE3, 0xC8, 0x06, 0x3C, 0xC5,
};
constexpr std::size_t kAc6TerrainDrawLocalDxilSize = 11156;
constexpr std::array<std::uint8_t, 32> kAc6TerrainDrawLocalDxilSha256{
    0x03, 0xCA, 0x9A, 0x3A, 0x68, 0xDF, 0xC8, 0xBD, 0x67, 0x72, 0x24, 0x9F, 0x99, 0xEB, 0x7C, 0xEE, 0x7F, 0xA4, 0x99, 0xEE, 0x68, 0x4C, 0x81, 0xD5, 0xE3, 0xFD, 0x00, 0xD9, 0xB2, 0x06, 0x04, 0xB8,
};
constexpr std::size_t kAc6SkyDrawLocalDxilSize = 7800;
constexpr std::array<std::uint8_t, 32> kAc6SkyDrawLocalDxilSha256{
    0x5F, 0x40, 0xFD, 0x64, 0x6C, 0x18, 0x10, 0x8A, 0x34, 0x13, 0x94, 0xCD, 0x5A, 0x55, 0x59, 0xDB, 0x2E, 0xB7, 0x53, 0xB9, 0xCF, 0x86, 0xD4, 0x15, 0xAA, 0x00, 0xA2, 0xE5, 0x80, 0xC4, 0xF2, 0xBC,
};
constexpr std::size_t kAc6SkyRestartDxilSize = 7680;
constexpr std::array<std::uint8_t, 32> kAc6SkyRestartDxilSha256{
    0x82, 0xDC, 0x76, 0xA8, 0xE1, 0x24, 0xE9, 0x85, 0x8B, 0xF3, 0x47,
    0xFC, 0x7B, 0x26, 0x7D, 0xC1, 0x69, 0xC1, 0x66, 0x7F, 0xC9, 0x4F,
    0xB6, 0xDB, 0x23, 0xEF, 0x32, 0xB7, 0xE9, 0x87, 0x01, 0x04,
};
constexpr std::size_t kAc6Pso537ShaderSourceSize = 3351;
constexpr std::array<std::uint8_t, 32> kAc6Pso537ShaderSourceSha256{
    0x10, 0x0A, 0x6B, 0xAA, 0xAC, 0x33, 0x57, 0x53, 0x0C, 0x7B, 0x69,
    0x94, 0xE1, 0xFF, 0xD5, 0x83, 0x79, 0xB0, 0x77, 0xCD, 0x66, 0xE5,
    0x9D, 0xAD, 0xF6, 0xD2, 0x06, 0x3F, 0xC9, 0x35, 0x11, 0xF9,
};
constexpr std::size_t kAc6Pso533PixelShaderSourceSize = 17879;
constexpr std::array<std::uint8_t, 32> kAc6Pso533PixelShaderSourceSha256{
    0x06, 0x54, 0x71, 0x85, 0x92, 0x82, 0x2F, 0x3A, 0x66, 0x47, 0x45,
    0x87, 0x34, 0xDD, 0x46, 0x8B, 0x4F, 0x8C, 0xC0, 0x2B, 0x2A, 0x5D,
    0x82, 0x63, 0x02, 0xE2, 0x15, 0x0C, 0xC3, 0x09, 0xA1, 0xE4,
};
constexpr std::size_t kAc6Pso540VertexShaderSourceSize = 7902;
constexpr std::array<std::uint8_t, 32> kAc6Pso540VertexShaderSourceSha256{
    0xBE, 0xE4, 0x1F, 0x9B, 0x23, 0x88, 0xAC, 0x41, 0xE1, 0x9E, 0x97,
    0x5C, 0xE8, 0xCC, 0xB5, 0x1C, 0xA7, 0x07, 0x27, 0xB2, 0xB5, 0xDB,
    0xB2, 0x1E, 0x15, 0xD3, 0xA6, 0xA1, 0x10, 0x0B, 0x59, 0x89,
};
constexpr std::array<std::uint8_t, 32> kAc6Pso535VertexShaderSha256{
    0xC6, 0xAB, 0x0E, 0x87, 0xB2, 0x08, 0x8B, 0x28, 0x49, 0x8A, 0x4B,
    0x49, 0x39, 0xFB, 0x02, 0x57, 0x50, 0xC0, 0x3A, 0xDA, 0xB1, 0x93,
    0xA5, 0xF3, 0xD9, 0xBA, 0x03, 0xED, 0x27, 0xD5, 0x78, 0x1F,
};
constexpr std::array<std::uint8_t, 32> kAc6Pso535PixelShaderSha256{
    0x4F, 0x41, 0xC8, 0xE3, 0x6C, 0xBC, 0x61, 0x25, 0x07, 0x3A, 0x79,
    0x04, 0xC6, 0x54, 0x04, 0x26, 0x2E, 0xC6, 0xE7, 0x2F, 0xBB, 0x9F,
    0x6D, 0xED, 0xE6, 0x5B, 0xC6, 0x48, 0xB3, 0xA5, 0xAC, 0x5D,
};
constexpr std::array<std::uint8_t, 32> kAc6Pso341ComputeShaderSha256{
    0xB1, 0x5D, 0x25, 0xCB, 0x2A, 0x20, 0x52, 0xA5, 0x2D, 0xCB, 0x65,
    0x68, 0x9B, 0x1E, 0xD5, 0x4D, 0x2A, 0xA3, 0x8E, 0x84, 0xF4, 0x0E,
    0x75, 0x36, 0x84, 0x56, 0x1C, 0x10, 0x39, 0xED, 0x4A, 0x54,
};
constexpr std::array<std::uint8_t, xeo3::vgpu::kFetchTableDetourSize>
    kExpectedPrologue{
        0x48, 0x89, 0x5C, 0x24, 0x10, 0x55, 0x56, 0x57, 0x41,
        0x56, 0x41, 0x57, 0x48, 0x8D, 0x6C, 0x24, 0xD1,
    };
constexpr std::array<std::uint8_t, xeo3::vgpu::kShaderCompileDetourSize>
    kExpectedShaderCompilePrologue{
        0x4C, 0x89, 0x4C, 0x24, 0x20, 0x4C, 0x89, 0x44,
        0x24, 0x18, 0x55, 0x53, 0x56, 0x57, 0x41, 0x56,
    };
constexpr std::array<std::uint8_t, xeo3::vgpu::kXenosTranslateDetourSize>
    kExpectedXenosTranslatePrologue{
        0x48, 0x89, 0x5C, 0x24, 0x20, 0x55, 0x56, 0x57,
        0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57,
    };
constexpr std::array<std::uint8_t, xeo3::vgpu::kTextureTransferDetourSize>
    kExpectedTextureTransferPrologue{
        0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55,
        0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0x6C, 0x24, 0xF9,
    };
constexpr std::array<std::uint8_t,
                     xeo3::vgpu::kStructuredTextureTransferDetourSize>
    kExpectedStructuredTextureTransferPrologue{
        0x40, 0x55, 0x56, 0x41, 0x54, 0x41, 0x55, 0x41,
        0x56, 0x41, 0x57, 0x48, 0x8D, 0x6C, 0x24, 0xF8,
    };
constexpr std::array<std::uint8_t, xeo3::vgpu::kConstantUploadDetourSize>
    kExpectedConstantUploadPrologue{
        0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24,
        0x18, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x49, 0x8B, 0xF8,
    };
constexpr std::array<std::uint8_t, xeo3::vgpu::kNullPipelineStateDetourSize>
    kExpectedNullPipelineStateSequence{
        0x48, 0x89, 0x93, 0x18, 0x10, 0x00, 0x00, 0x8B, 0x03,
        0x48, 0xC1, 0xE0, 0x05, 0x48, 0x8B, 0x4C, 0x18, 0x08,
    };
constexpr std::array<std::uint32_t, xeo3::vgpu::kXenosSamplerAddressModeCount>
    kExpectedSamplerAddressModes{1, 2, 3, 5, 0, 0, 4, 0};
// Xenos modes 4, 5, and 7 have no exact D3D12 equivalent. These are the
// closest mappings used by Xenia's D3D12 backend: clamp-to-edge for halfway,
// and mirror-once for the mirrored variants.
constexpr std::array<std::uint32_t, xeo3::vgpu::kXenosSamplerAddressModeCount>
    kFixedSamplerAddressModes{1, 2, 3, 5, 3, 5, 4, 5};
constexpr std::array<std::uint8_t, xeo3::vgpu::kTightAlignmentGateSize>
    kExpectedTightAlignmentGate{
        0x0F, 0x85, 0xB4, 0x4A, 0x04, 0x00,
    };
constexpr std::array<std::uint8_t, xeo3::vgpu::kTightAlignmentGateSize>
    kDisabledTightAlignmentGate{
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
    };
constexpr std::array<std::uint8_t, xeo3::vgpu::kPlacedResourceGateSize>
    kExpectedPlacedResourceGate{
        0x0F, 0x84, 0x86, 0x4A, 0x04, 0x00,
    };
constexpr std::array<std::uint8_t, xeo3::vgpu::kCreateHeapCallSize>
    kExpectedCreateHeapCall{
        0xFF, 0x90, 0xE0, 0x00, 0x00, 0x00,
    };
constexpr std::array<std::uint8_t, xeo3::vgpu::kPlacedResourceCallSize>
    kExpectedModernPlacedResourceCall{
        0x41, 0xFF, 0x93, 0x68, 0x02, 0x00, 0x00,
    };
constexpr std::array<std::uint8_t, xeo3::vgpu::kPlacedResourceCallSize>
    kExpectedLegacyPlacedResourceCall{
        0x41, 0xFF, 0x93, 0xE8, 0x00, 0x00, 0x00,
    };
constexpr std::array<std::uint8_t, xeo3::vgpu::kCreateHeapCallSize>
    kExpectedSecondaryModernPlacedResourceCall{
        0xFF, 0x90, 0x68, 0x02, 0x00, 0x00,
    };
constexpr std::array<std::uint8_t, xeo3::vgpu::kCreateHeapCallSize>
    kExpectedColdLegacyPlacedResourceSequence{
        0x49, 0x8B, 0xCB, 0x41, 0xFF, 0xD2,
    };

enum class PatchStatus : std::uint32_t {
  NotAttempted = 0,
  Installed = 1,
  Removed = 2,
  MissingHashCallback = 0x80000001,
  ModuleMissing = 0x80000002,
  InvalidPe = 0x80000003,
  IdentityMismatch = 0x80000004,
  PathFailure = 0x80000005,
  HashMismatch = 0x80000006,
  PrologueMismatch = 0x80000007,
  TrampolineAllocationFailure = 0x80000008,
  TrampolineProtectionFailure = 0x80000009,
  TargetProtectionFailure = 0x8000000A,
  TargetProtectionRestoreFailure = 0x8000000B,
  DetourChanged = 0x8000000C,
  InvalidExtendedCall = 0x8000000D,
  TightAlignmentGateMismatch = 0x8000000E,
  TightAlignmentProtectionFailure = 0x8000000F,
  TightAlignmentProtectionRestoreFailure = 0x80000010,
  TightAlignmentGateChanged = 0x80000011,
  PlacedResourceGateMismatch = 0x80000012,
  PlacedResourceProtectionFailure = 0x80000013,
  PlacedResourceProtectionRestoreFailure = 0x80000014,
  PlacedResourceGateChanged = 0x80000015,
  CreateHeapCallMismatch = 0x80000016,
  CreateHeapCallOutOfRange = 0x80000017,
  CreateHeapCallProtectionFailure = 0x80000018,
  CreateHeapCallProtectionRestoreFailure = 0x80000019,
  CreateHeapCallChanged = 0x8000001A,
  ModernPlacedResourceCallMismatch = 0x8000001B,
  LegacyPlacedResourceCallMismatch = 0x8000001C,
  PlacedResourceCallOutOfRange = 0x8000001D,
  PlacedResourceCallProtectionFailure = 0x8000001E,
  PlacedResourceCallProtectionRestoreFailure = 0x8000001F,
  PlacedResourceCallChanged = 0x80000020,
  SecondaryModernPlacedResourceCallMismatch = 0x80000021,
  ColdLegacyPlacedResourceCallMismatch = 0x80000022,
  ShaderCompilePrologueMismatch = 0x80000023,
  ShaderCompileTrampolineAllocationFailure = 0x80000024,
  ShaderCompileTrampolineProtectionFailure = 0x80000025,
  ShaderCompileTargetProtectionFailure = 0x80000026,
  ShaderCompileTargetProtectionRestoreFailure = 0x80000027,
  ShaderCompileDetourChanged = 0x80000028,
  SamplerAddressModeTableMismatch = 0x80000029,
  SamplerAddressModeProtectionFailure = 0x8000002A,
  SamplerAddressModeProtectionRestoreFailure = 0x8000002B,
  SamplerAddressModeTableChanged = 0x8000002C,
  CallRelayAllocationFailure = 0x8000002D,
  CallRelayProtectionFailure = 0x8000002E,
  CallRelayOutOfRange = 0x8000002F,
  TextureTransferPrologueMismatch = 0x80000030,
  TextureTransferTrampolineAllocationFailure = 0x80000031,
  TextureTransferTrampolineProtectionFailure = 0x80000032,
  TextureTransferTargetProtectionFailure = 0x80000033,
  TextureTransferTargetProtectionRestoreFailure = 0x80000034,
  TextureTransferDetourChanged = 0x80000035,
  StructuredTextureTransferPrologueMismatch = 0x80000036,
  StructuredTextureTransferTrampolineAllocationFailure = 0x80000037,
  StructuredTextureTransferTrampolineProtectionFailure = 0x80000038,
  StructuredTextureTransferTargetProtectionFailure = 0x80000039,
  StructuredTextureTransferTargetProtectionRestoreFailure = 0x8000003A,
  StructuredTextureTransferDetourChanged = 0x8000003B,
  ConstantUploadPrologueMismatch = 0x8000003C,
  ConstantUploadTrampolineAllocationFailure = 0x8000003D,
  ConstantUploadTrampolineProtectionFailure = 0x8000003E,
  ConstantUploadTargetProtectionFailure = 0x8000003F,
  ConstantUploadTargetProtectionRestoreFailure = 0x80000040,
  ConstantUploadDetourChanged = 0x80000041,
  NullPipelineStateSequenceMismatch = 0x80000042,
  NullPipelineStateProtectionFailure = 0x80000043,
  NullPipelineStateProtectionRestoreFailure = 0x80000044,
  NullPipelineStateDetourChanged = 0x80000045,
  XenosTranslatePrologueMismatch = 0x80000046,
  XenosTranslateTrampolineAllocationFailure = 0x80000047,
  XenosTranslateTrampolineProtectionFailure = 0x80000048,
  XenosTranslateTargetProtectionFailure = 0x80000049,
  XenosTranslateTargetProtectionRestoreFailure = 0x8000004A,
  XenosTranslateDetourChanged = 0x8000004B,
};

using NativeFetchTable = std::uint32_t (*)(void *cache,
                                           std::uint64_t *outputGpuAddress,
                                           const void *source,
                                           std::uint32_t entryCount,
                                           void *allocatorContext);
using NativeCompileHlsl = bool (*)(const void *source, std::uint64_t sourceSize,
                                   const wchar_t *targetProfile,
                                   const wchar_t *entryPoint, void *parameter5,
                                   void *parameter6, void *parameter7,
                                   void *parameter8);
using NativeGetXenosShaderBytes = const void *(*)(void *shaderSource,
                                                  std::uint64_t stage,
                                                  std::uint32_t *byteCount);
using NativeTranslateXenosShader = std::uint64_t (*)(
    std::uint64_t *parameter1, std::uint64_t stage, void *shaderSource,
    std::uint64_t parameter4, std::uint64_t parameter5,
    std::uint64_t parameter6, void *parameter7, std::uint8_t parameter8,
    std::uint64_t parameter9, std::uint64_t parameter10, void *parameter11,
    void *parameter12, void *parameter13);
using NativeTextureTransfer =
    void (*)(std::uint64_t parameter1, std::uint64_t parameter2,
             std::uint64_t parameter3, std::uint32_t parameter4,
             std::uint32_t parameter5, const std::uint64_t *parameter6,
             std::uint64_t parameter7, std::uint64_t parameter8,
             std::uint32_t parameter9, std::int32_t parameter10,
             std::uint32_t parameter11, const std::uint64_t *parameter12,
             std::uint32_t parameter13, std::uint32_t parameter14,
             std::uint32_t parameter15, std::uint32_t parameter16,
             std::uint32_t parameter17, std::uint32_t parameter18,
             std::uint32_t parameter19, std::uint32_t parameter20,
             std::uint64_t parameter21, std::uint64_t parameter22);
using NativeStructuredTextureTransfer = void (*)(
    std::uint64_t parameter1, const std::uint64_t *parameter2,
    std::uint64_t parameter3, std::uint64_t parameter4, std::int64_t parameter5,
    std::int64_t parameter6, std::int64_t parameter7, std::uint32_t parameter8,
    std::uint32_t parameter9, std::uint32_t parameter10,
    std::uint64_t parameter11, std::uint64_t parameter12);
using NativeConstantUpload = void (*)(std::uint64_t context, const void *source,
                                      std::size_t sourceSize);
using NativeCreateGraphicsPipelineState = HRESULT(STDMETHODCALLTYPE *)(
    ID3D12Device *device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *description,
    const IID &interfaceId, void **pipelineState);
using NativeCreateComputePipelineState = HRESULT(STDMETHODCALLTYPE *)(
    ID3D12Device *device, const D3D12_COMPUTE_PIPELINE_STATE_DESC *description,
    const IID &interfaceId, void **pipelineState);
using NativeCreatePipelineState = HRESULT(STDMETHODCALLTYPE *)(
    ID3D12Device2 *device, const D3D12_PIPELINE_STATE_STREAM_DESC *description,
    const IID &interfaceId, void **pipelineState);
using NativeCreateConstantBufferView = void(STDMETHODCALLTYPE *)(
    ID3D12Device *device, const D3D12_CONSTANT_BUFFER_VIEW_DESC *description,
    D3D12_CPU_DESCRIPTOR_HANDLE destinationDescriptor);
using NativeRelease = ULONG(STDMETHODCALLTYPE *)(IUnknown *object);
using NativeCreateRenderTargetView = void(STDMETHODCALLTYPE *)(
    ID3D12Device *, ID3D12Resource *, const D3D12_RENDER_TARGET_VIEW_DESC *,
    D3D12_CPU_DESCRIPTOR_HANDLE);
using NativeOmSetRenderTargets = void(STDMETHODCALLTYPE *)(
    ID3D12GraphicsCommandList *, UINT, const D3D12_CPU_DESCRIPTOR_HANDLE *, BOOL,
    const D3D12_CPU_DESCRIPTOR_HANDLE *);
using NativeCopyDescriptors = void(STDMETHODCALLTYPE *)(
    ID3D12Device *, UINT, const D3D12_CPU_DESCRIPTOR_HANDLE *, const UINT *,
    UINT, const D3D12_CPU_DESCRIPTOR_HANDLE *, const UINT *, D3D12_DESCRIPTOR_HEAP_TYPE);
using NativeCopyDescriptorsSimple = void(STDMETHODCALLTYPE *)(
    ID3D12Device *, UINT, D3D12_CPU_DESCRIPTOR_HANDLE,
    D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_DESCRIPTOR_HEAP_TYPE);
using NativeCreateShaderResourceView = void(STDMETHODCALLTYPE *)(
    ID3D12Device *, ID3D12Resource *, const D3D12_SHADER_RESOURCE_VIEW_DESC *,
    D3D12_CPU_DESCRIPTOR_HANDLE);
using NativeCreateUnorderedAccessView = void(STDMETHODCALLTYPE *)(
    ID3D12Device *, ID3D12Resource *, ID3D12Resource *,
    const D3D12_UNORDERED_ACCESS_VIEW_DESC *, D3D12_CPU_DESCRIPTOR_HANDLE);
using NativeReset = HRESULT(STDMETHODCALLTYPE *)(
    ID3D12GraphicsCommandList *commandList, ID3D12CommandAllocator *allocator,
    ID3D12PipelineState *initialPipelineState);
using NativeDrawInstanced = void(STDMETHODCALLTYPE *)(
    ID3D12GraphicsCommandList *commandList, UINT vertexCountPerInstance,
    UINT instanceCount, UINT startVertexLocation, UINT startInstanceLocation);
using NativeRsSetViewports = void(STDMETHODCALLTYPE *)(
    ID3D12GraphicsCommandList *commandList, UINT viewportCount,
    const D3D12_VIEWPORT *viewports);
using NativeRsSetScissorRects =
    void(STDMETHODCALLTYPE *)(ID3D12GraphicsCommandList *commandList,
                              UINT rectCount, const D3D12_RECT *rects);
using NativeSetPipelineState = void(STDMETHODCALLTYPE *)(
    ID3D12GraphicsCommandList *commandList, ID3D12PipelineState *pipelineState);
using NativeSetDescriptorHeaps = void(STDMETHODCALLTYPE *)(
    ID3D12GraphicsCommandList *commandList, UINT descriptorHeapCount,
    ID3D12DescriptorHeap *const *descriptorHeaps);
using NativeSetComputeRootSignature = void(STDMETHODCALLTYPE *)(
    ID3D12GraphicsCommandList *commandList, ID3D12RootSignature *rootSignature);
using NativeSetGraphicsRootSignature = void(STDMETHODCALLTYPE *)(
    ID3D12GraphicsCommandList *commandList, ID3D12RootSignature *rootSignature);
using NativeSetComputeRootDescriptorTable = void(STDMETHODCALLTYPE *)(
    ID3D12GraphicsCommandList *commandList, UINT rootParameterIndex,
    D3D12_GPU_DESCRIPTOR_HANDLE baseDescriptor);
using NativeSetGraphicsRootDescriptorTable = void(STDMETHODCALLTYPE *)(
    ID3D12GraphicsCommandList *commandList, UINT rootParameterIndex,
    D3D12_GPU_DESCRIPTOR_HANDLE baseDescriptor);

struct PlacedResourceCreationState {
  bool usesEnhancedBarriers;
  D3D12_BARRIER_LAYOUT initialLayout;
  D3D12_RESOURCE_STATES initialState;
};

std::uint8_t *g_moduleBase = nullptr;
std::uint8_t *g_patchTarget = nullptr;
std::uint8_t *g_shaderCompileTarget = nullptr;
std::uint8_t *g_xenosTranslateTarget = nullptr;
std::uint8_t *g_textureTransferTarget = nullptr;
std::uint8_t *g_structuredTextureTransferTarget = nullptr;
std::uint8_t *g_constantUploadTarget = nullptr;
std::uint8_t *g_nullPipelineStateTarget = nullptr;
std::uint32_t *g_samplerAddressModeTarget = nullptr;
std::uint8_t *g_tightAlignmentTarget = nullptr;
std::uint8_t *g_createHeapCallTarget = nullptr;
std::uint8_t *g_modernPlacedResourceCallTarget = nullptr;
std::uint8_t *g_legacyPlacedResourceCallTarget = nullptr;
std::uint8_t *g_secondaryModernPlacedResourceCallTarget = nullptr;
std::uint8_t *g_coldLegacyPlacedResourceSequenceTarget = nullptr;
void *g_trampoline = nullptr;
void *g_shaderCompileTrampoline = nullptr;
void *g_xenosTranslateTrampoline = nullptr;
void *g_textureTransferTrampoline = nullptr;
void *g_structuredTextureTransferTrampoline = nullptr;
void *g_constantUploadTrampoline = nullptr;
void *g_callRelayRegion = nullptr;
NativeFetchTable g_nativeFetchTable = nullptr;
NativeCompileHlsl g_nativeCompileHlsl = nullptr;
NativeTranslateXenosShader g_nativeTranslateXenosShader = nullptr;
NativeTextureTransfer g_nativeTextureTransfer = nullptr;
NativeStructuredTextureTransfer g_nativeStructuredTextureTransfer = nullptr;
NativeConstantUpload g_nativeConstantUpload = nullptr;
std::array<std::uint8_t, xeo3::vgpu::kFetchTableDetourSize> g_installedDetour{};
std::array<std::uint8_t, xeo3::vgpu::kShaderCompileDetourSize>
    g_installedShaderCompileDetour{};
std::array<std::uint8_t, xeo3::vgpu::kXenosTranslateDetourSize>
    g_installedXenosTranslateDetour{};
std::array<std::uint8_t, xeo3::vgpu::kTextureTransferDetourSize>
    g_installedTextureTransferDetour{};
std::array<std::uint8_t, xeo3::vgpu::kStructuredTextureTransferDetourSize>
    g_installedStructuredTextureTransferDetour{};
std::array<std::uint8_t, xeo3::vgpu::kConstantUploadDetourSize>
    g_installedConstantUploadDetour{};
std::array<std::uint8_t, xeo3::vgpu::kNullPipelineStateDetourSize>
    g_installedNullPipelineStateDetour{};
std::array<std::uint8_t, xeo3::vgpu::kCreateHeapCallSize>
    g_installedCreateHeapCall{};
std::array<std::uint8_t, xeo3::vgpu::kPlacedResourceCallSize>
    g_installedModernPlacedResourceCall{};
std::array<std::uint8_t, xeo3::vgpu::kPlacedResourceCallSize>
    g_installedLegacyPlacedResourceCall{};
std::array<std::uint8_t, xeo3::vgpu::kCreateHeapCallSize>
    g_installedSecondaryModernPlacedResourceCall{};
std::array<std::uint8_t, xeo3::vgpu::kCreateHeapCallSize>
    g_installedColdLegacyPlacedResourceSequence{};
std::atomic<std::uint64_t> g_extendedFetchCount{0};
std::atomic<std::uint64_t> g_zeroedHeapCount{0};
std::atomic<std::uint64_t> g_discardedResourceCount{0};
std::atomic<std::uint64_t> g_placedResourceCallCount{0};
std::atomic<std::uint64_t> g_legacyFallbackCount{0};
std::atomic<std::uint64_t> g_committedOverflowFallbackCount{0};
std::atomic<std::uint64_t> g_shaderCompileCount{0};
std::atomic<std::uint64_t> g_xenosTranslateCount{0};
std::atomic<std::uint64_t> g_xenosUcodeCaptureCount{0};
std::atomic<std::uint64_t> g_vertexShaderCompileCount{0};
std::atomic<std::uint64_t> g_groundFixShaderCount{0};
std::atomic<std::uint64_t> g_groundFixFetchCount{0};
std::atomic<std::uint64_t> g_indexFixShaderCount{0};
std::atomic<std::uint64_t> g_indexFixSiteCount{0};
std::atomic<std::uint64_t> g_aircraftRestartShaderCount{0};
std::atomic<std::uint64_t> g_shadowRestartShaderCount{0};
std::atomic<std::uint64_t> g_reciprocalFixShaderCount{0};
std::atomic<std::uint64_t> g_reciprocalFixInstructionCount{0};
std::atomic<std::uint64_t> g_waveBallotFingerprintMatchCount{0};
std::atomic<std::uint64_t> g_waveBallotFixShaderCount{0};
std::atomic<std::uint64_t> g_waveBallotFixSiteCount{0};
std::atomic<std::uint64_t> g_vposFixShaderCount{0};
std::atomic<std::uint64_t> g_vposFixSiteCount{0};
std::atomic<std::uint64_t> g_exposureFingerprintMatchCount{0};
std::atomic<std::uint64_t> g_exposureFixShaderCount{0};
std::atomic<std::uint64_t> g_exposureFixSiteCount{0};
std::atomic<std::uint64_t> g_textureTransferCallCount{0};
std::atomic<std::uint64_t> g_constantUploadCallCount{0};
std::atomic<std::uint64_t> g_constantUpload128Count{0};
std::atomic<std::uint64_t> g_constantUploadContextFailureCount{0};
std::atomic<std::uint64_t> g_transfer341WidthCandidateCount{0};
std::atomic<std::uint64_t> g_transfer341WidthSignatureMatchCount{0};
std::atomic<std::uint64_t> g_transfer341WidthPatchCount{0};
std::atomic<std::uint64_t> g_transfer341WidthFailureCount{0};
std::atomic<std::uint64_t> g_transfer341DescriptorCount{0};
std::atomic<std::uint64_t> g_transfer341BindCount{0};
std::atomic<std::uint64_t> g_transfer341DescriptorMissCount{0};
std::atomic<std::uint64_t> g_transfer341GpuAddressMissCount{0};
std::atomic<std::uint64_t> g_transfer341ArenaCandidateCount{0};
std::atomic<std::uint64_t> g_transfer341CommittedArenaCandidateCount{0};
std::atomic<std::uint64_t> g_transfer341UploadContextHitCount{0};
std::atomic<std::uint64_t> g_edramConstantCandidateCount{0};
std::atomic<std::uint64_t> g_edramLoadConstantCount{0};
std::atomic<std::uint64_t> g_edramScaleConstantCount{0};
std::atomic<std::uint64_t> g_edramConstantSnapshotSequence{0};
std::atomic_flag g_edramConstantSnapshotWriter = ATOMIC_FLAG_INIT;
std::atomic<std::uint64_t> g_textureEndian2CallCount{0};
std::atomic<std::uint64_t> g_textureEndianFixCount{0};
std::atomic<std::uint64_t> g_texturePreviewEndianFixCount{0};
std::atomic<std::uint64_t> g_textureEndianSignatureMatchCount{0};
std::atomic<bool> g_dredCaptured{false};
std::atomic<std::uint64_t> g_pipelineStateCreateCount{0};
std::atomic<std::uint64_t> g_pipelineStateCreateFailureCount{0};
std::atomic<std::uint64_t> g_pipelineStreamCreateCount{0};
std::atomic<std::uint64_t> g_pipelineStreamParseCount{0};
std::atomic<std::uint64_t> g_pipelineStreamParseFailureCount{0};
std::atomic<std::uint64_t> g_suppressedEdramRestorePsoCount{0};
std::atomic<std::uint64_t> g_pso535CullCandidateCount{0};
std::atomic<std::uint64_t> g_pso535CullFingerprintMatchCount{0};
std::atomic<std::uint64_t> g_pso535CullFixPipelineCount{0};
std::atomic<std::uint64_t> g_edramScaleCandidateCount{0};
std::atomic<std::uint64_t> g_edramScaleFingerprintMatchCount{0};
std::atomic<std::uint64_t> g_edramScaleFixPipelineCount{0};
std::atomic<std::uint64_t> g_edramLoadCandidateCount{0};
std::atomic<std::uint64_t> g_edramLoadFingerprintMatchCount{0};
std::atomic<std::uint64_t> g_edramLoadFixPipelineCount{0};
std::atomic<std::uint64_t> g_edramScaleDrawMatchCount{0};
std::atomic<std::uint64_t> g_edramScaleDrawSubstitutionCount{0};
std::atomic<std::uint64_t> g_edramLoadDrawMatchCount{0};
std::atomic<std::uint64_t> g_edramLoadDrawSubstitutionCount{0};
std::atomic<std::uint64_t> g_edramLoadScissorOverrideCount{0};
std::atomic<std::uint64_t> g_edramDrawRootSignatureMismatchCount{0};
std::atomic<std::uint64_t> g_edramDrawFingerprintCandidateCount{0};
std::atomic<std::uint64_t> g_edramDrawFingerprintQueryCount{0};
std::atomic<std::uint64_t> g_edramDrawFingerprintCacheHitCount{0};
std::atomic<std::uint64_t> g_edramDrawFingerprintCacheOverflowCount{0};
std::atomic<std::uint64_t> g_hostDrawCallCount{0};
std::atomic<std::uint64_t> g_restartTerrainPipelineCount{0};
std::atomic<std::uint64_t> g_restartSkyPipelineCount{0};
std::atomic<std::uint64_t> g_restartPipelineOverflowCount{0};
std::atomic<std::uint64_t> g_restartTerrainDrawCount{0};
std::atomic<std::uint64_t> g_restartSkyDrawCount{0};
std::atomic<std::uint64_t> g_restartTerrainStartVertexZeroCount{0};
std::atomic<std::uint64_t> g_restartTerrainStartVertexNonZeroCount{0};
std::atomic<std::uint64_t> g_restartSkyStartVertexZeroCount{0};
std::atomic<std::uint64_t> g_restartSkyStartVertexNonZeroCount{0};
std::atomic<std::uint32_t> g_restartTerrainMaxStartVertex{0};
std::atomic<std::uint32_t> g_restartSkyMaxStartVertex{0};
std::atomic<std::uint64_t> g_restartConstantResolveCount{0};
std::atomic<std::uint64_t> g_restartConstantResolveFailureCount{0};
std::atomic<std::uint64_t> g_restartStartMatchesVertexOffsetCount{0};
std::atomic<std::uint64_t> g_restartStartMismatchesVertexOffsetCount{0};
std::atomic<std::uint64_t> g_hostCommandListResetCount{0};
std::atomic<std::uint64_t> g_hostTransferDrawCount{0};
std::atomic<std::uint64_t> g_hostEdramRestoreDrawCandidateCount{0};
std::atomic<std::uint64_t> g_hostEdramRestoreDrawSkipCount{0};
std::atomic<std::uint64_t> g_hostSetDescriptorHeapsCount{0};
std::atomic<std::uint64_t> g_hostSetGraphicsRootDescriptorTableCount{0};
std::atomic<std::uint64_t> g_fullscreenScissorFixCount{0};
std::atomic<std::uint64_t> g_msaaViewportCandidateCount{0};
std::atomic<std::uint64_t> g_msaaViewportFixCount{0};
std::atomic<std::uint64_t> g_edramRestoreDrawCandidateCount{0};
std::atomic<std::uint64_t> g_edramRestoreDrawSkipCount{0};
std::atomic<std::uint64_t> g_edramRestoreDrawHashMismatchCount{0};
std::atomic<std::uint64_t> g_edramRestoreExperimentHitCount{0};
std::atomic<std::uint64_t> g_edramRestoreExperimentSkipCount{0};
std::atomic<std::uint64_t> g_drawRecordCallCount{0};
std::atomic<std::uint64_t> g_drawRecordInterestingCount{0};
std::atomic<std::uint64_t> g_drawRecordUniqueCount{0};
std::atomic<std::uint64_t> g_nullPipelineStateSkipCount{0};
std::atomic<NativeCreateGraphicsPipelineState>
    g_nativeCreateGraphicsPipelineState{nullptr};
std::atomic<NativeCreateComputePipelineState>
    g_nativeCreateComputePipelineState{nullptr};
std::atomic<NativeCreatePipelineState> g_nativeCreatePipelineState{nullptr};
std::atomic<NativeCreateConstantBufferView> g_nativeCreateConstantBufferView{
    nullptr};

struct XenosTranslateContext {
  bool valid = false;
  std::uint64_t sequence = 0;
  std::uint32_t stage = 0;
  std::uint32_t byteCount = 0;
  std::array<std::uint8_t, 32> sha256{};
};

thread_local XenosTranslateContext g_xenosTranslateContext{};
std::mutex g_xenosCaptureMutex;
std::mutex g_xenosShaderMapMutex;

struct MappedUploadBufferRecord {
  ID3D12Resource *resource = nullptr;
  std::uint8_t *cpuBase = nullptr;
  D3D12_GPU_VIRTUAL_ADDRESS gpuBase = 0;
  std::uint64_t size = 0;
};

struct ConstantBufferDescriptorRecord {
  std::uintptr_t cpuDescriptor = 0;
  D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0;
};

struct RenderTargetDescriptorRecord {
  std::uintptr_t cpuDescriptor = 0;
  std::uint32_t format = 0;
  std::uint32_t sampleCount = 0;
  std::uint64_t width = 0;
  std::uint32_t height = 0;
};
constexpr std::size_t kRtvDescriptorTableSize = 8192;
std::mutex g_rtvDescriptorMutex;
std::array<RenderTargetDescriptorRecord, kRtvDescriptorTableSize>
    g_rtvDescriptors{};
std::mutex g_rtvHookMutex;
void **g_rtvVtableSlot = nullptr;
std::atomic<NativeCreateRenderTargetView> g_nativeCreateRenderTargetView{nullptr};
std::atomic<std::uint64_t> g_pixEdramBoundMatchCount{0};
std::atomic<std::uint64_t> g_pixEdramBoundRejectCount{0};
std::mutex g_pixDescriptorHookMutex;
void **g_pixDescriptorVtable = nullptr;
std::atomic<NativeCopyDescriptors> g_nativeCopyDescriptors{nullptr};
std::atomic<NativeCopyDescriptorsSimple> g_nativeCopyDescriptorsSimple{nullptr};
std::atomic<NativeCreateShaderResourceView> g_nativeCreateShaderResourceView{nullptr};
std::atomic<NativeCreateUnorderedAccessView> g_nativeCreateUnorderedAccessView{nullptr};
std::atomic<std::uint64_t> g_pixDescriptorCopyCount{0};
std::atomic<std::uint64_t> g_pixConstantCopyCount{0};

struct UploadContextRecord {
  std::uintptr_t context = 0;
  std::uint8_t *cpuBase = nullptr;
  D3D12_GPU_VIRTUAL_ADDRESS gpuBase = 0;
  std::uint32_t stride = 0;
  std::uint32_t slotCount = 0;
  std::uint64_t mappedSpan = 0;
};

constexpr std::size_t kMaximumMappedUploadBuffers = 128;
constexpr std::size_t kMaximumUploadContexts = 64;
constexpr std::size_t kMaximumConstantUploadContexts = 64;
constexpr std::size_t kConstantBufferDescriptorTableSize = 1U << 18;
std::mutex g_transfer341MappingMutex;
std::array<MappedUploadBufferRecord, kMaximumMappedUploadBuffers>
    g_mappedUploadBuffers{};
std::size_t g_mappedUploadBufferCount = 0;
std::array<UploadContextRecord, kMaximumUploadContexts> g_uploadContexts{};
std::size_t g_uploadContextCount = 0;
std::array<UploadContextRecord, kMaximumConstantUploadContexts>
    g_constantUploadContexts{};
std::size_t g_constantUploadContextCount = 0;
std::atomic<std::uint64_t> g_constantUploadContextGeneration{1};
std::array<ConstantBufferDescriptorRecord, kConstantBufferDescriptorTableSize>
    g_constantBufferDescriptors{};
xeo3::vgpu::detail::ConstantDescriptorShadow<131072> g_fixedConstantDescriptors;

struct GraphicsCommandListHookRecord {
  std::atomic<ID3D12GraphicsCommandList *> commandList{nullptr};
  void **originalVtable = nullptr;
  void **hookVtable = nullptr;
  NativeRelease nativeRelease = nullptr;
  NativeReset nativeReset = nullptr;
  NativeDrawInstanced nativeDrawInstanced = nullptr;
  NativeOmSetRenderTargets nativeOmSetRenderTargets = nullptr;
  std::uintptr_t renderTargetDescriptor = 0;
  UINT renderTargetCount = 0;
  bool hasDepthStencil = false;
  NativeRsSetViewports nativeRsSetViewports = nullptr;
  NativeRsSetScissorRects nativeRsSetScissorRects = nullptr;
  NativeSetPipelineState nativeSetPipelineState = nullptr;
  NativeSetDescriptorHeaps nativeSetDescriptorHeaps = nullptr;
  NativeSetComputeRootSignature nativeSetComputeRootSignature = nullptr;
  NativeSetGraphicsRootSignature nativeSetGraphicsRootSignature = nullptr;
  NativeSetComputeRootDescriptorTable nativeSetComputeRootDescriptorTable =
      nullptr;
  NativeSetGraphicsRootDescriptorTable nativeSetGraphicsRootDescriptorTable =
      nullptr;
  ID3D12PipelineState *pipelineState = nullptr;
  ID3D12PipelineState *activePipelineState = nullptr;
  ID3D12RootSignature *computeRootSignature = nullptr;
  ID3D12RootSignature *rootSignature = nullptr;
  D3D12_VIEWPORT viewport{};
  D3D12_RECT scissor{};
  UINT viewportCount = 0;
  UINT scissorCount = 0;
  static constexpr std::size_t kTrackedDescriptorHeapCount = 2;
  static constexpr std::size_t kTrackedRootDescriptorTableCount = 8;
  std::array<ID3D12DescriptorHeap *, kTrackedDescriptorHeapCount>
      descriptorHeaps{};
  std::array<std::uint64_t, kTrackedDescriptorHeapCount>
      descriptorHeapGpuStarts{};
  std::array<std::uintptr_t, kTrackedDescriptorHeapCount>
      descriptorHeapCpuStarts{};
  std::array<std::uint64_t, kTrackedDescriptorHeapCount>
      descriptorHeapByteSpans{};
  std::array<std::uint32_t, kTrackedDescriptorHeapCount>
      descriptorHeapIncrements{};
  UINT descriptorHeapCount = 0;
  std::array<std::uint64_t, kTrackedRootDescriptorTableCount>
      graphicsRootDescriptorTables{};
  std::uint64_t graphicsRootDescriptorTableMask = 0;
};

constexpr std::size_t kMaximumGraphicsCommandListHooks = 1024;
std::mutex g_graphicsCommandListHookMutex;
std::array<GraphicsCommandListHookRecord, kMaximumGraphicsCommandListHooks>
    g_graphicsCommandListHooks{};
std::atomic<std::size_t> g_graphicsCommandListHookCount{0};
std::size_t g_graphicsCommandListActiveHookCount = 0;
thread_local ID3D12GraphicsCommandList *g_cachedGraphicsCommandList = nullptr;
thread_local GraphicsCommandListHookRecord *g_cachedGraphicsCommandListHook =
    nullptr;
std::atomic<ID3D12PipelineState *> g_ac6Pso341PipelineState{nullptr};
std::atomic<ID3D12PipelineState *> g_ac6Pso341ReplacementPipelineState{nullptr};
std::atomic<std::uint64_t> g_computePipelineStateCreateCount{0};
std::atomic<std::uint64_t> g_computePipelineStateFingerprintMatchCount{0};
std::atomic<std::uint64_t> g_transfer341PipelineBindCount{0};
std::atomic<std::uint64_t> g_transfer341CachedPsoQueryCount{0};
std::atomic<std::uint64_t> g_transfer341CachedPsoCacheHitCount{0};
std::atomic<std::uint64_t> g_transfer341CachedPsoMatchCount{0};
std::atomic<std::uint64_t> g_transfer341ReplacementCreateCount{0};
std::atomic<std::uint64_t> g_transfer341ReplacementSubstitutionCount{0};

struct ComputePipelineFingerprintRecord {
  ID3D12PipelineState *pipelineState = nullptr;
  std::uint32_t candidateId = 0;
  std::size_t cachedBlobSize = 0;
  std::array<std::uint8_t, 32> cachedBlobSha256{};
  std::uint64_t bindCount = 0;
  std::uintptr_t lastRootSignature = 0;
  std::uint64_t lastGpuDescriptor = 0;
  std::uintptr_t lastCpuDescriptor = 0;
  std::array<std::uint64_t, 4> lastDescriptorWords{};
  HRESULT result = E_PENDING;
  bool matchesPso341 = false;
};

constexpr std::size_t kMaximumComputePipelineFingerprints = 256;
std::mutex g_computePipelineFingerprintMutex;
std::array<ComputePipelineFingerprintRecord,
           kMaximumComputePipelineFingerprints>
    g_computePipelineFingerprints{};
std::size_t g_computePipelineFingerprintCount = 0;
std::mutex g_computePipelineReplacementMutex;
bool g_computePipelineReplacementCreationAttempted = false;

struct GraphicsCommandListVtableHookProfile {
  void **vtable = nullptr;
  NativeRelease nativeRelease = nullptr;
  NativeReset nativeReset = nullptr;
  NativeDrawInstanced nativeDrawInstanced = nullptr;
  NativeOmSetRenderTargets nativeOmSetRenderTargets = nullptr;
  NativeRsSetViewports nativeRsSetViewports = nullptr;
  NativeRsSetScissorRects nativeRsSetScissorRects = nullptr;
  NativeSetPipelineState nativeSetPipelineState = nullptr;
  NativeSetDescriptorHeaps nativeSetDescriptorHeaps = nullptr;
  NativeSetComputeRootSignature nativeSetComputeRootSignature = nullptr;
  NativeSetGraphicsRootSignature nativeSetGraphicsRootSignature = nullptr;
  NativeSetComputeRootDescriptorTable nativeSetComputeRootDescriptorTable =
      nullptr;
  NativeSetGraphicsRootDescriptorTable nativeSetGraphicsRootDescriptorTable =
      nullptr;
};

constexpr std::size_t kMaximumGraphicsCommandListVtableHookProfiles =
    kMaximumGraphicsCommandListHooks;
std::array<GraphicsCommandListVtableHookProfile,
           kMaximumGraphicsCommandListVtableHookProfiles>
    g_graphicsCommandListVtableHookProfiles{};
std::atomic<std::size_t> g_graphicsCommandListVtableHookProfileCount{0};

std::mutex g_discardMutex;
ID3D12Device *g_discardDevice = nullptr;
ID3D12CommandQueue *g_discardQueue = nullptr;
ID3D12CommandAllocator *g_discardAllocator = nullptr;
ID3D12GraphicsCommandList *g_discardList = nullptr;
ID3D12Fence *g_discardFence = nullptr;
HANDLE g_discardEvent = nullptr;
std::uint64_t g_discardFenceValue = 0;
std::mutex g_pipelineStateHookMutex;
void **g_pipelineStateVtableSlot = nullptr;
std::mutex g_computePipelineStateHookMutex;
void **g_computePipelineStateVtableSlot = nullptr;
std::mutex g_constantBufferViewHookMutex;
void **g_constantBufferViewVtableSlot = nullptr;
std::mutex g_pipelineStreamHookMutex;
void **g_pipelineStreamVtableSlot = nullptr;
std::mutex g_edramRestoreDrawMutex;
const void *g_edramRestoreDrawPipelineState = nullptr;
bool g_edramRestoreDrawPipelineMatches = false;

struct EdramDrawReplacementState {
  static constexpr std::size_t kMaximumOriginalPipelineStates = 32;
  std::array<const void *, kMaximumOriginalPipelineStates>
      originalPipelineStates{};
  std::size_t originalPipelineStateCount = 0;
  std::uintptr_t rootSignature = 0;
  ID3D12PipelineState *replacementPipelineState = nullptr;
  bool creationAttempted = false;
};

struct EdramDrawPipelineFingerprint {
  const void *pipelineState = nullptr;
  std::uint32_t candidateId = 0;
  xeo3::vgpu::Ac6EdramDrawPipeline classification =
      xeo3::vgpu::Ac6EdramDrawPipeline::None;
  HRESULT result = E_PENDING;
  std::uint64_t cachedBlobSize = 0;
  std::array<std::uint8_t, 32> cachedBlobSha256{};
  HRESULT objectNameResult = E_PENDING;
  std::array<char, 128> objectName{};
  bool hasObservedSignature = false;
  std::uint64_t observedCreateCount = 0;
  xeo3::vgpu::GraphicsPipelineSignature observedSignature{};
};

constexpr std::size_t kMaximumEdramDrawPipelineFingerprints = 256;

std::mutex g_edramDrawReplacementMutex;
std::uintptr_t g_edramDrawRootSignature = 0;
EdramDrawReplacementState g_edramScaleDrawReplacement{};
EdramDrawReplacementState g_edramLoadDrawReplacement{};
std::array<EdramDrawPipelineFingerprint, kMaximumEdramDrawPipelineFingerprints>
    g_edramDrawPipelineFingerprints{};
std::size_t g_edramDrawPipelineFingerprintCount = 0;

void ResetEdramDrawReplacementsLocked() noexcept {
  if (g_edramScaleDrawReplacement.replacementPipelineState != nullptr) {
    g_edramScaleDrawReplacement.replacementPipelineState->Release();
  }
  if (g_edramLoadDrawReplacement.replacementPipelineState != nullptr) {
    g_edramLoadDrawReplacement.replacementPipelineState->Release();
  }
  g_edramScaleDrawReplacement = {};
  g_edramLoadDrawReplacement = {};
  g_edramDrawPipelineFingerprints = {};
  g_edramDrawPipelineFingerprintCount = 0;
  g_edramDrawRootSignature = 0;
}

struct ObservedPipelineState {
  const void *pipelineState = nullptr;
  xeo3::vgpu::GraphicsPipelineSignature signature{};
  std::uint64_t createCount = 0;
};

struct EdramRestoreExperimentCandidate {
  const void *pipelineState = nullptr;
  xeo3::vgpu::GraphicsPipelineSignature signature{};
  std::array<std::uint8_t, 32> cachedBlobSha256{};
  std::uint64_t createCount = 0;
  std::uint64_t cachedBlobSize = 0;
  std::uint32_t candidateId = 0;
};

constexpr std::size_t kMaximumObservedPipelineStates = 4096;
constexpr std::size_t kMaximumEdramRestoreExperimentCandidates = 16;
constexpr std::size_t kMaximumPrimitiveRestartPipelineStates = 16;
std::mutex g_pipelineObservationMutex;
std::array<ObservedPipelineState, kMaximumObservedPipelineStates>
    g_observedPipelineStates{};
std::size_t g_observedPipelineStateCount = 0;
std::array<EdramRestoreExperimentCandidate,
           kMaximumEdramRestoreExperimentCandidates>
    g_edramRestoreExperimentCandidates{};
std::size_t g_edramRestoreExperimentCandidateCount = 0;
std::array<std::atomic<const void *>, kMaximumPrimitiveRestartPipelineStates>
    g_restartTerrainPipelineStates{};
std::array<std::atomic<const void *>, kMaximumPrimitiveRestartPipelineStates>
    g_restartSkyPipelineStates{};

struct ObservedDrawRecord {
  xeo3::vgpu::DrawRecordSignature signature{};
  std::uintptr_t pipelineState = 0;
  std::uintptr_t commandList = 0;
  std::uintptr_t commandContext = 0;
};

constexpr std::size_t kMaximumObservedDrawRecords = 2048;
std::mutex g_drawRecordTraceMutex;
std::array<ObservedDrawRecord, kMaximumObservedDrawRecords>
    g_observedDrawRecords{};
std::size_t g_observedDrawRecordCount = 0;

class VirtualAllocationOwner {
public:
  explicit VirtualAllocationOwner(void *allocation = nullptr) noexcept
      : allocation_(allocation) {}

  ~VirtualAllocationOwner() {
    if (allocation_ != nullptr) {
      VirtualFree(allocation_, 0, MEM_RELEASE);
    }
  }

  VirtualAllocationOwner(const VirtualAllocationOwner &) = delete;
  VirtualAllocationOwner &operator=(const VirtualAllocationOwner &) = delete;

  void *get() const noexcept { return allocation_; }

  void *release() noexcept {
    auto *const allocation = allocation_;
    allocation_ = nullptr;
    return allocation;
  }

private:
  void *allocation_;
};

std::uintptr_t AlignAddressUp(const std::uintptr_t address,
                              const std::uintptr_t alignment) noexcept {
  if (alignment == 0) {
    return address;
  }
  const auto remainder = address % alignment;
  if (remainder == 0) {
    return address;
  }
  const auto increment = alignment - remainder;
  if (address > (std::numeric_limits<std::uintptr_t>::max)() - increment) {
    return 0;
  }
  return address + increment;
}

void *AllocateCallRelayRegionNear(const void *const anchor,
                                  const std::size_t size) noexcept {
  if (anchor == nullptr || size == 0) {
    return nullptr;
  }

  SYSTEM_INFO systemInfo{};
  GetSystemInfo(&systemInfo);
  const auto minimumAddress =
      reinterpret_cast<std::uintptr_t>(systemInfo.lpMinimumApplicationAddress);
  const auto maximumAddress =
      reinterpret_cast<std::uintptr_t>(systemInfo.lpMaximumApplicationAddress);
  const auto anchorAddress = reinterpret_cast<std::uintptr_t>(anchor);
  const auto lowerBound =
      (std::max)(minimumAddress, anchorAddress > kCallRelayMaximumDistance
                                     ? anchorAddress - kCallRelayMaximumDistance
                                     : minimumAddress);
  const auto upperBound =
      (std::min)(maximumAddress,
                 anchorAddress <= maximumAddress - kCallRelayMaximumDistance
                     ? anchorAddress + kCallRelayMaximumDistance
                     : maximumAddress);
  if (lowerBound >= upperBound || size - 1 > upperBound - lowerBound) {
    return nullptr;
  }

  using VirtualAlloc2Function =
      void *(WINAPI *)(HANDLE, void *, SIZE_T, ULONG, ULONG,
                       MEM_EXTENDED_PARAMETER *, ULONG);
  auto *kernelBase = GetModuleHandleW(L"KernelBase.dll");
  if (kernelBase == nullptr) {
    kernelBase = GetModuleHandleW(L"Kernel32.dll");
  }
  const auto virtualAlloc2 =
      kernelBase == nullptr ? nullptr
                            : reinterpret_cast<VirtualAlloc2Function>(
                                  GetProcAddress(kernelBase, "VirtualAlloc2"));
  if (virtualAlloc2 != nullptr) {
    MEM_ADDRESS_REQUIREMENTS requirements{};
    requirements.LowestStartingAddress = reinterpret_cast<void *>(lowerBound);
    requirements.HighestEndingAddress = reinterpret_cast<void *>(upperBound);
    MEM_EXTENDED_PARAMETER parameter{};
    parameter.Type = MemExtendedParameterAddressRequirements;
    parameter.Pointer = &requirements;
    if (auto *const allocation = virtualAlloc2(GetCurrentProcess(), nullptr,
                                               size, MEM_COMMIT | MEM_RESERVE,
                                               PAGE_READWRITE, &parameter, 1);
        allocation != nullptr) {
      return allocation;
    }
  }

  const auto granularity =
      static_cast<std::uintptr_t>(systemInfo.dwAllocationGranularity);
  auto address = AlignAddressUp(lowerBound, granularity);
  while (address != 0 && address <= upperBound &&
         size - 1 <= upperBound - address) {
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(reinterpret_cast<const void *>(address), &information,
                     sizeof(information)) != sizeof(information)) {
      break;
    }

    const auto regionStart =
        reinterpret_cast<std::uintptr_t>(information.BaseAddress);
    const auto regionSize = static_cast<std::uintptr_t>(information.RegionSize);
    const auto regionEnd =
        regionSize <= (std::numeric_limits<std::uintptr_t>::max)() - regionStart
            ? regionStart + regionSize
            : (std::numeric_limits<std::uintptr_t>::max)();
    if (information.State == MEM_FREE) {
      const auto candidate =
          AlignAddressUp((std::max)(address, regionStart), granularity);
      if (candidate != 0 && candidate <= upperBound &&
          size - 1 <= upperBound - candidate && size <= regionEnd - candidate) {
        if (auto *const allocation =
                VirtualAlloc(reinterpret_cast<void *>(candidate), size,
                             MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            allocation != nullptr) {
          return allocation;
        }
      }
    }

    if (regionEnd <= address) {
      break;
    }
    address = AlignAddressUp(regionEnd, granularity);
  }
  return nullptr;
}

void SetStatus(const PatchStatus status) noexcept {
  BridgeVgpuPatchStatus = static_cast<std::uint32_t>(status);
}

void EmitPatchEvent(const char *event, const std::uint32_t detail,
                    const std::uint64_t count = 0) noexcept {
  char message[256]{};
  const auto length = std::snprintf(
      message, std::size(message),
      "{\"xeo3_ac6\":\"vgpu_patch\",\"event\":\"%s\","
      "\"status\":\"0x%08X\",\"detail\":%u,\"count\":%" PRIu64
      ",\"tid\":%lu}\n",
      event, BridgeVgpuPatchStatus, detail, count, GetCurrentThreadId());
  if (length > 0) {
    OutputDebugStringA(message);
  }
}

bool IsAccessibleMemoryRange(const void *const address,
                             const std::size_t byteCount,
                             const bool requireWritable) noexcept {
  if (address == nullptr || byteCount == 0) {
    return false;
  }
  const auto start = reinterpret_cast<std::uintptr_t>(address);
  if (byteCount > (std::numeric_limits<std::uintptr_t>::max)() - start) {
    return false;
  }
  const auto end = start + byteCount;
  auto current = start;
  while (current < end) {
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(reinterpret_cast<const void *>(current), &information,
                     sizeof(information)) != sizeof(information) ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
      return false;
    }
    if (requireWritable) {
      const auto protection = information.Protect & 0xFF;
      if (protection != PAGE_READWRITE && protection != PAGE_WRITECOPY &&
          protection != PAGE_EXECUTE_READWRITE &&
          protection != PAGE_EXECUTE_WRITECOPY) {
        return false;
      }
    }
    const auto regionStart =
        reinterpret_cast<std::uintptr_t>(information.BaseAddress);
    const auto regionSize = static_cast<std::uintptr_t>(information.RegionSize);
    if (regionSize >
        (std::numeric_limits<std::uintptr_t>::max)() - regionStart) {
      return false;
    }
    const auto regionEnd = regionStart + regionSize;
    if (regionEnd <= current) {
      return false;
    }
    current = (std::min)(end, regionEnd);
  }
  return true;
}

std::uint64_t
MeasureAccessibleWritableMemorySpan(const void *const address,
                                    const std::uint64_t maximumSpan) noexcept {
  if (address == nullptr || maximumSpan == 0) {
    return 0;
  }
  const auto start = reinterpret_cast<std::uintptr_t>(address);
  const auto cappedSpan =
      (std::min)(maximumSpan,
                 static_cast<std::uint64_t>(
                     (std::numeric_limits<std::uintptr_t>::max)() - start));
  const auto end = start + static_cast<std::uintptr_t>(cappedSpan);
  auto current = start;
  void *allocationBase = nullptr;
  while (current < end) {
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(reinterpret_cast<const void *>(current), &information,
                     sizeof(information)) != sizeof(information) ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
      break;
    }
    const auto protection = information.Protect & 0xFF;
    if (protection != PAGE_READWRITE && protection != PAGE_WRITECOPY &&
        protection != PAGE_EXECUTE_READWRITE &&
        protection != PAGE_EXECUTE_WRITECOPY) {
      break;
    }
    if (allocationBase == nullptr) {
      allocationBase = information.AllocationBase;
    } else if (information.AllocationBase != allocationBase) {
      break;
    }
    const auto regionStart =
        reinterpret_cast<std::uintptr_t>(information.BaseAddress);
    const auto regionSize = static_cast<std::uintptr_t>(information.RegionSize);
    if (regionSize >
        (std::numeric_limits<std::uintptr_t>::max)() - regionStart) {
      break;
    }
    const auto regionEnd = regionStart + regionSize;
    if (regionEnd <= current) {
      break;
    }
    current = (std::min)(end, regionEnd);
  }
  return static_cast<std::uint64_t>(current - start);
}

void RecordConstantUploadContext(const std::uint64_t context,
                                 const std::size_t sourceSize) noexcept {
  const auto recordFailure = []() noexcept {
    BridgeVgpuConstantUploadContextFailureCount =
        g_constantUploadContextFailureCount.fetch_add(
            1, std::memory_order_relaxed) +
        1;
  };
  if (context == 0 || sourceSize == 0 ||
      context > (std::numeric_limits<std::uintptr_t>::max)() -
                    kUploadContextPrefixSize) {
    recordFailure();
    return;
  }

  struct ThreadContextCache {
    std::uint64_t generation = 0;
    std::uint32_t validationCountdown = 0;
    std::size_t count = 0;
    std::array<std::uintptr_t, kMaximumConstantUploadContexts> contexts{};
  };
  thread_local ThreadContextCache cache{};

  const auto generation =
      g_constantUploadContextGeneration.load(std::memory_order_acquire);
  if (cache.generation != generation) {
    cache = {};
    cache.generation = generation;
  }
  const auto contextAddress = static_cast<std::uintptr_t>(context);
  const auto cacheEnd =
      cache.contexts.begin() + static_cast<std::ptrdiff_t>(cache.count);
  const auto cached =
      std::find(cache.contexts.begin(), cacheEnd, contextAddress) != cacheEnd;
  if (cached && cache.validationCountdown != 0) {
    --cache.validationCountdown;
    return;
  }
  cache.validationCountdown = 4095;
  const auto *const contextBytes =
      reinterpret_cast<const std::uint8_t *>(contextAddress);
  if (!IsAccessibleMemoryRange(contextBytes, kUploadContextPrefixSize, false)) {
    recordFailure();
    return;
  }

  std::uintptr_t cpuBaseAddress = 0;
  D3D12_GPU_VIRTUAL_ADDRESS gpuBase = 0;
  std::uint32_t stride = 0;
  std::uint32_t slotCount = 0;
  std::memcpy(&cpuBaseAddress, contextBytes + kUploadContextCpuBaseOffset,
              sizeof(cpuBaseAddress));
  std::memcpy(&gpuBase, contextBytes + kUploadContextGpuBaseOffset,
              sizeof(gpuBase));
  std::memcpy(&stride, contextBytes + kUploadContextStrideOffset,
              sizeof(stride));
  std::memcpy(&slotCount, contextBytes + kUploadContextSlotCountOffset,
              sizeof(slotCount));
  const auto mappedSpan = MeasureAccessibleWritableMemorySpan(
      reinterpret_cast<const void *>(cpuBaseAddress),
      kMaximumUploadContextLinearSpan);
  if (cpuBaseAddress == 0 ||
      !xeo3::vgpu::detail::IsConstantUploadContextGeometryValid(
          gpuBase, stride, slotCount, mappedSpan, sourceSize) ||
      !IsAccessibleMemoryRange(reinterpret_cast<const void *>(cpuBaseAddress),
                               sourceSize, true)) {
    recordFailure();
    return;
  }

  std::size_t contextCount = 0;
  {
    std::scoped_lock lock(g_transfer341MappingMutex);
    const auto end = g_constantUploadContexts.begin() +
                     static_cast<std::ptrdiff_t>(
                         g_constantUploadContextCount);
    auto found = std::find_if(
        g_constantUploadContexts.begin(), end,
        [contextAddress](const UploadContextRecord &record) {
          return record.context == contextAddress;
        });
    if (found == end) {
      if (g_constantUploadContextCount >= g_constantUploadContexts.size()) {
        recordFailure();
        return;
      }
      found = g_constantUploadContexts.begin() +
              static_cast<std::ptrdiff_t>(g_constantUploadContextCount++);
    }
    *found = {contextAddress, reinterpret_cast<std::uint8_t *>(cpuBaseAddress),
              gpuBase,        stride,
              slotCount,      mappedSpan};
    if (GetModuleHandleW(L"WinPixGpuCapturer.dll") != nullptr && slotCount <= 1024) {
      std::array<std::uint64_t, 2048> pairs{};
      SIZE_T bytesRead = 0;
      const auto pairBytes = static_cast<std::size_t>(slotCount) * 16;
      if (ReadProcessMemory(GetCurrentProcess(), contextBytes + 0x28,
                            pairs.data(), pairBytes, &bytesRead) && bytesRead == pairBytes) {
        for (std::size_t slot = 0; slot < slotCount; ++slot) {
          const auto address = gpuBase + slot * static_cast<std::uint64_t>(stride);
          // The pinned pool stores two handles per CBV. CopyDescriptors uses
          // the CPU one; direct table binds can use the GPU one. Neither is
          // interpreted as descriptor bytes (PIX handles are opaque).
          for (std::size_t half = 0; half < 2; ++half) {
            const auto handle = static_cast<std::uintptr_t>(pairs[slot * 2 + half]);
            if (handle != 0 && !g_fixedConstantDescriptors.Assign(handle, address))
              BridgeVgpuPixEdramResolveFailure = 6;
          }
        }
      }
    }
    contextCount = g_constantUploadContextCount;
  }

  if (!cached && cache.count < cache.contexts.size()) {
    cache.contexts[cache.count++] = contextAddress;
  }
  BridgeVgpuConstantUploadContextCount = contextCount;
  BridgeVgpuConstantUploadLastContext = contextAddress;
  BridgeVgpuConstantUploadLastCpuBase = cpuBaseAddress;
  BridgeVgpuConstantUploadLastGpuBase = gpuBase;
  BridgeVgpuConstantUploadLastStride = stride;
  BridgeVgpuConstantUploadLastSlotCount = slotCount;
  BridgeVgpuConstantUploadLastMappedSpan = mappedSpan;
  if (contextCount <= 16) {
    EmitPatchEvent("constant_upload_context", stride, contextCount);
  }
}

void RecordAc6Pso341UploadContext(const std::uint64_t context,
                                  const std::size_t sourceSize) noexcept {
  if (context == 0 || sourceSize != xeo3::vgpu::kAc6Pso341TaskBufferSize ||
      context > (std::numeric_limits<std::uintptr_t>::max)() -
                    kUploadContextPrefixSize) {
    return;
  }
  thread_local std::uintptr_t lastRegisteredContext = 0;
  const auto contextAddress = static_cast<std::uintptr_t>(context);
  if (contextAddress == lastRegisteredContext) {
    return;
  }
  const auto *const contextBytes =
      reinterpret_cast<const std::uint8_t *>(contextAddress);
  if (!IsAccessibleMemoryRange(contextBytes, kUploadContextPrefixSize, false)) {
    return;
  }

  std::uintptr_t cpuBaseAddress = 0;
  D3D12_GPU_VIRTUAL_ADDRESS gpuBase = 0;
  std::uint32_t stride = 0;
  std::uint32_t slotCount = 0;
  std::memcpy(&cpuBaseAddress, contextBytes + kUploadContextCpuBaseOffset,
              sizeof(cpuBaseAddress));
  std::memcpy(&gpuBase, contextBytes + kUploadContextGpuBaseOffset,
              sizeof(gpuBase));
  std::memcpy(&stride, contextBytes + kUploadContextStrideOffset,
              sizeof(stride));
  std::memcpy(&slotCount, contextBytes + kUploadContextSlotCountOffset,
              sizeof(slotCount));
  const auto mappedSpan = MeasureAccessibleWritableMemorySpan(
      reinterpret_cast<const void *>(cpuBaseAddress),
      kMaximumUploadContextLinearSpan);
  std::uint64_t firstOffset = 0;
  if (cpuBaseAddress == 0 ||
      !xeo3::vgpu::detail::ResolveAc6Pso341UploadOffset(
          gpuBase, stride, slotCount, mappedSpan, gpuBase, sourceSize,
          firstOffset) ||
      !IsAccessibleMemoryRange(reinterpret_cast<const void *>(cpuBaseAddress),
                               sourceSize, true)) {
    return;
  }

  std::scoped_lock lock(g_transfer341MappingMutex);
  const auto end = g_uploadContexts.begin() +
                   static_cast<std::ptrdiff_t>(g_uploadContextCount);
  auto found =
      std::find_if(g_uploadContexts.begin(), end,
                   [contextAddress](const UploadContextRecord &record) {
                     return record.context == contextAddress;
                   });
  if (found == end) {
    if (g_uploadContextCount >= g_uploadContexts.size()) {
      BridgeVgpuTransfer341WidthFailureCount =
          g_transfer341WidthFailureCount.fetch_add(1,
                                                   std::memory_order_relaxed) +
          1;
      return;
    }
    found = g_uploadContexts.begin() +
            static_cast<std::ptrdiff_t>(g_uploadContextCount++);
  }
  *found = {contextAddress, reinterpret_cast<std::uint8_t *>(cpuBaseAddress),
            gpuBase,        stride,
            slotCount,      mappedSpan};
  lastRegisteredContext = contextAddress;
  BridgeVgpuTransfer341UploadContextCount = g_uploadContextCount;
  BridgeVgpuTransfer341LastUploadContext = contextAddress;
  BridgeVgpuTransfer341LastUploadCpuBase = cpuBaseAddress;
  BridgeVgpuTransfer341LastUploadGpuBase = gpuBase;
  BridgeVgpuTransfer341LastUploadStride = stride;
  BridgeVgpuTransfer341LastUploadSlotCount = slotCount;
  BridgeVgpuTransfer341LastUploadMappedSpan = mappedSpan;
  if (g_uploadContextCount <= 16) {
    EmitPatchEvent("transfer341_upload_context", stride, g_uploadContextCount);
  }
}

std::size_t
ConstantBufferDescriptorIndex(const std::uintptr_t cpuDescriptor) noexcept {
  auto value = static_cast<std::uint64_t>(cpuDescriptor);
  value ^= value >> 29;
  value *= 0x9E3779B185EBCA87ULL;
  value ^= value >> 32;
  return static_cast<std::size_t>(value &
                                  (kConstantBufferDescriptorTableSize - 1));
}

bool TryPatchAc6Transfer341Source(std::uint8_t *const source,
                                  const std::uint64_t sourceSize,
                                  const std::uint64_t context,
                                  const std::uint64_t candidateCount,
                                  const char *const patchEvent) noexcept {
  if (!IsAccessibleMemoryRange(source, static_cast<std::size_t>(sourceSize),
                               false)) {
    return false;
  }
  const auto state = xeo3::vgpu::detail::ClassifyAc6Pso341TaskWidth(
      source, static_cast<std::size_t>(sourceSize));
  if (state == xeo3::vgpu::Ac6Pso341TaskWidthState::NotCandidate) {
    return false;
  }

  const auto matchCount = g_transfer341WidthSignatureMatchCount.fetch_add(
                              1, std::memory_order_relaxed) +
                          1;
  BridgeVgpuTransfer341WidthSignatureMatchCount = matchCount;
  BridgeVgpuTransfer341WidthLastCall = candidateCount;
  BridgeVgpuTransfer341WidthLastContext = context;
  BridgeVgpuTransfer341WidthLastSourceSize = sourceSize;

  std::uint32_t originalPackedDimensions = 0;
  std::uint32_t replacementPackedDimensions = 0;
  std::memcpy(&originalPackedDimensions, source + 2 * sizeof(std::uint32_t),
              sizeof(originalPackedDimensions));
  replacementPackedDimensions = originalPackedDimensions;
  BridgeVgpuTransfer341WidthLastOriginalPackedDimensions =
      originalPackedDimensions;
  BridgeVgpuTransfer341WidthLastReplacementPackedDimensions =
      replacementPackedDimensions;

  if (state == xeo3::vgpu::Ac6Pso341TaskWidthState::Broken &&
      BridgeVgpuTransfer341WidthFixEnabled != 0) {
    if (IsAccessibleMemoryRange(source, static_cast<std::size_t>(sourceSize),
                                true) &&
        xeo3::vgpu::detail::PatchAc6Pso341TaskWidth(
            source, static_cast<std::size_t>(sourceSize),
            originalPackedDimensions, replacementPackedDimensions)) {
      std::atomic_thread_fence(std::memory_order_release);
      BridgeVgpuTransfer341WidthLastOriginalPackedDimensions =
          originalPackedDimensions;
      BridgeVgpuTransfer341WidthLastReplacementPackedDimensions =
          replacementPackedDimensions;
      const auto patchCount =
          g_transfer341WidthPatchCount.fetch_add(1, std::memory_order_relaxed) +
          1;
      BridgeVgpuTransfer341WidthPatchCount = patchCount;
      if (patchCount <= 64 || (patchCount & (patchCount - 1)) == 0) {
        EmitPatchEvent(patchEvent, replacementPackedDimensions, patchCount);
      }
    } else {
      BridgeVgpuTransfer341WidthFailureCount =
          g_transfer341WidthFailureCount.fetch_add(1,
                                                   std::memory_order_relaxed) +
          1;
    }
  }
  return true;
}

bool TryPatchAc6Transfer341GpuAddressLocked(
    const D3D12_GPU_VIRTUAL_ADDRESS gpuAddress, const std::uint64_t sourceSize,
    const std::uint64_t context) noexcept {
  if (sourceSize != xeo3::vgpu::kAc6Pso341TaskBufferSize || gpuAddress == 0) {
    return false;
  }

  const auto candidateCount =
      g_transfer341WidthCandidateCount.fetch_add(1, std::memory_order_relaxed) +
      1;
  BridgeVgpuTransfer341WidthCandidateCount = candidateCount;
  BridgeVgpuTransfer341LastGpuAddress = gpuAddress;

  for (std::size_t index = 0; index < g_mappedUploadBufferCount; ++index) {
    const auto &mapping = g_mappedUploadBuffers[index];
    if (mapping.resource == nullptr || mapping.cpuBase == nullptr ||
        gpuAddress < mapping.gpuBase) {
      continue;
    }
    const auto offset = gpuAddress - mapping.gpuBase;
    if (offset > mapping.size || sourceSize > mapping.size - offset) {
      continue;
    }
    return TryPatchAc6Transfer341Source(mapping.cpuBase + offset, sourceSize,
                                        context, candidateCount,
                                        "transfer341_mapped_cbv_width_fix");
  }

  for (std::size_t index = 0; index < g_uploadContextCount; ++index) {
    const auto &uploadContext = g_uploadContexts[index];
    std::uint64_t cpuOffset = 0;
    if (uploadContext.cpuBase == nullptr ||
        !xeo3::vgpu::detail::ResolveAc6Pso341UploadOffset(
            uploadContext.gpuBase, uploadContext.stride,
            uploadContext.slotCount, uploadContext.mappedSpan, gpuAddress,
            sourceSize, cpuOffset)) {
      continue;
    }
    const auto cpuBase =
        reinterpret_cast<std::uintptr_t>(uploadContext.cpuBase);
    if (cpuOffset > (std::numeric_limits<std::uintptr_t>::max)() - cpuBase) {
      continue;
    }
    auto *const source = reinterpret_cast<std::uint8_t *>(
        cpuBase + static_cast<std::uintptr_t>(cpuOffset));
    BridgeVgpuTransfer341UploadContextHitCount =
        g_transfer341UploadContextHitCount.fetch_add(
            1, std::memory_order_relaxed) +
        1;
    BridgeVgpuTransfer341LastUploadContext = uploadContext.context;
    BridgeVgpuTransfer341LastUploadCpuBase = cpuBase;
    BridgeVgpuTransfer341LastUploadGpuBase = uploadContext.gpuBase;
    BridgeVgpuTransfer341LastUploadStride = uploadContext.stride;
    BridgeVgpuTransfer341LastUploadSlotCount = uploadContext.slotCount;
    BridgeVgpuTransfer341LastUploadMappedSpan = uploadContext.mappedSpan;
    BridgeVgpuTransfer341LastResolvedCpuAddress =
        reinterpret_cast<std::uintptr_t>(source);
    if (TryPatchAc6Transfer341Source(source, sourceSize, context,
                                     candidateCount,
                                     "transfer341_upload_context_width_fix")) {
      return true;
    }
  }
  BridgeVgpuTransfer341GpuAddressMissCount =
      g_transfer341GpuAddressMissCount.fetch_add(1, std::memory_order_relaxed) +
      1;
  return false;
}

void RecordAndPatchConstantBufferDescriptor(
    const D3D12_CONSTANT_BUFFER_VIEW_DESC *const description,
    const D3D12_CPU_DESCRIPTOR_HANDLE destinationDescriptor,
    const std::uint64_t context) noexcept {
  if (description == nullptr || destinationDescriptor.ptr == 0) {
    return;
  }

  std::scoped_lock lock(g_transfer341MappingMutex);
  auto &record = g_constantBufferDescriptors[ConstantBufferDescriptorIndex(
      destinationDescriptor.ptr)];
  record.cpuDescriptor = destinationDescriptor.ptr;
  record.gpuAddress = description->BufferLocation;
  if (description->SizeInBytes == xeo3::vgpu::kAc6Pso341TaskBufferSize) {
    BridgeVgpuTransfer341DescriptorCount =
        g_transfer341DescriptorCount.fetch_add(1, std::memory_order_relaxed) +
        1;
  }
  TryPatchAc6Transfer341GpuAddressLocked(description->BufferLocation,
                                         description->SizeInBytes, context);
}

void RecordTransfer341ComputeBindFingerprint(
    ID3D12PipelineState *pipelineState, ID3D12RootSignature *rootSignature,
    D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptor, std::uintptr_t cpuDescriptor,
    const std::array<std::uint64_t, 4> &descriptorWords) noexcept;

void PatchConstantBufferDescriptorAtComputeBind(
    const GraphicsCommandListHookRecord &hook,
    const D3D12_GPU_DESCRIPTOR_HANDLE baseDescriptor,
    const std::uint64_t context) noexcept {
  if (baseDescriptor.ptr == 0) {
    return;
  }

  BridgeVgpuTransfer341BindCount =
      g_transfer341BindCount.fetch_add(1, std::memory_order_relaxed) + 1;

  std::uintptr_t cpuDescriptor = 0;
  std::array<std::uint64_t, 4> descriptorWords{};
  std::uintptr_t taskCpuDescriptor = 0;
  std::array<std::uint64_t, 4> taskDescriptorWords{};
  bool mappedDescriptorFound = false;
  const auto targetPipelineState =
      g_ac6Pso341PipelineState.load(std::memory_order_acquire);
  const auto isTargetPipeline = targetPipelineState != nullptr &&
                                hook.pipelineState == targetPipelineState;
  {
    std::scoped_lock lock(g_transfer341MappingMutex);
    const auto trackedCount =
        (std::min)(static_cast<std::size_t>(hook.descriptorHeapCount),
                   hook.descriptorHeaps.size());
    for (std::size_t heapIndex = 0; heapIndex < trackedCount; ++heapIndex) {
      const auto gpuStart = hook.descriptorHeapGpuStarts[heapIndex];
      const auto cpuStart = hook.descriptorHeapCpuStarts[heapIndex];
      const auto byteSpan = hook.descriptorHeapByteSpans[heapIndex];
      const auto increment = hook.descriptorHeapIncrements[heapIndex];
      if (gpuStart == 0 || cpuStart == 0 || baseDescriptor.ptr < gpuStart) {
        continue;
      }
      const auto delta = baseDescriptor.ptr - gpuStart;
      if (byteSpan == 0 || delta >= byteSpan || increment == 0 ||
          delta % increment != 0 ||
          delta > (std::numeric_limits<std::uintptr_t>::max)() - cpuStart) {
        continue;
      }
      cpuDescriptor = cpuStart + static_cast<std::uintptr_t>(delta);
      if (IsAccessibleMemoryRange(reinterpret_cast<const void *>(cpuDescriptor),
                                  sizeof(descriptorWords), false)) {
        std::memcpy(descriptorWords.data(),
                    reinterpret_cast<const void *>(cpuDescriptor),
                    sizeof(descriptorWords));
      }

      if (isTargetPipeline) {
        BridgeVgpuTransfer341PipelineBindCount =
            g_transfer341PipelineBindCount.fetch_add(
                1, std::memory_order_relaxed) +
            1;
        BridgeVgpuTransfer341LastCpuDescriptor = cpuDescriptor;
        BridgeVgpuTransfer341LastDescriptor0 = descriptorWords[0];
        BridgeVgpuTransfer341LastDescriptor1 = descriptorWords[1];
        BridgeVgpuTransfer341LastDescriptor2 = descriptorWords[2];
        BridgeVgpuTransfer341LastDescriptor3 = descriptorWords[3];
        if (increment < byteSpan - delta) {
          const auto taskDelta = delta + increment;
          if (taskDelta <=
              (std::numeric_limits<std::uintptr_t>::max)() - cpuStart) {
            taskCpuDescriptor =
                cpuStart + static_cast<std::uintptr_t>(taskDelta);
            if (IsAccessibleMemoryRange(
                    reinterpret_cast<const void *>(taskCpuDescriptor),
                    sizeof(taskDescriptorWords), false)) {
              std::memcpy(taskDescriptorWords.data(),
                          reinterpret_cast<const void *>(taskCpuDescriptor),
                          sizeof(taskDescriptorWords));
            }
          }
        }
        BridgeVgpuTransfer341LastTaskCpuDescriptor = taskCpuDescriptor;
        BridgeVgpuTransfer341LastTaskDescriptor0 = taskDescriptorWords[0];
        BridgeVgpuTransfer341LastTaskDescriptor1 = taskDescriptorWords[1];
        BridgeVgpuTransfer341LastTaskDescriptor2 = taskDescriptorWords[2];
        BridgeVgpuTransfer341LastTaskDescriptor3 = taskDescriptorWords[3];

        D3D12_GPU_VIRTUAL_ADDRESS taskGpuAddress = 0;
        if (taskCpuDescriptor != 0) {
          const auto &record =
              g_constantBufferDescriptors[ConstantBufferDescriptorIndex(
                  taskCpuDescriptor)];
          if (record.cpuDescriptor == taskCpuDescriptor &&
              record.gpuAddress != 0) {
            taskGpuAddress = record.gpuAddress;
          }
        }
        if (taskGpuAddress == 0) {
          taskGpuAddress =
              xeo3::vgpu::detail::DecodeAmdConstantBufferGpuAddress(
                  taskDescriptorWords);
        }
        BridgeVgpuTransfer341LastTaskGpuAddress = taskGpuAddress;
        if (taskGpuAddress != 0) {
          mappedDescriptorFound = TryPatchAc6Transfer341GpuAddressLocked(
              taskGpuAddress, xeo3::vgpu::kAc6Pso341TaskBufferSize, context);
        }
      }
      break;
    }
  }
  RecordTransfer341ComputeBindFingerprint(
      hook.pipelineState, hook.computeRootSignature, baseDescriptor,
      cpuDescriptor, descriptorWords);
  if (!isTargetPipeline) {
    return;
  }
  if (mappedDescriptorFound) {
    return;
  }
  BridgeVgpuTransfer341DescriptorMissCount =
      g_transfer341DescriptorMissCount.fetch_add(1, std::memory_order_relaxed) +
      1;
}

void PublishOwnedAc6Pso341PipelineState(
    ID3D12PipelineState *const pipelineState) noexcept {
  if (pipelineState == nullptr) {
    return;
  }
  auto *const previous = g_ac6Pso341PipelineState.exchange(
      pipelineState, std::memory_order_acq_rel);
  if (previous != nullptr) {
    previous->Release();
  }
  BridgeVgpuTransfer341PipelineState =
      reinterpret_cast<std::uintptr_t>(pipelineState);
}

void DumpTransfer341ComputePipelineCachedBlob(
    const ID3D12PipelineState *const pipelineState,
    const std::uint32_t candidateId, const void *const bytes,
    const std::size_t byteCount) noexcept {
  if (pipelineState == nullptr || bytes == nullptr || byteCount == 0 ||
      byteCount > MAXDWORD) {
    BridgeVgpuTransfer341FingerprintDumpFailure = ERROR_INVALID_PARAMETER;
    return;
  }
  char path[512]{};
  const auto pathLength =
      std::snprintf(path, std::size(path),
                    "D:\\Games\\AC6 shit\\XeO3-AC6-lab\\ProbeLogs\\"
                    "ac6-compute-pso-%lu-%03u-%016" PRIXPTR ".bin",
                    GetCurrentProcessId(), candidateId,
                    reinterpret_cast<std::uintptr_t>(pipelineState));
  if (pathLength <= 0 ||
      static_cast<std::size_t>(pathLength) >= std::size(path)) {
    BridgeVgpuTransfer341FingerprintDumpFailure = ERROR_INSUFFICIENT_BUFFER;
    return;
  }
  const auto file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    BridgeVgpuTransfer341FingerprintDumpFailure = GetLastError();
    return;
  }
  DWORD bytesWritten = 0;
  const auto written = WriteFile(file, bytes, static_cast<DWORD>(byteCount),
                                 &bytesWritten, nullptr);
  const auto error = written != FALSE ? ERROR_SUCCESS : GetLastError();
  CloseHandle(file);
  BridgeVgpuTransfer341FingerprintDumpFailure =
      written != FALSE && bytesWritten == byteCount ? ERROR_SUCCESS
      : written != FALSE                            ? ERROR_WRITE_FAULT
                                                    : error;
}

void DumpTransfer341ComputeBindFingerprint(
    const ComputePipelineFingerprintRecord &record) noexcept {
  char path[384]{};
  const auto pathLength =
      std::snprintf(path, std::size(path),
                    "D:\\Games\\AC6 shit\\XeO3-AC6-lab\\ProbeLogs\\"
                    "ac6-compute-bind-fingerprints-%lu.jsonl",
                    GetCurrentProcessId());
  if (pathLength <= 0 ||
      static_cast<std::size_t>(pathLength) >= std::size(path)) {
    BridgeVgpuTransfer341FingerprintDumpFailure = ERROR_INSUFFICIENT_BUFFER;
    return;
  }

  char line[1024]{};
  const auto lineLength = std::snprintf(
      line, std::size(line),
      "{\"xeo3_ac6\":\"vgpu_patch\","
      "\"event\":\"compute_root0_bind_fingerprint\","
      "\"candidate_id\":%u,\"bind_count\":%" PRIu64 ","
      "\"pipeline_state\":\"0x%016" PRIXPTR "\","
      "\"root_signature\":\"0x%016" PRIXPTR "\","
      "\"gpu_descriptor\":\"0x%016" PRIX64 "\","
      "\"cpu_descriptor\":\"0x%016" PRIXPTR "\","
      "\"descriptor0\":\"0x%016" PRIX64 "\","
      "\"descriptor1\":\"0x%016" PRIX64 "\","
      "\"descriptor2\":\"0x%016" PRIX64 "\","
      "\"descriptor3\":\"0x%016" PRIX64 "\",\"tid\":%lu}\n",
      record.candidateId, record.bindCount,
      reinterpret_cast<std::uintptr_t>(record.pipelineState),
      record.lastRootSignature, record.lastGpuDescriptor,
      record.lastCpuDescriptor, record.lastDescriptorWords[0],
      record.lastDescriptorWords[1], record.lastDescriptorWords[2],
      record.lastDescriptorWords[3], GetCurrentThreadId());
  if (lineLength <= 0 ||
      static_cast<std::size_t>(lineLength) >= std::size(line)) {
    BridgeVgpuTransfer341FingerprintDumpFailure = ERROR_INSUFFICIENT_BUFFER;
    return;
  }

  const auto file =
      CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                  nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    BridgeVgpuTransfer341FingerprintDumpFailure = GetLastError();
    return;
  }
  DWORD bytesWritten = 0;
  const auto written = WriteFile(file, line, static_cast<DWORD>(lineLength),
                                 &bytesWritten, nullptr);
  const auto error = written != FALSE ? ERROR_SUCCESS : GetLastError();
  CloseHandle(file);
  BridgeVgpuTransfer341FingerprintDumpFailure =
      written != FALSE && bytesWritten == static_cast<DWORD>(lineLength)
          ? ERROR_SUCCESS
      : written != FALSE ? ERROR_WRITE_FAULT
                         : error;
}

void RecordTransfer341ComputeBindFingerprint(
    ID3D12PipelineState *const pipelineState,
    ID3D12RootSignature *const rootSignature,
    const D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptor,
    const std::uintptr_t cpuDescriptor,
    const std::array<std::uint64_t, 4> &descriptorWords) noexcept {
  if (pipelineState == nullptr) {
    return;
  }

  ComputePipelineFingerprintRecord snapshot{};
  bool shouldDump = false;
  {
    std::scoped_lock lock(g_computePipelineFingerprintMutex);
    const auto end =
        g_computePipelineFingerprints.begin() +
        static_cast<std::ptrdiff_t>(g_computePipelineFingerprintCount);
    const auto found = std::find_if(
        g_computePipelineFingerprints.begin(), end,
        [pipelineState](const ComputePipelineFingerprintRecord &record) {
          return record.pipelineState == pipelineState;
        });
    if (found == end) {
      return;
    }
    found->lastRootSignature = reinterpret_cast<std::uintptr_t>(rootSignature);
    found->lastGpuDescriptor = gpuDescriptor.ptr;
    found->lastCpuDescriptor = cpuDescriptor;
    found->lastDescriptorWords = descriptorWords;
    shouldDump = found->bindCount <= 4 ||
                 (found->bindCount & (found->bindCount - 1)) == 0;
    if (shouldDump) {
      snapshot = *found;
    }
  }
  if (shouldDump) {
    DumpTransfer341ComputeBindFingerprint(snapshot);
  }
}

bool IsAc6Pso341PipelineState(
    ID3D12PipelineState *const pipelineState) noexcept {
  if (pipelineState == nullptr) {
    return false;
  }

  std::scoped_lock lock(g_computePipelineFingerprintMutex);
  const auto end =
      g_computePipelineFingerprints.begin() +
      static_cast<std::ptrdiff_t>(g_computePipelineFingerprintCount);
  const auto found = std::find_if(
      g_computePipelineFingerprints.begin(), end,
      [pipelineState](const ComputePipelineFingerprintRecord &record) {
        return record.pipelineState == pipelineState;
      });
  if (found != end) {
    ++found->bindCount;
    BridgeVgpuTransfer341CachedPsoCacheHitCount =
        g_transfer341CachedPsoCacheHitCount.fetch_add(
            1, std::memory_order_relaxed) +
        1;
    return found->matchesPso341;
  }
  if (g_computePipelineFingerprintCount >=
      g_computePipelineFingerprints.size()) {
    BridgeVgpuTransfer341ReplacementFailure = ERROR_INSUFFICIENT_BUFFER;
    return false;
  }

  ComputePipelineFingerprintRecord fingerprint{};
  fingerprint.pipelineState = pipelineState;
  fingerprint.candidateId =
      static_cast<std::uint32_t>(g_computePipelineFingerprintCount + 1);
  fingerprint.bindCount = 1;
  ID3DBlob *cachedBlob = nullptr;
  fingerprint.result = pipelineState->GetCachedBlob(&cachedBlob);
  BridgeVgpuTransfer341CachedPsoQueryCount =
      g_transfer341CachedPsoQueryCount.fetch_add(1, std::memory_order_relaxed) +
      1;
  if (SUCCEEDED(fingerprint.result) && cachedBlob != nullptr) {
    fingerprint.cachedBlobSize = cachedBlob->GetBufferSize();
    DumpTransfer341ComputePipelineCachedBlob(
        pipelineState, fingerprint.candidateId, cachedBlob->GetBufferPointer(),
        fingerprint.cachedBlobSize);
    if (!xeo3::vgpu::detail::HashBytesSha256(cachedBlob->GetBufferPointer(),
                                             fingerprint.cachedBlobSize,
                                             fingerprint.cachedBlobSha256)) {
      fingerprint.result = E_FAIL;
      fingerprint.cachedBlobSize = 0;
      fingerprint.cachedBlobSha256.fill(0);
    }
    cachedBlob->Release();
  } else if (SUCCEEDED(fingerprint.result)) {
    fingerprint.result = E_POINTER;
  }
  if (SUCCEEDED(fingerprint.result)) {
    fingerprint.matchesPso341 =
        xeo3::vgpu::detail::MatchesAc6Pso341CachedPipelineBlob(
            fingerprint.cachedBlobSize, fingerprint.cachedBlobSha256);
  }

  BridgeVgpuTransfer341LastCachedPso =
      reinterpret_cast<std::uintptr_t>(pipelineState);
  BridgeVgpuTransfer341LastCachedBlobSize = fingerprint.cachedBlobSize;
  std::array<std::uint64_t, 4> digestWords{};
  std::memcpy(digestWords.data(), fingerprint.cachedBlobSha256.data(),
              fingerprint.cachedBlobSha256.size());
  BridgeVgpuTransfer341LastCachedBlobHash0 = digestWords[0];
  BridgeVgpuTransfer341LastCachedBlobHash1 = digestWords[1];
  BridgeVgpuTransfer341LastCachedBlobHash2 = digestWords[2];
  BridgeVgpuTransfer341LastCachedBlobHash3 = digestWords[3];
  BridgeVgpuTransfer341LastCachedBlobResult =
      static_cast<std::uint32_t>(fingerprint.result);

  pipelineState->AddRef();
  g_computePipelineFingerprints[g_computePipelineFingerprintCount++] =
      fingerprint;
  if (!fingerprint.matchesPso341) {
    return false;
  }

  const auto matchCount =
      g_transfer341CachedPsoMatchCount.fetch_add(1, std::memory_order_relaxed) +
      1;
  BridgeVgpuTransfer341CachedPsoMatchCount = matchCount;
  pipelineState->AddRef();
  PublishOwnedAc6Pso341PipelineState(pipelineState);
  EmitPatchEvent("transfer341_cached_pso_match", fingerprint.candidateId,
                 matchCount);
  return true;
}

ID3D12PipelineState *EnsureAc6Pso341ReplacementPipelineState(
    ID3D12GraphicsCommandList *const commandList) noexcept {
  if (auto *const existing =
          g_ac6Pso341ReplacementPipelineState.load(std::memory_order_acquire)) {
    return existing;
  }
  if (commandList == nullptr) {
    BridgeVgpuTransfer341ReplacementFailure = ERROR_NOT_READY;
    return nullptr;
  }

  std::scoped_lock lock(g_computePipelineReplacementMutex);
  if (auto *const existing =
          g_ac6Pso341ReplacementPipelineState.load(std::memory_order_acquire)) {
    return existing;
  }
  if (g_computePipelineReplacementCreationAttempted) {
    return nullptr;
  }
  g_computePipelineReplacementCreationAttempted = true;

  ID3D12Device *device = nullptr;
  const auto deviceResult = commandList->GetDevice(IID_PPV_ARGS(&device));
  if (FAILED(deviceResult) || device == nullptr) {
    BridgeVgpuTransfer341ReplacementFailure = static_cast<std::uint32_t>(
        FAILED(deviceResult) ? deviceResult : E_POINTER);
    return nullptr;
  }

  std::size_t shaderSize = 0;
  const auto *const shader =
      xeo3::vgpu::detail::GetAc6Pso341WidthFixComputeShader(shaderSize);
  D3D12_COMPUTE_PIPELINE_STATE_DESC description{};
  // The patched DXIL retains PSO341's creation-time RTS0 signature. XeO3
  // binds a distinct compatible command-list signature (PIX root 343), which
  // D3D12 rejects if passed as the PSO creation signature. A null pointer tells
  // D3D12 to use the embedded signature, matching the successful PIX replay.
  description.pRootSignature = nullptr;
  description.CS = {shader, shaderSize};
  description.NodeMask = 0;
  description.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

  ID3D12PipelineState *replacement = nullptr;
  const auto native =
      g_nativeCreateComputePipelineState.load(std::memory_order_acquire);
  const auto createCount = g_transfer341ReplacementCreateCount.fetch_add(
                               1, std::memory_order_relaxed) +
                           1;
  BridgeVgpuTransfer341ReplacementCreateCount = createCount;
  const auto result =
      native != nullptr
          ? native(device, &description, __uuidof(ID3D12PipelineState),
                   reinterpret_cast<void **>(&replacement))
          : E_UNEXPECTED;
  device->Release();
  if (FAILED(result) || replacement == nullptr) {
    if (replacement != nullptr) {
      replacement->Release();
    }
    BridgeVgpuTransfer341ReplacementFailure =
        static_cast<std::uint32_t>(FAILED(result) ? result : E_POINTER);
    EmitPatchEvent("transfer341_replacement_create_failure",
                   BridgeVgpuTransfer341ReplacementFailure, createCount);
    return nullptr;
  }

  g_ac6Pso341ReplacementPipelineState.store(replacement,
                                            std::memory_order_release);
  BridgeVgpuTransfer341ReplacementPipelineState =
      reinterpret_cast<std::uintptr_t>(replacement);
  BridgeVgpuTransfer341ReplacementFailure = ERROR_SUCCESS;
  EmitPatchEvent("transfer341_replacement_created", 341, createCount);
  return replacement;
}

bool ActivateAc6Pso341Replacement(
    GraphicsCommandListHookRecord &hook,
    ID3D12GraphicsCommandList *const commandList) noexcept {
  const auto matchesPso341 = IsAc6Pso341PipelineState(hook.pipelineState);
  if (!matchesPso341 || BridgeVgpuTransfer341PipelineReplacementEnabled == 0) {
    return false;
  }
  auto *const replacement =
      EnsureAc6Pso341ReplacementPipelineState(commandList);
  if (replacement == nullptr || hook.nativeSetPipelineState == nullptr) {
    return false;
  }
  if (hook.activePipelineState != replacement) {
    hook.nativeSetPipelineState(commandList, replacement);
    hook.activePipelineState = replacement;
    const auto substitutionCount =
        g_transfer341ReplacementSubstitutionCount.fetch_add(
            1, std::memory_order_relaxed) +
        1;
    BridgeVgpuTransfer341ReplacementSubstitutionCount = substitutionCount;
    if (substitutionCount <= 64 ||
        (substitutionCount & (substitutionCount - 1)) == 0) {
      EmitPatchEvent("transfer341_replacement_bound", 341, substitutionCount);
    }
  }
  return true;
}

void RegisterMappedUploadBuffer(const std::uint32_t heapType,
                                const bool committed,
                                const std::uint64_t heapOffset,
                                const D3D12_RESOURCE_DIMENSION dimension,
                                const std::uint64_t size,
                                void *const outputResource) noexcept {
  if (outputResource == nullptr) {
    return;
  }

  if (!xeo3::vgpu::detail::ShouldTrackAc6Pso341UploadBuffer(
          heapType, dimension == D3D12_RESOURCE_DIMENSION_BUFFER, size)) {
    return;
  }
  const auto candidateCount =
      g_transfer341ArenaCandidateCount.fetch_add(1, std::memory_order_relaxed) +
      1;
  BridgeVgpuTransfer341ArenaCandidateCount = candidateCount;
  BridgeVgpuTransfer341LastArenaHeapType = heapType;
  if (committed) {
    BridgeVgpuTransfer341CommittedArenaCandidateCount =
        g_transfer341CommittedArenaCandidateCount.fetch_add(
            1, std::memory_order_relaxed) +
        1;
  }

  ID3D12Resource *resource = nullptr;
  const auto queryResult = reinterpret_cast<IUnknown *>(outputResource)
                               ->QueryInterface(IID_PPV_ARGS(&resource));
  if (FAILED(queryResult) || resource == nullptr) {
    return;
  }

  {
    std::scoped_lock lock(g_transfer341MappingMutex);
    const auto existing =
        std::find_if(g_mappedUploadBuffers.begin(),
                     g_mappedUploadBuffers.begin() + g_mappedUploadBufferCount,
                     [resource](const MappedUploadBufferRecord &record) {
                       return record.resource == resource;
                     });
    if (existing != g_mappedUploadBuffers.begin() + g_mappedUploadBufferCount) {
      resource->Release();
      return;
    }
  }

  D3D12_RANGE noCpuReads{0, 0};
  void *mapped = nullptr;
  const auto mapResult = resource->Map(0, &noCpuReads, &mapped);
  BridgeVgpuTransfer341LastArenaMapResult =
      static_cast<std::uint32_t>(mapResult);
  const auto gpuBase = resource->GetGPUVirtualAddress();
  if (FAILED(mapResult) || mapped == nullptr || gpuBase == 0) {
    resource->Release();
    return;
  }

  std::scoped_lock lock(g_transfer341MappingMutex);
  if (g_mappedUploadBufferCount >= g_mappedUploadBuffers.size()) {
    resource->Unmap(0, nullptr);
    resource->Release();
    BridgeVgpuTransfer341WidthFailureCount =
        g_transfer341WidthFailureCount.fetch_add(1, std::memory_order_relaxed) +
        1;
    EmitPatchEvent("transfer341_mapped_buffer_overflow",
                   ERROR_INSUFFICIENT_BUFFER, g_mappedUploadBufferCount);
    return;
  }

  g_mappedUploadBuffers[g_mappedUploadBufferCount++] = {
      resource, static_cast<std::uint8_t *>(mapped), gpuBase, size};
  BridgeVgpuTransfer341MappedBufferCount = g_mappedUploadBufferCount;
  BridgeVgpuTransfer341LastMappedGpuBase = gpuBase;
  BridgeVgpuTransfer341LastMappedBufferSize = size;
  if (g_mappedUploadBufferCount <= 16) {
    EmitPatchEvent("transfer341_mapped_upload_buffer",
                   static_cast<std::uint32_t>(heapOffset),
                   g_mappedUploadBufferCount);
  }
}

void ReleaseMappedUploadBuffers() noexcept {
  std::scoped_lock lock(g_transfer341MappingMutex);
  for (std::size_t index = 0; index < g_mappedUploadBufferCount; ++index) {
    auto &record = g_mappedUploadBuffers[index];
    if (record.resource != nullptr) {
      record.resource->Unmap(0, nullptr);
      record.resource->Release();
    }
    record = {};
  }
  g_mappedUploadBufferCount = 0;
  BridgeVgpuTransfer341MappedBufferCount = 0;
  g_uploadContexts = {};
  g_uploadContextCount = 0;
  BridgeVgpuTransfer341UploadContextCount = 0;
  g_constantUploadContexts = {};
  g_constantUploadContextCount = 0;
  BridgeVgpuConstantUploadContextCount = 0;
  g_constantUploadContextGeneration.fetch_add(1, std::memory_order_release);
  g_constantBufferDescriptors = {};
}

void EmitPipelineSignature(
    const xeo3::vgpu::GraphicsPipelineSignature &signature,
    const std::uint64_t count) noexcept {
  if (count > 4096) {
    return;
  }

  char vertexHash[65]{};
  char pixelHash[65]{};
  for (std::size_t index = 0; index < signature.vertexShaderSha256.size();
       ++index) {
    std::snprintf(vertexHash + index * 2, 3, "%02X",
                  signature.vertexShaderSha256[index]);
    std::snprintf(pixelHash + index * 2, 3, "%02X",
                  signature.pixelShaderSha256[index]);
  }

  char message[768]{};
  const auto length = std::snprintf(
      message, std::size(message),
      "{\"xeo3_ac6\":\"vgpu_patch\",\"event\":\"pipeline_signature\","
      "\"count\":%" PRIu64 ",\"vs_size\":%" PRIu64 ",\"ps_size\":%" PRIu64
      ",\"sample_count\":%u,"
      "\"rt_count\":%u,\"rt0_format\":%u,\"dsv_format\":%u,"
      "\"write_mask\":%u,\"input_count\":%u,"
      "\"input_layout\":%u,\"fixed_state\":%u,"
      "\"vs_sha256\":\"%s\",\"ps_sha256\":\"%s\"}",
      count, signature.vertexShaderSize, signature.pixelShaderSize,
      signature.sampleCount, signature.renderTargetCount,
      signature.renderTarget0Format, signature.depthStencilFormat,
      signature.renderTarget0WriteMask, signature.inputElementCount,
      signature.hasExpectedInputLayout ? 1U : 0U,
      signature.hasExpectedFixedState ? 1U : 0U, vertexHash, pixelHash);
  if (length > 0) {
    OutputDebugStringA(message);
    OutputDebugStringA("\n");

    // OutputDebugString is useful under a debugger, but the title creates
    // pipelines from worker fibers where a debugger may not be attached.
    // Keep a per-process JSONL trace in the lab so a package-version-specific
    // fingerprint can be derived without changing the installed package.
    char path[512]{};
    const auto pathLength =
        std::snprintf(path, std::size(path),
                      "D:\\Games\\AC6 shit\\XeO3-AC6-lab\\ProbeLogs\\"
                      "ac6-pipeline-signatures-%lu.jsonl",
                      GetCurrentProcessId());
    if (pathLength > 0 &&
        static_cast<std::size_t>(pathLength) < std::size(path)) {
      const auto file = CreateFileA(
          path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
          OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (file != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten = 0;
        const auto byteCount = static_cast<DWORD>(length);
        WriteFile(file, message, byteCount, &bytesWritten, nullptr);
        WriteFile(file, "\n", 1, &bytesWritten, nullptr);
        CloseHandle(file);
      }
    }
  }
}

void EmitPipelineResult(const std::uint64_t count, const HRESULT result,
                        const void *const pipelineState) noexcept {
  if (count > 4096) {
    return;
  }

  char message[256]{};
  const auto length = std::snprintf(
      message, std::size(message),
      "{\"xeo3_ac6\":\"vgpu_patch\",\"event\":\"pipeline_result\","
      "\"count\":%" PRIu64 ",\"result\":\"0x%08X\","
      "\"pipeline_state\":\"0x%016" PRIXPTR "\"}",
      count, static_cast<std::uint32_t>(result),
      reinterpret_cast<std::uintptr_t>(pipelineState));
  if (length <= 0) {
    return;
  }

  OutputDebugStringA(message);
  OutputDebugStringA("\n");
  char path[512]{};
  const auto pathLength =
      std::snprintf(path, std::size(path),
                    "D:\\Games\\AC6 shit\\XeO3-AC6-lab\\ProbeLogs\\"
                    "ac6-pipeline-signatures-%lu.jsonl",
                    GetCurrentProcessId());
  if (pathLength <= 0 ||
      static_cast<std::size_t>(pathLength) >= std::size(path)) {
    return;
  }
  const auto file =
      CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                  nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return;
  }
  DWORD bytesWritten = 0;
  WriteFile(file, message, static_cast<DWORD>(length), &bytesWritten, nullptr);
  WriteFile(file, "\n", 1, &bytesWritten, nullptr);
  CloseHandle(file);
}

void EmitDrawRecordSignature(const xeo3::vgpu::DrawRecordSignature &signature,
                             const std::uintptr_t record,
                             const std::uintptr_t commandList,
                             const std::uintptr_t commandContext,
                             const std::uintptr_t pipelineState,
                             const std::uint64_t count) noexcept {
  char message[768]{};
  const auto length = std::snprintf(
      message, std::size(message),
      "{\"xeo3_ac6\":\"vgpu_patch\",\"event\":\"draw_record\","
      "\"count\":%" PRIu64 ",\"record\":\"0x%016" PRIXPTR "\","
      "\"command_list\":\"0x%016" PRIXPTR "\","
      "\"command_context\":\"0x%016" PRIXPTR "\","
      "\"root_signature\":\"0x%016" PRIXPTR "\","
      "\"pipeline_state\":\"0x%016" PRIXPTR "\","
      "\"record_pipeline_state\":\"0x%016" PRIXPTR "\","
      "\"viewport_width_bits\":\"0x%08X\","
      "\"viewport_height_bits\":\"0x%08X\","
      "\"viewport_min_depth_bits\":\"0x%08X\","
      "\"viewport_max_depth_bits\":\"0x%08X\","
      "\"viewport_top_left_x_bits\":\"0x%08X\","
      "\"viewport_top_left_y_bits\":\"0x%08X\","
      "\"scissor_right\":%u,\"scissor_bottom\":%u,"
      "\"record_kind\":%u,\"vertex_count\":%u,"
      "\"start_vertex\":%u}",
      count, record, commandList, commandContext, signature.rootSignature,
      pipelineState, signature.pipelineState, signature.viewportWidthBits,
      signature.viewportHeightBits, signature.viewportMinDepthBits,
      signature.viewportMaxDepthBits, signature.viewportTopLeftXBits,
      signature.viewportTopLeftYBits, signature.scissorRight,
      signature.scissorBottom, signature.recordKind, signature.vertexCount,
      signature.startVertex);
  if (length <= 0) {
    return;
  }

  OutputDebugStringA(message);
  OutputDebugStringA("\n");
  char path[512]{};
  const auto pathLength =
      std::snprintf(path, std::size(path),
                    "D:\\Games\\AC6 shit\\XeO3-AC6-lab\\ProbeLogs\\"
                    "ac6-draw-records-%lu.jsonl",
                    GetCurrentProcessId());
  if (pathLength <= 0 ||
      static_cast<std::size_t>(pathLength) >= std::size(path)) {
    return;
  }
  const auto file =
      CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                  nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return;
  }
  DWORD bytesWritten = 0;
  WriteFile(file, message, static_cast<DWORD>(length), &bytesWritten, nullptr);
  WriteFile(file, "\n", 1, &bytesWritten, nullptr);
  CloseHandle(file);
}

bool EqualDrawRecordSignature(
    const xeo3::vgpu::DrawRecordSignature &left,
    const xeo3::vgpu::DrawRecordSignature &right) noexcept {
  return left.rootSignature == right.rootSignature &&
         left.pipelineState == right.pipelineState &&
         left.viewportWidthBits == right.viewportWidthBits &&
         left.viewportHeightBits == right.viewportHeightBits &&
         left.viewportMinDepthBits == right.viewportMinDepthBits &&
         left.viewportMaxDepthBits == right.viewportMaxDepthBits &&
         left.viewportTopLeftXBits == right.viewportTopLeftXBits &&
         left.viewportTopLeftYBits == right.viewportTopLeftYBits &&
         left.scissorRight == right.scissorRight &&
         left.scissorBottom == right.scissorBottom &&
         left.recordKind == right.recordKind &&
         left.vertexCount == right.vertexCount &&
         left.startVertex == right.startVertex;
}

void RecordInterestingDraw(const xeo3::vgpu::DrawRecordSignature &signature,
                           const std::uintptr_t record,
                           const std::uintptr_t commandList,
                           const std::uintptr_t commandContext,
                           const std::uintptr_t pipelineState) noexcept {
  constexpr std::uint32_t kViewportWidth1280 = 0x44A00000U;
  constexpr std::uint32_t kViewportHeight720 = 0x44340000U;
  const bool hasTargetViewport =
      signature.viewportWidthBits == kViewportWidth1280 &&
      signature.viewportHeightBits == kViewportHeight720;
  const bool hasHalfWidthScissor = signature.scissorRight == 640;
  const bool hasExpectedHeight =
      signature.scissorBottom == 360 || signature.scissorBottom == 720;
  const bool hasSmallFullscreenDraw =
      signature.recordKind == 0 && signature.vertexCount <= 6;
  if (!hasTargetViewport || !hasHalfWidthScissor || !hasExpectedHeight ||
      !hasSmallFullscreenDraw) {
    return;
  }

  const auto interestingCount =
      g_drawRecordInterestingCount.fetch_add(1, std::memory_order_relaxed) + 1;
  BridgeVgpuDrawRecordInterestingCount = interestingCount;

  bool unique = false;
  {
    std::scoped_lock lock(g_drawRecordTraceMutex);
    const auto end = g_observedDrawRecords.begin() +
                     static_cast<std::ptrdiff_t>(g_observedDrawRecordCount);
    const auto found = std::find_if(
        g_observedDrawRecords.begin(), end,
        [&signature, pipelineState](const ObservedDrawRecord &observed) {
          return observed.pipelineState == pipelineState &&
                 EqualDrawRecordSignature(observed.signature, signature);
        });
    if (found == end &&
        g_observedDrawRecordCount < g_observedDrawRecords.size()) {
      g_observedDrawRecords[g_observedDrawRecordCount++] = {
          signature, pipelineState, commandList, commandContext};
      unique = true;
    }
  }
  if (!unique) {
    return;
  }

  const auto uniqueCount =
      g_drawRecordUniqueCount.fetch_add(1, std::memory_order_relaxed) + 1;
  BridgeVgpuDrawRecordUniqueCount = uniqueCount;
  EmitDrawRecordSignature(signature, record, commandList, commandContext,
                          pipelineState, uniqueCount);
}

bool RegisterPrimitiveRestartPipelineState(
    const void *const pipelineState,
    const xeo3::vgpu::GraphicsPipelineSignature &signature) noexcept {
  if (pipelineState == nullptr) {
    return false;
  }

  const auto classification =
      xeo3::vgpu::detail::ClassifyAc6PrimitiveRestartPipeline(signature);
  if (classification == xeo3::vgpu::Ac6PrimitiveRestartPipeline::None) {
    return false;
  }

  auto &slots =
      classification == xeo3::vgpu::Ac6PrimitiveRestartPipeline::TerrainFan
          ? g_restartTerrainPipelineStates
          : g_restartSkyPipelineStates;
  for (auto &slot : slots) {
    auto *observed = slot.load(std::memory_order_acquire);
    if (observed == pipelineState) {
      return true;
    }
    if (observed != nullptr) {
      continue;
    }
    if (!slot.compare_exchange_strong(observed, pipelineState,
                                      std::memory_order_release,
                                      std::memory_order_acquire)) {
      if (observed == pipelineState) {
        return true;
      }
      continue;
    }

    std::uint64_t count = 0;
    if (classification ==
        xeo3::vgpu::Ac6PrimitiveRestartPipeline::TerrainFan) {
      count = g_restartTerrainPipelineCount.fetch_add(
                  1, std::memory_order_relaxed) +
              1;
      BridgeVgpuRestartTerrainPipelineCount = count;
    } else {
      count =
          g_restartSkyPipelineCount.fetch_add(1, std::memory_order_relaxed) + 1;
      BridgeVgpuRestartSkyPipelineCount = count;
    }
    EmitPatchEvent("primitive_restart_pipeline_registered",
                   static_cast<std::uint32_t>(classification),
                   reinterpret_cast<std::uintptr_t>(pipelineState));
    return true;
  }

  const auto overflowCount =
      g_restartPipelineOverflowCount.fetch_add(1, std::memory_order_relaxed) +
      1;
  BridgeVgpuRestartPipelineOverflowCount = overflowCount;
  EmitPatchEvent("primitive_restart_pipeline_registry_overflow",
                 static_cast<std::uint32_t>(classification), overflowCount);
  return false;
}

xeo3::vgpu::Ac6PrimitiveRestartPipeline
FindPrimitiveRestartPipelineState(const void *const pipelineState) noexcept {
  if (pipelineState == nullptr) {
    return xeo3::vgpu::Ac6PrimitiveRestartPipeline::None;
  }
  for (const auto &slot : g_restartTerrainPipelineStates) {
    if (slot.load(std::memory_order_acquire) == pipelineState) {
      return xeo3::vgpu::Ac6PrimitiveRestartPipeline::TerrainFan;
    }
  }
  for (const auto &slot : g_restartSkyPipelineStates) {
    if (slot.load(std::memory_order_acquire) == pipelineState) {
      return xeo3::vgpu::Ac6PrimitiveRestartPipeline::SkyStrip;
    }
  }
  return xeo3::vgpu::Ac6PrimitiveRestartPipeline::None;
}

struct Ac6BufferStateConstants {
  std::int32_t vertexOffset = 0;
  std::uint32_t useIndexBuffer = 0;
  std::uint32_t indexCount = 0;
  std::uint32_t vfetchEndianness = 0;
  std::uint32_t packedIbDesc = 0;
  std::uint32_t resetIndex = 0;
  std::uint32_t ibBase = 0;
};
static_assert(sizeof(Ac6BufferStateConstants) == 7 * sizeof(std::uint32_t));

bool ResolveAc6BufferStateConstants(
    const GraphicsCommandListHookRecord &hook,
    Ac6BufferStateConstants &constants,
    D3D12_GPU_VIRTUAL_ADDRESS &gpuAddress,
    std::uintptr_t &cpuAddress) noexcept {
  constants = {};
  gpuAddress = 0;
  cpuAddress = 0;
  BridgeVgpuRestartLastResolveFailure = 0;
  BridgeVgpuRestartLastRootDescriptorTable = 0;
  BridgeVgpuRestartLastDescriptorHeapGpuStart = 0;
  BridgeVgpuRestartLastDescriptorHeapCpuStart = 0;
  BridgeVgpuRestartLastDescriptorHeapByteSpan = 0;
  BridgeVgpuRestartLastDescriptorHeapIncrement = 0;
  BridgeVgpuRestartLastCpuDescriptor = 0;
  BridgeVgpuRestartLastDescriptorWord0 = 0;
  BridgeVgpuRestartLastDescriptorWord1 = 0;
  BridgeVgpuRestartLastDecodedGpuAddress = 0;
  BridgeVgpuRestartLastUploadContextKind = 0;
  constexpr std::size_t kBufferStateRootParameter = 0;
  constexpr std::uint64_t kBufferStateDescriptorOffset = 5;
  if ((hook.graphicsRootDescriptorTableMask &
       (std::uint64_t{1} << kBufferStateRootParameter)) == 0) {
    BridgeVgpuRestartLastResolveFailure = 1;
    return false;
  }

  const auto tableGpuAddress =
      hook.graphicsRootDescriptorTables[kBufferStateRootParameter];
  BridgeVgpuRestartLastRootDescriptorTable = tableGpuAddress;
  if (tableGpuAddress == 0) {
    BridgeVgpuRestartLastResolveFailure = 2;
    return false;
  }

  std::scoped_lock lock(g_transfer341MappingMutex);
  const auto trackedHeapCount =
      (std::min)(static_cast<std::size_t>(hook.descriptorHeapCount),
                 hook.descriptorHeaps.size());
  bool descriptorHeapMatched = false;
  for (std::size_t heapIndex = 0; heapIndex < trackedHeapCount; ++heapIndex) {
    const auto heapGpuStart = hook.descriptorHeapGpuStarts[heapIndex];
    const auto heapCpuStart = hook.descriptorHeapCpuStarts[heapIndex];
    const auto heapByteSpan = hook.descriptorHeapByteSpans[heapIndex];
    const auto descriptorIncrement = hook.descriptorHeapIncrements[heapIndex];
    if (heapGpuStart == 0 || heapCpuStart == 0 || heapByteSpan == 0 ||
        descriptorIncrement == 0 || tableGpuAddress < heapGpuStart) {
      continue;
    }
    const auto tableDelta = tableGpuAddress - heapGpuStart;
    const auto bufferStateDelta =
        kBufferStateDescriptorOffset * descriptorIncrement;
    if (tableDelta >= heapByteSpan ||
        bufferStateDelta > heapByteSpan - tableDelta) {
      continue;
    }
    const auto descriptorDelta = tableDelta + bufferStateDelta;
    if (descriptorDelta >= heapByteSpan ||
        descriptorDelta >
            (std::numeric_limits<std::uintptr_t>::max)() - heapCpuStart) {
      continue;
    }
    const auto cpuDescriptor =
        heapCpuStart + static_cast<std::uintptr_t>(descriptorDelta);
    descriptorHeapMatched = true;
    BridgeVgpuRestartLastDescriptorHeapGpuStart = heapGpuStart;
    BridgeVgpuRestartLastDescriptorHeapCpuStart = heapCpuStart;
    BridgeVgpuRestartLastDescriptorHeapByteSpan = heapByteSpan;
    BridgeVgpuRestartLastDescriptorHeapIncrement = descriptorIncrement;
    BridgeVgpuRestartLastCpuDescriptor = cpuDescriptor;
    const auto &descriptorRecord =
        g_constantBufferDescriptors[ConstantBufferDescriptorIndex(
            cpuDescriptor)];
    if (descriptorRecord.cpuDescriptor == cpuDescriptor) {
      gpuAddress = descriptorRecord.gpuAddress;
    }
    std::array<std::uint64_t, 4> descriptorWords{};
    if (IsAccessibleMemoryRange(reinterpret_cast<const void *>(cpuDescriptor),
                                sizeof(descriptorWords), false)) {
      std::memcpy(descriptorWords.data(),
                  reinterpret_cast<const void *>(cpuDescriptor),
                  sizeof(descriptorWords));
      BridgeVgpuRestartLastDescriptorWord0 = descriptorWords[0];
      BridgeVgpuRestartLastDescriptorWord1 = descriptorWords[1];
      const auto decodedGpuAddress =
          xeo3::vgpu::detail::DecodeAmdConstantBufferGpuAddress(
              descriptorWords);
      BridgeVgpuRestartLastDecodedGpuAddress = decodedGpuAddress;
      if (gpuAddress == 0) {
        gpuAddress = decodedGpuAddress;
      }
    }
    break;
  }
  if (!descriptorHeapMatched) {
    BridgeVgpuRestartLastResolveFailure = 3;
    return false;
  }
  if (gpuAddress == 0) {
    BridgeVgpuRestartLastResolveFailure = 4;
    return false;
  }

  const std::uint8_t *source = nullptr;
  for (std::size_t index = 0; index < g_mappedUploadBufferCount; ++index) {
    const auto &mapping = g_mappedUploadBuffers[index];
    if (mapping.cpuBase == nullptr || gpuAddress < mapping.gpuBase) {
      continue;
    }
    const auto offset = gpuAddress - mapping.gpuBase;
    if (offset <= mapping.size &&
        sizeof(constants) <= mapping.size - offset) {
      source = mapping.cpuBase + offset;
      BridgeVgpuRestartLastUploadContextKind = 1;
      break;
    }
  }
  if (source == nullptr) {
    for (std::size_t index = 0; index < g_constantUploadContextCount; ++index) {
      const auto &uploadContext = g_constantUploadContexts[index];
      if (uploadContext.cpuBase == nullptr ||
          gpuAddress < uploadContext.gpuBase) {
        continue;
      }
      const auto offset = gpuAddress - uploadContext.gpuBase;
      if (offset <= uploadContext.mappedSpan &&
          sizeof(constants) <= uploadContext.mappedSpan - offset) {
        source = uploadContext.cpuBase + offset;
        BridgeVgpuRestartLastUploadContextKind = 2;
        break;
      }
    }
  }
  if (source == nullptr) {
    for (std::size_t index = 0; index < g_uploadContextCount; ++index) {
      const auto &uploadContext = g_uploadContexts[index];
      if (uploadContext.cpuBase == nullptr ||
          gpuAddress < uploadContext.gpuBase) {
        continue;
      }
      const auto offset = gpuAddress - uploadContext.gpuBase;
      if (offset <= uploadContext.mappedSpan &&
          sizeof(constants) <= uploadContext.mappedSpan - offset) {
        source = uploadContext.cpuBase + offset;
        BridgeVgpuRestartLastUploadContextKind = 3;
        break;
      }
    }
  }
  if (source == nullptr) {
    BridgeVgpuRestartLastResolveFailure = 5;
    return false;
  }
  if (!IsAccessibleMemoryRange(source, sizeof(constants), false)) {
    BridgeVgpuRestartLastResolveFailure = 6;
    return false;
  }

  std::atomic_thread_fence(std::memory_order_acquire);
  std::memcpy(&constants, source, sizeof(constants));
  cpuAddress = reinterpret_cast<std::uintptr_t>(source);
  return true;
}

std::uint32_t UpdateMaximum(std::atomic<std::uint32_t> &maximum,
                            const std::uint32_t value) noexcept {
  auto observed = maximum.load(std::memory_order_relaxed);
  while (observed < value &&
         !maximum.compare_exchange_weak(observed, value,
                                        std::memory_order_relaxed,
                                        std::memory_order_relaxed)) {
  }
  return observed < value ? value : observed;
}

void RecordPrimitiveRestartDraw(
    const GraphicsCommandListHookRecord &hook,
    const xeo3::vgpu::Ac6PrimitiveRestartPipeline classification,
    const void *const pipelineState, const std::uint64_t hostDrawCount,
    const UINT vertexCountPerInstance, const UINT instanceCount,
    const UINT startVertexLocation, const UINT startInstanceLocation) noexcept {
  if (classification == xeo3::vgpu::Ac6PrimitiveRestartPipeline::None) {
    return;
  }

  std::uint64_t restartDrawCount = 0;
  if (classification ==
      xeo3::vgpu::Ac6PrimitiveRestartPipeline::TerrainFan) {
    restartDrawCount =
        g_restartTerrainDrawCount.fetch_add(1, std::memory_order_relaxed) + 1;
    BridgeVgpuRestartTerrainDrawCount = restartDrawCount;
    BridgeVgpuRestartTerrainLastVertexCount = vertexCountPerInstance;
    BridgeVgpuRestartTerrainLastStartVertex = startVertexLocation;
    BridgeVgpuRestartTerrainMaxStartVertex =
        UpdateMaximum(g_restartTerrainMaxStartVertex, startVertexLocation);
    if (startVertexLocation == 0) {
      BridgeVgpuRestartTerrainStartVertexZeroCount =
          g_restartTerrainStartVertexZeroCount.fetch_add(
              1, std::memory_order_relaxed) +
          1;
    } else {
      BridgeVgpuRestartTerrainStartVertexNonZeroCount =
          g_restartTerrainStartVertexNonZeroCount.fetch_add(
              1, std::memory_order_relaxed) +
          1;
    }
  } else {
    restartDrawCount =
        g_restartSkyDrawCount.fetch_add(1, std::memory_order_relaxed) + 1;
    BridgeVgpuRestartSkyDrawCount = restartDrawCount;
    BridgeVgpuRestartSkyLastVertexCount = vertexCountPerInstance;
    BridgeVgpuRestartSkyLastStartVertex = startVertexLocation;
    BridgeVgpuRestartSkyMaxStartVertex =
        UpdateMaximum(g_restartSkyMaxStartVertex, startVertexLocation);
    if (startVertexLocation == 0) {
      BridgeVgpuRestartSkyStartVertexZeroCount =
          g_restartSkyStartVertexZeroCount.fetch_add(
              1, std::memory_order_relaxed) +
          1;
    } else {
      BridgeVgpuRestartSkyStartVertexNonZeroCount =
          g_restartSkyStartVertexNonZeroCount.fetch_add(
              1, std::memory_order_relaxed) +
          1;
    }
  }

  BridgeVgpuRestartLastClassification =
      static_cast<std::uint32_t>(classification);
  BridgeVgpuRestartLastPipelineState =
      reinterpret_cast<std::uintptr_t>(pipelineState);
  BridgeVgpuRestartLastInstanceCount = instanceCount;
  BridgeVgpuRestartLastStartInstance = startInstanceLocation;
  BridgeVgpuRestartLastThreadId = GetCurrentThreadId();
  BridgeVgpuRestartLastDrawCall = hostDrawCount;

  Ac6BufferStateConstants constants{};
  D3D12_GPU_VIRTUAL_ADDRESS constantGpuAddress = 0;
  std::uintptr_t constantCpuAddress = 0;
  if (ResolveAc6BufferStateConstants(hook, constants, constantGpuAddress,
                                    constantCpuAddress)) {
    const auto resolveCount =
        g_restartConstantResolveCount.fetch_add(1, std::memory_order_relaxed) +
        1;
    BridgeVgpuRestartConstantResolveCount = resolveCount;
    BridgeVgpuRestartLastConstantGpuAddress = constantGpuAddress;
    BridgeVgpuRestartLastConstantCpuAddress = constantCpuAddress;
    std::uint32_t vertexOffsetBits = 0;
    std::memcpy(&vertexOffsetBits, &constants.vertexOffset,
                sizeof(vertexOffsetBits));
    BridgeVgpuRestartLastVertexOffsetBits = vertexOffsetBits;
    BridgeVgpuRestartLastUseIndexBuffer = constants.useIndexBuffer;
    BridgeVgpuRestartLastIndexCount = constants.indexCount;
    BridgeVgpuRestartLastVfetchEndianness = constants.vfetchEndianness;
    BridgeVgpuRestartLastPackedIbDesc = constants.packedIbDesc;
    BridgeVgpuRestartLastResetIndex = constants.resetIndex;
    BridgeVgpuRestartLastIbBase = constants.ibBase;
    if (classification ==
        xeo3::vgpu::Ac6PrimitiveRestartPipeline::TerrainFan) {
      BridgeVgpuRestartTerrainLastVertexOffsetBits = vertexOffsetBits;
      BridgeVgpuRestartTerrainLastIndexCount = constants.indexCount;
      BridgeVgpuRestartTerrainLastPackedIbDesc = constants.packedIbDesc;
    } else {
      BridgeVgpuRestartSkyLastVertexOffsetBits = vertexOffsetBits;
      BridgeVgpuRestartSkyLastIndexCount = constants.indexCount;
      BridgeVgpuRestartSkyLastPackedIbDesc = constants.packedIbDesc;
    }
    if (startVertexLocation == vertexOffsetBits) {
      BridgeVgpuRestartStartMatchesVertexOffsetCount =
          g_restartStartMatchesVertexOffsetCount.fetch_add(
              1, std::memory_order_relaxed) +
          1;
    } else {
      BridgeVgpuRestartStartMismatchesVertexOffsetCount =
          g_restartStartMismatchesVertexOffsetCount.fetch_add(
              1, std::memory_order_relaxed) +
          1;
    }
    if (resolveCount <= 64 || (resolveCount & (resolveCount - 1)) == 0) {
      const auto packedConstants =
          (static_cast<std::uint64_t>(constants.indexCount) << 32) |
          vertexOffsetBits;
      EmitPatchEvent("primitive_restart_constants", constants.packedIbDesc,
                     packedConstants);
    }
  } else {
    BridgeVgpuRestartConstantResolveFailureCount =
        g_restartConstantResolveFailureCount.fetch_add(
            1, std::memory_order_relaxed) +
        1;
  }

  if (restartDrawCount <= 64 ||
      (restartDrawCount & (restartDrawCount - 1)) == 0) {
    const auto packedArguments =
        (static_cast<std::uint64_t>(startVertexLocation) << 32) |
        vertexCountPerInstance;
    EmitPatchEvent("primitive_restart_draw",
                   static_cast<std::uint32_t>(classification),
                   packedArguments);
  }
}

void RecordObservedPipelineState(
    const void *const pipelineState,
    const xeo3::vgpu::GraphicsPipelineSignature &signature,
    const std::uint64_t createCount) noexcept {
  if (pipelineState == nullptr) {
    return;
  }

  RegisterPrimitiveRestartPipelineState(pipelineState, signature);

  std::scoped_lock lock(g_pipelineObservationMutex);
  const auto end = g_observedPipelineStates.begin() +
                   static_cast<std::ptrdiff_t>(g_observedPipelineStateCount);
  const auto found =
      std::find_if(g_observedPipelineStates.begin(), end,
                   [pipelineState](const ObservedPipelineState &observed) {
                     return observed.pipelineState == pipelineState;
                   });
  if (found != end ||
      g_observedPipelineStateCount >= g_observedPipelineStates.size()) {
    return;
  }
  g_observedPipelineStates[g_observedPipelineStateCount++] = {
      pipelineState, signature, createCount};
}

bool FindObservedPipelineState(const void *const pipelineState,
                               ObservedPipelineState &observed) noexcept {
  observed = {};
  if (pipelineState == nullptr) {
    return false;
  }

  std::scoped_lock lock(g_pipelineObservationMutex);
  const auto end = g_observedPipelineStates.begin() +
                   static_cast<std::ptrdiff_t>(g_observedPipelineStateCount);
  const auto found =
      std::find_if(g_observedPipelineStates.begin(), end,
                   [pipelineState](const ObservedPipelineState &entry) {
                     return entry.pipelineState == pipelineState;
                   });
  if (found == end) {
    return false;
  }
  observed = *found;
  return true;
}

bool IsNativeHalfWidthSmallFullscreenDraw(
    const xeo3::vgpu::DrawRecordSignature &signature,
    const void *const pipelineState) noexcept {
  if (pipelineState == nullptr) {
    return false;
  }
  const auto activePipelineState =
      reinterpret_cast<std::uintptr_t>(pipelineState);
  return signature.rootSignature != 0 &&
         (signature.pipelineState == 0 ||
          signature.pipelineState == activePipelineState) &&
         signature.viewportWidthBits == 0x44A00000U &&
         signature.viewportHeightBits == 0x44340000U &&
         signature.viewportMinDepthBits == 0 &&
         signature.viewportMaxDepthBits == 0x3F800000U &&
         signature.viewportTopLeftXBits == 0 &&
         signature.viewportTopLeftYBits == 0 && signature.scissorRight == 640 &&
         signature.scissorBottom == 720 && signature.recordKind == 0 &&
         signature.vertexCount != 0 && signature.vertexCount <= 6 &&
         signature.startVertex == 0;
}

void EmitEdramRestoreExperimentCandidate(
    const EdramRestoreExperimentCandidate &candidate,
    const HRESULT cachedBlobResult) noexcept {
  char vertexHash[65]{};
  char pixelHash[65]{};
  char blobHash[65]{};
  for (std::size_t index = 0;
       index < candidate.signature.vertexShaderSha256.size(); ++index) {
    std::snprintf(vertexHash + index * 2, 3, "%02X",
                  candidate.signature.vertexShaderSha256[index]);
    std::snprintf(pixelHash + index * 2, 3, "%02X",
                  candidate.signature.pixelShaderSha256[index]);
    std::snprintf(blobHash + index * 2, 3, "%02X",
                  candidate.cachedBlobSha256[index]);
  }

  char message[768]{};
  const auto length = std::snprintf(
      message, std::size(message),
      "{\"xeo3_ac6\":\"vgpu_patch\","
      "\"event\":\"edram_restore_experiment_candidate\","
      "\"candidate_id\":%u,\"create_count\":%" PRIu64 ","
      "\"pipeline_state\":\"0x%016" PRIXPTR "\","
      "\"vs_size\":%" PRIu64 ",\"ps_size\":%" PRIu64 ","
      "\"vs_sha256\":\"%s\",\"ps_sha256\":\"%s\","
      "\"cached_blob_result\":\"0x%08X\","
      "\"cached_blob_size\":%" PRIu64 ","
      "\"cached_blob_sha256\":\"%s\"}",
      candidate.candidateId, candidate.createCount,
      reinterpret_cast<std::uintptr_t>(candidate.pipelineState),
      candidate.signature.vertexShaderSize, candidate.signature.pixelShaderSize,
      vertexHash, pixelHash, static_cast<std::uint32_t>(cachedBlobResult),
      candidate.cachedBlobSize, blobHash);
  if (length <= 0) {
    return;
  }

  OutputDebugStringA(message);
  OutputDebugStringA("\n");
  char path[512]{};
  const auto pathLength =
      std::snprintf(path, std::size(path),
                    "D:\\Games\\AC6 shit\\XeO3-AC6-lab\\ProbeLogs\\"
                    "ac6-edram-candidates-%lu.jsonl",
                    GetCurrentProcessId());
  if (pathLength <= 0 ||
      static_cast<std::size_t>(pathLength) >= std::size(path)) {
    return;
  }
  const auto file =
      CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                  nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return;
  }
  DWORD bytesWritten = 0;
  WriteFile(file, message, static_cast<DWORD>(length), &bytesWritten, nullptr);
  WriteFile(file, "\n", 1, &bytesWritten, nullptr);
  CloseHandle(file);
}

std::uint32_t RegisterEdramRestoreExperimentCandidate(
    const void *const pipelineState) noexcept {
  if (pipelineState == nullptr) {
    return 0;
  }

  {
    std::scoped_lock lock(g_pipelineObservationMutex);
    const auto end =
        g_edramRestoreExperimentCandidates.begin() +
        static_cast<std::ptrdiff_t>(g_edramRestoreExperimentCandidateCount);
    const auto found = std::find_if(
        g_edramRestoreExperimentCandidates.begin(), end,
        [pipelineState](const EdramRestoreExperimentCandidate &candidate) {
          return candidate.pipelineState == pipelineState;
        });
    if (found != end) {
      return found->candidateId;
    }
  }

  ObservedPipelineState observed{};
  if (!FindObservedPipelineState(pipelineState, observed)) {
    return 0;
  }

  EdramRestoreExperimentCandidate candidate{};
  candidate.pipelineState = pipelineState;
  candidate.signature = observed.signature;
  candidate.createCount = observed.createCount;

  HRESULT blobResult = E_POINTER;
  ID3DBlob *cachedBlob = nullptr;
  blobResult =
      static_cast<ID3D12PipelineState *>(const_cast<void *>(pipelineState))
          ->GetCachedBlob(&cachedBlob);
  if (SUCCEEDED(blobResult) && cachedBlob != nullptr) {
    candidate.cachedBlobSize = cachedBlob->GetBufferSize();
    if (!xeo3::vgpu::detail::HashBytesSha256(cachedBlob->GetBufferPointer(),
                                             candidate.cachedBlobSize,
                                             candidate.cachedBlobSha256)) {
      candidate.cachedBlobSize = 0;
      candidate.cachedBlobSha256.fill(0);
      blobResult = E_FAIL;
    }
    cachedBlob->Release();
  }

  {
    std::scoped_lock lock(g_pipelineObservationMutex);
    const auto end =
        g_edramRestoreExperimentCandidates.begin() +
        static_cast<std::ptrdiff_t>(g_edramRestoreExperimentCandidateCount);
    const auto found = std::find_if(
        g_edramRestoreExperimentCandidates.begin(), end,
        [pipelineState](const EdramRestoreExperimentCandidate &entry) {
          return entry.pipelineState == pipelineState;
        });
    if (found != end) {
      return found->candidateId;
    }
    if (g_edramRestoreExperimentCandidateCount >=
        g_edramRestoreExperimentCandidates.size()) {
      return 0;
    }
    candidate.candidateId =
        static_cast<std::uint32_t>(g_edramRestoreExperimentCandidateCount + 1);
    g_edramRestoreExperimentCandidates
        [g_edramRestoreExperimentCandidateCount++] = candidate;
    BridgeVgpuEdramRestoreExperimentCandidateCount =
        static_cast<std::uint32_t>(g_edramRestoreExperimentCandidateCount);
  }

  BridgeVgpuEdramRestoreExperimentLastCandidate = candidate.candidateId;
  BridgeVgpuEdramRestoreExperimentLastCreateCount = candidate.createCount;
  EmitEdramRestoreExperimentCandidate(candidate, blobResult);
  return candidate.candidateId;
}

void AppendDiagnosticLine(const HANDLE file, const char *const format,
                          ...) noexcept {
  if (file == INVALID_HANDLE_VALUE || format == nullptr) {
    return;
  }

  char line[4096]{};
  va_list arguments;
  va_start(arguments, format);
  const auto length = std::vsnprintf(line, std::size(line), format, arguments);
  va_end(arguments);
  if (length <= 0) {
    return;
  }

  const auto byteCount = static_cast<DWORD>(
      (std::min)(static_cast<std::size_t>(length), std::size(line) - 1));
  DWORD bytesWritten = 0;
  WriteFile(file, line, byteCount, &bytesWritten, nullptr);
}

void CaptureD3d12InfoQueue(ID3D12Device *const device,
                           const HANDLE file) noexcept {
  ID3D12InfoQueue *infoQueue = nullptr;
  const auto queryResult = device->QueryInterface(IID_PPV_ARGS(&infoQueue));
  if (FAILED(queryResult) || infoQueue == nullptr) {
    AppendDiagnosticLine(file, "info_queue_query=0x%08X\n",
                         static_cast<std::uint32_t>(queryResult));
    return;
  }

  const auto messageCount =
      infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
  BridgeVgpuD3d12MessageCount = messageCount;
  AppendDiagnosticLine(file, "info_queue_messages=%" PRIu64 "\n", messageCount);

  constexpr std::uint64_t kMaximumMessages = 128;
  const auto firstMessage =
      messageCount > kMaximumMessages ? messageCount - kMaximumMessages : 0;
  for (auto index = firstMessage; index < messageCount; ++index) {
    SIZE_T messageSize = 0;
    auto result = infoQueue->GetMessage(index, nullptr, &messageSize);
    if (FAILED(result) || messageSize == 0 || messageSize > 64 * 1024) {
      AppendDiagnosticLine(file, "message[%" PRIu64 "]_size=0x%08X bytes=%zu\n",
                           index, static_cast<std::uint32_t>(result),
                           messageSize);
      continue;
    }

    auto *const message = static_cast<D3D12_MESSAGE *>(
        HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, messageSize));
    if (message == nullptr) {
      AppendDiagnosticLine(file,
                           "message[%" PRIu64 "]_allocation_failed bytes=%zu\n",
                           index, messageSize);
      break;
    }

    result = infoQueue->GetMessage(index, message, &messageSize);
    if (SUCCEEDED(result)) {
      AppendDiagnosticLine(
          file, "message[%" PRIu64 "] category=%u severity=%u id=%u text=%s\n",
          index, static_cast<std::uint32_t>(message->Category),
          static_cast<std::uint32_t>(message->Severity),
          static_cast<std::uint32_t>(message->ID),
          message->pDescription == nullptr ? "<null>" : message->pDescription);
    } else {
      AppendDiagnosticLine(file, "message[%" PRIu64 "]_read=0x%08X\n", index,
                           static_cast<std::uint32_t>(result));
    }
    HeapFree(GetProcessHeap(), 0, message);
  }

  infoQueue->Release();
}

void CaptureDredDiagnostics(ID3D12Device *const device,
                            const HRESULT observedResult) noexcept {
  constexpr std::uint32_t kDxgiErrorDeviceRemoved = 0x887A0005;
  if (device == nullptr ||
      static_cast<std::uint32_t>(observedResult) != kDxgiErrorDeviceRemoved ||
      g_dredCaptured.exchange(true, std::memory_order_acq_rel)) {
    return;
  }

  BridgeVgpuDeviceRemovedReason =
      static_cast<std::uint32_t>(device->GetDeviceRemovedReason());

  constexpr wchar_t kProbeDirectory[] =
      L"D:\\Games\\AC6 shit\\XeO3-AC6-lab\\ProbeLogs";
  CreateDirectoryW(kProbeDirectory, nullptr);

  wchar_t diagnosticPath[512]{};
  const auto pathLength = swprintf_s(diagnosticPath, std::size(diagnosticPath),
                                     L"%ls\\ac6-dred-%lu.txt", kProbeDirectory,
                                     GetCurrentProcessId());
  if (pathLength <= 0) {
    BridgeVgpuDredCaptureFailure = ERROR_BUFFER_OVERFLOW;
    return;
  }

  const auto file = CreateFileW(
      diagnosticPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    BridgeVgpuDredCaptureFailure = GetLastError();
    return;
  }

  AppendDiagnosticLine(
      file,
      "observed_result=0x%08X device_removed_reason=0x%08X "
      "pid=%lu tid=%lu shaders=%" PRIu64 " extended_fetches=%" PRIu64
      " last_fetch=%u placed_calls=%" PRIu64 "\n",
      static_cast<std::uint32_t>(observedResult), BridgeVgpuDeviceRemovedReason,
      GetCurrentProcessId(), GetCurrentThreadId(),
      BridgeVgpuVertexShaderCompileCount, BridgeVgpuExtendedFetchCount,
      BridgeVgpuLastFetchCount, BridgeVgpuPlacedResourceCallCount);

  CaptureD3d12InfoQueue(device, file);

  ID3D12DeviceRemovedExtendedData2 *dred = nullptr;
  const auto queryResult = device->QueryInterface(IID_PPV_ARGS(&dred));
  AppendDiagnosticLine(file, "dred_query=0x%08X interface=%p\n",
                       static_cast<std::uint32_t>(queryResult),
                       static_cast<void *>(dred));
  if (SUCCEEDED(queryResult) && dred != nullptr) {
    const auto state = dred->GetDeviceState();
    BridgeVgpuDredDeviceState = static_cast<std::uint32_t>(state);

    D3D12_DRED_PAGE_FAULT_OUTPUT2 pageFault{};
    const auto pageFaultResult =
        dred->GetPageFaultAllocationOutput2(&pageFault);
    BridgeVgpuDredPageFaultAddress = pageFault.PageFaultVA;
    AppendDiagnosticLine(file,
                         "dred_state=%u page_fault_result=0x%08X "
                         "page_fault_va=0x%016" PRIX64 " flags=0x%08X\n",
                         BridgeVgpuDredDeviceState,
                         static_cast<std::uint32_t>(pageFaultResult),
                         static_cast<std::uint64_t>(pageFault.PageFaultVA),
                         static_cast<std::uint32_t>(pageFault.PageFaultFlags));

    auto dumpAllocations =
        [file](const char *const label,
               const D3D12_DRED_ALLOCATION_NODE1 *node) noexcept {
          constexpr std::uint32_t kMaximumAllocationNodes = 128;
          for (std::uint32_t index = 0;
               node != nullptr && index < kMaximumAllocationNodes;
               ++index, node = node->pNext) {
            AppendDiagnosticLine(
                file, "%s[%u] type=%u object=%p name=%s wide_name=%p\n", label,
                index, static_cast<std::uint32_t>(node->AllocationType),
                static_cast<const void *>(node->pObject),
                node->ObjectNameA == nullptr ? "<null>" : node->ObjectNameA,
                static_cast<const void *>(node->ObjectNameW));
          }
        };
    dumpAllocations("existing", pageFault.pHeadExistingAllocationNode);
    dumpAllocations("recent_freed", pageFault.pHeadRecentFreedAllocationNode);

    D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 breadcrumbs{};
    const auto breadcrumbResult = dred->GetAutoBreadcrumbsOutput1(&breadcrumbs);
    AppendDiagnosticLine(
        file, "breadcrumbs_result=0x%08X head=%p\n",
        static_cast<std::uint32_t>(breadcrumbResult),
        static_cast<const void *>(breadcrumbs.pHeadAutoBreadcrumbNode));

    constexpr std::uint32_t kMaximumBreadcrumbNodes = 256;
    auto *node = breadcrumbs.pHeadAutoBreadcrumbNode;
    std::uint32_t nodeCount = 0;
    for (; node != nullptr && nodeCount < kMaximumBreadcrumbNodes;
         ++nodeCount, node = node->pNext) {
      const auto last = node->pLastBreadcrumbValue == nullptr
                            ? UINT32_MAX
                            : *node->pLastBreadcrumbValue;
      AppendDiagnosticLine(file,
                           "breadcrumb[%u] list=%p queue=%p count=%u last=%u "
                           "list_name=%s queue_name=%s contexts=%u\n",
                           nodeCount, static_cast<void *>(node->pCommandList),
                           static_cast<void *>(node->pCommandQueue),
                           node->BreadcrumbCount, last,
                           node->pCommandListDebugNameA == nullptr
                               ? "<null>"
                               : node->pCommandListDebugNameA,
                           node->pCommandQueueDebugNameA == nullptr
                               ? "<null>"
                               : node->pCommandQueueDebugNameA,
                           node->BreadcrumbContextsCount);

      if (node->pCommandHistory == nullptr || last == UINT32_MAX ||
          node->BreadcrumbCount == 0) {
        continue;
      }

      const auto firstOperation = last > 12 ? last - 12 : 0;
      const auto finalOperation = (std::min)(node->BreadcrumbCount, last + 5);
      for (auto operation = firstOperation; operation < finalOperation;
           ++operation) {
        const auto op =
            static_cast<std::uint32_t>(node->pCommandHistory[operation]);
        if (operation == last) {
          BridgeVgpuDredLastBreadcrumbOp = op;
        }
        AppendDiagnosticLine(file, "  op[%u]=%u%s\n", operation, op,
                             operation == last ? " <next>" : "");
      }
    }
    BridgeVgpuDredBreadcrumbNodeCount = nodeCount;
    dred->Release();
  }

  FlushFileBuffers(file);
  CloseHandle(file);
  BridgeVgpuDredCaptureFailure = ERROR_SUCCESS;
  EmitPatchEvent("device_removed_dred_capture", BridgeVgpuDredDeviceState,
                 BridgeVgpuDredPageFaultAddress);
}

bool FailInstall(const PatchStatus status) noexcept {
  SetStatus(status);
  EmitPatchEvent("install_failure", static_cast<std::uint32_t>(status));
  return false;
}

bool ValidatePinnedModule(HMODULE module,
                          const xeo3::vgpu::HashFileSha256 hashFile) noexcept {
  if (hashFile == nullptr) {
    return FailInstall(PatchStatus::MissingHashCallback);
  }
  if (module == nullptr) {
    return FailInstall(PatchStatus::ModuleMissing);
  }

  const auto *const base = reinterpret_cast<const std::uint8_t *>(module);
  const auto *const dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
    return FailInstall(PatchStatus::InvalidPe);
  }
  const auto *const nt =
      reinterpret_cast<const IMAGE_NT_HEADERS64 *>(base + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE ||
      nt->FileHeader.TimeDateStamp != kExpectedTimestamp ||
      nt->OptionalHeader.SizeOfImage != kExpectedImageSize) {
    return FailInstall(PatchStatus::IdentityMismatch);
  }

  std::array<wchar_t, 32768> path{};
  const auto pathLength =
      GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
  if (pathLength == 0 || pathLength >= path.size()) {
    return FailInstall(PatchStatus::PathFailure);
  }

  std::array<std::uint8_t, 32> digest{};
  if (!hashFile(path.data(), digest) || digest != kExpectedSha256) {
    return FailInstall(PatchStatus::HashMismatch);
  }
  return true;
}

bool WriteInstructionGate(std::uint8_t *target, const std::uint8_t *bytes,
                          const std::size_t byteCount,
                          const PatchStatus protectFailure,
                          const PatchStatus restoreFailure) noexcept {
  DWORD oldProtection = 0;
  if (!VirtualProtect(target, byteCount, PAGE_EXECUTE_READWRITE,
                      &oldProtection)) {
    SetStatus(protectFailure);
    return false;
  }

  std::memcpy(target, bytes, byteCount);
  FlushInstructionCache(GetCurrentProcess(), target, byteCount);

  DWORD ignoredProtection = 0;
  if (!VirtualProtect(target, byteCount, oldProtection, &ignoredProtection)) {
    SetStatus(restoreFailure);
    return false;
  }
  return true;
}

bool WriteSamplerAddressModeTable(
    std::uint32_t *const target,
    const std::array<std::uint32_t, xeo3::vgpu::kXenosSamplerAddressModeCount>
        &modes) noexcept {
  DWORD oldProtection = 0;
  if (!VirtualProtect(target, sizeof(modes), PAGE_READWRITE, &oldProtection)) {
    SetStatus(PatchStatus::SamplerAddressModeProtectionFailure);
    return false;
  }

  std::memcpy(target, modes.data(), sizeof(modes));

  DWORD ignoredProtection = 0;
  if (!VirtualProtect(target, sizeof(modes), oldProtection,
                      &ignoredProtection)) {
    SetStatus(PatchStatus::SamplerAddressModeProtectionRestoreFailure);
    return false;
  }
  return true;
}

bool HasExpectedEdramRestoreInputLayout(
    const D3D12_INPUT_LAYOUT_DESC &layout) noexcept {
  if (layout.NumElements != 2 || layout.pInputElementDescs == nullptr) {
    return false;
  }

  const auto &position = layout.pInputElementDescs[0];
  const auto &texcoord = layout.pInputElementDescs[1];
  return position.SemanticName != nullptr &&
         std::strcmp(position.SemanticName, "POSITION") == 0 &&
         position.SemanticIndex == 0 &&
         position.Format == DXGI_FORMAT_R32G32_FLOAT &&
         position.InputSlot == 0 && position.AlignedByteOffset == 0 &&
         position.InputSlotClass ==
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA &&
         position.InstanceDataStepRate == 0 &&
         texcoord.SemanticName != nullptr &&
         std::strcmp(texcoord.SemanticName, "TEXCOORD") == 0 &&
         texcoord.SemanticIndex == 0 &&
         texcoord.Format == DXGI_FORMAT_R32G32_FLOAT &&
         texcoord.InputSlot == 0 && texcoord.AlignedByteOffset == 8 &&
         texcoord.InputSlotClass ==
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA &&
         texcoord.InstanceDataStepRate == 0;
}

bool HasExpectedEdramRestoreFixedState(
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC &description) noexcept {
  if (description.StreamOutput.NumEntries != 0 ||
      description.StreamOutput.NumStrides != 0 ||
      description.BlendState.AlphaToCoverageEnable ||
      description.BlendState.IndependentBlendEnable ||
      description.SampleMask != UINT_MAX ||
      description.IBStripCutValue !=
          D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED ||
      description.PrimitiveTopologyType !=
          D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE ||
      description.DSVFormat != DXGI_FORMAT_UNKNOWN ||
      description.SampleDesc.Count != 2 ||
      description.SampleDesc.Quality != 0 ||
      description.Flags != D3D12_PIPELINE_STATE_FLAG_NONE ||
      description.NodeMask != 0 || description.NumRenderTargets != 1 ||
      description.RTVFormats[0] != DXGI_FORMAT_R8G8B8A8_UINT ||
      description.VS.BytecodeLength != 2224 ||
      description.PS.BytecodeLength != 2440 ||
      description.VS.pShaderBytecode == nullptr ||
      description.PS.pShaderBytecode == nullptr ||
      description.DS.BytecodeLength != 0 ||
      description.HS.BytecodeLength != 0 ||
      description.GS.BytecodeLength != 0) {
    return false;
  }

  for (std::size_t index = 1; index < std::size(description.RTVFormats);
       ++index) {
    if (description.RTVFormats[index] != DXGI_FORMAT_UNKNOWN) {
      return false;
    }
  }

  const auto &renderTarget = description.BlendState.RenderTarget[0];
  if (renderTarget.BlendEnable || renderTarget.LogicOpEnable ||
      renderTarget.SrcBlend != D3D12_BLEND_ONE ||
      renderTarget.DestBlend != D3D12_BLEND_ZERO ||
      renderTarget.BlendOp != D3D12_BLEND_OP_ADD ||
      renderTarget.SrcBlendAlpha != D3D12_BLEND_ONE ||
      renderTarget.DestBlendAlpha != D3D12_BLEND_ZERO ||
      renderTarget.BlendOpAlpha != D3D12_BLEND_OP_ADD ||
      renderTarget.LogicOp != D3D12_LOGIC_OP_CLEAR ||
      renderTarget.RenderTargetWriteMask != D3D12_COLOR_WRITE_ENABLE_ALL) {
    return false;
  }

  const auto &rasterizer = description.RasterizerState;
  if (rasterizer.FillMode != D3D12_FILL_MODE_SOLID ||
      rasterizer.CullMode != D3D12_CULL_MODE_NONE ||
      rasterizer.FrontCounterClockwise || rasterizer.DepthBias != 0 ||
      rasterizer.DepthBiasClamp != 0.0F ||
      rasterizer.SlopeScaledDepthBias != 0.0F || rasterizer.DepthClipEnable ||
      rasterizer.MultisampleEnable || rasterizer.AntialiasedLineEnable ||
      rasterizer.ForcedSampleCount != 0 ||
      rasterizer.ConservativeRaster !=
          D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF) {
    return false;
  }

  const auto &depthStencil = description.DepthStencilState;
  return !depthStencil.DepthEnable &&
         depthStencil.DepthWriteMask == D3D12_DEPTH_WRITE_MASK_ZERO &&
         !depthStencil.StencilEnable;
}

bool HasExpectedEdramScaleFixedState(
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC &description) noexcept {
  if (description.StreamOutput.NumEntries != 0 ||
      description.StreamOutput.NumStrides != 0 ||
      description.BlendState.AlphaToCoverageEnable ||
      description.BlendState.IndependentBlendEnable ||
      description.SampleMask != UINT_MAX ||
      description.IBStripCutValue !=
          D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED ||
      description.PrimitiveTopologyType !=
          D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE ||
      description.DSVFormat != DXGI_FORMAT_UNKNOWN ||
      description.SampleDesc.Count != 4 ||
      description.SampleDesc.Quality != 0 ||
      description.Flags != D3D12_PIPELINE_STATE_FLAG_NONE ||
      description.NodeMask != 0 || description.NumRenderTargets != 1 ||
      description.RTVFormats[0] != DXGI_FORMAT_R8G8B8A8_UINT ||
      description.VS.BytecodeLength != 2224 ||
      description.PS.BytecodeLength != 2528 ||
      description.VS.pShaderBytecode == nullptr ||
      description.PS.pShaderBytecode == nullptr ||
      description.DS.BytecodeLength != 0 ||
      description.HS.BytecodeLength != 0 ||
      description.GS.BytecodeLength != 0) {
    return false;
  }

  for (std::size_t index = 1; index < std::size(description.RTVFormats);
       ++index) {
    if (description.RTVFormats[index] != DXGI_FORMAT_UNKNOWN) {
      return false;
    }
  }

  const auto &renderTarget = description.BlendState.RenderTarget[0];
  if (renderTarget.BlendEnable || renderTarget.LogicOpEnable ||
      renderTarget.SrcBlend != D3D12_BLEND_ONE ||
      renderTarget.DestBlend != D3D12_BLEND_ZERO ||
      renderTarget.BlendOp != D3D12_BLEND_OP_ADD ||
      renderTarget.SrcBlendAlpha != D3D12_BLEND_ONE ||
      renderTarget.DestBlendAlpha != D3D12_BLEND_ZERO ||
      renderTarget.BlendOpAlpha != D3D12_BLEND_OP_ADD ||
      renderTarget.LogicOp != D3D12_LOGIC_OP_CLEAR ||
      renderTarget.RenderTargetWriteMask != D3D12_COLOR_WRITE_ENABLE_ALL) {
    return false;
  }

  const auto &rasterizer = description.RasterizerState;
  if (rasterizer.FillMode != D3D12_FILL_MODE_SOLID ||
      rasterizer.CullMode != D3D12_CULL_MODE_NONE ||
      rasterizer.FrontCounterClockwise || rasterizer.DepthBias != 0 ||
      rasterizer.DepthBiasClamp != 0.0F ||
      rasterizer.SlopeScaledDepthBias != 0.0F || rasterizer.DepthClipEnable ||
      rasterizer.MultisampleEnable || rasterizer.AntialiasedLineEnable ||
      rasterizer.ForcedSampleCount != 0 ||
      rasterizer.ConservativeRaster !=
          D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF) {
    return false;
  }

  const auto &depthStencil = description.DepthStencilState;
  return !depthStencil.DepthEnable &&
         depthStencil.DepthWriteMask == D3D12_DEPTH_WRITE_MASK_ZERO &&
         !depthStencil.StencilEnable;
}

bool HasExpectedEdramLoadFixedState(
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC &description) noexcept {
  if (description.StreamOutput.NumEntries != 0 ||
      description.StreamOutput.NumStrides != 0 ||
      description.BlendState.AlphaToCoverageEnable ||
      description.BlendState.IndependentBlendEnable ||
      description.SampleMask != UINT_MAX ||
      description.IBStripCutValue !=
          D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED ||
      description.PrimitiveTopologyType !=
          D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE ||
      description.DSVFormat != DXGI_FORMAT_UNKNOWN ||
      description.SampleDesc.Count != 1 ||
      description.SampleDesc.Quality != 0 ||
      description.Flags != D3D12_PIPELINE_STATE_FLAG_NONE ||
      description.NodeMask != 0 || description.NumRenderTargets != 1 ||
      description.RTVFormats[0] != DXGI_FORMAT_R8G8B8A8_UINT ||
      description.VS.BytecodeLength != 2224 ||
      description.PS.BytecodeLength != 2440 ||
      description.VS.pShaderBytecode == nullptr ||
      description.PS.pShaderBytecode == nullptr ||
      description.DS.BytecodeLength != 0 ||
      description.HS.BytecodeLength != 0 ||
      description.GS.BytecodeLength != 0) {
    return false;
  }

  for (std::size_t index = 1; index < std::size(description.RTVFormats);
       ++index) {
    if (description.RTVFormats[index] != DXGI_FORMAT_UNKNOWN) {
      return false;
    }
  }

  const auto &renderTarget = description.BlendState.RenderTarget[0];
  if (renderTarget.BlendEnable || renderTarget.LogicOpEnable ||
      renderTarget.SrcBlend != D3D12_BLEND_ONE ||
      renderTarget.DestBlend != D3D12_BLEND_ZERO ||
      renderTarget.BlendOp != D3D12_BLEND_OP_ADD ||
      renderTarget.SrcBlendAlpha != D3D12_BLEND_ONE ||
      renderTarget.DestBlendAlpha != D3D12_BLEND_ZERO ||
      renderTarget.BlendOpAlpha != D3D12_BLEND_OP_ADD ||
      renderTarget.LogicOp != D3D12_LOGIC_OP_CLEAR ||
      renderTarget.RenderTargetWriteMask != D3D12_COLOR_WRITE_ENABLE_ALL) {
    return false;
  }

  const auto &rasterizer = description.RasterizerState;
  if (rasterizer.FillMode != D3D12_FILL_MODE_SOLID ||
      rasterizer.CullMode != D3D12_CULL_MODE_NONE ||
      rasterizer.FrontCounterClockwise || rasterizer.DepthBias != 0 ||
      rasterizer.DepthBiasClamp != 0.0F ||
      rasterizer.SlopeScaledDepthBias != 0.0F || rasterizer.DepthClipEnable ||
      rasterizer.MultisampleEnable || rasterizer.AntialiasedLineEnable ||
      rasterizer.ForcedSampleCount != 0 ||
      rasterizer.ConservativeRaster !=
          D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF) {
    return false;
  }

  const auto &depthStencil = description.DepthStencilState;
  return !depthStencil.DepthEnable &&
         depthStencil.DepthWriteMask == D3D12_DEPTH_WRITE_MASK_ZERO &&
         !depthStencil.StencilEnable;
}

template <D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type, typename Value>
struct alignas(void *) PipelineStateStreamSubobject {
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type = Type;
  Value value{};
};

template <D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type, typename Value>
bool ReadPipelineStateStreamSubobject(const std::uint8_t *const stream,
                                      const std::size_t streamSize,
                                      const std::size_t offset,
                                      const Value *&value,
                                      std::size_t &nextOffset) noexcept {
  using Subobject = PipelineStateStreamSubobject<Type, Value>;
  value = nullptr;
  nextOffset = offset;
  if (stream == nullptr || offset % alignof(void *) != 0 ||
      offset > streamSize || sizeof(Subobject) > streamSize - offset) {
    return false;
  }

  const auto *const subobject =
      reinterpret_cast<const Subobject *>(stream + offset);
  if (subobject->type != Type) {
    return false;
  }
  value = &subobject->value;
  nextOffset = offset + sizeof(Subobject);
  return true;
}

xeo3::vgpu::GraphicsPipelineSignature BuildGraphicsPipelineSignature(
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC &description) noexcept {
  xeo3::vgpu::GraphicsPipelineSignature signature{};
  signature.vertexShaderSize = description.VS.BytecodeLength;
  signature.pixelShaderSize = description.PS.BytecodeLength;
  signature.sampleMask = description.SampleMask;
  signature.primitiveTopologyType =
      static_cast<std::uint32_t>(description.PrimitiveTopologyType);
  signature.sampleCount = description.SampleDesc.Count;
  signature.sampleQuality = description.SampleDesc.Quality;
  signature.renderTargetCount = description.NumRenderTargets;
  signature.renderTarget0Format =
      static_cast<std::uint32_t>(description.RTVFormats[0]);
  signature.depthStencilFormat =
      static_cast<std::uint32_t>(description.DSVFormat);
  signature.inputElementCount = description.InputLayout.NumElements;
  signature.renderTarget0WriteMask =
      description.BlendState.RenderTarget[0].RenderTargetWriteMask;
  signature.fillMode =
      static_cast<std::uint32_t>(description.RasterizerState.FillMode);
  signature.cullMode =
      static_cast<std::uint32_t>(description.RasterizerState.CullMode);
  signature.depthBias = description.RasterizerState.DepthBias;
  signature.depthWriteMask =
      static_cast<std::uint32_t>(description.DepthStencilState.DepthWriteMask);
  signature.depthFunc =
      static_cast<std::uint32_t>(description.DepthStencilState.DepthFunc);
  signature.frontCounterClockwise =
      description.RasterizerState.FrontCounterClockwise != FALSE;
  signature.depthClipEnable =
      description.RasterizerState.DepthClipEnable != FALSE;
  signature.multisampleEnable =
      description.RasterizerState.MultisampleEnable != FALSE;
  signature.antialiasedLineEnable =
      description.RasterizerState.AntialiasedLineEnable != FALSE;
  signature.depthEnable = description.DepthStencilState.DepthEnable != FALSE;
  signature.stencilEnable =
      description.DepthStencilState.StencilEnable != FALSE;
  signature.renderTarget0BlendEnable =
      description.BlendState.RenderTarget[0].BlendEnable != FALSE;
  signature.hasExpectedInputLayout =
      HasExpectedEdramRestoreInputLayout(description.InputLayout);
  signature.hasExpectedFixedState =
      HasExpectedEdramRestoreFixedState(description);
  signature.hasExpectedEdramScaleFixedState =
      HasExpectedEdramScaleFixedState(description);
  signature.hasExpectedEdramLoadFixedState =
      HasExpectedEdramLoadFixedState(description);

  if (!xeo3::vgpu::detail::HashBytesSha256(description.VS.pShaderBytecode,
                                           description.VS.BytecodeLength,
                                           signature.vertexShaderSha256) ||
      !xeo3::vgpu::detail::HashBytesSha256(description.PS.pShaderBytecode,
                                           description.PS.BytecodeLength,
                                           signature.pixelShaderSha256)) {
    signature.vertexShaderSha256.fill(0);
    signature.pixelShaderSha256.fill(0);
  }
  return signature;
}

HRESULT CreateEmbeddedRootSignature(
    ID3D12Device *const device,
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC &description,
    ID3D12RootSignature **const rootSignature) noexcept {
  if (device == nullptr || rootSignature == nullptr ||
      description.PS.pShaderBytecode == nullptr ||
      description.PS.BytecodeLength == 0) {
    return E_INVALIDARG;
  }
  *rootSignature = nullptr;

  ID3DBlob *serializedRootSignature = nullptr;
  const auto extractResult = D3DGetBlobPart(
      description.PS.pShaderBytecode, description.PS.BytecodeLength,
      D3D_BLOB_ROOT_SIGNATURE, 0, &serializedRootSignature);
  if (FAILED(extractResult) || serializedRootSignature == nullptr) {
    if (serializedRootSignature != nullptr) {
      serializedRootSignature->Release();
    }
    return FAILED(extractResult) ? extractResult : E_FAIL;
  }

  const auto createResult = device->CreateRootSignature(
      description.NodeMask, serializedRootSignature->GetBufferPointer(),
      serializedRootSignature->GetBufferSize(), __uuidof(ID3D12RootSignature),
      reinterpret_cast<void **>(rootSignature));
  serializedRootSignature->Release();
  return createResult;
}

constexpr D3D12_INPUT_ELEMENT_DESC kAc6EdramTransferInputElements[]{
    {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
     D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,
     D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
};

D3D12_GRAPHICS_PIPELINE_STATE_DESC BuildAc6EdramDrawReplacementDescription(
    const xeo3::vgpu::Ac6EdramDrawPipeline pipeline,
    ID3D12RootSignature *const rootSignature) noexcept {
  D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};
  description.pRootSignature = rootSignature;
  description.InputLayout = {
      kAc6EdramTransferInputElements,
      static_cast<UINT>(std::size(kAc6EdramTransferInputElements))};

  for (auto &renderTarget : description.BlendState.RenderTarget) {
    renderTarget.BlendEnable = FALSE;
    renderTarget.LogicOpEnable = FALSE;
    renderTarget.SrcBlend = D3D12_BLEND_ONE;
    renderTarget.DestBlend = D3D12_BLEND_ZERO;
    renderTarget.BlendOp = D3D12_BLEND_OP_ADD;
    renderTarget.SrcBlendAlpha = D3D12_BLEND_ONE;
    renderTarget.DestBlendAlpha = D3D12_BLEND_ZERO;
    renderTarget.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    renderTarget.LogicOp = D3D12_LOGIC_OP_CLEAR;
    renderTarget.RenderTargetWriteMask = 0;
  }
  description.BlendState.RenderTarget[0].RenderTargetWriteMask =
      D3D12_COLOR_WRITE_ENABLE_ALL;

  description.SampleMask = UINT_MAX;
  description.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  description.RasterizerState.FrontCounterClockwise = FALSE;
  description.RasterizerState.DepthBias = 0;
  description.RasterizerState.DepthBiasClamp = 0.0F;
  description.RasterizerState.SlopeScaledDepthBias = 0.0F;
  description.RasterizerState.DepthClipEnable = FALSE;
  description.RasterizerState.MultisampleEnable = FALSE;
  description.RasterizerState.AntialiasedLineEnable = FALSE;
  description.RasterizerState.ForcedSampleCount = 0;
  description.RasterizerState.ConservativeRaster =
      D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

  description.DepthStencilState.DepthEnable = FALSE;
  description.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
  description.DepthStencilState.DepthFunc =
      static_cast<D3D12_COMPARISON_FUNC>(0);
  description.DepthStencilState.StencilEnable = FALSE;
  description.DepthStencilState.StencilReadMask = 0;
  description.DepthStencilState.StencilWriteMask = 0;
  description.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
  description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  description.NumRenderTargets = 1;
  description.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UINT;
  description.DSVFormat = DXGI_FORMAT_UNKNOWN;
  description.SampleDesc.Count =
      pipeline == xeo3::vgpu::Ac6EdramDrawPipeline::Scale ? 4U : 1U;
  description.SampleDesc.Quality = 0;
  description.NodeMask = 0;
  description.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

  std::size_t vertexShaderSize = 0;
  description.VS.pShaderBytecode =
      xeo3::vgpu::detail::GetAc6EdramTransferVertexShader(vertexShaderSize);
  description.VS.BytecodeLength = vertexShaderSize;

  std::size_t pixelShaderSize = 0;
  description.PS.pShaderBytecode =
      pipeline == xeo3::vgpu::Ac6EdramDrawPipeline::Scale
          ? xeo3::vgpu::detail::GetAc6EdramScaleFixPixelShader(pixelShaderSize)
          : xeo3::vgpu::detail::GetAc6EdramLoadFixPixelShader(pixelShaderSize);
  description.PS.BytecodeLength = pixelShaderSize;
  return description;
}

HRESULT CreateAc6EdramDrawReplacement(
    const xeo3::vgpu::Ac6EdramDrawPipeline pipeline,
    const void *const commandList, const std::uintptr_t rootSignature,
    ID3D12PipelineState **const replacement) noexcept {
  if (commandList == nullptr || rootSignature == 0 || replacement == nullptr) {
    return E_INVALIDARG;
  }
  *replacement = nullptr;

  ID3D12Device *device = nullptr;
  const auto deviceResult =
      static_cast<ID3D12GraphicsCommandList *>(const_cast<void *>(commandList))
          ->GetDevice(__uuidof(ID3D12Device),
                      reinterpret_cast<void **>(&device));
  if (FAILED(deviceResult) || device == nullptr) {
    if (device != nullptr) {
      device->Release();
    }
    return FAILED(deviceResult) ? deviceResult : E_POINTER;
  }

  const auto description = BuildAc6EdramDrawReplacementDescription(
      pipeline, reinterpret_cast<ID3D12RootSignature *>(rootSignature));
  void *output = nullptr;
  const auto native =
      g_nativeCreateGraphicsPipelineState.load(std::memory_order_acquire);
  const auto createResult =
      native != nullptr
          ? native(device, &description, __uuidof(ID3D12PipelineState), &output)
          : device->CreateGraphicsPipelineState(
                &description, __uuidof(ID3D12PipelineState), &output);
  device->Release();
  if (FAILED(createResult) || output == nullptr) {
    if (output != nullptr) {
      static_cast<ID3D12PipelineState *>(output)->Release();
    }
    return FAILED(createResult) ? createResult : E_POINTER;
  }

  *replacement = static_cast<ID3D12PipelineState *>(output);
  return createResult;
}

bool IsAc6EdramDrawFingerprintCandidate(
    const xeo3::vgpu::DrawRecordSignature &signature,
    const void *const activePipelineState) noexcept {
  if (activePipelineState == nullptr || signature.rootSignature == 0) {
    return false;
  }
  const auto activePipelineAddress =
      reinterpret_cast<std::uintptr_t>(activePipelineState);
  return (signature.pipelineState == 0 ||
          signature.pipelineState == activePipelineAddress) &&
         signature.viewportWidthBits != 0 &&
         signature.viewportHeightBits != 0 &&
         signature.viewportMinDepthBits == 0 &&
         signature.viewportMaxDepthBits == 0x3F800000U &&
         signature.viewportTopLeftXBits == 0 &&
         signature.viewportTopLeftYBits == 0 && signature.recordKind == 0 &&
         signature.vertexCount == 3 && signature.startVertex == 0;
}

xeo3::vgpu::Ac6EdramBoundEvidence ReadPixEdramBoundEvidence(
    const GraphicsCommandListHookRecord &hook) noexcept {
  xeo3::vgpu::Ac6EdramBoundEvidence evidence{};
  evidence.renderTargetCount = hook.renderTargetCount;
  evidence.hasDepthStencil = hook.hasDepthStencil;
  evidence.rootTableMask = hook.graphicsRootDescriptorTableMask;
  BridgeVgpuPixEdramResolveFailure = 0;
  if (BridgeVgpuPixDescriptorHookFailure != 0) {
    BridgeVgpuPixEdramResolveFailure = 7;
    return evidence;
  }
  if (hook.renderTargetCount != 1 || hook.hasDepthStencil ||
      (hook.graphicsRootDescriptorTableMask & 3) != 3 ||
      hook.graphicsRootDescriptorTables[0] == 0 ||
      hook.graphicsRootDescriptorTables[1] == 0) {
    BridgeVgpuPixEdramResolveFailure = 1;
    return evidence;
  }
  {
    std::scoped_lock lock(g_rtvDescriptorMutex);
    const auto &view = g_rtvDescriptors[
        (hook.renderTargetDescriptor >> 4) % kRtvDescriptorTableSize];
    if (view.cpuDescriptor != hook.renderTargetDescriptor ||
        view.cpuDescriptor == 0) {
      BridgeVgpuPixEdramResolveFailure = 2;
      return evidence;
    }
    evidence.viewFormat = view.format;
    evidence.sampleCount = view.sampleCount;
    evidence.targetWidth = view.width;
    evidence.targetHeight = view.height;
  }
  if (evidence.viewFormat != DXGI_FORMAT_R8G8B8A8_UINT ||
      (evidence.sampleCount != 1 && evidence.sampleCount != 4)) {
    BridgeVgpuPixEdramResolveFailure = 3;
    return evidence;
  }

  const auto tableGpu = hook.graphicsRootDescriptorTables[0];
  std::scoped_lock lock(g_transfer341MappingMutex);
  D3D12_GPU_VIRTUAL_ADDRESS constantGpu = 0;
  const auto heapCount = (std::min)(static_cast<std::size_t>(hook.descriptorHeapCount),
                                    hook.descriptorHeaps.size());
  for (std::size_t index = 0; index < heapCount; ++index) {
    const auto gpuStart = hook.descriptorHeapGpuStarts[index];
    const auto cpuStart = hook.descriptorHeapCpuStarts[index];
    const auto increment = hook.descriptorHeapIncrements[index];
    if (gpuStart == 0 || cpuStart == 0 || increment == 0 || tableGpu < gpuStart)
      continue;
    const auto delta = tableGpu - gpuStart;
    if (delta >= hook.descriptorHeapByteSpans[index] || delta % increment != 0 ||
        delta > (std::numeric_limits<std::uintptr_t>::max)() - cpuStart)
      continue;
    const auto descriptor = cpuStart + static_cast<std::uintptr_t>(delta);
    const auto &record = g_constantBufferDescriptors[ConstantBufferDescriptorIndex(descriptor)];
    if (record.cpuDescriptor == descriptor) constantGpu = record.gpuAddress;
    break;
  }
  const auto read = [&evidence](const std::uint8_t *source) noexcept {
    SIZE_T bytesRead = 0;
    evidence.hasConstants = source != nullptr && ReadProcessMemory(
        GetCurrentProcess(), source, evidence.constants.data(),
        sizeof(evidence.constants), &bytesRead) && bytesRead == sizeof(evidence.constants);
    return evidence.hasConstants;
  };
  if (constantGpu != 0) {
    for (std::size_t index = 0; index < g_constantUploadContextCount; ++index) {
      const auto &context = g_constantUploadContexts[index];
      if (context.cpuBase == nullptr || constantGpu < context.gpuBase) continue;
      const auto offset = constantGpu - context.gpuBase;
      if (offset <= context.mappedSpan && sizeof(evidence.constants) <= context.mappedSpan - offset &&
          read(context.cpuBase + offset)) return evidence;
    }
  }

  // Fixed transfer CBVs can predate the public CreateConstantBufferView hook.
  // The pinned native pool retains a CPU/GPU descriptor pair per slot after
  // its 0x28-byte prefix. Match the bound opaque handle, not PIX's descriptor
  // storage or AMD's hardware descriptor encoding.
  for (std::size_t index = 0; index < g_constantUploadContextCount; ++index) {
    const auto &context = g_constantUploadContexts[index];
    if (context.context == 0 || context.cpuBase == nullptr || context.slotCount == 0 ||
        context.slotCount > 1024 || context.stride < sizeof(evidence.constants)) continue;
    std::array<std::uint64_t, 2048> pairs{};
    const auto pairBytes = static_cast<std::size_t>(context.slotCount) * 16;
    SIZE_T bytesRead = 0;
    if (!ReadProcessMemory(GetCurrentProcess(),
        reinterpret_cast<const void *>(context.context + 0x28), pairs.data(), pairBytes,
        &bytesRead) || bytesRead != pairBytes) continue;
    for (std::size_t slot = 0; slot < context.slotCount; ++slot) {
      if (pairs[slot * 2] != tableGpu && pairs[slot * 2 + 1] != tableGpu) continue;
      const auto offset = static_cast<std::uint64_t>(slot) * context.stride;
      if (offset <= context.mappedSpan && sizeof(evidence.constants) <= context.mappedSpan - offset &&
          read(context.cpuBase + offset)) return evidence;
    }
  }
  BridgeVgpuPixEdramResolveFailure = 4;
  return evidence;
}

void EmitEdramDrawPipelineFingerprint(
    const EdramDrawPipelineFingerprint &fingerprint,
    const std::uint64_t candidateCount,
    const xeo3::vgpu::Ac6EdramDrawPipeline observedShape) noexcept {
  char digest[65]{};
  for (std::size_t index = 0; index < fingerprint.cachedBlobSha256.size();
       ++index) {
    std::snprintf(digest + index * 2, 3, "%02X",
                  fingerprint.cachedBlobSha256[index]);
  }

  char vertexDigest[65]{};
  char pixelDigest[65]{};
  if (fingerprint.hasObservedSignature) {
    for (std::size_t index = 0;
         index < fingerprint.observedSignature.vertexShaderSha256.size();
         ++index) {
      std::snprintf(vertexDigest + index * 2, 3, "%02X",
                    fingerprint.observedSignature.vertexShaderSha256[index]);
      std::snprintf(pixelDigest + index * 2, 3, "%02X",
                    fingerprint.observedSignature.pixelShaderSha256[index]);
    }
  }

  char message[1024]{};
  const auto length = std::snprintf(
      message, std::size(message),
      "{\"xeo3_ac6\":\"vgpu_patch\","
      "\"event\":\"edram_draw_pipeline_fingerprint\","
      "\"candidate_count\":%" PRIu64 ","
      "\"pipeline_state\":\"0x%016" PRIXPTR "\","
      "\"cached_blob_result\":\"0x%08X\","
      "\"cached_blob_size\":%" PRIu64 ","
      "\"cached_blob_sha256\":\"%s\","
      "\"object_name_result\":\"0x%08X\","
      "\"object_name\":\"%s\","
      "\"observed_signature\":%u,"
      "\"observed_create_count\":%" PRIu64 ","
      "\"vs_size\":%" PRIu64 ",\"ps_size\":%" PRIu64 ","
      "\"vs_sha256\":\"%s\",\"ps_sha256\":\"%s\","
      "\"classification\":%u,"
      "\"draw_shape\":%u}",
      candidateCount,
      reinterpret_cast<std::uintptr_t>(fingerprint.pipelineState),
      static_cast<std::uint32_t>(fingerprint.result),
      fingerprint.cachedBlobSize, digest,
      static_cast<std::uint32_t>(fingerprint.objectNameResult),
      fingerprint.objectName.data(), fingerprint.hasObservedSignature ? 1U : 0U,
      fingerprint.observedCreateCount,
      fingerprint.observedSignature.vertexShaderSize,
      fingerprint.observedSignature.pixelShaderSize, vertexDigest, pixelDigest,
      static_cast<std::uint32_t>(fingerprint.classification),
      static_cast<std::uint32_t>(observedShape));
  if (length <= 0 || static_cast<std::size_t>(length) >= std::size(message)) {
    return;
  }

  OutputDebugStringA(message);
  OutputDebugStringA("\n");
  char path[512]{};
  const auto pathLength =
      std::snprintf(path, std::size(path),
                    "D:\\Games\\AC6 shit\\XeO3-AC6-lab\\ProbeLogs\\"
                    "ac6-edram-draw-fingerprints-%lu.jsonl",
                    GetCurrentProcessId());
  if (pathLength <= 0 ||
      static_cast<std::size_t>(pathLength) >= std::size(path)) {
    return;
  }
  const auto file =
      CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                  nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return;
  }
  DWORD bytesWritten = 0;
  WriteFile(file, message, static_cast<DWORD>(length), &bytesWritten, nullptr);
  WriteFile(file, "\n", 1, &bytesWritten, nullptr);
  CloseHandle(file);
}

bool DumpEdramDrawPipelineCachedBlob(const void *const pipelineState,
                                     const std::uint64_t candidateCount,
                                     const void *const bytes,
                                     const std::size_t byteCount) noexcept {
  if (BridgeVgpuEdramDrawFingerprintDumpEnabled == 0 || bytes == nullptr ||
      byteCount == 0 || byteCount > MAXDWORD) {
    return BridgeVgpuEdramDrawFingerprintDumpEnabled == 0;
  }
  char path[512]{};
  const auto pathLength =
      std::snprintf(path, std::size(path),
                    "D:\\Games\\AC6 shit\\XeO3-AC6-lab\\ProbeLogs\\"
                    "ac6-edram-pso-%lu-%03" PRIu64 "-%016" PRIXPTR ".bin",
                    GetCurrentProcessId(), candidateCount,
                    reinterpret_cast<std::uintptr_t>(pipelineState));
  if (pathLength <= 0 ||
      static_cast<std::size_t>(pathLength) >= std::size(path)) {
    BridgeVgpuEdramDrawFingerprintDumpFailure = ERROR_INSUFFICIENT_BUFFER;
    return false;
  }
  const auto file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    BridgeVgpuEdramDrawFingerprintDumpFailure = GetLastError();
    return false;
  }
  DWORD bytesWritten = 0;
  const auto written = WriteFile(file, bytes, static_cast<DWORD>(byteCount),
                                 &bytesWritten, nullptr);
  const auto error = written != FALSE ? ERROR_SUCCESS : GetLastError();
  CloseHandle(file);
  if (written == FALSE || bytesWritten != byteCount) {
    BridgeVgpuEdramDrawFingerprintDumpFailure =
        written != FALSE ? ERROR_WRITE_FAULT : error;
    return false;
  }
  BridgeVgpuEdramDrawFingerprintDumpFailure = ERROR_SUCCESS;
  return true;
}

xeo3::vgpu::Ac6EdramDrawPipeline ResolveEdramDrawFingerprintClassification(
    const EdramDrawPipelineFingerprint &fingerprint,
    const xeo3::vgpu::DrawRecordSignature &signature,
    const xeo3::vgpu::Ac6EdramDrawPipeline boundEvidence) noexcept {
  auto classification = fingerprint.classification;
  if (classification == xeo3::vgpu::Ac6EdramDrawPipeline::None &&
      boundEvidence != xeo3::vgpu::Ac6EdramDrawPipeline::None &&
      GetModuleHandleW(L"WinPixGpuCapturer.dll") != nullptr &&
      xeo3::vgpu::detail::IsPixOpaquePipelineBlob(
          fingerprint.cachedBlobSize, fingerprint.cachedBlobSha256)) {
    // Never cache the opaque PIX digest as a shader identity. Re-prove the
    // bound target and constants on each draw, including pointer reuse.
    classification = boundEvidence;
    const auto count = g_pixEdramBoundMatchCount.fetch_add(1, std::memory_order_relaxed) + 1;
    BridgeVgpuPixEdramBoundMatchCount = count;
    if (count <= 16 || (count & (count - 1)) == 0)
      EmitPatchEvent("pix_edram_bound_match", static_cast<std::uint32_t>(classification), count);
  }
  if (classification == xeo3::vgpu::Ac6EdramDrawPipeline::None &&
      BridgeVgpuHostTransferExperimentSelector == fingerprint.candidateId) {
    classification = xeo3::vgpu::detail::ClassifyAc6EdramDrawPipeline(
        signature, fingerprint.pipelineState);
  }
  BridgeVgpuHostTransferLastCandidate = fingerprint.candidateId;
  BridgeVgpuHostTransferLastClassification =
      static_cast<std::uint32_t>(classification);
  BridgeVgpuHostTransferLastPipelineState =
      reinterpret_cast<std::uintptr_t>(fingerprint.pipelineState);
  BridgeVgpuHostTransferLastRootSignature = signature.rootSignature;
  return classification;
}

xeo3::vgpu::Ac6EdramDrawPipeline ClassifyAc6EdramPipelineStateLocked(
    const xeo3::vgpu::DrawRecordSignature &signature,
    const void *const pipelineState,
    const xeo3::vgpu::Ac6EdramDrawPipeline boundEvidence) noexcept {
  if (!IsAc6EdramDrawFingerprintCandidate(signature, pipelineState)) {
    return xeo3::vgpu::Ac6EdramDrawPipeline::None;
  }

  const auto end =
      g_edramDrawPipelineFingerprints.begin() +
      static_cast<std::ptrdiff_t>(g_edramDrawPipelineFingerprintCount);
  const auto found =
      std::find_if(g_edramDrawPipelineFingerprints.begin(), end,
                   [pipelineState](const EdramDrawPipelineFingerprint &entry) {
                     return entry.pipelineState == pipelineState;
                   });
  if (found != end) {
    const auto cacheHitCount = g_edramDrawFingerprintCacheHitCount.fetch_add(
                                   1, std::memory_order_relaxed) +
                               1;
    BridgeVgpuEdramDrawFingerprintCacheHitCount = cacheHitCount;
    return ResolveEdramDrawFingerprintClassification(*found, signature, boundEvidence);
  }

  const auto candidateCount = g_edramDrawFingerprintCandidateCount.fetch_add(
                                  1, std::memory_order_relaxed) +
                              1;
  BridgeVgpuEdramDrawFingerprintCandidateCount = candidateCount;
  if (g_edramDrawPipelineFingerprintCount >=
      g_edramDrawPipelineFingerprints.size()) {
    const auto overflowCount =
        g_edramDrawFingerprintCacheOverflowCount.fetch_add(
            1, std::memory_order_relaxed) +
        1;
    BridgeVgpuEdramDrawFingerprintCacheOverflowCount = overflowCount;
    return xeo3::vgpu::Ac6EdramDrawPipeline::None;
  }

  EdramDrawPipelineFingerprint fingerprint{};
  fingerprint.pipelineState = pipelineState;
  fingerprint.candidateId = static_cast<std::uint32_t>(candidateCount);
  constexpr GUID kD3dDebugObjectNameW{
      0x4CCA5FD8,
      0x921F,
      0x42C8,
      {0x85, 0x66, 0x70, 0xCA, 0xF2, 0xA9, 0xB7, 0x41}};
  std::array<wchar_t, 128> wideObjectName{};
  UINT objectNameBytes =
      static_cast<UINT>(wideObjectName.size() * sizeof(wideObjectName.front()));
  fingerprint.objectNameResult =
      static_cast<ID3D12PipelineState *>(const_cast<void *>(pipelineState))
          ->GetPrivateData(kD3dDebugObjectNameW, &objectNameBytes,
                           wideObjectName.data());
  if (SUCCEEDED(fingerprint.objectNameResult) && objectNameBytes != 0) {
    const auto converted = WideCharToMultiByte(
        CP_UTF8, 0, wideObjectName.data(),
        static_cast<int>(objectNameBytes / sizeof(wideObjectName.front())),
        fingerprint.objectName.data(),
        static_cast<int>(fingerprint.objectName.size() - 1), nullptr, nullptr);
    if (converted <= 0) {
      fingerprint.objectNameResult = HRESULT_FROM_WIN32(GetLastError());
      fingerprint.objectName.fill(0);
    } else {
      fingerprint.objectName[static_cast<std::size_t>(converted)] = '\0';
      for (auto &character : fingerprint.objectName) {
        if (character == '\"' || character == '\\' ||
            static_cast<unsigned char>(character) < 0x20) {
          if (character != '\0') {
            character = '_';
          }
        }
      }
    }
  }
  ID3DBlob *cachedBlob = nullptr;
  fingerprint.result =
      static_cast<ID3D12PipelineState *>(const_cast<void *>(pipelineState))
          ->GetCachedBlob(&cachedBlob);
  const auto queryCount =
      g_edramDrawFingerprintQueryCount.fetch_add(1, std::memory_order_relaxed) +
      1;
  BridgeVgpuEdramDrawFingerprintQueryCount = queryCount;
  if (SUCCEEDED(fingerprint.result) && cachedBlob != nullptr) {
    fingerprint.cachedBlobSize = cachedBlob->GetBufferSize();
    DumpEdramDrawPipelineCachedBlob(pipelineState, candidateCount,
                                    cachedBlob->GetBufferPointer(),
                                    fingerprint.cachedBlobSize);
    if (!xeo3::vgpu::detail::HashBytesSha256(cachedBlob->GetBufferPointer(),
                                             fingerprint.cachedBlobSize,
                                             fingerprint.cachedBlobSha256)) {
      fingerprint.result = E_FAIL;
      fingerprint.cachedBlobSize = 0;
      fingerprint.cachedBlobSha256.fill(0);
    }
    cachedBlob->Release();
  } else if (SUCCEEDED(fingerprint.result)) {
    fingerprint.result = E_POINTER;
  }
  if (SUCCEEDED(fingerprint.result)) {
    fingerprint.classification =
        xeo3::vgpu::detail::ClassifyAc6EdramCachedPipelineBlob(
            fingerprint.cachedBlobSize, fingerprint.cachedBlobSha256);
  }

  ObservedPipelineState observed{};
  if (FindObservedPipelineState(pipelineState, observed)) {
    fingerprint.hasObservedSignature = true;
    fingerprint.observedCreateCount = observed.createCount;
    fingerprint.observedSignature = observed.signature;
  }

  BridgeVgpuEdramDrawLastFingerprintPipelineState =
      reinterpret_cast<std::uintptr_t>(pipelineState);
  BridgeVgpuEdramDrawLastFingerprintBlobSize = fingerprint.cachedBlobSize;
  std::array<std::uint64_t, 4> digestWords{};
  std::memcpy(digestWords.data(), fingerprint.cachedBlobSha256.data(),
              fingerprint.cachedBlobSha256.size());
  BridgeVgpuEdramDrawLastFingerprintHash0 = digestWords[0];
  BridgeVgpuEdramDrawLastFingerprintHash1 = digestWords[1];
  BridgeVgpuEdramDrawLastFingerprintHash2 = digestWords[2];
  BridgeVgpuEdramDrawLastFingerprintHash3 = digestWords[3];
  BridgeVgpuEdramDrawLastFingerprintResult =
      static_cast<std::uint32_t>(fingerprint.result);
  BridgeVgpuEdramDrawLastFingerprintClassification =
      static_cast<std::uint32_t>(fingerprint.classification);

  g_edramDrawPipelineFingerprints[g_edramDrawPipelineFingerprintCount++] =
      fingerprint;
  EmitEdramDrawPipelineFingerprint(
      fingerprint, candidateCount,
      xeo3::vgpu::detail::ClassifyAc6EdramDrawPipeline(signature,
                                                       pipelineState));
  return ResolveEdramDrawFingerprintClassification(fingerprint, signature, boundEvidence);
}

bool ContainsEdramDrawOriginalPipelineState(
    const EdramDrawReplacementState &state,
    const void *const pipelineState) noexcept {
  const auto end =
      state.originalPipelineStates.begin() +
      static_cast<std::ptrdiff_t>(state.originalPipelineStateCount);
  return std::find(state.originalPipelineStates.begin(), end, pipelineState) !=
         end;
}

bool RegisterEdramDrawOriginalPipelineState(
    EdramDrawReplacementState &state,
    const void *const pipelineState) noexcept {
  if (ContainsEdramDrawOriginalPipelineState(state, pipelineState)) {
    return true;
  }
  if (state.originalPipelineStateCount >= state.originalPipelineStates.size()) {
    return false;
  }
  state.originalPipelineStates[state.originalPipelineStateCount++] =
      pipelineState;
  return true;
}

const void *ResolveAc6EdramDrawPipelineState(
    const xeo3::vgpu::DrawRecordSignature &signature,
    const void *const commandList, const void *const pipelineState,
    xeo3::vgpu::Ac6EdramDrawPipeline *const classification,
    const xeo3::vgpu::Ac6EdramDrawPipeline boundEvidence =
        xeo3::vgpu::Ac6EdramDrawPipeline::None) noexcept {
  if (classification != nullptr) {
    *classification = xeo3::vgpu::Ac6EdramDrawPipeline::None;
  }
  if (pipelineState == nullptr) {
    return nullptr;
  }

  std::scoped_lock lock(g_edramDrawReplacementMutex);

  const auto substitute = [&](EdramDrawReplacementState &state,
                              const bool enabled,
                              std::atomic<std::uint64_t> &counter,
                              volatile std::uint64_t &telemetry,
                              const std::uint32_t detail) -> const void * {
    if (!enabled || signature.rootSignature != state.rootSignature ||
        state.replacementPipelineState == nullptr) {
      return pipelineState;
    }
    const auto count = counter.fetch_add(1, std::memory_order_relaxed) + 1;
    telemetry = count;
    if (count <= 64 || (count & (count - 1)) == 0) {
      EmitPatchEvent("edram_draw_pipeline_substituted", detail, count);
    }
    return state.replacementPipelineState;
  };

  if (pipelineState == g_edramScaleDrawReplacement.replacementPipelineState ||
      pipelineState == g_edramLoadDrawReplacement.replacementPipelineState) {
    return pipelineState;
  }
  const auto pipeline =
      ClassifyAc6EdramPipelineStateLocked(signature, pipelineState, boundEvidence);
  if (classification != nullptr) {
    *classification = pipeline;
  }
  if (pipeline == xeo3::vgpu::Ac6EdramDrawPipeline::None) {
    return pipelineState;
  }

  const bool scale = pipeline == xeo3::vgpu::Ac6EdramDrawPipeline::Scale;
  if ((scale && BridgeVgpuEdramScaleFixEnabled == 0) ||
      (!scale && BridgeVgpuEdramLoadFixEnabled == 0)) {
    return pipelineState;
  }

  auto &state =
      scale ? g_edramScaleDrawReplacement : g_edramLoadDrawReplacement;
  if (state.rootSignature == 0) {
    state.rootSignature = signature.rootSignature;
    g_edramDrawRootSignature = signature.rootSignature;
    BridgeVgpuEdramDrawRootSignature = signature.rootSignature;
  } else if (state.rootSignature != signature.rootSignature) {
    const auto mismatchCount = g_edramDrawRootSignatureMismatchCount.fetch_add(
                                   1, std::memory_order_relaxed) +
                               1;
    BridgeVgpuEdramDrawRootSignatureMismatchCount = mismatchCount;
    EmitPatchEvent("edram_draw_root_signature_mismatch",
                   static_cast<std::uint32_t>(pipeline), mismatchCount);
    return pipelineState;
  }

  if (ContainsEdramDrawOriginalPipelineState(state, pipelineState)) {
    return substitute(state, true,
                      scale ? g_edramScaleDrawSubstitutionCount
                            : g_edramLoadDrawSubstitutionCount,
                      scale ? BridgeVgpuEdramScaleDrawSubstitutionCount
                            : BridgeVgpuEdramLoadDrawSubstitutionCount,
                      static_cast<std::uint32_t>(pipeline));
  }
  if (!RegisterEdramDrawOriginalPipelineState(state, pipelineState)) {
    EmitPatchEvent("edram_draw_pipeline_identity_overflow",
                   static_cast<std::uint32_t>(pipeline),
                   state.originalPipelineStateCount);
    return pipelineState;
  }
  if (state.originalPipelineStateCount == 1) {
    if (scale) {
      BridgeVgpuEdramScaleDrawOriginalPipelineState =
          reinterpret_cast<std::uintptr_t>(pipelineState);
    } else {
      BridgeVgpuEdramLoadDrawOriginalPipelineState =
          reinterpret_cast<std::uintptr_t>(pipelineState);
    }
  }

  auto &matchCounter =
      scale ? g_edramScaleDrawMatchCount : g_edramLoadDrawMatchCount;
  const auto matchCount =
      matchCounter.fetch_add(1, std::memory_order_relaxed) + 1;
  if (scale) {
    BridgeVgpuEdramScaleDrawMatchCount = matchCount;
  } else {
    BridgeVgpuEdramLoadDrawMatchCount = matchCount;
  }

  if (!state.creationAttempted) {
    state.creationAttempted = true;
    ID3D12PipelineState *replacement = nullptr;
    const auto result = CreateAc6EdramDrawReplacement(
        pipeline, commandList, signature.rootSignature, &replacement);
    if (FAILED(result) || replacement == nullptr) {
      if (scale) {
        BridgeVgpuEdramScaleFixFailure =
            static_cast<std::uint32_t>(FAILED(result) ? result : E_POINTER);
      } else {
        BridgeVgpuEdramLoadFixFailure =
            static_cast<std::uint32_t>(FAILED(result) ? result : E_POINTER);
      }
      EmitPatchEvent("edram_draw_pipeline_create_failure",
                     static_cast<std::uint32_t>(pipeline),
                     static_cast<std::uint32_t>(result));
      return pipelineState;
    }

    state.replacementPipelineState = replacement;
    if (scale) {
      const auto fixedCount =
          g_edramScaleFixPipelineCount.fetch_add(1, std::memory_order_relaxed) +
          1;
      BridgeVgpuEdramScaleFixPipelineCount = fixedCount;
      BridgeVgpuEdramScaleFixFailure = ERROR_SUCCESS;
      BridgeVgpuEdramScaleDrawReplacementPipelineState =
          reinterpret_cast<std::uintptr_t>(replacement);
    } else {
      const auto fixedCount =
          g_edramLoadFixPipelineCount.fetch_add(1, std::memory_order_relaxed) +
          1;
      BridgeVgpuEdramLoadFixPipelineCount = fixedCount;
      BridgeVgpuEdramLoadFixFailure = ERROR_SUCCESS;
      BridgeVgpuEdramLoadDrawReplacementPipelineState =
          reinterpret_cast<std::uintptr_t>(replacement);
    }
    EmitPatchEvent("edram_draw_pipeline_created",
                   static_cast<std::uint32_t>(pipeline), matchCount);
  }

  return substitute(state, true,
                    scale ? g_edramScaleDrawSubstitutionCount
                          : g_edramLoadDrawSubstitutionCount,
                    scale ? BridgeVgpuEdramScaleDrawSubstitutionCount
                          : BridgeVgpuEdramLoadDrawSubstitutionCount,
                    static_cast<std::uint32_t>(pipeline));
}

GraphicsCommandListHookRecord *FindGraphicsCommandListHook(
    ID3D12GraphicsCommandList *const commandList) noexcept {
  if (commandList != nullptr && g_cachedGraphicsCommandList == commandList &&
      g_cachedGraphicsCommandListHook != nullptr &&
      g_cachedGraphicsCommandListHook->commandList.load(
          std::memory_order_acquire) == commandList) {
    return g_cachedGraphicsCommandListHook;
  }

  const auto count =
      g_graphicsCommandListHookCount.load(std::memory_order_acquire);
  for (std::size_t index = 0; index < count; ++index) {
    if (g_graphicsCommandListHooks[index].commandList.load(
            std::memory_order_acquire) == commandList) {
      g_cachedGraphicsCommandList = commandList;
      g_cachedGraphicsCommandListHook = &g_graphicsCommandListHooks[index];
      return g_cachedGraphicsCommandListHook;
    }
  }
  return nullptr;
}

void ClearGraphicsCommandListHookRecord(
    GraphicsCommandListHookRecord &record) noexcept {
  record.originalVtable = nullptr;
  record.hookVtable = nullptr;
  record.nativeRelease = nullptr;
  record.nativeReset = nullptr;
  record.nativeDrawInstanced = nullptr;
  record.nativeRsSetViewports = nullptr;
  record.nativeRsSetScissorRects = nullptr;
  record.nativeSetPipelineState = nullptr;
  record.nativeSetDescriptorHeaps = nullptr;
  record.nativeSetComputeRootSignature = nullptr;
  record.nativeSetGraphicsRootSignature = nullptr;
  record.nativeSetComputeRootDescriptorTable = nullptr;
  record.nativeSetGraphicsRootDescriptorTable = nullptr;
  record.nativeOmSetRenderTargets = nullptr;
  record.renderTargetDescriptor = 0;
  record.renderTargetCount = 0;
  record.hasDepthStencil = false;
  record.pipelineState = nullptr;
  record.activePipelineState = nullptr;
  record.computeRootSignature = nullptr;
  record.rootSignature = nullptr;
  record.viewport = {};
  record.scissor = {};
  record.viewportCount = 0;
  record.scissorCount = 0;
  record.descriptorHeaps = {};
  record.descriptorHeapGpuStarts = {};
  record.descriptorHeapCpuStarts = {};
  record.descriptorHeapByteSpans = {};
  record.descriptorHeapIncrements = {};
  record.descriptorHeapCount = 0;
  record.graphicsRootDescriptorTables = {};
  record.graphicsRootDescriptorTableMask = 0;
}

const GraphicsCommandListVtableHookProfile *
FindGraphicsCommandListVtableHookProfile(
    ID3D12GraphicsCommandList *const commandList) noexcept {
  auto ***const object = reinterpret_cast<void ***>(commandList);
  auto **const vtable = object == nullptr ? nullptr : *object;
  if (vtable == nullptr) {
    return nullptr;
  }
  const auto count = g_graphicsCommandListVtableHookProfileCount.load(
      std::memory_order_acquire);
  for (std::size_t index = 0; index < count; ++index) {
    if (g_graphicsCommandListVtableHookProfiles[index].vtable == vtable) {
      return &g_graphicsCommandListVtableHookProfiles[index];
    }
  }
  return nullptr;
}

GraphicsCommandListHookRecord *RegisterGraphicsCommandListHookLocked(
    ID3D12GraphicsCommandList *const commandList,
    const GraphicsCommandListVtableHookProfile &profile) noexcept {
  if (commandList == nullptr || profile.vtable == nullptr ||
      profile.nativeRelease == nullptr || profile.nativeReset == nullptr ||
      profile.nativeDrawInstanced == nullptr ||
      profile.nativeRsSetViewports == nullptr ||
      profile.nativeRsSetScissorRects == nullptr ||
      profile.nativeSetPipelineState == nullptr ||
      profile.nativeSetDescriptorHeaps == nullptr ||
      profile.nativeSetComputeRootSignature == nullptr ||
      profile.nativeSetGraphicsRootSignature == nullptr ||
      profile.nativeSetComputeRootDescriptorTable == nullptr ||
      profile.nativeSetGraphicsRootDescriptorTable == nullptr ||
      profile.nativeOmSetRenderTargets == nullptr) {
    BridgeVgpuHostCommandListHookFailure = ERROR_INVALID_STATE;
    return nullptr;
  }

  const auto count =
      g_graphicsCommandListHookCount.load(std::memory_order_relaxed);
  std::size_t slotIndex = count;
  for (std::size_t index = 0; index < count; ++index) {
    if (g_graphicsCommandListHooks[index].commandList.load(
            std::memory_order_acquire) == nullptr) {
      slotIndex = index;
      break;
    }
  }
  if (slotIndex >= g_graphicsCommandListHooks.size()) {
    BridgeVgpuHostCommandListHookFailure = ERROR_INSUFFICIENT_BUFFER;
    return nullptr;
  }

  auto &record = g_graphicsCommandListHooks[slotIndex];
  ClearGraphicsCommandListHookRecord(record);
  record.originalVtable = profile.vtable;
  record.hookVtable = profile.vtable;
  record.nativeRelease = profile.nativeRelease;
  record.nativeReset = profile.nativeReset;
  record.nativeDrawInstanced = profile.nativeDrawInstanced;
  record.nativeRsSetViewports = profile.nativeRsSetViewports;
  record.nativeRsSetScissorRects = profile.nativeRsSetScissorRects;
  record.nativeSetPipelineState = profile.nativeSetPipelineState;
  record.nativeSetDescriptorHeaps = profile.nativeSetDescriptorHeaps;
  record.nativeSetComputeRootSignature = profile.nativeSetComputeRootSignature;
  record.nativeSetGraphicsRootSignature =
      profile.nativeSetGraphicsRootSignature;
  record.nativeSetComputeRootDescriptorTable =
      profile.nativeSetComputeRootDescriptorTable;
  record.nativeSetGraphicsRootDescriptorTable =
      profile.nativeSetGraphicsRootDescriptorTable;
  record.nativeOmSetRenderTargets = profile.nativeOmSetRenderTargets;
  record.commandList.store(commandList, std::memory_order_release);

  const auto highWater = (std::max)(count, slotIndex + 1);
  g_graphicsCommandListHookCount.store(highWater, std::memory_order_release);
  ++g_graphicsCommandListActiveHookCount;
  BridgeVgpuHostCommandListHookCount =
      static_cast<std::uint32_t>(g_graphicsCommandListActiveHookCount);
  BridgeVgpuHostCommandListHookFailure = ERROR_SUCCESS;
  return &record;
}

GraphicsCommandListHookRecord *FindOrRegisterGraphicsCommandListHook(
    ID3D12GraphicsCommandList *const commandList) noexcept {
  if (auto *const existing = FindGraphicsCommandListHook(commandList)) {
    return existing;
  }

  std::scoped_lock lock(g_graphicsCommandListHookMutex);
  if (auto *const existing = FindGraphicsCommandListHook(commandList)) {
    return existing;
  }
  auto ***const object = reinterpret_cast<void ***>(commandList);
  const auto *const profile =
      FindGraphicsCommandListVtableHookProfile(commandList);
  if (object == nullptr || *object == nullptr || profile == nullptr) {
    BridgeVgpuHostCommandListHookFailure = ERROR_INVALID_STATE;
    return nullptr;
  }
  return RegisterGraphicsCommandListHookLocked(commandList, *profile);
}

void RetireGraphicsCommandListHook(
    GraphicsCommandListHookRecord &record,
    ID3D12GraphicsCommandList *const commandList) noexcept {
  std::scoped_lock lock(g_graphicsCommandListHookMutex);
  if (record.commandList.load(std::memory_order_acquire) != commandList) {
    return;
  }
  record.commandList.store(nullptr, std::memory_order_release);
  ClearGraphicsCommandListHookRecord(record);
  if (g_graphicsCommandListActiveHookCount != 0) {
    --g_graphicsCommandListActiveHookCount;
  }
  BridgeVgpuHostCommandListHookCount =
      static_cast<std::uint32_t>(g_graphicsCommandListActiveHookCount);

  auto highWater =
      g_graphicsCommandListHookCount.load(std::memory_order_relaxed);
  while (highWater != 0 &&
         g_graphicsCommandListHooks[highWater - 1].commandList.load(
             std::memory_order_acquire) == nullptr) {
    --highWater;
  }
  g_graphicsCommandListHookCount.store(highWater, std::memory_order_release);
}

ULONG STDMETHODCALLTYPE VgpuHostReleaseHook(IUnknown *const object) noexcept {
  auto *const commandList =
      reinterpret_cast<ID3D12GraphicsCommandList *>(object);
  auto *const hook = FindGraphicsCommandListHook(commandList);
  const auto *const profile =
      FindGraphicsCommandListVtableHookProfile(commandList);
  const auto nativeRelease = hook != nullptr      ? hook->nativeRelease
                             : profile != nullptr ? profile->nativeRelease
                                                  : nullptr;
  if (nativeRelease == nullptr) {
    BridgeVgpuHostCommandListHookFailure = ERROR_INVALID_STATE;
    return 1;
  }
  const auto referenceCount = nativeRelease(object);
  if (referenceCount == 0 && hook != nullptr) {
    RetireGraphicsCommandListHook(*hook, commandList);
  }
  return referenceCount;
}

HRESULT STDMETHODCALLTYPE
VgpuHostResetHook(ID3D12GraphicsCommandList *const commandList,
                  ID3D12CommandAllocator *const allocator,
                  ID3D12PipelineState *const initialPipelineState) noexcept {
  auto *const hook = FindOrRegisterGraphicsCommandListHook(commandList);
  const auto *const profile =
      FindGraphicsCommandListVtableHookProfile(commandList);
  const auto nativeReset = hook != nullptr      ? hook->nativeReset
                           : profile != nullptr ? profile->nativeReset
                                                : nullptr;
  if (nativeReset == nullptr) {
    BridgeVgpuHostCommandListHookFailure = ERROR_INVALID_STATE;
    return E_UNEXPECTED;
  }

  auto *effectivePipelineState = initialPipelineState;
  const auto *const targetPipelineState =
      g_ac6Pso341PipelineState.load(std::memory_order_acquire);
  auto *const replacementPipelineState =
      g_ac6Pso341ReplacementPipelineState.load(std::memory_order_acquire);
  if (BridgeVgpuTransfer341PipelineReplacementEnabled != 0 &&
      initialPipelineState != nullptr &&
      initialPipelineState == targetPipelineState &&
      replacementPipelineState != nullptr) {
    effectivePipelineState = replacementPipelineState;
  }

  const auto result =
      nativeReset(commandList, allocator, effectivePipelineState);
  const auto resetCount =
      g_hostCommandListResetCount.fetch_add(1, std::memory_order_relaxed) + 1;
  BridgeVgpuHostCommandListResetCount = resetCount;
  BridgeVgpuHostCommandListResetLastInitialPipelineState =
      reinterpret_cast<std::uintptr_t>(initialPipelineState);
  BridgeVgpuHostCommandListResetLastResult = static_cast<std::uint32_t>(result);

  if (SUCCEEDED(result) && hook != nullptr) {
    hook->pipelineState = initialPipelineState;
    hook->activePipelineState = effectivePipelineState;
    hook->computeRootSignature = nullptr;
    hook->rootSignature = nullptr;
    hook->viewport = {};
    hook->scissor = {};
    hook->viewportCount = 0;
    hook->scissorCount = 0;
    hook->descriptorHeaps = {};
    hook->descriptorHeapGpuStarts = {};
    hook->descriptorHeapCpuStarts = {};
    hook->descriptorHeapByteSpans = {};
    hook->descriptorHeapIncrements = {};
    hook->descriptorHeapCount = 0;
    hook->graphicsRootDescriptorTables = {};
    hook->graphicsRootDescriptorTableMask = 0;
    hook->renderTargetDescriptor = 0;
    hook->renderTargetCount = 0;
    hook->hasDepthStencil = false;
  }
  return result;
}

std::uint32_t FloatBits(const float value) noexcept {
  std::uint32_t bits = 0;
  static_assert(sizeof(bits) == sizeof(value));
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

void STDMETHODCALLTYPE VgpuHostDrawInstancedHook(
    ID3D12GraphicsCommandList *const commandList,
    const UINT vertexCountPerInstance, const UINT instanceCount,
    const UINT startVertexLocation, const UINT startInstanceLocation) noexcept {
  auto *const hook = FindOrRegisterGraphicsCommandListHook(commandList);
  const auto *const profile =
      FindGraphicsCommandListVtableHookProfile(commandList);
  const auto nativeDrawInstanced = hook != nullptr ? hook->nativeDrawInstanced
                                   : profile != nullptr
                                       ? profile->nativeDrawInstanced
                                       : nullptr;
  if (nativeDrawInstanced == nullptr) {
    BridgeVgpuHostCommandListHookFailure = ERROR_INVALID_STATE;
    return;
  }

  const auto drawCount =
      g_hostDrawCallCount.fetch_add(1, std::memory_order_relaxed) + 1;
  BridgeVgpuHostDrawCallCount = drawCount;

  if (hook != nullptr && hook->pipelineState != nullptr) {
    const auto restartPipeline =
        FindPrimitiveRestartPipelineState(hook->pipelineState);
    RecordPrimitiveRestartDraw(
        *hook, restartPipeline, hook->pipelineState, drawCount,
        vertexCountPerInstance, instanceCount, startVertexLocation,
        startInstanceLocation);
  }

  if (hook != nullptr && instanceCount == 1 && startInstanceLocation == 0 &&
      hook->pipelineState != nullptr && hook->rootSignature != nullptr &&
      hook->viewportCount == 1 && hook->scissorCount == 1) {
    xeo3::vgpu::DrawRecordSignature signature{};
    signature.rootSignature =
        reinterpret_cast<std::uintptr_t>(hook->rootSignature);
    signature.pipelineState =
        reinterpret_cast<std::uintptr_t>(hook->pipelineState);
    signature.viewportWidthBits = FloatBits(hook->viewport.Width);
    signature.viewportHeightBits = FloatBits(hook->viewport.Height);
    signature.viewportMinDepthBits = FloatBits(hook->viewport.MinDepth);
    signature.viewportMaxDepthBits = FloatBits(hook->viewport.MaxDepth);
    signature.viewportTopLeftXBits = FloatBits(hook->viewport.TopLeftX);
    signature.viewportTopLeftYBits = FloatBits(hook->viewport.TopLeftY);
    signature.scissorRight =
        hook->scissor.right >= 0
            ? static_cast<std::uint32_t>(hook->scissor.right)
            : UINT_MAX;
    signature.scissorBottom =
        hook->scissor.bottom >= 0
            ? static_cast<std::uint32_t>(hook->scissor.bottom)
            : UINT_MAX;
    signature.recordKind = 0;
    signature.vertexCount = vertexCountPerInstance;
    signature.startVertex = startVertexLocation;

    const auto observedShape = xeo3::vgpu::detail::ClassifyAc6EdramDrawPipeline(
        signature, hook->pipelineState);
    if (observedShape != xeo3::vgpu::Ac6EdramDrawPipeline::None) {
      const auto transferCount =
          g_hostTransferDrawCount.fetch_add(1, std::memory_order_relaxed) + 1;
      BridgeVgpuHostTransferDrawCount = transferCount;
      BridgeVgpuHostTransferLastDescriptorHeapCount = hook->descriptorHeapCount;
      BridgeVgpuHostTransferLastDescriptorHeap0 =
          reinterpret_cast<std::uintptr_t>(hook->descriptorHeaps[0]);
      BridgeVgpuHostTransferLastDescriptorHeap1 =
          reinterpret_cast<std::uintptr_t>(hook->descriptorHeaps[1]);
      BridgeVgpuHostTransferLastDescriptorHeap0GpuStart =
          hook->descriptorHeapGpuStarts[0];
      BridgeVgpuHostTransferLastDescriptorHeap1GpuStart =
          hook->descriptorHeapGpuStarts[1];
      BridgeVgpuHostTransferLastRootDescriptorTableMask =
          hook->graphicsRootDescriptorTableMask;
      BridgeVgpuHostTransferLastRootDescriptorTable0 =
          hook->graphicsRootDescriptorTables[0];
      BridgeVgpuHostTransferLastRootDescriptorTable1 =
          hook->graphicsRootDescriptorTables[1];
      xeo3::vgpu::Ac6EdramDrawPipeline fingerprintedPipeline =
          xeo3::vgpu::Ac6EdramDrawPipeline::None;
      auto boundEvidence = xeo3::vgpu::Ac6EdramDrawPipeline::None;
      if (GetModuleHandleW(L"WinPixGpuCapturer.dll") != nullptr &&
          hook->scissor.left == 0 && hook->scissor.top == 0) {
        const auto evidence = ReadPixEdramBoundEvidence(*hook);
        boundEvidence = xeo3::vgpu::detail::ClassifyAc6BoundEdramDraw(
            signature, hook->pipelineState, evidence);
        if (boundEvidence == xeo3::vgpu::Ac6EdramDrawPipeline::None) {
          const auto count = g_pixEdramBoundRejectCount.fetch_add(
                                 1, std::memory_order_relaxed) + 1;
          BridgeVgpuPixEdramBoundRejectCount = count;
          if (BridgeVgpuPixEdramResolveFailure == 0)
            BridgeVgpuPixEdramResolveFailure = 5;
          if (count <= 16 || (count & (count - 1)) == 0)
            EmitPatchEvent("pix_edram_bound_reject",
                           BridgeVgpuPixEdramResolveFailure, count);
        }
      }
      const auto *const resolved = ResolveAc6EdramDrawPipelineState(
          signature, commandList, hook->pipelineState, &fingerprintedPipeline,
          boundEvidence);
      BridgeVgpuHostTransferLastClassification =
          static_cast<std::uint32_t>(observedShape);
      const auto hostRestoreCandidate =
          xeo3::vgpu::detail::ShouldSuppressAc6HostEdramRestoreDraw(
              signature, hook->pipelineState, fingerprintedPipeline, true);
      if (hostRestoreCandidate) {
        const auto candidateCount =
            g_hostEdramRestoreDrawCandidateCount.fetch_add(
                1, std::memory_order_relaxed) +
            1;
        BridgeVgpuHostEdramRestoreDrawCandidateCount = candidateCount;
        BridgeVgpuHostEdramRestoreDrawLastDrawCall = drawCount;
        BridgeVgpuHostEdramRestoreDrawLastPipelineState =
            reinterpret_cast<std::uintptr_t>(hook->pipelineState);
        BridgeVgpuHostEdramRestoreDrawLastRootSignature =
            signature.rootSignature;
        if (xeo3::vgpu::detail::ShouldSuppressAc6HostEdramRestoreDraw(
                signature, hook->pipelineState, fingerprintedPipeline,
                BridgeVgpuHostEdramRestoreDrawSkipEnabled != 0)) {
          const auto skipCount = g_hostEdramRestoreDrawSkipCount.fetch_add(
                                     1, std::memory_order_relaxed) +
                                 1;
          BridgeVgpuHostEdramRestoreDrawSkipCount = skipCount;
          if (skipCount <= 64 || (skipCount & (skipCount - 1)) == 0) {
            EmitPatchEvent("host_edram_restore_draw_skipped",
                           static_cast<std::uint32_t>(fingerprintedPipeline),
                           skipCount);
          }
          return;
        }
      }
      if (resolved != hook->pipelineState && resolved != nullptr &&
          hook->nativeSetPipelineState != nullptr) {
        hook->nativeSetPipelineState(
            commandList,
            static_cast<ID3D12PipelineState *>(const_cast<void *>(resolved)));
        nativeDrawInstanced(commandList, vertexCountPerInstance, instanceCount,
                            startVertexLocation, startInstanceLocation);
        hook->nativeSetPipelineState(commandList, hook->pipelineState);
        return;
      }
    }
  }

  nativeDrawInstanced(commandList, vertexCountPerInstance, instanceCount,
                      startVertexLocation, startInstanceLocation);
}

void STDMETHODCALLTYPE VgpuHostRsSetViewportsHook(
    ID3D12GraphicsCommandList *const commandList, const UINT viewportCount,
    const D3D12_VIEWPORT *const viewports) noexcept {
  auto *const hook = FindOrRegisterGraphicsCommandListHook(commandList);
  const auto *const profile =
      FindGraphicsCommandListVtableHookProfile(commandList);
  const auto nativeRsSetViewports = hook != nullptr ? hook->nativeRsSetViewports
                                    : profile != nullptr
                                        ? profile->nativeRsSetViewports
                                        : nullptr;
  if (nativeRsSetViewports == nullptr) {
    BridgeVgpuHostCommandListHookFailure = ERROR_INVALID_STATE;
    return;
  }
  if (hook != nullptr) {
    hook->viewportCount = viewportCount;
    hook->viewport = viewportCount == 1 && viewports != nullptr
                         ? viewports[0]
                         : D3D12_VIEWPORT{};
  }
  nativeRsSetViewports(commandList, viewportCount, viewports);
}

void STDMETHODCALLTYPE VgpuHostRsSetScissorRectsHook(
    ID3D12GraphicsCommandList *const commandList, const UINT rectCount,
    const D3D12_RECT *const rects) noexcept {
  auto *const hook = FindOrRegisterGraphicsCommandListHook(commandList);
  const auto *const profile =
      FindGraphicsCommandListVtableHookProfile(commandList);
  const auto nativeRsSetScissorRects =
      hook != nullptr      ? hook->nativeRsSetScissorRects
      : profile != nullptr ? profile->nativeRsSetScissorRects
                           : nullptr;
  if (nativeRsSetScissorRects == nullptr) {
    BridgeVgpuHostCommandListHookFailure = ERROR_INVALID_STATE;
    return;
  }
  if (hook != nullptr) {
    hook->scissorCount = rectCount;
    hook->scissor =
        rectCount == 1 && rects != nullptr ? rects[0] : D3D12_RECT{};
  }
  nativeRsSetScissorRects(commandList, rectCount, rects);
}

void STDMETHODCALLTYPE VgpuHostSetPipelineStateHook(
    ID3D12GraphicsCommandList *const commandList,
    ID3D12PipelineState *const pipelineState) noexcept {
  auto *const hook = FindOrRegisterGraphicsCommandListHook(commandList);
  const auto *const profile =
      FindGraphicsCommandListVtableHookProfile(commandList);
  const auto nativeSetPipelineState =
      hook != nullptr      ? hook->nativeSetPipelineState
      : profile != nullptr ? profile->nativeSetPipelineState
                           : nullptr;
  if (nativeSetPipelineState == nullptr) {
    BridgeVgpuHostCommandListHookFailure = ERROR_INVALID_STATE;
    return;
  }
  auto *effectivePipelineState = pipelineState;
  const auto *const targetPipelineState =
      g_ac6Pso341PipelineState.load(std::memory_order_acquire);
  auto *const replacementPipelineState =
      g_ac6Pso341ReplacementPipelineState.load(std::memory_order_acquire);
  if (BridgeVgpuTransfer341PipelineReplacementEnabled != 0 &&
      pipelineState != nullptr && pipelineState == targetPipelineState &&
      replacementPipelineState != nullptr) {
    effectivePipelineState = replacementPipelineState;
    const auto substitutionCount =
        g_transfer341ReplacementSubstitutionCount.fetch_add(
            1, std::memory_order_relaxed) +
        1;
    BridgeVgpuTransfer341ReplacementSubstitutionCount = substitutionCount;
  }
  if (hook != nullptr) {
    hook->pipelineState = pipelineState;
    hook->activePipelineState = effectivePipelineState;
  }
  nativeSetPipelineState(commandList, effectivePipelineState);
}

void STDMETHODCALLTYPE VgpuHostSetDescriptorHeapsHook(
    ID3D12GraphicsCommandList *const commandList,
    const UINT descriptorHeapCount,
    ID3D12DescriptorHeap *const *const descriptorHeaps) noexcept {
  auto *const hook = FindOrRegisterGraphicsCommandListHook(commandList);
  const auto *const profile =
      FindGraphicsCommandListVtableHookProfile(commandList);
  const auto nativeSetDescriptorHeaps =
      hook != nullptr      ? hook->nativeSetDescriptorHeaps
      : profile != nullptr ? profile->nativeSetDescriptorHeaps
                           : nullptr;
  if (nativeSetDescriptorHeaps == nullptr) {
    BridgeVgpuHostCommandListHookFailure = ERROR_INVALID_STATE;
    return;
  }
  const auto callCount =
      g_hostSetDescriptorHeapsCount.fetch_add(1, std::memory_order_relaxed) + 1;
  BridgeVgpuHostSetDescriptorHeapsCount = callCount;
  if (hook != nullptr) {
    hook->descriptorHeaps = {};
    hook->descriptorHeapGpuStarts = {};
    hook->descriptorHeapCpuStarts = {};
    hook->descriptorHeapByteSpans = {};
    hook->descriptorHeapIncrements = {};
    hook->descriptorHeapCount = descriptorHeapCount;
    const auto trackedCount =
        (std::min)(static_cast<std::size_t>(descriptorHeapCount),
                   hook->descriptorHeaps.size());
    for (std::size_t index = 0; index < trackedCount; ++index) {
      auto *const heap =
          descriptorHeaps != nullptr ? descriptorHeaps[index] : nullptr;
      hook->descriptorHeaps[index] = heap;
      if (heap != nullptr) {
        hook->descriptorHeapGpuStarts[index] =
            heap->GetGPUDescriptorHandleForHeapStart().ptr;
        hook->descriptorHeapCpuStarts[index] =
            heap->GetCPUDescriptorHandleForHeapStart().ptr;
        const auto heapDescription = heap->GetDesc();
        ID3D12Device *device = nullptr;
        if (SUCCEEDED(heap->GetDevice(IID_PPV_ARGS(&device))) &&
            device != nullptr) {
          const auto increment =
              device->GetDescriptorHandleIncrementSize(heapDescription.Type);
          device->Release();
          hook->descriptorHeapIncrements[index] = increment;
          if (increment != 0 &&
              heapDescription.NumDescriptors <=
                  (std::numeric_limits<std::uint64_t>::max)() / increment) {
            hook->descriptorHeapByteSpans[index] =
                static_cast<std::uint64_t>(heapDescription.NumDescriptors) *
                increment;
          }
        }
      }
    }
    // D3D12 invalidates descriptor-table bindings when the descriptor heaps
    // change. A transfer draw now proves both tables were rebound afterwards.
    hook->graphicsRootDescriptorTables = {};
    hook->graphicsRootDescriptorTableMask = 0;
  }
  nativeSetDescriptorHeaps(commandList, descriptorHeapCount, descriptorHeaps);
}

void STDMETHODCALLTYPE VgpuHostSetComputeRootSignatureHook(
    ID3D12GraphicsCommandList *const commandList,
    ID3D12RootSignature *const rootSignature) noexcept {
  auto *const hook = FindOrRegisterGraphicsCommandListHook(commandList);
  const auto *const profile =
      FindGraphicsCommandListVtableHookProfile(commandList);
  const auto nativeSetComputeRootSignature =
      hook != nullptr      ? hook->nativeSetComputeRootSignature
      : profile != nullptr ? profile->nativeSetComputeRootSignature
                           : nullptr;
  if (nativeSetComputeRootSignature == nullptr) {
    BridgeVgpuHostCommandListHookFailure = ERROR_INVALID_STATE;
    return;
  }
  if (hook != nullptr) {
    hook->computeRootSignature = rootSignature;
  }
  BridgeVgpuTransfer341LastComputeRootSignature =
      reinterpret_cast<std::uintptr_t>(rootSignature);
  nativeSetComputeRootSignature(commandList, rootSignature);
}

void STDMETHODCALLTYPE VgpuHostSetGraphicsRootSignatureHook(
    ID3D12GraphicsCommandList *const commandList,
    ID3D12RootSignature *const rootSignature) noexcept {
  auto *const hook = FindOrRegisterGraphicsCommandListHook(commandList);
  const auto *const profile =
      FindGraphicsCommandListVtableHookProfile(commandList);
  const auto nativeSetGraphicsRootSignature =
      hook != nullptr      ? hook->nativeSetGraphicsRootSignature
      : profile != nullptr ? profile->nativeSetGraphicsRootSignature
                           : nullptr;
  if (nativeSetGraphicsRootSignature == nullptr) {
    BridgeVgpuHostCommandListHookFailure = ERROR_INVALID_STATE;
    return;
  }
  if (hook != nullptr) {
    hook->rootSignature = rootSignature;
    hook->graphicsRootDescriptorTables = {};
    hook->graphicsRootDescriptorTableMask = 0;
  }
  nativeSetGraphicsRootSignature(commandList, rootSignature);
}

void STDMETHODCALLTYPE VgpuHostSetComputeRootDescriptorTableHook(
    ID3D12GraphicsCommandList *const commandList, const UINT rootParameterIndex,
    const D3D12_GPU_DESCRIPTOR_HANDLE baseDescriptor) noexcept {
  auto *const hook = FindOrRegisterGraphicsCommandListHook(commandList);
  const auto *const profile =
      FindGraphicsCommandListVtableHookProfile(commandList);
  const auto nativeSetComputeRootDescriptorTable =
      hook != nullptr      ? hook->nativeSetComputeRootDescriptorTable
      : profile != nullptr ? profile->nativeSetComputeRootDescriptorTable
                           : nullptr;
  if (nativeSetComputeRootDescriptorTable == nullptr) {
    BridgeVgpuHostCommandListHookFailure = ERROR_INVALID_STATE;
    return;
  }

  // PSO341 root parameter 0 starts a table containing CB0 followed by the
  // 0x300-byte CB1 task buffer. The table and task signatures keep this
  // independent of transient descriptor indices and GPU virtual addresses.
  if (hook != nullptr && rootParameterIndex == 0) {
    ActivateAc6Pso341Replacement(*hook, commandList);
    PatchConstantBufferDescriptorAtComputeBind(
        *hook, baseDescriptor, reinterpret_cast<std::uintptr_t>(commandList));
  }
  nativeSetComputeRootDescriptorTable(commandList, rootParameterIndex,
                                      baseDescriptor);
}

void STDMETHODCALLTYPE VgpuHostSetGraphicsRootDescriptorTableHook(
    ID3D12GraphicsCommandList *const commandList, const UINT rootParameterIndex,
    const D3D12_GPU_DESCRIPTOR_HANDLE baseDescriptor) noexcept {
  auto *const hook = FindOrRegisterGraphicsCommandListHook(commandList);
  const auto *const profile =
      FindGraphicsCommandListVtableHookProfile(commandList);
  const auto nativeSetGraphicsRootDescriptorTable =
      hook != nullptr      ? hook->nativeSetGraphicsRootDescriptorTable
      : profile != nullptr ? profile->nativeSetGraphicsRootDescriptorTable
                           : nullptr;
  if (nativeSetGraphicsRootDescriptorTable == nullptr) {
    BridgeVgpuHostCommandListHookFailure = ERROR_INVALID_STATE;
    return;
  }
  const auto callCount = g_hostSetGraphicsRootDescriptorTableCount.fetch_add(
                             1, std::memory_order_relaxed) +
                         1;
  BridgeVgpuHostSetGraphicsRootDescriptorTableCount = callCount;
  if (hook != nullptr) {
    if (rootParameterIndex < hook->graphicsRootDescriptorTables.size()) {
      hook->graphicsRootDescriptorTables[rootParameterIndex] =
          baseDescriptor.ptr;
    }
    if (rootParameterIndex < 64) {
      hook->graphicsRootDescriptorTableMask |= std::uint64_t{1}
                                               << rootParameterIndex;
    }
  }
  nativeSetGraphicsRootDescriptorTable(commandList, rootParameterIndex,
                                       baseDescriptor);
}

void STDMETHODCALLTYPE VgpuHostOmSetRenderTargetsHook(
    ID3D12GraphicsCommandList *const commandList, const UINT count,
    const D3D12_CPU_DESCRIPTOR_HANDLE *const descriptors,
    const BOOL contiguous,
    const D3D12_CPU_DESCRIPTOR_HANDLE *const depthStencil) noexcept {
  auto *const hook = FindOrRegisterGraphicsCommandListHook(commandList);
  const auto *const profile =
      FindGraphicsCommandListVtableHookProfile(commandList);
  const auto native = hook != nullptr ? hook->nativeOmSetRenderTargets
                      : profile != nullptr ? profile->nativeOmSetRenderTargets
                                           : nullptr;
  if (native == nullptr) {
    BridgeVgpuHostCommandListHookFailure = ERROR_INVALID_STATE;
    return;
  }
  if (hook != nullptr) {
    hook->renderTargetCount = count;
    // With exactly one RTV, both descriptor-array encodings use element 0.
    hook->renderTargetDescriptor =
        count == 1 && descriptors != nullptr ? descriptors[0].ptr : 0;
    hook->hasDepthStencil = depthStencil != nullptr;
  }
  native(commandList, count, descriptors, contiguous, depthStencil);
}

bool EnsureGraphicsCommandListHooks(
    ID3D12GraphicsCommandList *const commandList) noexcept {
  if (commandList == nullptr) {
    BridgeVgpuHostCommandListHookFailure = ERROR_INVALID_PARAMETER;
    return false;
  }
  if (FindGraphicsCommandListHook(commandList) != nullptr) {
    return true;
  }

  std::scoped_lock lock(g_graphicsCommandListHookMutex);
  if (FindGraphicsCommandListHook(commandList) != nullptr) {
    return true;
  }

  auto ***const object = reinterpret_cast<void ***>(commandList);
  auto **const originalVtable = object == nullptr ? nullptr : *object;
  if (originalVtable == nullptr) {
    BridgeVgpuHostCommandListHookFailure = ERROR_INVALID_ADDRESS;
    return false;
  }

  if (const auto *const profile =
          FindGraphicsCommandListVtableHookProfile(commandList)) {
    return RegisterGraphicsCommandListHookLocked(commandList, *profile) !=
           nullptr;
  }
  const auto profileIndex = g_graphicsCommandListVtableHookProfileCount.load(
      std::memory_order_relaxed);
  if (profileIndex >= g_graphicsCommandListVtableHookProfiles.size()) {
    BridgeVgpuHostCommandListHookFailure = ERROR_INSUFFICIENT_BUFFER;
    return false;
  }

  constexpr auto vtableByteCount =
      kGraphicsCommandListVtableEntryCount * sizeof(void *);
  MEMORY_BASIC_INFORMATION information{};
  if (VirtualQuery(originalVtable, &information, sizeof(information)) == 0 ||
      information.State != MEM_COMMIT ||
      (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
    BridgeVgpuHostCommandListHookFailure = ERROR_INVALID_ADDRESS;
    return false;
  }
  const auto regionBegin =
      reinterpret_cast<std::uintptr_t>(information.BaseAddress);
  const auto vtableBegin = reinterpret_cast<std::uintptr_t>(originalVtable);
  if (vtableBegin < regionBegin ||
      vtableBegin - regionBegin > information.RegionSize ||
      vtableByteCount > information.RegionSize - (vtableBegin - regionBegin)) {
    BridgeVgpuHostCommandListHookFailure = ERROR_INSUFFICIENT_BUFFER;
    return false;
  }

  const std::array<std::size_t, 12> requiredSlots{
      kReleaseVtableIndex,
      kResetVtableIndex,
      kDrawInstancedVtableIndex,
      kRsSetViewportsVtableIndex,
      kRsSetScissorRectsVtableIndex,
      kSetPipelineStateVtableIndex,
      kSetDescriptorHeapsVtableIndex,
      kSetComputeRootSignatureVtableIndex,
      kSetGraphicsRootSignatureVtableIndex,
      kSetComputeRootDescriptorTableVtableIndex,
      kSetGraphicsRootDescriptorTableVtableIndex,
      kOmSetRenderTargetsVtableIndex};
  if (std::any_of(requiredSlots.begin(), requiredSlots.end(),
                  [originalVtable](const std::size_t index) {
                    return originalVtable[index] == nullptr;
                  })) {
    BridgeVgpuHostCommandListHookFailure = ERROR_INVALID_FUNCTION;
    return false;
  }

  const std::array<void *, 12> hookAddresses{
      reinterpret_cast<void *>(&VgpuHostReleaseHook),
      reinterpret_cast<void *>(&VgpuHostResetHook),
      reinterpret_cast<void *>(&VgpuHostDrawInstancedHook),
      reinterpret_cast<void *>(&VgpuHostRsSetViewportsHook),
      reinterpret_cast<void *>(&VgpuHostRsSetScissorRectsHook),
      reinterpret_cast<void *>(&VgpuHostSetPipelineStateHook),
      reinterpret_cast<void *>(&VgpuHostSetDescriptorHeapsHook),
      reinterpret_cast<void *>(&VgpuHostSetComputeRootSignatureHook),
      reinterpret_cast<void *>(&VgpuHostSetGraphicsRootSignatureHook),
      reinterpret_cast<void *>(&VgpuHostSetComputeRootDescriptorTableHook),
      reinterpret_cast<void *>(&VgpuHostSetGraphicsRootDescriptorTableHook),
      reinterpret_cast<void *>(&VgpuHostOmSetRenderTargetsHook)};
  std::array<void *, 12> nativeAddresses{};
  for (std::size_t index = 0; index < requiredSlots.size(); ++index) {
    nativeAddresses[index] = originalVtable[requiredSlots[index]];
    if (nativeAddresses[index] == hookAddresses[index]) {
      BridgeVgpuHostCommandListHookFailure = ERROR_INVALID_FUNCTION;
      return false;
    }
  }

  auto **const protectedBegin = originalVtable + kReleaseVtableIndex;
  constexpr auto protectedByteCount =
      (kOmSetRenderTargetsVtableIndex - kReleaseVtableIndex + 1) *
      sizeof(void *);
  DWORD oldProtection = 0;
  if (!VirtualProtect(protectedBegin, protectedByteCount, PAGE_READWRITE,
                      &oldProtection)) {
    BridgeVgpuHostCommandListHookFailure = GetLastError();
    return false;
  }

  auto &profile = g_graphicsCommandListVtableHookProfiles[profileIndex];
  profile = {};
  profile.vtable = originalVtable;
  profile.nativeRelease = reinterpret_cast<NativeRelease>(nativeAddresses[0]);
  profile.nativeReset = reinterpret_cast<NativeReset>(nativeAddresses[1]);
  profile.nativeDrawInstanced =
      reinterpret_cast<NativeDrawInstanced>(nativeAddresses[2]);
  profile.nativeRsSetViewports =
      reinterpret_cast<NativeRsSetViewports>(nativeAddresses[3]);
  profile.nativeRsSetScissorRects =
      reinterpret_cast<NativeRsSetScissorRects>(nativeAddresses[4]);
  profile.nativeSetPipelineState =
      reinterpret_cast<NativeSetPipelineState>(nativeAddresses[5]);
  profile.nativeSetDescriptorHeaps =
      reinterpret_cast<NativeSetDescriptorHeaps>(nativeAddresses[6]);
  profile.nativeSetComputeRootSignature =
      reinterpret_cast<NativeSetComputeRootSignature>(nativeAddresses[7]);
  profile.nativeSetGraphicsRootSignature =
      reinterpret_cast<NativeSetGraphicsRootSignature>(nativeAddresses[8]);
  profile.nativeSetComputeRootDescriptorTable =
      reinterpret_cast<NativeSetComputeRootDescriptorTable>(nativeAddresses[9]);
  profile.nativeSetGraphicsRootDescriptorTable =
      reinterpret_cast<NativeSetGraphicsRootDescriptorTable>(
          nativeAddresses[10]);
  profile.nativeOmSetRenderTargets =
      reinterpret_cast<NativeOmSetRenderTargets>(nativeAddresses[11]);
  g_graphicsCommandListVtableHookProfileCount.store(profileIndex + 1,
                                                    std::memory_order_release);

  std::size_t installedCount = 0;
  for (; installedCount < requiredSlots.size(); ++installedCount) {
    auto **const slot = originalVtable + requiredSlots[installedCount];
    const auto previous = InterlockedCompareExchangePointer(
        reinterpret_cast<void *volatile *>(slot), hookAddresses[installedCount],
        nativeAddresses[installedCount]);
    if (previous != nativeAddresses[installedCount]) {
      break;
    }
  }
  if (installedCount != requiredSlots.size()) {
    while (installedCount != 0) {
      --installedCount;
      auto **const slot = originalVtable + requiredSlots[installedCount];
      InterlockedCompareExchangePointer(
          reinterpret_cast<void *volatile *>(slot),
          nativeAddresses[installedCount], hookAddresses[installedCount]);
    }
    DWORD ignoredProtection = 0;
    VirtualProtect(protectedBegin, protectedByteCount, oldProtection,
                   &ignoredProtection);
    g_graphicsCommandListVtableHookProfileCount.store(
        profileIndex, std::memory_order_release);
    profile = {};
    BridgeVgpuHostCommandListHookFailure = ERROR_INVALID_STATE;
    return false;
  }

  DWORD ignoredProtection = 0;
  if (!VirtualProtect(protectedBegin, protectedByteCount, oldProtection,
                      &ignoredProtection)) {
    const auto error = GetLastError();
    for (std::size_t index = 0; index < requiredSlots.size(); ++index) {
      auto **const slot = originalVtable + requiredSlots[index];
      InterlockedCompareExchangePointer(
          reinterpret_cast<void *volatile *>(slot), nativeAddresses[index],
          hookAddresses[index]);
    }
    VirtualProtect(protectedBegin, protectedByteCount, oldProtection,
                   &ignoredProtection);
    g_graphicsCommandListVtableHookProfileCount.store(
        profileIndex, std::memory_order_release);
    profile = {};
    BridgeVgpuHostCommandListHookFailure = error;
    return false;
  }

  auto *const record =
      RegisterGraphicsCommandListHookLocked(commandList, profile);
  if (record == nullptr) {
    BridgeVgpuHostCommandListHookFailure = ERROR_INSUFFICIENT_BUFFER;
    return false;
  }
  EmitPatchEvent("host_command_list_shared_vtable_hook_install",
                 static_cast<std::uint32_t>(profileIndex + 1));
  return true;
}

void RemoveGraphicsCommandListHooks() noexcept {
  std::scoped_lock lock(g_graphicsCommandListHookMutex);
  std::uint32_t failure = ERROR_SUCCESS;
  const std::array<std::size_t, 12> requiredSlots{
      kReleaseVtableIndex,
      kResetVtableIndex,
      kDrawInstancedVtableIndex,
      kRsSetViewportsVtableIndex,
      kRsSetScissorRectsVtableIndex,
      kSetPipelineStateVtableIndex,
      kSetDescriptorHeapsVtableIndex,
      kSetComputeRootSignatureVtableIndex,
      kSetGraphicsRootSignatureVtableIndex,
      kSetComputeRootDescriptorTableVtableIndex,
      kSetGraphicsRootDescriptorTableVtableIndex,
      kOmSetRenderTargetsVtableIndex};
  const std::array<void *, 12> hookAddresses{
      reinterpret_cast<void *>(&VgpuHostReleaseHook),
      reinterpret_cast<void *>(&VgpuHostResetHook),
      reinterpret_cast<void *>(&VgpuHostDrawInstancedHook),
      reinterpret_cast<void *>(&VgpuHostRsSetViewportsHook),
      reinterpret_cast<void *>(&VgpuHostRsSetScissorRectsHook),
      reinterpret_cast<void *>(&VgpuHostSetPipelineStateHook),
      reinterpret_cast<void *>(&VgpuHostSetDescriptorHeapsHook),
      reinterpret_cast<void *>(&VgpuHostSetComputeRootSignatureHook),
      reinterpret_cast<void *>(&VgpuHostSetGraphicsRootSignatureHook),
      reinterpret_cast<void *>(&VgpuHostSetComputeRootDescriptorTableHook),
      reinterpret_cast<void *>(&VgpuHostSetGraphicsRootDescriptorTableHook),
      reinterpret_cast<void *>(&VgpuHostOmSetRenderTargetsHook)};
  constexpr auto protectedByteCount =
      (kOmSetRenderTargetsVtableIndex - kReleaseVtableIndex + 1) *
      sizeof(void *);

  const auto profileCount = g_graphicsCommandListVtableHookProfileCount.load(
      std::memory_order_acquire);
  for (std::size_t profileIndex = profileCount; profileIndex != 0;
       --profileIndex) {
    auto &profile = g_graphicsCommandListVtableHookProfiles[profileIndex - 1];
    auto **const sharedVtable = profile.vtable;
    if (sharedVtable == nullptr) {
      continue;
    }
    const std::array<void *, 12> nativeAddresses{
        reinterpret_cast<void *>(profile.nativeRelease),
        reinterpret_cast<void *>(profile.nativeReset),
        reinterpret_cast<void *>(profile.nativeDrawInstanced),
        reinterpret_cast<void *>(profile.nativeRsSetViewports),
        reinterpret_cast<void *>(profile.nativeRsSetScissorRects),
        reinterpret_cast<void *>(profile.nativeSetPipelineState),
        reinterpret_cast<void *>(profile.nativeSetDescriptorHeaps),
        reinterpret_cast<void *>(profile.nativeSetComputeRootSignature),
        reinterpret_cast<void *>(profile.nativeSetGraphicsRootSignature),
        reinterpret_cast<void *>(profile.nativeSetComputeRootDescriptorTable),
        reinterpret_cast<void *>(profile.nativeSetGraphicsRootDescriptorTable),
        reinterpret_cast<void *>(profile.nativeOmSetRenderTargets)};
    auto **const protectedBegin = sharedVtable + kReleaseVtableIndex;
    DWORD oldProtection = 0;
    if (!VirtualProtect(protectedBegin, protectedByteCount, PAGE_READWRITE,
                        &oldProtection)) {
      failure = GetLastError();
      continue;
    }
    for (std::size_t index = 0; index < requiredSlots.size(); ++index) {
      auto **const slot = sharedVtable + requiredSlots[index];
      if (*slot == hookAddresses[index] && nativeAddresses[index] != nullptr) {
        const auto previous = InterlockedCompareExchangePointer(
            reinterpret_cast<void *volatile *>(slot), nativeAddresses[index],
            hookAddresses[index]);
        if (previous != hookAddresses[index]) {
          failure = ERROR_INVALID_STATE;
        }
      } else if (*slot != nativeAddresses[index]) {
        failure = ERROR_INVALID_STATE;
      }
    }
    DWORD ignoredProtection = 0;
    if (!VirtualProtect(protectedBegin, protectedByteCount, oldProtection,
                        &ignoredProtection)) {
      failure = GetLastError();
    }
  }

  const auto count =
      g_graphicsCommandListHookCount.load(std::memory_order_acquire);
  for (std::size_t index = 0; index < count; ++index) {
    auto &record = g_graphicsCommandListHooks[index];
    record.commandList.store(nullptr, std::memory_order_release);
    ClearGraphicsCommandListHookRecord(record);
  }
  g_graphicsCommandListHookCount.store(0, std::memory_order_release);
  g_graphicsCommandListActiveHookCount = 0;
  for (std::size_t index = 0; index < profileCount; ++index) {
    g_graphicsCommandListVtableHookProfiles[index] = {};
  }
  g_graphicsCommandListVtableHookProfileCount.store(0,
                                                    std::memory_order_release);
  BridgeVgpuHostCommandListHookCount = 0;
  BridgeVgpuHostCommandListHookFailure = failure;
  EmitPatchEvent("host_command_list_shared_vtable_hook_remove", failure);
}

void CaptureGeneratedShaderArtifact(const wchar_t *targetProfile,
                                    std::uint64_t sequence,
                                    const wchar_t *artifact,
                                    const wchar_t *extension, const void *data,
                                    std::uint64_t dataSize) noexcept;

HRESULT STDMETHODCALLTYPE VgpuCreateComputePipelineStateHook(
    ID3D12Device *const device,
    const D3D12_COMPUTE_PIPELINE_STATE_DESC *const description,
    const IID &interfaceId, void **const pipelineState) noexcept {
  const auto native =
      g_nativeCreateComputePipelineState.load(std::memory_order_acquire);
  if (native == nullptr) {
    BridgeVgpuComputePipelineStateHookFailure = ERROR_INVALID_FUNCTION;
    return E_UNEXPECTED;
  }

  const auto createCount = g_computePipelineStateCreateCount.fetch_add(
                               1, std::memory_order_relaxed) +
                           1;
  BridgeVgpuComputePipelineStateCreateCount = createCount;

  std::array<std::uint8_t, 32> shaderDigest{};
  const auto shaderSize =
      description == nullptr ? 0 : description->CS.BytecodeLength;
  const bool hashed =
      description != nullptr && description->CS.pShaderBytecode != nullptr &&
      shaderSize <= (std::numeric_limits<std::size_t>::max)() &&
      xeo3::vgpu::detail::HashBytesSha256(description->CS.pShaderBytecode,
                                          static_cast<std::size_t>(shaderSize),
                                          shaderDigest);
  const bool matches =
      hashed && xeo3::vgpu::detail::MatchesAc6Pso341ComputeShaderFingerprint(
                    static_cast<std::size_t>(shaderSize), shaderDigest);

  BridgeVgpuComputePipelineStateLastShaderSize = shaderSize;
  if (hashed) {
    std::array<std::uint64_t, 4> hashWords{};
    std::memcpy(hashWords.data(), shaderDigest.data(), shaderDigest.size());
    BridgeVgpuComputePipelineStateLastShaderHash0 = hashWords[0];
    BridgeVgpuComputePipelineStateLastShaderHash1 = hashWords[1];
    BridgeVgpuComputePipelineStateLastShaderHash2 = hashWords[2];
    BridgeVgpuComputePipelineStateLastShaderHash3 = hashWords[3];
  }

  std::uint64_t matchCount = 0;
  if (matches) {
    matchCount = g_computePipelineStateFingerprintMatchCount.fetch_add(
                     1, std::memory_order_relaxed) +
                 1;
    BridgeVgpuComputePipelineStateFingerprintMatchCount = matchCount;
    CaptureGeneratedShaderArtifact(L"cs_6_0", matchCount, L"pso341-original",
                                   L"dxil", description->CS.pShaderBytecode,
                                   shaderSize);
    EmitPatchEvent("compute_pipeline_state_341_match", 341, matchCount);
  }

  const auto result = native(device, description, interfaceId, pipelineState);
  auto *const output = pipelineState == nullptr ? nullptr : *pipelineState;
  BridgeVgpuComputePipelineStateLastCreateResult =
      static_cast<std::uint32_t>(result);
  BridgeVgpuComputePipelineStateLastCreateOutput =
      reinterpret_cast<std::uintptr_t>(output);

  if (matches && SUCCEEDED(result) && output != nullptr) {
    ID3D12PipelineState *typedPipelineState = nullptr;
    const auto queryResult =
        reinterpret_cast<IUnknown *>(output)->QueryInterface(
            IID_PPV_ARGS(&typedPipelineState));
    if (SUCCEEDED(queryResult) && typedPipelineState != nullptr) {
      PublishOwnedAc6Pso341PipelineState(typedPipelineState);
    } else {
      BridgeVgpuComputePipelineStateHookFailure =
          static_cast<std::uint32_t>(queryResult);
      EmitPatchEvent("compute_pipeline_state_341_query_failure",
                     BridgeVgpuComputePipelineStateHookFailure, matchCount);
    }
  }
  return result;
}

void CaptureFailedGraphicsPipeline(
    ID3D12Device *const device,
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC *const description,
    const std::uint64_t createSequence, const std::uint64_t failureSequence,
    const HRESULT result) noexcept {
  BridgeVgpuPipelineStateLastFailureResult = static_cast<std::uint32_t>(result);
  BridgeVgpuPipelineStateLastFailureCreateSequence = createSequence;
  if (description == nullptr || failureSequence > 32) {
    return;
  }

  // Retain the actual attempted stages, including GS linkage, rather than
  // only the VS/PS fingerprint of the original (possibly replaced) descriptor.
  const D3D12_SHADER_BYTECODE stages[]{description->VS, description->PS,
                                      description->GS, description->HS,
                                      description->DS};
  constexpr const wchar_t *names[]{L"vertex", L"pixel", L"geometry", L"hull",
                                    L"domain"};
  for (std::size_t index = 0; index < std::size(stages); ++index) {
    CaptureGeneratedShaderArtifact(L"pso_failure", failureSequence, names[index],
                                    L"dxil", stages[index].pShaderBytecode,
                                    stages[index].BytecodeLength);
  }

  // Pointer-redacted, native x64 D3D12 descriptor; shader lengths and all fixed
  // state remain available for an offline CreateGraphicsPipelineState replay.
  auto descriptor = *description;
  descriptor.pRootSignature = nullptr;
  descriptor.VS.pShaderBytecode = nullptr;
  descriptor.PS.pShaderBytecode = nullptr;
  descriptor.GS.pShaderBytecode = nullptr;
  descriptor.HS.pShaderBytecode = nullptr;
  descriptor.DS.pShaderBytecode = nullptr;
  descriptor.StreamOutput.pSODeclaration = nullptr;
  descriptor.StreamOutput.pBufferStrides = nullptr;
  descriptor.InputLayout.pInputElementDescs = nullptr;
  descriptor.CachedPSO.pCachedBlob = nullptr;
  CaptureGeneratedShaderArtifact(L"pso_failure", failureSequence, L"descriptor",
                                  L"bin", &descriptor, sizeof(descriptor));

  wchar_t path[512]{};
  const auto pathLength = swprintf_s(
      path, std::size(path),
      L"D:\\Games\\AC6 shit\\XeO3-AC6-lab\\ProbeLogs\\"
      L"ac6-shader-%lu-%04llu-pso_failure-diagnostics.txt",
      GetCurrentProcessId(), static_cast<unsigned long long>(failureSequence));
  if (pathLength <= 0) {
    return;
  }
  const auto file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    BridgeVgpuShaderCaptureFailure = GetLastError();
    return;
  }
  AppendDiagnosticLine(file,
      "create_sequence=%" PRIu64 "\nfailure_sequence=%" PRIu64
      "\nresult=0x%08X\nthread_id=%lu\ndescriptor_size=%zu\n",
      createSequence, failureSequence, static_cast<std::uint32_t>(result),
      GetCurrentThreadId(), sizeof(descriptor));
  CaptureD3d12InfoQueue(device, file);
  CloseHandle(file);
}

HRESULT STDMETHODCALLTYPE VgpuCreateGraphicsPipelineStateHook(
    ID3D12Device *device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *description,
    const IID &interfaceId, void **pipelineState) noexcept;
HRESULT STDMETHODCALLTYPE VgpuCreatePipelineStateHook(
    ID3D12Device2 *device, const D3D12_PIPELINE_STATE_STREAM_DESC *description,
    const IID &interfaceId, void **pipelineState) noexcept;
void STDMETHODCALLTYPE VgpuCreateConstantBufferViewHook(
    ID3D12Device *device, const D3D12_CONSTANT_BUFFER_VIEW_DESC *description,
    D3D12_CPU_DESCRIPTOR_HANDLE destinationDescriptor) noexcept;

bool EnsureGraphicsPipelineStateHook(ID3D12Device *const device) noexcept {
  if (device == nullptr) {
    BridgeVgpuPipelineStateHookFailure = ERROR_INVALID_PARAMETER;
    return false;
  }

  auto ***const object = reinterpret_cast<void ***>(device);
  if (object == nullptr || *object == nullptr) {
    BridgeVgpuPipelineStateHookFailure = ERROR_INVALID_ADDRESS;
    return false;
  }
  auto **const slot = *object + kCreateGraphicsPipelineStateVtableIndex;

  std::scoped_lock lock(g_pipelineStateHookMutex);
  if (g_pipelineStateVtableSlot == slot &&
      *slot == reinterpret_cast<void *>(&VgpuCreateGraphicsPipelineStateHook)) {
    return true;
  }
  if (g_pipelineStateVtableSlot != nullptr) {
    BridgeVgpuPipelineStateHookFailure = ERROR_ALREADY_EXISTS;
    return false;
  }

  auto *const nativeAddress = *slot;
  if (nativeAddress == nullptr ||
      nativeAddress ==
          reinterpret_cast<void *>(&VgpuCreateGraphicsPipelineStateHook)) {
    BridgeVgpuPipelineStateHookFailure = ERROR_INVALID_FUNCTION;
    return false;
  }

  DWORD oldProtection = 0;
  if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &oldProtection)) {
    BridgeVgpuPipelineStateHookFailure = GetLastError();
    return false;
  }

  const auto native =
      reinterpret_cast<NativeCreateGraphicsPipelineState>(nativeAddress);
  g_nativeCreateGraphicsPipelineState.store(native, std::memory_order_release);
  InterlockedExchangePointer(
      reinterpret_cast<void *volatile *>(slot),
      reinterpret_cast<void *>(&VgpuCreateGraphicsPipelineStateHook));

  DWORD ignoredProtection = 0;
  if (!VirtualProtect(slot, sizeof(*slot), oldProtection, &ignoredProtection)) {
    InterlockedExchangePointer(reinterpret_cast<void *volatile *>(slot),
                               nativeAddress);
    VirtualProtect(slot, sizeof(*slot), oldProtection, &ignoredProtection);
    g_nativeCreateGraphicsPipelineState.store(nullptr,
                                              std::memory_order_release);
    BridgeVgpuPipelineStateHookFailure = GetLastError();
    return false;
  }

  g_pipelineStateVtableSlot = slot;
  BridgeVgpuPipelineStateHookInstalled = 1;
  BridgeVgpuPipelineStateHookFailure = ERROR_SUCCESS;
  EmitPatchEvent(
      "pipeline_state_hook_install",
      static_cast<std::uint32_t>(kCreateGraphicsPipelineStateVtableIndex));
  return true;
}

void RemoveGraphicsPipelineStateHook() noexcept {
  std::scoped_lock lock(g_pipelineStateHookMutex);
  auto **const slot = g_pipelineStateVtableSlot;
  if (slot == nullptr) {
    return;
  }

  const auto hookAddress =
      reinterpret_cast<void *>(&VgpuCreateGraphicsPipelineStateHook);
  const auto native =
      g_nativeCreateGraphicsPipelineState.load(std::memory_order_acquire);
  if (*slot != hookAddress || native == nullptr) {
    BridgeVgpuPipelineStateHookFailure = ERROR_INVALID_STATE;
    g_pipelineStateVtableSlot = nullptr;
    g_nativeCreateGraphicsPipelineState.store(nullptr,
                                              std::memory_order_release);
    BridgeVgpuPipelineStateHookInstalled = 0;
    EmitPatchEvent("pipeline_state_hook_remove_failure",
                   BridgeVgpuPipelineStateHookFailure);
    return;
  }

  DWORD oldProtection = 0;
  if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &oldProtection)) {
    BridgeVgpuPipelineStateHookFailure = GetLastError();
    EmitPatchEvent("pipeline_state_hook_remove_failure",
                   BridgeVgpuPipelineStateHookFailure);
    return;
  }
  InterlockedExchangePointer(reinterpret_cast<void *volatile *>(slot),
                             reinterpret_cast<void *>(native));
  DWORD ignoredProtection = 0;
  if (!VirtualProtect(slot, sizeof(*slot), oldProtection, &ignoredProtection)) {
    BridgeVgpuPipelineStateHookFailure = GetLastError();
  } else {
    BridgeVgpuPipelineStateHookFailure = ERROR_SUCCESS;
  }
  g_pipelineStateVtableSlot = nullptr;
  g_nativeCreateGraphicsPipelineState.store(nullptr, std::memory_order_release);
  BridgeVgpuPipelineStateHookInstalled = 0;
  EmitPatchEvent("pipeline_state_hook_remove",
                 BridgeVgpuPipelineStateHookFailure);
}

bool EnsureComputePipelineStateHook(ID3D12Device *const device) noexcept {
  if (device == nullptr) {
    BridgeVgpuComputePipelineStateHookFailure = ERROR_INVALID_PARAMETER;
    return false;
  }

  auto ***const object = reinterpret_cast<void ***>(device);
  if (object == nullptr || *object == nullptr) {
    BridgeVgpuComputePipelineStateHookFailure = ERROR_INVALID_ADDRESS;
    return false;
  }
  auto **const slot = *object + kCreateComputePipelineStateVtableIndex;

  std::scoped_lock lock(g_computePipelineStateHookMutex);
  if (g_computePipelineStateVtableSlot == slot &&
      *slot == reinterpret_cast<void *>(&VgpuCreateComputePipelineStateHook)) {
    return true;
  }
  if (g_computePipelineStateVtableSlot != nullptr) {
    BridgeVgpuComputePipelineStateHookFailure = ERROR_ALREADY_EXISTS;
    return false;
  }

  auto *const nativeAddress = *slot;
  if (nativeAddress == nullptr ||
      nativeAddress ==
          reinterpret_cast<void *>(&VgpuCreateComputePipelineStateHook)) {
    BridgeVgpuComputePipelineStateHookFailure = ERROR_INVALID_FUNCTION;
    return false;
  }

  DWORD oldProtection = 0;
  if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &oldProtection)) {
    BridgeVgpuComputePipelineStateHookFailure = GetLastError();
    return false;
  }

  const auto native =
      reinterpret_cast<NativeCreateComputePipelineState>(nativeAddress);
  g_nativeCreateComputePipelineState.store(native, std::memory_order_release);
  InterlockedExchangePointer(
      reinterpret_cast<void *volatile *>(slot),
      reinterpret_cast<void *>(&VgpuCreateComputePipelineStateHook));

  DWORD ignoredProtection = 0;
  if (!VirtualProtect(slot, sizeof(*slot), oldProtection, &ignoredProtection)) {
    InterlockedExchangePointer(reinterpret_cast<void *volatile *>(slot),
                               nativeAddress);
    VirtualProtect(slot, sizeof(*slot), oldProtection, &ignoredProtection);
    g_nativeCreateComputePipelineState.store(nullptr,
                                             std::memory_order_release);
    BridgeVgpuComputePipelineStateHookFailure = GetLastError();
    return false;
  }

  g_computePipelineStateVtableSlot = slot;
  BridgeVgpuComputePipelineStateHookInstalled = 1;
  BridgeVgpuComputePipelineStateHookFailure = ERROR_SUCCESS;
  EmitPatchEvent(
      "compute_pipeline_state_hook_install",
      static_cast<std::uint32_t>(kCreateComputePipelineStateVtableIndex));
  return true;
}

void RemoveComputePipelineStateHook() noexcept {
  std::scoped_lock lock(g_computePipelineStateHookMutex);
  auto **const slot = g_computePipelineStateVtableSlot;
  if (slot != nullptr) {
    const auto hookAddress =
        reinterpret_cast<void *>(&VgpuCreateComputePipelineStateHook);
    const auto native =
        g_nativeCreateComputePipelineState.load(std::memory_order_acquire);
    if (*slot == hookAddress && native != nullptr) {
      DWORD oldProtection = 0;
      if (VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &oldProtection)) {
        InterlockedCompareExchangePointer(
            reinterpret_cast<void *volatile *>(slot),
            reinterpret_cast<void *>(native), hookAddress);
        DWORD ignoredProtection = 0;
        if (!VirtualProtect(slot, sizeof(*slot), oldProtection,
                            &ignoredProtection)) {
          BridgeVgpuComputePipelineStateHookFailure = GetLastError();
        }
      } else {
        BridgeVgpuComputePipelineStateHookFailure = GetLastError();
      }
    } else if (*slot != reinterpret_cast<void *>(native)) {
      BridgeVgpuComputePipelineStateHookFailure = ERROR_INVALID_STATE;
    }
  }

  g_computePipelineStateVtableSlot = nullptr;
  g_nativeCreateComputePipelineState.store(nullptr, std::memory_order_release);
  BridgeVgpuComputePipelineStateHookInstalled = 0;
  auto *const pipelineState =
      g_ac6Pso341PipelineState.exchange(nullptr, std::memory_order_acq_rel);
  if (pipelineState != nullptr) {
    pipelineState->Release();
  }
  {
    std::scoped_lock replacementLock(g_computePipelineReplacementMutex);
    auto *const replacementPipelineState =
        g_ac6Pso341ReplacementPipelineState.exchange(nullptr,
                                                     std::memory_order_acq_rel);
    if (replacementPipelineState != nullptr) {
      replacementPipelineState->Release();
    }
    g_computePipelineReplacementCreationAttempted = false;
  }
  {
    std::scoped_lock fingerprintLock(g_computePipelineFingerprintMutex);
    for (std::size_t index = 0; index < g_computePipelineFingerprintCount;
         ++index) {
      auto *const fingerprintedPipelineState =
          g_computePipelineFingerprints[index].pipelineState;
      if (fingerprintedPipelineState != nullptr) {
        fingerprintedPipelineState->Release();
      }
      g_computePipelineFingerprints[index] = {};
    }
    g_computePipelineFingerprintCount = 0;
  }
  BridgeVgpuTransfer341PipelineState = 0;
  BridgeVgpuTransfer341ReplacementPipelineState = 0;
  EmitPatchEvent("compute_pipeline_state_hook_remove",
                 BridgeVgpuComputePipelineStateHookFailure);
}

void InvalidatePixConstantDescriptor(const D3D12_CPU_DESCRIPTOR_HANDLE handle) noexcept {
  std::scoped_lock lock(g_transfer341MappingMutex);
  g_constantBufferDescriptors[ConstantBufferDescriptorIndex(handle.ptr)] = {handle.ptr, 0};
  if (g_fixedConstantDescriptors.Resolve(handle.ptr) != 0)
    g_fixedConstantDescriptors.Assign(handle.ptr, 0);
}

void STDMETHODCALLTYPE VgpuPixCreateShaderResourceViewHook(
    ID3D12Device *const device, ID3D12Resource *const resource,
    const D3D12_SHADER_RESOURCE_VIEW_DESC *const description,
    const D3D12_CPU_DESCRIPTOR_HANDLE destination) noexcept {
  const auto native = g_nativeCreateShaderResourceView.load(std::memory_order_acquire);
  if (native == nullptr) return;
  InvalidatePixConstantDescriptor(destination);
  native(device, resource, description, destination);
}

void STDMETHODCALLTYPE VgpuPixCreateUnorderedAccessViewHook(
    ID3D12Device *const device, ID3D12Resource *const resource,
    ID3D12Resource *const counter,
    const D3D12_UNORDERED_ACCESS_VIEW_DESC *const description,
    const D3D12_CPU_DESCRIPTOR_HANDLE destination) noexcept {
  const auto native = g_nativeCreateUnorderedAccessView.load(std::memory_order_acquire);
  if (native == nullptr) return;
  InvalidatePixConstantDescriptor(destination);
  native(device, resource, counter, description, destination);
}

void RecordPixConstantDescriptorCopyLocked(const std::uintptr_t destination,
                                          const std::uintptr_t source) noexcept {
  const auto &sourceRecord = g_constantBufferDescriptors[ConstantBufferDescriptorIndex(source)];
  const auto address = sourceRecord.cpuDescriptor == source ? sourceRecord.gpuAddress
                                                          : g_fixedConstantDescriptors.Resolve(source);
  // Snapshot the value, not a source alias. Unknown/SRV/UAV sources clear any
  // old CBV at the destination, including descriptor-heap ring reuse.
  g_constantBufferDescriptors[ConstantBufferDescriptorIndex(destination)] = {destination, address};
  BridgeVgpuPixDescriptorCopyCount = g_pixDescriptorCopyCount.fetch_add(1, std::memory_order_relaxed) + 1;
  if (address != 0)
    BridgeVgpuPixConstantCopyCount = g_pixConstantCopyCount.fetch_add(1, std::memory_order_relaxed) + 1;
}

void STDMETHODCALLTYPE VgpuPixCopyDescriptorsSimpleHook(
    ID3D12Device *const device, const UINT count,
    const D3D12_CPU_DESCRIPTOR_HANDLE destination,
    const D3D12_CPU_DESCRIPTOR_HANDLE source,
    const D3D12_DESCRIPTOR_HEAP_TYPE type) noexcept {
  const auto native = g_nativeCopyDescriptorsSimple.load(std::memory_order_acquire);
  if (native == nullptr) return;
  if (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) {
    const auto increment = device->GetDescriptorHandleIncrementSize(type);
    const auto span = static_cast<std::uint64_t>(count) * increment;
    if (increment == 0 || count > 65536 || destination.ptr > UINTPTR_MAX - span ||
        source.ptr > UINTPTR_MAX - span) {
      BridgeVgpuPixDescriptorHookFailure = ERROR_INVALID_DATA;
    } else {
      std::scoped_lock lock(g_transfer341MappingMutex);
      for (std::size_t index = 0; index < count; ++index)
        RecordPixConstantDescriptorCopyLocked(destination.ptr + index * increment,
                                              source.ptr + index * increment);
    }
  }
  native(device, count, destination, source, type);
}

void STDMETHODCALLTYPE VgpuPixCopyDescriptorsHook(
    ID3D12Device *const device, const UINT destinationCount,
    const D3D12_CPU_DESCRIPTOR_HANDLE *const destinations, const UINT *const destinationSizes,
    const UINT sourceCount, const D3D12_CPU_DESCRIPTOR_HANDLE *const sources,
    const UINT *const sourceSizes, const D3D12_DESCRIPTOR_HEAP_TYPE type) noexcept {
  const auto native = g_nativeCopyDescriptors.load(std::memory_order_acquire);
  if (native == nullptr) return;
  if (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) {
    const auto increment = device->GetDescriptorHandleIncrementSize(type);
    const auto validate = [increment](UINT count, const D3D12_CPU_DESCRIPTOR_HANDLE *handles,
                                     const UINT *sizes, std::uint64_t &total) noexcept {
      total = 0;
      if (count > 1024 || (count != 0 && handles == nullptr) || increment == 0) return false;
      for (UINT index = 0; index < count; ++index) {
        const auto size = sizes != nullptr ? sizes[index] : 1U;
        total += size;
        if (total > 65536 || handles[index].ptr > UINTPTR_MAX -
                static_cast<std::uint64_t>(size) * increment) return false;
      }
      return true;
    };
    std::uint64_t destinationsTotal = 0, sourcesTotal = 0;
    if (!validate(destinationCount, destinations, destinationSizes, destinationsTotal) ||
        !validate(sourceCount, sources, sourceSizes, sourcesTotal) ||
        destinationsTotal != sourcesTotal) {
      BridgeVgpuPixDescriptorHookFailure = ERROR_INVALID_DATA;
    } else {
      std::scoped_lock lock(g_transfer341MappingMutex);
      UINT sourceRange = 0, sourceOffset = 0;
      for (UINT destinationRange = 0; destinationRange < destinationCount; ++destinationRange) {
        const auto count = destinationSizes != nullptr ? destinationSizes[destinationRange] : 1U;
        for (UINT offset = 0; offset < count; ++offset) {
          while (sourceRange < sourceCount && sourceOffset >=
                 (sourceSizes != nullptr ? sourceSizes[sourceRange] : 1U)) {
            ++sourceRange;
            sourceOffset = 0;
          }
          RecordPixConstantDescriptorCopyLocked(
              destinations[destinationRange].ptr + static_cast<std::uint64_t>(offset) * increment,
              sources[sourceRange].ptr + static_cast<std::uint64_t>(sourceOffset++) * increment);
        }
      }
    }
  }
  native(device, destinationCount, destinations, destinationSizes, sourceCount, sources, sourceSizes, type);
}

constexpr std::array<std::size_t, 4> kPixDescriptorSlots{
    18, 19, kCopyDescriptorsVtableIndex, kCopyDescriptorsSimpleVtableIndex};

std::array<void *, 4> PixDescriptorHookAddresses() noexcept {
  return {reinterpret_cast<void *>(&VgpuPixCreateShaderResourceViewHook),
          reinterpret_cast<void *>(&VgpuPixCreateUnorderedAccessViewHook),
          reinterpret_cast<void *>(&VgpuPixCopyDescriptorsHook),
          reinterpret_cast<void *>(&VgpuPixCopyDescriptorsSimpleHook)};
}

bool EnsurePixDescriptorHooks(ID3D12Device *const device) noexcept {
  if (GetModuleHandleW(L"WinPixGpuCapturer.dll") == nullptr) return true;
  if (device == nullptr) return false;
  std::scoped_lock lock(g_pixDescriptorHookMutex);
  auto **const vtable = *reinterpret_cast<void ***>(device);
  if (vtable == nullptr) return false;
  if (g_pixDescriptorVtable == vtable) return true;
  if (g_pixDescriptorVtable != nullptr) {
    BridgeVgpuPixDescriptorHookFailure = ERROR_ALREADY_EXISTS;
    return false;
  }
  const auto hooks = PixDescriptorHookAddresses();
  std::array<void *, 4> originals{};
  for (std::size_t index = 0; index < originals.size(); ++index) {
    originals[index] = vtable[kPixDescriptorSlots[index]];
    if (originals[index] == nullptr || originals[index] == hooks[index]) {
      BridgeVgpuPixDescriptorHookFailure = ERROR_INVALID_FUNCTION;
      return false;
    }
  }
  constexpr auto bytes = (kCopyDescriptorsSimpleVtableIndex - 18 + 1) * sizeof(void *);
  DWORD protection = 0;
  if (!VirtualProtect(vtable + 18, bytes, PAGE_READWRITE, &protection)) {
    BridgeVgpuPixDescriptorHookFailure = GetLastError();
    return false;
  }
  g_nativeCreateShaderResourceView.store(reinterpret_cast<NativeCreateShaderResourceView>(originals[0]), std::memory_order_release);
  g_nativeCreateUnorderedAccessView.store(reinterpret_cast<NativeCreateUnorderedAccessView>(originals[1]), std::memory_order_release);
  g_nativeCopyDescriptors.store(reinterpret_cast<NativeCopyDescriptors>(originals[2]), std::memory_order_release);
  g_nativeCopyDescriptorsSimple.store(reinterpret_cast<NativeCopyDescriptorsSimple>(originals[3]), std::memory_order_release);
  std::size_t installed = 0;
  for (; installed < originals.size(); ++installed) {
    if (InterlockedCompareExchangePointer(
            reinterpret_cast<void *volatile *>(vtable + kPixDescriptorSlots[installed]),
            hooks[installed], originals[installed]) != originals[installed]) break;
  }
  DWORD ignored = 0;
  if (installed != originals.size()) {
    while (installed != 0) {
      --installed;
      InterlockedCompareExchangePointer(
          reinterpret_cast<void *volatile *>(vtable + kPixDescriptorSlots[installed]),
          originals[installed], hooks[installed]);
    }
    VirtualProtect(vtable + 18, bytes, protection, &ignored);
    BridgeVgpuPixDescriptorHookFailure = ERROR_INVALID_STATE;
    return false;
  }
  g_pixDescriptorVtable = vtable;
  if (!VirtualProtect(vtable + 18, bytes, protection, &ignored)) {
    BridgeVgpuPixDescriptorHookFailure = GetLastError();
    return false;
  }
  EmitPatchEvent("pix_descriptor_hooks_install", 4);
  return true;
}

void RemovePixDescriptorHooks() noexcept {
  std::scoped_lock lock(g_pixDescriptorHookMutex);
  auto **const vtable = g_pixDescriptorVtable;
  if (vtable == nullptr) return;
  const auto hooks = PixDescriptorHookAddresses();
  const std::array<void *, 4> originals{
      reinterpret_cast<void *>(g_nativeCreateShaderResourceView.load(std::memory_order_acquire)),
      reinterpret_cast<void *>(g_nativeCreateUnorderedAccessView.load(std::memory_order_acquire)),
      reinterpret_cast<void *>(g_nativeCopyDescriptors.load(std::memory_order_acquire)),
      reinterpret_cast<void *>(g_nativeCopyDescriptorsSimple.load(std::memory_order_acquire))};
  constexpr auto bytes = (kCopyDescriptorsSimpleVtableIndex - 18 + 1) * sizeof(void *);
  DWORD protection = 0;
  if (!VirtualProtect(vtable + 18, bytes, PAGE_READWRITE, &protection)) {
    BridgeVgpuPixDescriptorHookFailure = GetLastError();
    return;
  }
  for (std::size_t index = 0; index < originals.size(); ++index)
    InterlockedCompareExchangePointer(
        reinterpret_cast<void *volatile *>(vtable + kPixDescriptorSlots[index]), originals[index], hooks[index]);
  DWORD ignored = 0;
  if (!VirtualProtect(vtable + 18, bytes, protection, &ignored))
    BridgeVgpuPixDescriptorHookFailure = GetLastError();
  g_pixDescriptorVtable = nullptr;
  {
    std::scoped_lock mappingLock(g_transfer341MappingMutex);
    g_fixedConstantDescriptors.Clear();
  }
  EmitPatchEvent("pix_descriptor_hooks_remove", BridgeVgpuPixDescriptorHookFailure);
}

void STDMETHODCALLTYPE VgpuCreateRenderTargetViewHook(
    ID3D12Device *const device, ID3D12Resource *const resource,
    const D3D12_RENDER_TARGET_VIEW_DESC *const description,
    const D3D12_CPU_DESCRIPTOR_HANDLE destination) noexcept {
  const auto native =
      g_nativeCreateRenderTargetView.load(std::memory_order_acquire);
  if (native == nullptr) {
    BridgeVgpuRtvHookFailure = ERROR_INVALID_STATE;
    return;
  }
  native(device, resource, description, destination);
  RenderTargetDescriptorRecord record{};
  record.cpuDescriptor = destination.ptr;
  if (resource != nullptr) {
    const auto desc = resource->GetDesc();
    const auto dimension = description != nullptr ? description->ViewDimension
        : desc.SampleDesc.Count > 1 ? D3D12_RTV_DIMENSION_TEXTURE2DMS
                                   : D3D12_RTV_DIMENSION_TEXTURE2D;
    // The captured transfers use one ordinary 2D target. Array and null
    // descriptors deliberately invalidate any older record at this handle.
    if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D &&
        desc.DepthOrArraySize == 1 &&
        (dimension == D3D12_RTV_DIMENSION_TEXTURE2D ||
         dimension == D3D12_RTV_DIMENSION_TEXTURE2DMS)) {
      const auto mip = description != nullptr &&
                               dimension == D3D12_RTV_DIMENSION_TEXTURE2D
                           ? description->Texture2D.MipSlice : 0U;
      if (mip < desc.MipLevels && mip < 32) {
        record.format = description != nullptr ? description->Format : desc.Format;
        record.sampleCount = desc.SampleDesc.Count;
        record.width = (std::max)(UINT64{1}, desc.Width >> mip);
        record.height = (std::max)(1U, desc.Height >> mip);
      }
    }
  }
  std::scoped_lock lock(g_rtvDescriptorMutex);
  g_rtvDescriptors[(destination.ptr >> 4) % kRtvDescriptorTableSize] = record;
}

bool EnsureRenderTargetViewHook(ID3D12Device *const device) noexcept {
  if (GetModuleHandleW(L"WinPixGpuCapturer.dll") == nullptr) return true;
  if (device == nullptr) return false;
  if (!EnsurePixDescriptorHooks(device)) return false;
  auto **const vtable = *reinterpret_cast<void ***>(device);
  if (vtable == nullptr) return false;
  auto **const slot = vtable + kCreateRenderTargetViewVtableIndex;
  std::scoped_lock lock(g_rtvHookMutex);
  auto *const hookAddress = reinterpret_cast<void *>(&VgpuCreateRenderTargetViewHook);
  if (g_rtvVtableSlot == slot && *slot == hookAddress) return true;
  if (g_rtvVtableSlot != nullptr) {
    BridgeVgpuRtvHookFailure = ERROR_ALREADY_EXISTS;
    return false;
  }
  MEMORY_BASIC_INFORMATION information{};
  if (VirtualQuery(slot, &information, sizeof(information)) == 0 ||
      information.State != MEM_COMMIT ||
      (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0 ||
      *slot == nullptr || *slot == hookAddress) {
    BridgeVgpuRtvHookFailure = ERROR_INVALID_ADDRESS;
    return false;
  }
  auto *const original = *slot;
  DWORD protection = 0;
  if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &protection)) {
    BridgeVgpuRtvHookFailure = GetLastError();
    return false;
  }
  g_nativeCreateRenderTargetView.store(
      reinterpret_cast<NativeCreateRenderTargetView>(original), std::memory_order_release);
  const auto previous = InterlockedCompareExchangePointer(
      reinterpret_cast<void *volatile *>(slot), hookAddress, original);
  DWORD ignored = 0;
  const auto restored = VirtualProtect(slot, sizeof(*slot), protection, &ignored);
  if (previous != original || !restored) {
    const auto error = restored ? ERROR_INVALID_STATE : GetLastError();
    if (previous == original)
      InterlockedCompareExchangePointer(
          reinterpret_cast<void *volatile *>(slot), original, hookAddress);
    VirtualProtect(slot, sizeof(*slot), protection, &ignored);
    g_nativeCreateRenderTargetView.store(nullptr, std::memory_order_release);
    BridgeVgpuRtvHookFailure = error;
    return false;
  }
  g_rtvVtableSlot = slot;
  BridgeVgpuRtvHookInstalled = 1;
  BridgeVgpuRtvHookFailure = ERROR_SUCCESS;
  EmitPatchEvent("pix_rtv_hook_install", kCreateRenderTargetViewVtableIndex);
  return true;
}

void RemoveRenderTargetViewHook() noexcept {
  std::scoped_lock lock(g_rtvHookMutex);
  auto **const slot = g_rtvVtableSlot;
  if (slot == nullptr) return;
  auto *const hookAddress = reinterpret_cast<void *>(&VgpuCreateRenderTargetViewHook);
  auto *const original = reinterpret_cast<void *>(
      g_nativeCreateRenderTargetView.load(std::memory_order_acquire));
  if (*slot == hookAddress && original != nullptr) {
    DWORD protection = 0;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &protection)) {
      BridgeVgpuRtvHookFailure = GetLastError();
      return;
    }
    InterlockedCompareExchangePointer(
        reinterpret_cast<void *volatile *>(slot), original, hookAddress);
    DWORD ignored = 0;
    if (!VirtualProtect(slot, sizeof(*slot), protection, &ignored))
      BridgeVgpuRtvHookFailure = GetLastError();
  } else if (*slot != original) {
    BridgeVgpuRtvHookFailure = ERROR_INVALID_STATE;
  }
  g_rtvVtableSlot = nullptr;
  g_nativeCreateRenderTargetView.store(nullptr, std::memory_order_release);
  BridgeVgpuRtvHookInstalled = 0;
  {
    std::scoped_lock descriptorLock(g_rtvDescriptorMutex);
    g_rtvDescriptors.fill({});
  }
  EmitPatchEvent("pix_rtv_hook_remove", BridgeVgpuRtvHookFailure);
}

bool EnsureConstantBufferViewHook(ID3D12Device *const device) noexcept {
  if (device == nullptr) {
    return false;
  }

  auto ***const object = reinterpret_cast<void ***>(device);
  if (object == nullptr || *object == nullptr) {
    return false;
  }
  auto **const slot = *object + kCreateConstantBufferViewVtableIndex;

  std::scoped_lock lock(g_constantBufferViewHookMutex);
  if (g_constantBufferViewVtableSlot == slot &&
      *slot == reinterpret_cast<void *>(&VgpuCreateConstantBufferViewHook)) {
    return true;
  }
  if (g_constantBufferViewVtableSlot != nullptr) {
    return false;
  }

  auto *const nativeAddress = *slot;
  if (nativeAddress == nullptr ||
      nativeAddress ==
          reinterpret_cast<void *>(&VgpuCreateConstantBufferViewHook)) {
    return false;
  }

  DWORD oldProtection = 0;
  if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &oldProtection)) {
    return false;
  }

  const auto native =
      reinterpret_cast<NativeCreateConstantBufferView>(nativeAddress);
  g_nativeCreateConstantBufferView.store(native, std::memory_order_release);
  InterlockedExchangePointer(
      reinterpret_cast<void *volatile *>(slot),
      reinterpret_cast<void *>(&VgpuCreateConstantBufferViewHook));

  DWORD ignoredProtection = 0;
  if (!VirtualProtect(slot, sizeof(*slot), oldProtection, &ignoredProtection)) {
    InterlockedExchangePointer(reinterpret_cast<void *volatile *>(slot),
                               nativeAddress);
    VirtualProtect(slot, sizeof(*slot), oldProtection, &ignoredProtection);
    g_nativeCreateConstantBufferView.store(nullptr, std::memory_order_release);
    return false;
  }

  g_constantBufferViewVtableSlot = slot;
  EmitPatchEvent(
      "constant_buffer_view_hook_install",
      static_cast<std::uint32_t>(kCreateConstantBufferViewVtableIndex));
  return true;
}

void RemoveConstantBufferViewHook() noexcept {
  std::scoped_lock lock(g_constantBufferViewHookMutex);
  auto **const slot = g_constantBufferViewVtableSlot;
  if (slot == nullptr) {
    return;
  }

  const auto hookAddress =
      reinterpret_cast<void *>(&VgpuCreateConstantBufferViewHook);
  const auto native =
      g_nativeCreateConstantBufferView.load(std::memory_order_acquire);
  if (*slot == hookAddress && native != nullptr) {
    DWORD oldProtection = 0;
    if (VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &oldProtection)) {
      InterlockedCompareExchangePointer(
          reinterpret_cast<void *volatile *>(slot),
          reinterpret_cast<void *>(native), hookAddress);
      DWORD ignoredProtection = 0;
      VirtualProtect(slot, sizeof(*slot), oldProtection, &ignoredProtection);
    }
  }
  g_constantBufferViewVtableSlot = nullptr;
  g_nativeCreateConstantBufferView.store(nullptr, std::memory_order_release);
  EmitPatchEvent("constant_buffer_view_hook_remove", ERROR_SUCCESS);
}

void STDMETHODCALLTYPE VgpuCreateConstantBufferViewHook(
    ID3D12Device *const device,
    const D3D12_CONSTANT_BUFFER_VIEW_DESC *const description,
    const D3D12_CPU_DESCRIPTOR_HANDLE destinationDescriptor) noexcept {
  const auto native =
      g_nativeCreateConstantBufferView.load(std::memory_order_acquire);
  if (native == nullptr) {
    return;
  }

  RecordAndPatchConstantBufferDescriptor(
      description, destinationDescriptor,
      reinterpret_cast<std::uintptr_t>(device));
  native(device, description, destinationDescriptor);
}

bool EnsurePipelineStateStreamHook(ID3D12Device *const device) noexcept {
  if (device == nullptr) {
    BridgeVgpuPipelineStreamHookFailure = ERROR_INVALID_PARAMETER;
    return false;
  }

  ID3D12Device2 *device2 = nullptr;
  const auto queryResult = device->QueryInterface(
      __uuidof(ID3D12Device2), reinterpret_cast<void **>(&device2));
  if (FAILED(queryResult) || device2 == nullptr) {
    BridgeVgpuPipelineStreamHookFailure =
        static_cast<std::uint32_t>(queryResult);
    return false;
  }

  auto ***const object = reinterpret_cast<void ***>(device2);
  if (object == nullptr || *object == nullptr) {
    device2->Release();
    BridgeVgpuPipelineStreamHookFailure = ERROR_INVALID_ADDRESS;
    return false;
  }
  auto **const slot = *object + kCreatePipelineStateVtableIndex;

  std::scoped_lock lock(g_pipelineStreamHookMutex);
  if (g_pipelineStreamVtableSlot == slot &&
      *slot == reinterpret_cast<void *>(&VgpuCreatePipelineStateHook)) {
    device2->Release();
    return true;
  }
  if (g_pipelineStreamVtableSlot != nullptr) {
    device2->Release();
    BridgeVgpuPipelineStreamHookFailure = ERROR_ALREADY_EXISTS;
    return false;
  }

  auto *const nativeAddress = *slot;
  if (nativeAddress == nullptr ||
      nativeAddress == reinterpret_cast<void *>(&VgpuCreatePipelineStateHook)) {
    device2->Release();
    BridgeVgpuPipelineStreamHookFailure = ERROR_INVALID_FUNCTION;
    return false;
  }

  DWORD oldProtection = 0;
  if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &oldProtection)) {
    device2->Release();
    BridgeVgpuPipelineStreamHookFailure = GetLastError();
    return false;
  }

  const auto native =
      reinterpret_cast<NativeCreatePipelineState>(nativeAddress);
  g_nativeCreatePipelineState.store(native, std::memory_order_release);
  InterlockedExchangePointer(
      reinterpret_cast<void *volatile *>(slot),
      reinterpret_cast<void *>(&VgpuCreatePipelineStateHook));

  DWORD ignoredProtection = 0;
  if (!VirtualProtect(slot, sizeof(*slot), oldProtection, &ignoredProtection)) {
    InterlockedExchangePointer(reinterpret_cast<void *volatile *>(slot),
                               nativeAddress);
    VirtualProtect(slot, sizeof(*slot), oldProtection, &ignoredProtection);
    g_nativeCreatePipelineState.store(nullptr, std::memory_order_release);
    device2->Release();
    BridgeVgpuPipelineStreamHookFailure = GetLastError();
    return false;
  }

  g_pipelineStreamVtableSlot = slot;
  BridgeVgpuPipelineStreamHookInstalled = 1;
  BridgeVgpuPipelineStreamHookFailure = ERROR_SUCCESS;
  device2->Release();
  EmitPatchEvent("pipeline_stream_hook_install",
                 static_cast<std::uint32_t>(kCreatePipelineStateVtableIndex));
  return true;
}

void RemovePipelineStateStreamHook() noexcept {
  std::scoped_lock lock(g_pipelineStreamHookMutex);
  auto **const slot = g_pipelineStreamVtableSlot;
  if (slot == nullptr) {
    return;
  }

  const auto hookAddress =
      reinterpret_cast<void *>(&VgpuCreatePipelineStateHook);
  const auto native =
      g_nativeCreatePipelineState.load(std::memory_order_acquire);
  if (*slot != hookAddress || native == nullptr) {
    BridgeVgpuPipelineStreamHookFailure = ERROR_INVALID_STATE;
    g_pipelineStreamVtableSlot = nullptr;
    g_nativeCreatePipelineState.store(nullptr, std::memory_order_release);
    BridgeVgpuPipelineStreamHookInstalled = 0;
    EmitPatchEvent("pipeline_stream_hook_remove_failure",
                   BridgeVgpuPipelineStreamHookFailure);
    return;
  }

  DWORD oldProtection = 0;
  if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &oldProtection)) {
    BridgeVgpuPipelineStreamHookFailure = GetLastError();
    EmitPatchEvent("pipeline_stream_hook_remove_failure",
                   BridgeVgpuPipelineStreamHookFailure);
    return;
  }
  InterlockedExchangePointer(reinterpret_cast<void *volatile *>(slot),
                             reinterpret_cast<void *>(native));
  DWORD ignoredProtection = 0;
  if (!VirtualProtect(slot, sizeof(*slot), oldProtection, &ignoredProtection)) {
    BridgeVgpuPipelineStreamHookFailure = GetLastError();
  } else {
    BridgeVgpuPipelineStreamHookFailure = ERROR_SUCCESS;
  }
  g_pipelineStreamVtableSlot = nullptr;
  g_nativeCreatePipelineState.store(nullptr, std::memory_order_release);
  BridgeVgpuPipelineStreamHookInstalled = 0;
  EmitPatchEvent("pipeline_stream_hook_remove",
                 BridgeVgpuPipelineStreamHookFailure);
}

HRESULT STDMETHODCALLTYPE VgpuCreateGraphicsPipelineStateHook(
    ID3D12Device *const device,
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC *const description,
    const IID &interfaceId, void **const pipelineState) noexcept {
  const auto native =
      g_nativeCreateGraphicsPipelineState.load(std::memory_order_acquire);
  if (native == nullptr) {
    BridgeVgpuPipelineStateHookFailure = ERROR_INVALID_FUNCTION;
    return E_UNEXPECTED;
  }

  const auto createCount =
      g_pipelineStateCreateCount.fetch_add(1, std::memory_order_relaxed) + 1;
  BridgeVgpuPipelineStateCreateCount = createCount;
  xeo3::vgpu::GraphicsPipelineSignature signature{};
  const bool hasSignature = description != nullptr;
  if (hasSignature) {
    signature = BuildGraphicsPipelineSignature(*description);
  }
  const auto invokeNative =
      [&](const D3D12_GRAPHICS_PIPELINE_STATE_DESC *const nativeDescription) {
        const auto result =
            native(device, nativeDescription, interfaceId, pipelineState);
        auto *const output =
            pipelineState == nullptr ? nullptr : *pipelineState;
        if (SUCCEEDED(result) && output != nullptr && hasSignature) {
          RecordObservedPipelineState(output, signature, createCount);
        }
        BridgeVgpuPipelineStateLastCreateResult =
            static_cast<std::uint32_t>(result);
        BridgeVgpuPipelineStateLastCreateOutput =
            reinterpret_cast<std::uintptr_t>(output);
        EmitPipelineResult(createCount, result, output);
        if (FAILED(result) || output == nullptr) {
          const auto failureCount = g_pipelineStateCreateFailureCount.fetch_add(
                                        1, std::memory_order_relaxed) +
                                    1;
          BridgeVgpuPipelineStateCreateFailureCount = failureCount;
          CaptureFailedGraphicsPipeline(device, nativeDescription, createCount,
                                         failureCount, result);
          EmitPatchEvent("pipeline_state_create_failure",
                         static_cast<std::uint32_t>(result), failureCount);
        }
        return result;
      };
  if (description == nullptr) {
    return invokeNative(description);
  }

  EmitPipelineSignature(signature, createCount);
  BridgeVgpuPipelineLastVertexShaderSize = signature.vertexShaderSize;
  BridgeVgpuPipelineLastPixelShaderSize = signature.pixelShaderSize;
  BridgeVgpuPipelineLastSampleCount = signature.sampleCount;
  BridgeVgpuPipelineLastRenderTargetFormat = signature.renderTarget0Format;
  BridgeVgpuPipelineLastInputElementCount = signature.inputElementCount;
  BridgeVgpuPipelineLastExpectedInputLayout =
      signature.hasExpectedInputLayout ? 1U : 0U;
  BridgeVgpuPipelineLastExpectedFixedState =
      signature.hasExpectedFixedState ? 1U : 0U;
  if (signature.hasExpectedInputLayout && signature.hasExpectedFixedState) {
    BridgeVgpuEdramRestoreFingerprintCount =
        BridgeVgpuEdramRestoreFingerprintCount + 1;
  }

  if (xeo3::vgpu::detail::IsAc6Pso535CullPipelineDescriptor(signature)) {
    const auto candidateCount = g_pso535CullCandidateCount.fetch_add(
                                    1, std::memory_order_relaxed) +
                                1;
    BridgeVgpuPso535CullCandidateCount = candidateCount;
    EmitPatchEvent("pso535_cull_pipeline_candidate", signature.cullMode,
                   candidateCount);
  }

  if (xeo3::vgpu::detail::IsAc6Pso535CullPipeline(signature)) {
    const auto matchCount = g_pso535CullFingerprintMatchCount.fetch_add(
                                1, std::memory_order_relaxed) +
                            1;
    BridgeVgpuPso535CullFingerprintMatchCount = matchCount;
    BridgeVgpuPso535CullLastOriginalMode = signature.cullMode;
    EmitPatchEvent("pso535_cull_pipeline_match", signature.cullMode,
                   matchCount);

    if (BridgeVgpuPso535CullFixEnabled != 0) {
      auto patchedDescription = *description;
      patchedDescription.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
      BridgeVgpuPso535CullLastReplacementMode =
          static_cast<std::uint32_t>(D3D12_CULL_MODE_NONE);

      const auto fixedResult = invokeNative(&patchedDescription);
      if (SUCCEEDED(fixedResult)) {
        const auto fixedCount = g_pso535CullFixPipelineCount.fetch_add(
                                    1, std::memory_order_relaxed) +
                                1;
        BridgeVgpuPso535CullFixPipelineCount = fixedCount;
        BridgeVgpuPso535CullFixFailure = ERROR_SUCCESS;
        EmitPatchEvent("pso535_cull_pipeline_fixed",
                       BridgeVgpuPso535CullLastReplacementMode, fixedCount);
        return fixedResult;
      }

      BridgeVgpuPso535CullFixFailure =
          static_cast<std::uint32_t>(fixedResult);
      EmitPatchEvent("pso535_cull_pipeline_fix_failure",
                     BridgeVgpuPso535CullFixFailure, matchCount);
      if (pipelineState != nullptr) {
        *pipelineState = nullptr;
      }
      return invokeNative(description);
    }
  }

  if (xeo3::vgpu::detail::IsAc6EdramScalePipelineDescriptor(signature)) {
    const auto candidateCount =
        g_edramScaleCandidateCount.fetch_add(1, std::memory_order_relaxed) + 1;
    BridgeVgpuEdramScaleCandidateCount = candidateCount;
    std::array<std::uint64_t, 4> vertexHashWords{};
    std::array<std::uint64_t, 4> pixelHashWords{};
    std::memcpy(vertexHashWords.data(), signature.vertexShaderSha256.data(),
                signature.vertexShaderSha256.size());
    std::memcpy(pixelHashWords.data(), signature.pixelShaderSha256.data(),
                signature.pixelShaderSha256.size());
    BridgeVgpuEdramScaleCandidateVsHash0 = vertexHashWords[0];
    BridgeVgpuEdramScaleCandidateVsHash1 = vertexHashWords[1];
    BridgeVgpuEdramScaleCandidateVsHash2 = vertexHashWords[2];
    BridgeVgpuEdramScaleCandidateVsHash3 = vertexHashWords[3];
    BridgeVgpuEdramScaleCandidatePsHash0 = pixelHashWords[0];
    BridgeVgpuEdramScaleCandidatePsHash1 = pixelHashWords[1];
    BridgeVgpuEdramScaleCandidatePsHash2 = pixelHashWords[2];
    BridgeVgpuEdramScaleCandidatePsHash3 = pixelHashWords[3];
    EmitPatchEvent("edram_scale_pipeline_candidate", 620, candidateCount);
  }

  if (xeo3::vgpu::detail::IsAc6EdramScalePipeline(signature)) {
    const auto matchCount = g_edramScaleFingerprintMatchCount.fetch_add(
                                1, std::memory_order_relaxed) +
                            1;
    BridgeVgpuEdramScaleFingerprintMatchCount = matchCount;
    EmitPatchEvent("edram_scale_pipeline_match", 620, matchCount);

    if (BridgeVgpuEdramScaleFixEnabled != 0) {
      auto patchedDescription = *description;
      std::size_t replacementSize = 0;
      patchedDescription.PS.pShaderBytecode =
          xeo3::vgpu::detail::GetAc6EdramScaleFixPixelShader(replacementSize);
      patchedDescription.PS.BytecodeLength = replacementSize;

      ID3D12RootSignature *ownedRootSignature = nullptr;
      if (patchedDescription.pRootSignature == nullptr) {
        const auto rootResult = CreateEmbeddedRootSignature(
            device, *description, &ownedRootSignature);
        if (FAILED(rootResult) || ownedRootSignature == nullptr) {
          BridgeVgpuEdramScaleFixFailure =
              static_cast<std::uint32_t>(rootResult);
          EmitPatchEvent("edram_scale_root_signature_failure",
                         BridgeVgpuEdramScaleFixFailure, matchCount);
          return invokeNative(description);
        }
        patchedDescription.pRootSignature = ownedRootSignature;
      }

      const auto fixedResult = invokeNative(&patchedDescription);
      if (ownedRootSignature != nullptr) {
        ownedRootSignature->Release();
      }
      if (SUCCEEDED(fixedResult)) {
        const auto fixedCount = g_edramScaleFixPipelineCount.fetch_add(
                                    1, std::memory_order_relaxed) +
                                1;
        BridgeVgpuEdramScaleFixPipelineCount = fixedCount;
        BridgeVgpuEdramScaleFixFailure = ERROR_SUCCESS;
        EmitPatchEvent("edram_scale_pipeline_fixed", 620, fixedCount);
        return fixedResult;
      }

      BridgeVgpuEdramScaleFixFailure = static_cast<std::uint32_t>(fixedResult);
      EmitPatchEvent("edram_scale_pipeline_fix_failure",
                     BridgeVgpuEdramScaleFixFailure, matchCount);
      if (pipelineState != nullptr) {
        *pipelineState = nullptr;
      }
      return invokeNative(description);
    }
  }

  if (xeo3::vgpu::detail::IsAc6EdramLoadPipelineDescriptor(signature)) {
    const auto candidateCount =
        g_edramLoadCandidateCount.fetch_add(1, std::memory_order_relaxed) + 1;
    BridgeVgpuEdramLoadCandidateCount = candidateCount;
    std::array<std::uint64_t, 4> vertexHashWords{};
    std::array<std::uint64_t, 4> pixelHashWords{};
    std::memcpy(vertexHashWords.data(), signature.vertexShaderSha256.data(),
                signature.vertexShaderSha256.size());
    std::memcpy(pixelHashWords.data(), signature.pixelShaderSha256.data(),
                signature.pixelShaderSha256.size());
    BridgeVgpuEdramLoadCandidateVsHash0 = vertexHashWords[0];
    BridgeVgpuEdramLoadCandidateVsHash1 = vertexHashWords[1];
    BridgeVgpuEdramLoadCandidateVsHash2 = vertexHashWords[2];
    BridgeVgpuEdramLoadCandidateVsHash3 = vertexHashWords[3];
    BridgeVgpuEdramLoadCandidatePsHash0 = pixelHashWords[0];
    BridgeVgpuEdramLoadCandidatePsHash1 = pixelHashWords[1];
    BridgeVgpuEdramLoadCandidatePsHash2 = pixelHashWords[2];
    BridgeVgpuEdramLoadCandidatePsHash3 = pixelHashWords[3];
    EmitPatchEvent("edram_load_pipeline_candidate", 523, candidateCount);
  }

  if (xeo3::vgpu::detail::IsAc6EdramLoadPipeline(signature)) {
    const auto matchCount = g_edramLoadFingerprintMatchCount.fetch_add(
                                1, std::memory_order_relaxed) +
                            1;
    BridgeVgpuEdramLoadFingerprintMatchCount = matchCount;
    EmitPatchEvent("edram_load_pipeline_match", 523, matchCount);

    if (BridgeVgpuEdramLoadFixEnabled != 0) {
      auto patchedDescription = *description;
      std::size_t replacementSize = 0;
      patchedDescription.PS.pShaderBytecode =
          xeo3::vgpu::detail::GetAc6EdramLoadFixPixelShader(replacementSize);
      patchedDescription.PS.BytecodeLength = replacementSize;

      ID3D12RootSignature *ownedRootSignature = nullptr;
      if (patchedDescription.pRootSignature == nullptr) {
        const auto rootResult = CreateEmbeddedRootSignature(
            device, *description, &ownedRootSignature);
        if (FAILED(rootResult) || ownedRootSignature == nullptr) {
          BridgeVgpuEdramLoadFixFailure =
              static_cast<std::uint32_t>(rootResult);
          EmitPatchEvent("edram_load_root_signature_failure",
                         BridgeVgpuEdramLoadFixFailure, matchCount);
          return invokeNative(description);
        }
        patchedDescription.pRootSignature = ownedRootSignature;
      }

      const auto fixedResult = invokeNative(&patchedDescription);
      if (ownedRootSignature != nullptr) {
        ownedRootSignature->Release();
      }
      if (SUCCEEDED(fixedResult)) {
        const auto fixedCount = g_edramLoadFixPipelineCount.fetch_add(
                                    1, std::memory_order_relaxed) +
                                1;
        BridgeVgpuEdramLoadFixPipelineCount = fixedCount;
        BridgeVgpuEdramLoadFixFailure = ERROR_SUCCESS;
        EmitPatchEvent("edram_load_pipeline_fixed", 523, fixedCount);
        return fixedResult;
      }

      BridgeVgpuEdramLoadFixFailure = static_cast<std::uint32_t>(fixedResult);
      EmitPatchEvent("edram_load_pipeline_fix_failure",
                     BridgeVgpuEdramLoadFixFailure, matchCount);
      if (pipelineState != nullptr) {
        *pipelineState = nullptr;
      }
      return invokeNative(description);
    }
  }

  if (!xeo3::vgpu::detail::IsAc6CorruptEdramRestorePipeline(signature)) {
    return invokeNative(description);
  }

  auto patchedDescription = *description;
  patchedDescription.BlendState.RenderTarget[0].RenderTargetWriteMask = 0;
  const auto suppressedCount =
      g_suppressedEdramRestorePsoCount.fetch_add(1, std::memory_order_relaxed) +
      1;
  BridgeVgpuSuppressedEdramRestorePsoCount = suppressedCount;
  EmitPatchEvent("edram_restore_color_write_suppressed", 536, suppressedCount);
  return invokeNative(&patchedDescription);
}

HRESULT STDMETHODCALLTYPE VgpuCreatePipelineStateHook(
    ID3D12Device2 *const device,
    const D3D12_PIPELINE_STATE_STREAM_DESC *const description,
    const IID &interfaceId, void **const pipelineState) noexcept {
  const auto native =
      g_nativeCreatePipelineState.load(std::memory_order_acquire);
  if (native == nullptr) {
    BridgeVgpuPipelineStreamHookFailure = ERROR_INVALID_FUNCTION;
    return E_UNEXPECTED;
  }

  const auto createCount =
      g_pipelineStreamCreateCount.fetch_add(1, std::memory_order_relaxed) + 1;
  BridgeVgpuPipelineStreamCreateCount = createCount;
  xeo3::vgpu::GraphicsPipelineSignature signature{};
  std::size_t pixelShaderBytecodeOffset =
      (std::numeric_limits<std::size_t>::max)();
  const bool hasSignature =
      description != nullptr &&
      description->pPipelineStateSubobjectStream != nullptr &&
      description->SizeInBytes != 0 &&
      xeo3::vgpu::detail::ExtractGraphicsPipelineStreamSignature(
          description->pPipelineStateSubobjectStream, description->SizeInBytes,
          signature, pixelShaderBytecodeOffset);
  if (hasSignature) {
    const auto parseCount =
        g_pipelineStreamParseCount.fetch_add(1, std::memory_order_relaxed) + 1;
    BridgeVgpuPipelineStreamParseCount = parseCount;
    EmitPipelineSignature(signature, createCount);
  } else if (description != nullptr && description->SizeInBytes != 0) {
    const auto failureCount = g_pipelineStreamParseFailureCount.fetch_add(
                                  1, std::memory_order_relaxed) +
                              1;
    BridgeVgpuPipelineStreamParseFailureCount = failureCount;
    EmitPatchEvent(
        "pipeline_stream_parse_failure",
        static_cast<std::uint32_t>((std::min)(description->SizeInBytes,
                                              static_cast<SIZE_T>(UINT_MAX))),
        failureCount);
  }

  const auto invokeNative =
      [&](const D3D12_PIPELINE_STATE_STREAM_DESC *const nativeDescription) {
        const auto result =
            native(device, nativeDescription, interfaceId, pipelineState);
        auto *const output =
            pipelineState == nullptr ? nullptr : *pipelineState;
        BridgeVgpuPipelineStreamLastCreateResult =
            static_cast<std::uint32_t>(result);
        BridgeVgpuPipelineStreamLastCreateOutput =
            reinterpret_cast<std::uintptr_t>(output);
        EmitPipelineResult(createCount, result, output);
        if (SUCCEEDED(result) && output != nullptr && hasSignature) {
          RecordObservedPipelineState(output, signature, createCount);
        }
        return result;
      };

  if (!hasSignature ||
      pixelShaderBytecodeOffset == (std::numeric_limits<std::size_t>::max)()) {
    return invokeNative(description);
  }

  if (xeo3::vgpu::detail::IsAc6EdramScalePipelineDescriptor(signature)) {
    const auto candidateCount =
        g_edramScaleCandidateCount.fetch_add(1, std::memory_order_relaxed) + 1;
    BridgeVgpuEdramScaleCandidateCount = candidateCount;
    std::array<std::uint64_t, 4> vertexHashWords{};
    std::array<std::uint64_t, 4> pixelHashWords{};
    std::memcpy(vertexHashWords.data(), signature.vertexShaderSha256.data(),
                signature.vertexShaderSha256.size());
    std::memcpy(pixelHashWords.data(), signature.pixelShaderSha256.data(),
                signature.pixelShaderSha256.size());
    BridgeVgpuEdramScaleCandidateVsHash0 = vertexHashWords[0];
    BridgeVgpuEdramScaleCandidateVsHash1 = vertexHashWords[1];
    BridgeVgpuEdramScaleCandidateVsHash2 = vertexHashWords[2];
    BridgeVgpuEdramScaleCandidateVsHash3 = vertexHashWords[3];
    BridgeVgpuEdramScaleCandidatePsHash0 = pixelHashWords[0];
    BridgeVgpuEdramScaleCandidatePsHash1 = pixelHashWords[1];
    BridgeVgpuEdramScaleCandidatePsHash2 = pixelHashWords[2];
    BridgeVgpuEdramScaleCandidatePsHash3 = pixelHashWords[3];
    EmitPatchEvent("edram_scale_stream_candidate", 620, candidateCount);
  }

  if (xeo3::vgpu::detail::IsAc6EdramLoadPipelineDescriptor(signature)) {
    const auto candidateCount =
        g_edramLoadCandidateCount.fetch_add(1, std::memory_order_relaxed) + 1;
    BridgeVgpuEdramLoadCandidateCount = candidateCount;
    std::array<std::uint64_t, 4> vertexHashWords{};
    std::array<std::uint64_t, 4> pixelHashWords{};
    std::memcpy(vertexHashWords.data(), signature.vertexShaderSha256.data(),
                signature.vertexShaderSha256.size());
    std::memcpy(pixelHashWords.data(), signature.pixelShaderSha256.data(),
                signature.pixelShaderSha256.size());
    BridgeVgpuEdramLoadCandidateVsHash0 = vertexHashWords[0];
    BridgeVgpuEdramLoadCandidateVsHash1 = vertexHashWords[1];
    BridgeVgpuEdramLoadCandidateVsHash2 = vertexHashWords[2];
    BridgeVgpuEdramLoadCandidateVsHash3 = vertexHashWords[3];
    BridgeVgpuEdramLoadCandidatePsHash0 = pixelHashWords[0];
    BridgeVgpuEdramLoadCandidatePsHash1 = pixelHashWords[1];
    BridgeVgpuEdramLoadCandidatePsHash2 = pixelHashWords[2];
    BridgeVgpuEdramLoadCandidatePsHash3 = pixelHashWords[3];
    EmitPatchEvent("edram_load_stream_candidate", 523, candidateCount);
  }

  const bool fixScale = BridgeVgpuEdramScaleFixEnabled != 0 &&
                        xeo3::vgpu::detail::IsAc6EdramScalePipeline(signature);
  const bool fixLoad = BridgeVgpuEdramLoadFixEnabled != 0 &&
                       xeo3::vgpu::detail::IsAc6EdramLoadPipeline(signature);
  if (!fixScale && !fixLoad) {
    return invokeNative(description);
  }

  if (pixelShaderBytecodeOffset > description->SizeInBytes ||
      sizeof(D3D12_SHADER_BYTECODE) >
          description->SizeInBytes - pixelShaderBytecodeOffset) {
    if (fixScale) {
      BridgeVgpuEdramScaleFixFailure = ERROR_INVALID_DATA;
    } else {
      BridgeVgpuEdramLoadFixFailure = ERROR_INVALID_DATA;
    }
    return invokeNative(description);
  }

  VirtualAllocationOwner streamCopy(
      VirtualAlloc(nullptr, description->SizeInBytes, MEM_COMMIT | MEM_RESERVE,
                   PAGE_READWRITE));
  if (streamCopy.get() == nullptr) {
    if (fixScale) {
      BridgeVgpuEdramScaleFixFailure = ERROR_NOT_ENOUGH_MEMORY;
    } else {
      BridgeVgpuEdramLoadFixFailure = ERROR_NOT_ENOUGH_MEMORY;
    }
    return invokeNative(description);
  }
  std::memcpy(streamCopy.get(), description->pPipelineStateSubobjectStream,
              description->SizeInBytes);
  auto *const pixelShader = reinterpret_cast<D3D12_SHADER_BYTECODE *>(
      static_cast<std::uint8_t *>(streamCopy.get()) +
      pixelShaderBytecodeOffset);
  std::size_t replacementSize = 0;
  pixelShader->pShaderBytecode =
      fixScale
          ? xeo3::vgpu::detail::GetAc6EdramScaleFixPixelShader(replacementSize)
          : xeo3::vgpu::detail::GetAc6EdramLoadFixPixelShader(replacementSize);
  pixelShader->BytecodeLength = replacementSize;
  D3D12_PIPELINE_STATE_STREAM_DESC patchedDescription{description->SizeInBytes,
                                                      streamCopy.get()};

  if (fixScale) {
    const auto matchCount = g_edramScaleFingerprintMatchCount.fetch_add(
                                1, std::memory_order_relaxed) +
                            1;
    BridgeVgpuEdramScaleFingerprintMatchCount = matchCount;
    EmitPatchEvent("edram_scale_stream_match", 620, matchCount);
  } else {
    const auto matchCount = g_edramLoadFingerprintMatchCount.fetch_add(
                                1, std::memory_order_relaxed) +
                            1;
    BridgeVgpuEdramLoadFingerprintMatchCount = matchCount;
    EmitPatchEvent("edram_load_stream_match", 523, matchCount);
  }

  const auto fixedResult = invokeNative(&patchedDescription);
  if (SUCCEEDED(fixedResult)) {
    if (fixScale) {
      const auto fixedCount =
          g_edramScaleFixPipelineCount.fetch_add(1, std::memory_order_relaxed) +
          1;
      BridgeVgpuEdramScaleFixPipelineCount = fixedCount;
      BridgeVgpuEdramScaleFixFailure = ERROR_SUCCESS;
      EmitPatchEvent("edram_scale_stream_fixed", 620, fixedCount);
    } else {
      const auto fixedCount =
          g_edramLoadFixPipelineCount.fetch_add(1, std::memory_order_relaxed) +
          1;
      BridgeVgpuEdramLoadFixPipelineCount = fixedCount;
      BridgeVgpuEdramLoadFixFailure = ERROR_SUCCESS;
      EmitPatchEvent("edram_load_stream_fixed", 523, fixedCount);
    }
    return fixedResult;
  }

  if (fixScale) {
    BridgeVgpuEdramScaleFixFailure = static_cast<std::uint32_t>(fixedResult);
  } else {
    BridgeVgpuEdramLoadFixFailure = static_cast<std::uint32_t>(fixedResult);
  }
  if (pipelineState != nullptr) {
    *pipelineState = nullptr;
  }
  return invokeNative(description);
}

HRESULT STDMETHODCALLTYPE VgpuCreateHeapHook(
    ID3D12Device *const device, const D3D12_HEAP_DESC *const description,
    const IID &interfaceId, void **const heap) noexcept {
  if (device == nullptr || description == nullptr || heap == nullptr) {
    return E_INVALIDARG;
  }
  EnsureGraphicsPipelineStateHook(device);
  EnsureComputePipelineStateHook(device);
  EnsurePipelineStateStreamHook(device);
  EnsureConstantBufferViewHook(device);
  EnsureRenderTargetViewHook(device);

  auto patchedDescription = *description;
  const auto originalFlags = patchedDescription.Flags;
  patchedDescription.Flags = static_cast<D3D12_HEAP_FLAGS>(
      originalFlags & ~D3D12_HEAP_FLAG_CREATE_NOT_ZEROED);
  BridgeVgpuLastHeapFlags = static_cast<std::uint32_t>(originalFlags);
  if (patchedDescription.Flags != originalFlags) {
    const auto count =
        g_zeroedHeapCount.fetch_add(1, std::memory_order_relaxed) + 1;
    BridgeVgpuZeroedHeapCount = count;
    if (count <= 64 || (count & (count - 1)) == 0) {
      EmitPatchEvent("create_heap_zeroed",
                     static_cast<std::uint32_t>(originalFlags), count);
    }
  }

  return device->CreateHeap(&patchedDescription, interfaceId, heap);
}

void ResetDiscardContextLocked() noexcept {
  if (g_discardList != nullptr) {
    g_discardList->Release();
    g_discardList = nullptr;
  }
  if (g_discardAllocator != nullptr) {
    g_discardAllocator->Release();
    g_discardAllocator = nullptr;
  }
  if (g_discardQueue != nullptr) {
    g_discardQueue->Release();
    g_discardQueue = nullptr;
  }
  if (g_discardFence != nullptr) {
    g_discardFence->Release();
    g_discardFence = nullptr;
  }
  if (g_discardDevice != nullptr) {
    g_discardDevice->Release();
    g_discardDevice = nullptr;
  }
  if (g_discardEvent != nullptr) {
    CloseHandle(g_discardEvent);
    g_discardEvent = nullptr;
  }
  g_discardFenceValue = 0;
}

HRESULT EnsureDiscardContextLocked(ID3D12Device *const device) noexcept {
  if (device == nullptr) {
    return E_INVALIDARG;
  }
  if (g_discardDevice == device && g_discardQueue != nullptr &&
      g_discardAllocator != nullptr && g_discardList != nullptr &&
      g_discardFence != nullptr && g_discardEvent != nullptr) {
    return S_OK;
  }

  ResetDiscardContextLocked();
  device->AddRef();
  g_discardDevice = device;

  D3D12_COMMAND_QUEUE_DESC queueDescription{};
  queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  queueDescription.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
  queueDescription.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  queueDescription.NodeMask = 0;

  auto result = device->CreateCommandQueue(&queueDescription,
                                           IID_PPV_ARGS(&g_discardQueue));
  if (FAILED(result)) {
    ResetDiscardContextLocked();
    return result;
  }

  result = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                          IID_PPV_ARGS(&g_discardAllocator));
  if (FAILED(result)) {
    ResetDiscardContextLocked();
    return result;
  }

  result = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                     g_discardAllocator, nullptr,
                                     IID_PPV_ARGS(&g_discardList));
  if (FAILED(result)) {
    ResetDiscardContextLocked();
    return result;
  }
  result = g_discardList->Close();
  if (FAILED(result)) {
    ResetDiscardContextLocked();
    return result;
  }

  result = device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                               IID_PPV_ARGS(&g_discardFence));
  if (FAILED(result)) {
    ResetDiscardContextLocked();
    return result;
  }

  g_discardEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (g_discardEvent == nullptr) {
    result = HRESULT_FROM_WIN32(GetLastError());
    ResetDiscardContextLocked();
    return result;
  }
  return S_OK;
}

HRESULT DiscardPlacedResource(
    ID3D12Device *const device, ID3D12Resource *const resource,
    const D3D12_RESOURCE_FLAGS resourceFlags,
    const PlacedResourceCreationState &creationState) noexcept {
  if (device == nullptr || resource == nullptr) {
    return E_INVALIDARG;
  }

  std::scoped_lock lock(g_discardMutex);
  auto result = EnsureDiscardContextLocked(device);
  if (FAILED(result)) {
    return result;
  }

  result = g_discardAllocator->Reset();
  if (FAILED(result)) {
    ResetDiscardContextLocked();
    return result;
  }
  result = g_discardList->Reset(g_discardAllocator, nullptr);
  if (FAILED(result)) {
    ResetDiscardContextLocked();
    return result;
  }

  if (creationState.usesEnhancedBarriers) {
    ID3D12GraphicsCommandList7 *enhancedList = nullptr;
    result = g_discardList->QueryInterface(IID_PPV_ARGS(&enhancedList));
    if (FAILED(result) || enhancedList == nullptr) {
      ResetDiscardContextLocked();
      return FAILED(result) ? result : E_NOINTERFACE;
    }

    const bool isDepthStencil =
        (resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0;
    const auto discardSync = isDepthStencil ? D3D12_BARRIER_SYNC_DEPTH_STENCIL
                                            : D3D12_BARRIER_SYNC_RENDER_TARGET;
    const auto discardAccess = isDepthStencil
                                   ? D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE
                                   : D3D12_BARRIER_ACCESS_RENDER_TARGET;
    const auto discardLayout = isDepthStencil
                                   ? D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE
                                   : D3D12_BARRIER_LAYOUT_RENDER_TARGET;
    const bool needsTransition = creationState.initialLayout != discardLayout;

    D3D12_TEXTURE_BARRIER transition{};
    transition.SyncBefore = D3D12_BARRIER_SYNC_NONE;
    transition.SyncAfter = discardSync;
    transition.AccessBefore = D3D12_BARRIER_ACCESS_NO_ACCESS;
    transition.AccessAfter = discardAccess;
    transition.LayoutBefore = creationState.initialLayout;
    transition.LayoutAfter = discardLayout;
    transition.pResource = resource;
    transition.Subresources = {
        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, 0, 0, 0, 0, 0,
    };
    transition.Flags = D3D12_TEXTURE_BARRIER_FLAG_NONE;

    D3D12_BARRIER_GROUP barrierGroup{};
    barrierGroup.Type = D3D12_BARRIER_TYPE_TEXTURE;
    barrierGroup.NumBarriers = 1;
    barrierGroup.pTextureBarriers = &transition;
    if (needsTransition) {
      enhancedList->Barrier(1, &barrierGroup);
    }
    g_discardList->DiscardResource(resource, nullptr);
    if (needsTransition) {
      transition.SyncBefore = discardSync;
      transition.SyncAfter = D3D12_BARRIER_SYNC_NONE;
      transition.AccessBefore = discardAccess;
      transition.AccessAfter = D3D12_BARRIER_ACCESS_NO_ACCESS;
      transition.LayoutBefore = discardLayout;
      transition.LayoutAfter = creationState.initialLayout;
      enhancedList->Barrier(1, &barrierGroup);
    }
    enhancedList->Release();
    EmitPatchEvent(
        "discard_enhanced_transition",
        (static_cast<std::uint32_t>(creationState.initialLayout) << 16) |
            static_cast<std::uint32_t>(discardLayout));
  } else {
    const auto discardState =
        (resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0
            ? D3D12_RESOURCE_STATE_DEPTH_WRITE
            : D3D12_RESOURCE_STATE_RENDER_TARGET;
    const bool needsTransition = creationState.initialState != discardState;
    D3D12_RESOURCE_BARRIER transition{};
    transition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    transition.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    transition.Transition.pResource = resource;
    transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    transition.Transition.StateBefore = creationState.initialState;
    transition.Transition.StateAfter = discardState;
    if (needsTransition) {
      g_discardList->ResourceBarrier(1, &transition);
    }
    g_discardList->DiscardResource(resource, nullptr);
    if (needsTransition) {
      std::swap(transition.Transition.StateBefore,
                transition.Transition.StateAfter);
      g_discardList->ResourceBarrier(1, &transition);
    }
    EmitPatchEvent("discard_legacy_barrier",
                   static_cast<std::uint32_t>(creationState.initialState));
  }

  result = g_discardList->Close();
  if (FAILED(result)) {
    ResetDiscardContextLocked();
    return result;
  }

  ID3D12CommandList *const commandLists[]{g_discardList};
  g_discardQueue->ExecuteCommandLists(
      static_cast<UINT>(std::size(commandLists)), commandLists);

  const auto fenceValue = ++g_discardFenceValue;
  result = g_discardQueue->Signal(g_discardFence, fenceValue);
  if (FAILED(result)) {
    ResetDiscardContextLocked();
    return result;
  }
  if (g_discardFence->GetCompletedValue() >= fenceValue) {
    return S_OK;
  }

  result = g_discardFence->SetEventOnCompletion(fenceValue, g_discardEvent);
  if (FAILED(result)) {
    ResetDiscardContextLocked();
    return result;
  }

  constexpr DWORD kDiscardTimeoutMilliseconds = 10'000;
  const auto waitResult =
      WaitForSingleObject(g_discardEvent, kDiscardTimeoutMilliseconds);
  if (waitResult != WAIT_OBJECT_0) {
    result = waitResult == WAIT_TIMEOUT ? HRESULT_FROM_WIN32(ERROR_TIMEOUT)
                                        : HRESULT_FROM_WIN32(GetLastError());
    ResetDiscardContextLocked();
    return result;
  }
  return S_OK;
}

[[maybe_unused]] void MaybeDiscardPlacedResource(
    ID3D12Device *const device, ID3D12Heap *const heap,
    const D3D12_RESOURCE_FLAGS requestedResourceFlags, void *const output,
    const PlacedResourceCreationState &creationState) noexcept {
  if (device == nullptr || heap == nullptr || output == nullptr) {
    EmitPatchEvent("discard_skip_invalid_argument", 0);
    return;
  }

  ID3D12Resource *resource = nullptr;
  const auto queryResult =
      static_cast<IUnknown *>(output)->QueryInterface(IID_PPV_ARGS(&resource));
  if (FAILED(queryResult) || resource == nullptr) {
    BridgeVgpuDiscardFailure = static_cast<std::uint32_t>(queryResult);
    EmitPatchEvent("discard_query_failure",
                   static_cast<std::uint32_t>(queryResult));
    return;
  }

  const auto heapDescription = heap->GetDesc();
  const auto resourceDescription = resource->GetDesc();
  const auto resourceFlags = resourceDescription.Flags;
  BridgeVgpuLastHeapFlags = static_cast<std::uint32_t>(heapDescription.Flags);
  BridgeVgpuLastResourceFlags = static_cast<std::uint32_t>(resourceFlags);
  const auto observedFlags =
      static_cast<std::uint64_t>(
          static_cast<std::uint32_t>(requestedResourceFlags)) |
      (static_cast<std::uint64_t>(static_cast<std::uint32_t>(resourceFlags))
       << 32);
  EmitPatchEvent("placed_resource_observed",
                 static_cast<std::uint32_t>(heapDescription.Flags),
                 observedFlags);

  if ((resourceFlags & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET |
                        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) == 0) {
    resource->Release();
    EmitPatchEvent("discard_skip_non_rt_ds",
                   static_cast<std::uint32_t>(resourceFlags), observedFlags);
    return;
  }

  // CREATE_NOT_ZEROED is absent from GetDesc on the pinned VGPU runtime
  // even though the debug layer retains that creation policy. These
  // hash-pinned call sites only initialize newly created RT/DS resources.
  if ((heapDescription.Flags & D3D12_HEAP_FLAG_CREATE_NOT_ZEROED) == 0) {
    EmitPatchEvent("discard_unreported_not_zeroed_heap",
                   static_cast<std::uint32_t>(heapDescription.Flags),
                   observedFlags);
  }

  const auto discardResult =
      DiscardPlacedResource(device, resource, resourceFlags, creationState);
  resource->Release();
  if (FAILED(discardResult)) {
    BridgeVgpuDiscardFailure = static_cast<std::uint32_t>(discardResult);
    EmitPatchEvent("discard_failure",
                   static_cast<std::uint32_t>(discardResult));
    return;
  }

  const auto count =
      g_discardedResourceCount.fetch_add(1, std::memory_order_relaxed) + 1;
  BridgeVgpuDiscardedResourceCount = count;
  BridgeVgpuDiscardFailure = 0;
  if (count <= 64 || (count & (count - 1)) == 0) {
    EmitPatchEvent("discard_resource",
                   static_cast<std::uint32_t>(resourceFlags), count);
  }
}

void RecordPlacedResourceCall(const std::uintptr_t siteRva) noexcept {
  const auto count =
      g_placedResourceCallCount.fetch_add(1, std::memory_order_relaxed) + 1;
  BridgeVgpuPlacedResourceCallCount = count;
  BridgeVgpuLastPlacedResourceSite = static_cast<std::uint32_t>(siteRva);
  if (count <= 64 || (count & (count - 1)) == 0) {
    EmitPatchEvent("placed_resource_call", static_cast<std::uint32_t>(siteRva),
                   count);
  }
}

HRESULT VgpuCreatePlacedResource2Impl(
    const std::uintptr_t siteRva, ID3D12Device10 *const device,
    ID3D12Heap *const heap, const UINT64 heapOffset,
    const D3D12_RESOURCE_DESC1 *const description,
    const D3D12_BARRIER_LAYOUT initialLayout,
    const D3D12_CLEAR_VALUE *const optimizedClearValue,
    const UINT castableFormatCount, const DXGI_FORMAT *const castableFormats,
    const IID &interfaceId, void **const resource) noexcept {
  RecordPlacedResourceCall(siteRva);
  if (device == nullptr || description == nullptr || resource == nullptr) {
    return E_INVALIDARG;
  }
  EnsureGraphicsPipelineStateHook(static_cast<ID3D12Device *>(device));
  EnsureComputePipelineStateHook(static_cast<ID3D12Device *>(device));
  EnsurePipelineStateStreamHook(static_cast<ID3D12Device *>(device));
  EnsureConstantBufferViewHook(static_cast<ID3D12Device *>(device));
  EnsureRenderTargetViewHook(static_cast<ID3D12Device *>(device));

  D3D12_HEAP_DESC heapDescription{};
  D3D12_RESOURCE_ALLOCATION_INFO allocationInfo{};
  const bool hasPlacementInfo = heap != nullptr;
  if (hasPlacementInfo) {
    heapDescription = heap->GetDesc();
    allocationInfo = device->GetResourceAllocationInfo2(
        heapDescription.Properties.VisibleNodeMask, 1, description, nullptr);
    BridgeVgpuLastHeapFlags = static_cast<std::uint32_t>(heapDescription.Flags);
    BridgeVgpuLastResourceFlags =
        static_cast<std::uint32_t>(description->Flags);
    BridgeVgpuLastHeapType =
        static_cast<std::uint32_t>(heapDescription.Properties.Type);
    BridgeVgpuLastHeapCpuPageProperty =
        static_cast<std::uint32_t>(heapDescription.Properties.CPUPageProperty);
    BridgeVgpuLastHeapMemoryPoolPreference = static_cast<std::uint32_t>(
        heapDescription.Properties.MemoryPoolPreference);
    BridgeVgpuLastHeapOffset = heapOffset;
    BridgeVgpuLastHeapSize = heapDescription.SizeInBytes;
    BridgeVgpuLastAllocationSize = allocationInfo.SizeInBytes;
  }
  const auto committedHeapFlags = static_cast<D3D12_HEAP_FLAGS>(
      xeo3::vgpu::detail::GetCommittedResourceHeapFlags(
          static_cast<std::uint32_t>(heapDescription.Flags)));
  BridgeVgpuLastCommittedHeapFlags =
      static_cast<std::uint32_t>(committedHeapFlags);

  auto recordCommittedFallback = [siteRva](const HRESULT result,
                                           void *const output) noexcept {
    if (SUCCEEDED(result) && output != nullptr) {
      const auto count = g_committedOverflowFallbackCount.fetch_add(
                             1, std::memory_order_relaxed) +
                         1;
      BridgeVgpuCommittedOverflowFallbackCount = count;
      BridgeVgpuCommittedOverflowFallbackFailure = 0;
      EmitPatchEvent("placed_resource_committed_overflow_fallback",
                     static_cast<std::uint32_t>(siteRva), count);
    } else {
      BridgeVgpuCommittedOverflowFallbackFailure =
          static_cast<std::uint32_t>(result);
      EmitPatchEvent("placed_resource_committed_overflow_failure",
                     static_cast<std::uint32_t>(result),
                     static_cast<std::uint64_t>(siteRva));
    }
  };

  const bool placementOverflows =
      hasPlacementInfo &&
      xeo3::vgpu::detail::DoesPlacedResourceOverflowHeap(
          heapDescription.SizeInBytes, heapOffset, allocationInfo.SizeInBytes);
  HRESULT result = S_OK;
  bool usedCommittedOverflowFallback = false;
  *resource = nullptr;
  if (placementOverflows) {
    usedCommittedOverflowFallback = true;
    result = device->CreateCommittedResource3(
        &heapDescription.Properties, committedHeapFlags, description,
        initialLayout, optimizedClearValue, nullptr, castableFormatCount,
        castableFormats, interfaceId, resource);
    recordCommittedFallback(result, *resource);
  } else {
    result = device->CreatePlacedResource2(
        heap, heapOffset, description, initialLayout, optimizedClearValue,
        castableFormatCount, castableFormats, interfaceId, resource);
  }

  bool usedLegacyFallback = false;
  if (!usedCommittedOverflowFallback && FAILED(result) && hasPlacementInfo) {
    if (xeo3::vgpu::detail::ShouldFallbackPlacedResourceToCommitted(
            static_cast<std::uint32_t>(result), *resource != nullptr,
            heapDescription.SizeInBytes, heapOffset,
            allocationInfo.SizeInBytes)) {
      usedCommittedOverflowFallback = true;
      *resource = nullptr;
      result = device->CreateCommittedResource3(
          &heapDescription.Properties, committedHeapFlags, description,
          initialLayout, optimizedClearValue, nullptr, castableFormatCount,
          castableFormats, interfaceId, resource);
      recordCommittedFallback(result, *resource);
    }
  }
  if (!usedCommittedOverflowFallback &&
      xeo3::vgpu::detail::ShouldRetryInvalidModernBufferAsLegacy(
          static_cast<std::uint32_t>(result), *resource != nullptr,
          static_cast<std::uint32_t>(description->Dimension),
          static_cast<std::uint32_t>(initialLayout), castableFormatCount)) {
    const D3D12_RESOURCE_DESC legacyDescription{
        description->Dimension,
        description->Alignment,
        description->Width,
        description->Height,
        description->DepthOrArraySize,
        description->MipLevels,
        description->Format,
        description->SampleDesc,
        description->Layout,
        description->Flags,
    };
    *resource = nullptr;
    result = static_cast<ID3D12Device *>(device)->CreatePlacedResource(
        heap, heapOffset, &legacyDescription, D3D12_RESOURCE_STATE_COMMON,
        optimizedClearValue, interfaceId, resource);
    if (SUCCEEDED(result) && *resource != nullptr) {
      const auto count =
          g_legacyFallbackCount.fetch_add(1, std::memory_order_relaxed) + 1;
      BridgeVgpuLegacyFallbackCount = count;
      BridgeVgpuLegacyFallbackFailure = 0;
      usedLegacyFallback = true;
      EmitPatchEvent("placed_resource_legacy_fallback",
                     static_cast<std::uint32_t>(siteRva), count);
    } else {
      BridgeVgpuLegacyFallbackFailure = static_cast<std::uint32_t>(result);
      EmitPatchEvent("placed_resource_legacy_fallback_failure",
                     static_cast<std::uint32_t>(result),
                     static_cast<std::uint64_t>(siteRva));
    }
  }
  if (FAILED(result)) {
    BridgeVgpuLastPlacedResourceFailure = static_cast<std::uint32_t>(result);
    CaptureDredDiagnostics(static_cast<ID3D12Device *>(device), result);
  }
  if (SUCCEEDED(result) && *resource != nullptr) {
    if (!usedCommittedOverflowFallback) {
      RegisterMappedUploadBuffer(
          static_cast<std::uint32_t>(heapDescription.Properties.Type), false,
          heapOffset, description->Dimension, description->Width, *resource);
    } else {
      RegisterMappedUploadBuffer(
          static_cast<std::uint32_t>(heapDescription.Properties.Type), true, 0,
          description->Dimension, description->Width, *resource);
    }
    if constexpr (kEnableIndependentDiscardQueue) {
      MaybeDiscardPlacedResource(static_cast<ID3D12Device *>(device), heap,
                                 description->Flags, *resource,
                                 PlacedResourceCreationState{
                                     !usedLegacyFallback,
                                     initialLayout,
                                     D3D12_RESOURCE_STATE_COMMON,
                                 });
    }
  }
  return result;
}

HRESULT STDMETHODCALLTYPE VgpuCreatePlacedResource2Hook(
    ID3D12Device10 *const device, ID3D12Heap *const heap,
    const UINT64 heapOffset, const D3D12_RESOURCE_DESC1 *const description,
    const D3D12_BARRIER_LAYOUT initialLayout,
    const D3D12_CLEAR_VALUE *const optimizedClearValue,
    const UINT castableFormatCount, const DXGI_FORMAT *const castableFormats,
    const IID &interfaceId, void **const resource) noexcept {
  return VgpuCreatePlacedResource2Impl(
      kModernPlacedResourceCallRva, device, heap, heapOffset, description,
      initialLayout, optimizedClearValue, castableFormatCount, castableFormats,
      interfaceId, resource);
}

HRESULT STDMETHODCALLTYPE VgpuCreatePlacedResource2SecondaryHook(
    ID3D12Device10 *const device, ID3D12Heap *const heap,
    const UINT64 heapOffset, const D3D12_RESOURCE_DESC1 *const description,
    const D3D12_BARRIER_LAYOUT initialLayout,
    const D3D12_CLEAR_VALUE *const optimizedClearValue,
    const UINT castableFormatCount, const DXGI_FORMAT *const castableFormats,
    const IID &interfaceId, void **const resource) noexcept {
  return VgpuCreatePlacedResource2Impl(
      kSecondaryModernPlacedResourceCallRva, device, heap, heapOffset,
      description, initialLayout, optimizedClearValue, castableFormatCount,
      castableFormats, interfaceId, resource);
}

HRESULT VgpuCreatePlacedResourceImpl(
    const std::uintptr_t siteRva, ID3D12Device *const device,
    ID3D12Heap *const heap, const UINT64 heapOffset,
    const D3D12_RESOURCE_DESC *const description,
    const D3D12_RESOURCE_STATES initialState,
    const D3D12_CLEAR_VALUE *const optimizedClearValue, const IID &interfaceId,
    void **const resource) noexcept {
  RecordPlacedResourceCall(siteRva);
  if (device == nullptr || description == nullptr || resource == nullptr) {
    return E_INVALIDARG;
  }
  EnsureGraphicsPipelineStateHook(device);
  EnsureComputePipelineStateHook(device);
  EnsurePipelineStateStreamHook(device);
  EnsureConstantBufferViewHook(device);
  EnsureRenderTargetViewHook(device);

  D3D12_HEAP_DESC heapDescription{};
  D3D12_RESOURCE_ALLOCATION_INFO allocationInfo{};
  const bool hasPlacementInfo = heap != nullptr;
  if (hasPlacementInfo) {
    heapDescription = heap->GetDesc();
    allocationInfo = device->GetResourceAllocationInfo(
        heapDescription.Properties.VisibleNodeMask, 1, description);
    BridgeVgpuLastHeapFlags = static_cast<std::uint32_t>(heapDescription.Flags);
    BridgeVgpuLastResourceFlags =
        static_cast<std::uint32_t>(description->Flags);
    BridgeVgpuLastHeapType =
        static_cast<std::uint32_t>(heapDescription.Properties.Type);
    BridgeVgpuLastHeapCpuPageProperty =
        static_cast<std::uint32_t>(heapDescription.Properties.CPUPageProperty);
    BridgeVgpuLastHeapMemoryPoolPreference = static_cast<std::uint32_t>(
        heapDescription.Properties.MemoryPoolPreference);
    BridgeVgpuLastHeapOffset = heapOffset;
    BridgeVgpuLastHeapSize = heapDescription.SizeInBytes;
    BridgeVgpuLastAllocationSize = allocationInfo.SizeInBytes;
  }
  const auto committedHeapFlags = static_cast<D3D12_HEAP_FLAGS>(
      xeo3::vgpu::detail::GetCommittedResourceHeapFlags(
          static_cast<std::uint32_t>(heapDescription.Flags)));
  BridgeVgpuLastCommittedHeapFlags =
      static_cast<std::uint32_t>(committedHeapFlags);

  auto recordCommittedFallback = [siteRva](const HRESULT result,
                                           void *const output) noexcept {
    if (SUCCEEDED(result) && output != nullptr) {
      const auto count = g_committedOverflowFallbackCount.fetch_add(
                             1, std::memory_order_relaxed) +
                         1;
      BridgeVgpuCommittedOverflowFallbackCount = count;
      BridgeVgpuCommittedOverflowFallbackFailure = 0;
      EmitPatchEvent("legacy_resource_committed_overflow_fallback",
                     static_cast<std::uint32_t>(siteRva), count);
    } else {
      BridgeVgpuCommittedOverflowFallbackFailure =
          static_cast<std::uint32_t>(result);
      EmitPatchEvent("legacy_resource_committed_overflow_failure",
                     static_cast<std::uint32_t>(result),
                     static_cast<std::uint64_t>(siteRva));
    }
  };

  const bool placementOverflows =
      hasPlacementInfo &&
      xeo3::vgpu::detail::DoesPlacedResourceOverflowHeap(
          heapDescription.SizeInBytes, heapOffset, allocationInfo.SizeInBytes);
  HRESULT result = S_OK;
  bool usedCommittedOverflowFallback = false;
  *resource = nullptr;
  if (placementOverflows) {
    usedCommittedOverflowFallback = true;
    result = device->CreateCommittedResource(
        &heapDescription.Properties, committedHeapFlags, description,
        initialState, optimizedClearValue, interfaceId, resource);
    recordCommittedFallback(result, *resource);
  } else {
    result = device->CreatePlacedResource(heap, heapOffset, description,
                                          initialState, optimizedClearValue,
                                          interfaceId, resource);
  }

  if (!usedCommittedOverflowFallback && FAILED(result) && hasPlacementInfo) {
    if (xeo3::vgpu::detail::ShouldFallbackPlacedResourceToCommitted(
            static_cast<std::uint32_t>(result), *resource != nullptr,
            heapDescription.SizeInBytes, heapOffset,
            allocationInfo.SizeInBytes)) {
      usedCommittedOverflowFallback = true;
      *resource = nullptr;
      result = device->CreateCommittedResource(
          &heapDescription.Properties, committedHeapFlags, description,
          initialState, optimizedClearValue, interfaceId, resource);
      recordCommittedFallback(result, *resource);
    }
  }
  if (FAILED(result)) {
    CaptureDredDiagnostics(device, result);
  }
  if (SUCCEEDED(result) && *resource != nullptr) {
    if (!usedCommittedOverflowFallback) {
      RegisterMappedUploadBuffer(
          static_cast<std::uint32_t>(heapDescription.Properties.Type), false,
          heapOffset, description->Dimension, description->Width, *resource);
    } else {
      RegisterMappedUploadBuffer(
          static_cast<std::uint32_t>(heapDescription.Properties.Type), true, 0,
          description->Dimension, description->Width, *resource);
    }
    if constexpr (kEnableIndependentDiscardQueue) {
      MaybeDiscardPlacedResource(device, heap, description->Flags, *resource,
                                 PlacedResourceCreationState{
                                     false,
                                     D3D12_BARRIER_LAYOUT_UNDEFINED,
                                     initialState,
                                 });
    }
  }
  return result;
}

HRESULT STDMETHODCALLTYPE VgpuCreatePlacedResourceHook(
    ID3D12Device *const device, ID3D12Heap *const heap, const UINT64 heapOffset,
    const D3D12_RESOURCE_DESC *const description,
    const D3D12_RESOURCE_STATES initialState,
    const D3D12_CLEAR_VALUE *const optimizedClearValue, const IID &interfaceId,
    void **const resource) noexcept {
  return VgpuCreatePlacedResourceImpl(
      kLegacyPlacedResourceCallRva, device, heap, heapOffset, description,
      initialState, optimizedClearValue, interfaceId, resource);
}

void CaptureGeneratedVertexShader(const void *const source,
                                  const std::uint64_t sourceSize,
                                  const std::uint64_t sequence) noexcept {
  constexpr std::uint64_t kMaximumCaptureSize = 16ULL * 1024 * 1024;
  constexpr std::uint64_t kMaximumCaptureCount = 256;
  if (source == nullptr || sourceSize == 0 ||
      sourceSize > kMaximumCaptureSize || sequence > kMaximumCaptureCount) {
    return;
  }

  constexpr wchar_t kProbeDirectory[] =
      L"D:\\Games\\AC6 shit\\XeO3-AC6-lab\\ProbeLogs";
  if (!CreateDirectoryW(kProbeDirectory, nullptr)) {
    const auto error = GetLastError();
    if (error != ERROR_ALREADY_EXISTS) {
      BridgeVgpuVertexShaderCaptureFailure = error;
      EmitPatchEvent("vertex_shader_capture_failure", error, sequence);
      return;
    }
  }

  wchar_t capturePath[512]{};
  const auto pathLength = swprintf_s(capturePath, std::size(capturePath),
                                     L"%ls\\ac6-vertex-shader-%lu-%04llu.hlsl",
                                     kProbeDirectory, GetCurrentProcessId(),
                                     static_cast<unsigned long long>(sequence));
  if (pathLength <= 0) {
    BridgeVgpuVertexShaderCaptureFailure = ERROR_BUFFER_OVERFLOW;
    return;
  }

  const auto file =
      CreateFileW(capturePath, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    BridgeVgpuVertexShaderCaptureFailure = GetLastError();
    return;
  }

  DWORD bytesWritten = 0;
  const auto captureSize = static_cast<DWORD>(sourceSize);
  const auto wroteSource =
      WriteFile(file, source, captureSize, &bytesWritten, nullptr) != FALSE &&
      bytesWritten == captureSize;
  const auto writeError = wroteSource ? ERROR_SUCCESS : GetLastError();
  CloseHandle(file);

  if (!wroteSource) {
    BridgeVgpuVertexShaderCaptureFailure =
        writeError == ERROR_SUCCESS ? ERROR_WRITE_FAULT : writeError;
    EmitPatchEvent("vertex_shader_capture_failure",
                   BridgeVgpuVertexShaderCaptureFailure, sequence);
  } else if (sequence == 1) {
    EmitPatchEvent("vertex_shader_capture",
                   static_cast<std::uint32_t>(sourceSize), sequence);
  }
}

void CaptureGeneratedShaderArtifact(const wchar_t *const targetProfile,
                                    const std::uint64_t sequence,
                                    const wchar_t *const artifact,
                                    const wchar_t *const extension,
                                    const void *const data,
                                    const std::uint64_t dataSize) noexcept {
  constexpr std::uint64_t kMaximumCaptureSize = 64ULL * 1024 * 1024;
  constexpr std::uint64_t kMaximumCaptureCount = 512;
  if (artifact == nullptr || extension == nullptr || data == nullptr ||
      dataSize == 0 || dataSize > kMaximumCaptureSize ||
      sequence > kMaximumCaptureCount) {
    return;
  }

  constexpr wchar_t kProbeDirectory[] =
      L"D:\\Games\\AC6 shit\\XeO3-AC6-lab\\ProbeLogs";
  if (!CreateDirectoryW(kProbeDirectory, nullptr)) {
    const auto error = GetLastError();
    if (error != ERROR_ALREADY_EXISTS) {
      BridgeVgpuShaderCaptureFailure = error;
      EmitPatchEvent("shader_capture_failure", error, sequence);
      return;
    }
  }

  wchar_t profile[32]{};
  std::size_t profileLength = 0;
  if (targetProfile != nullptr) {
    while (targetProfile[profileLength] != L'\0' &&
           profileLength + 1 < std::size(profile)) {
      const auto character = targetProfile[profileLength];
      profile[profileLength] =
          (character >= L'a' && character <= L'z') ||
                  (character >= L'A' && character <= L'Z') ||
                  (character >= L'0' && character <= L'9') ||
                  character == L'_' || character == L'-'
              ? character
              : L'_';
      ++profileLength;
    }
  }
  if (profileLength == 0) {
    constexpr wchar_t kUnknownProfile[] = L"unknown";
    static_assert(std::size(kUnknownProfile) <= std::size(profile));
    std::copy(std::begin(kUnknownProfile), std::end(kUnknownProfile), profile);
  }

  wchar_t capturePath[512]{};
  const auto pathLength = swprintf_s(capturePath, std::size(capturePath),
                                     L"%ls\\ac6-shader-%lu-%04llu-%ls-%ls.%ls",
                                     kProbeDirectory, GetCurrentProcessId(),
                                     static_cast<unsigned long long>(sequence),
                                     profile, artifact, extension);
  if (pathLength <= 0) {
    BridgeVgpuShaderCaptureFailure = ERROR_BUFFER_OVERFLOW;
    return;
  }

  const auto file =
      CreateFileW(capturePath, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    BridgeVgpuShaderCaptureFailure = GetLastError();
    return;
  }

  bool wroteArtifact = true;
  const auto *cursor = static_cast<const std::uint8_t *>(data);
  auto remaining = dataSize;
  while (remaining != 0) {
    const auto chunk = static_cast<DWORD>(
        (std::min)(remaining, static_cast<std::uint64_t>(MAXDWORD)));
    DWORD bytesWritten = 0;
    if (!WriteFile(file, cursor, chunk, &bytesWritten, nullptr) ||
        bytesWritten != chunk) {
      wroteArtifact = false;
      break;
    }
    cursor += bytesWritten;
    remaining -= bytesWritten;
  }
  const auto writeError = wroteArtifact ? ERROR_SUCCESS : GetLastError();
  CloseHandle(file);

  if (!wroteArtifact) {
    BridgeVgpuShaderCaptureFailure =
        writeError == ERROR_SUCCESS ? ERROR_WRITE_FAULT : writeError;
    EmitPatchEvent("shader_capture_failure", BridgeVgpuShaderCaptureFailure,
                   sequence);
  } else if (sequence == 1 && artifact[0] == L'o') {
    EmitPatchEvent("shader_capture",
                   dataSize > UINT32_MAX ? UINT32_MAX
                                         : static_cast<std::uint32_t>(dataSize),
                   sequence);
  }
}

constexpr wchar_t kProbeDirectory[] =
    L"D:\\Games\\AC6 shit\\XeO3-AC6-lab\\ProbeLogs";

bool EnsureXenosProbeDirectory(volatile std::uint32_t &failure) noexcept {
  if (CreateDirectoryW(kProbeDirectory, nullptr)) {
    return true;
  }
  const auto error = GetLastError();
  if (error == ERROR_ALREADY_EXISTS) {
    return true;
  }
  failure = error;
  return false;
}

void FormatSha256(const std::array<std::uint8_t, 32> &digest,
                  char (&text)[65]) noexcept {
  constexpr char kHexDigits[] = "0123456789ABCDEF";
  for (std::size_t index = 0; index < digest.size(); ++index) {
    text[index * 2] = kHexDigits[digest[index] >> 4];
    text[index * 2 + 1] = kHexDigits[digest[index] & 0x0F];
  }
  text[64] = '\0';
}

const char *GetXenosStageName(const std::uint32_t stage) noexcept {
  switch (stage) {
  case 0:
    return "vertex";
  case 1:
    return "pixel";
  default:
    return "unknown";
  }
}

bool CaptureXenosUcode(const void *const bytes, const std::uint32_t byteCount,
                       const XenosTranslateContext &context) noexcept {
  if (!EnsureXenosProbeDirectory(BridgeVgpuXenosUcodeCaptureFailure)) {
    return false;
  }

  char digest[65]{};
  FormatSha256(context.sha256, digest);
  char path[640]{};
  const auto pathLength =
      std::snprintf(path, std::size(path),
                    "D:\\Games\\AC6 shit\\XeO3-AC6-lab\\ProbeLogs\\"
                    "ac6-xenos-ucode-%lu-%06" PRIu64 "-%s-%s.bin",
                    GetCurrentProcessId(), context.sequence,
                    GetXenosStageName(context.stage), digest);
  if (pathLength <= 0 ||
      static_cast<std::size_t>(pathLength) >= std::size(path)) {
    BridgeVgpuXenosUcodeCaptureFailure = ERROR_INSUFFICIENT_BUFFER;
    return false;
  }

  std::scoped_lock lock(g_xenosCaptureMutex);
  const auto file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    BridgeVgpuXenosUcodeCaptureFailure = GetLastError();
    return false;
  }
  DWORD bytesWritten = 0;
  const auto written =
      WriteFile(file, bytes, byteCount, &bytesWritten, nullptr) != FALSE;
  const auto error = written ? ERROR_SUCCESS : GetLastError();
  CloseHandle(file);
  if (!written || bytesWritten != byteCount) {
    BridgeVgpuXenosUcodeCaptureFailure = written ? ERROR_WRITE_FAULT : error;
    return false;
  }

  BridgeVgpuXenosUcodeCaptureFailure = ERROR_SUCCESS;
  const auto captureCount =
      g_xenosUcodeCaptureCount.fetch_add(1, std::memory_order_relaxed) + 1;
  BridgeVgpuXenosUcodeCaptureCount = captureCount;
  if (captureCount == 1 || (captureCount & (captureCount - 1)) == 0) {
    EmitPatchEvent("xenos_ucode_capture", context.stage, captureCount);
  }
  return true;
}

void CaptureXenosShaderMapping(const void *const hlsl,
                               const std::uint64_t hlslByteCount,
                               const wchar_t *const targetProfile,
                               const std::uint64_t compileSequence) noexcept {
  constexpr std::uint64_t kMaximumHlslSize = 16ULL * 1024 * 1024;
  const auto context = g_xenosTranslateContext;
  if (!context.valid) {
    return;
  }
  if (hlsl == nullptr || hlslByteCount == 0 ||
      hlslByteCount > kMaximumHlslSize ||
      hlslByteCount > (std::numeric_limits<std::size_t>::max)()) {
    BridgeVgpuXenosShaderMapFailure = ERROR_INVALID_DATA;
    return;
  }

  std::array<std::uint8_t, 32> hlslDigest{};
  if (!xeo3::vgpu::detail::HashBytesSha256(
          hlsl, static_cast<std::size_t>(hlslByteCount), hlslDigest)) {
    BridgeVgpuXenosShaderMapFailure = ERROR_INVALID_DATA;
    return;
  }
  if (!EnsureXenosProbeDirectory(BridgeVgpuXenosShaderMapFailure)) {
    return;
  }

  char ucodeHash[65]{};
  char hlslHash[65]{};
  FormatSha256(context.sha256, ucodeHash);
  FormatSha256(hlslDigest, hlslHash);
  char profile[32]{'u', 'n', 'k', 'n', 'o', 'w', 'n', '\0'};
  if (targetProfile != nullptr && targetProfile[0] != L'\0') {
    std::size_t index = 0;
    for (; targetProfile[index] != L'\0' && index + 1 < std::size(profile);
         ++index) {
      const auto character = targetProfile[index];
      profile[index] = character >= 0x20 && character <= 0x7E &&
                               character != L'"' && character != L'\\'
                           ? static_cast<char>(character)
                           : '_';
    }
    profile[index] = '\0';
  }

  char line[1024]{};
  const auto lineLength = std::snprintf(
      line, std::size(line),
      "{\"xeo3_ac6\":\"xenos_shader_map\","
      "\"xenos_sequence\":%" PRIu64 ",\"compile_sequence\":%" PRIu64
      ",\"stage\":%u,\"stage_name\":\"%s\",\"ucode_size\":%u,"
      "\"ucode_sha256\":\"%s\",\"target_profile\":\"%s\","
      "\"hlsl_size\":%" PRIu64 ",\"hlsl_sha256\":\"%s\","
      "\"tid\":%lu}\n",
      context.sequence, compileSequence, context.stage,
      GetXenosStageName(context.stage), context.byteCount, ucodeHash, profile,
      hlslByteCount, hlslHash, GetCurrentThreadId());
  if (lineLength <= 0 ||
      static_cast<std::size_t>(lineLength) >= std::size(line)) {
    BridgeVgpuXenosShaderMapFailure = ERROR_INSUFFICIENT_BUFFER;
    return;
  }

  char path[512]{};
  const auto pathLength =
      std::snprintf(path, std::size(path),
                    "D:\\Games\\AC6 shit\\XeO3-AC6-lab\\ProbeLogs\\"
                    "ac6-xenos-shader-map-%lu.jsonl",
                    GetCurrentProcessId());
  if (pathLength <= 0 ||
      static_cast<std::size_t>(pathLength) >= std::size(path)) {
    BridgeVgpuXenosShaderMapFailure = ERROR_INSUFFICIENT_BUFFER;
    return;
  }

  std::scoped_lock lock(g_xenosShaderMapMutex);
  const auto file =
      CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                  nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    BridgeVgpuXenosShaderMapFailure = GetLastError();
    return;
  }
  DWORD bytesWritten = 0;
  const auto lineByteCount = static_cast<DWORD>(lineLength);
  const auto written =
      WriteFile(file, line, lineByteCount, &bytesWritten, nullptr) != FALSE;
  const auto error = written ? ERROR_SUCCESS : GetLastError();
  CloseHandle(file);
  BridgeVgpuXenosShaderMapFailure = written && bytesWritten == lineByteCount
                                        ? ERROR_SUCCESS
                                    : written ? ERROR_WRITE_FAULT
                                              : error;
}

std::uint64_t VgpuTranslateXenosShaderHook(
    std::uint64_t *const parameter1, const std::uint64_t stage,
    void *const shaderSource, const std::uint64_t parameter4,
    const std::uint64_t parameter5, const std::uint64_t parameter6,
    void *const parameter7, const std::uint8_t parameter8,
    const std::uint64_t parameter9, const std::uint64_t parameter10,
    void *const parameter11, void *const parameter12,
    void *const parameter13) noexcept {
  constexpr std::uint32_t kMaximumUcodeSize = 16U * 1024 * 1024;
  const auto sequence =
      g_xenosTranslateCount.fetch_add(1, std::memory_order_relaxed) + 1;
  BridgeVgpuXenosTranslateCount = sequence;
  BridgeVgpuXenosLastStage = static_cast<std::uint32_t>(stage);

  const auto previousContext = g_xenosTranslateContext;
  g_xenosTranslateContext = {};
  if (shaderSource != nullptr) {
    auto *const vtable = *static_cast<void ***>(shaderSource);
    if (vtable != nullptr && vtable[1] != nullptr) {
      const auto getBytes =
          reinterpret_cast<NativeGetXenosShaderBytes>(vtable[1]);
      std::uint32_t byteCount = 0;
      const auto *const bytes = getBytes(shaderSource, stage, &byteCount);
      if (bytes != nullptr && byteCount != 0 &&
          byteCount <= kMaximumUcodeSize) {
        XenosTranslateContext context{};
        context.sequence = sequence;
        context.stage = static_cast<std::uint32_t>(stage);
        context.byteCount = byteCount;
        if (xeo3::vgpu::detail::HashBytesSha256(bytes, byteCount,
                                                context.sha256)) {
          context.valid = true;
          g_xenosTranslateContext = context;
          BridgeVgpuXenosLastUcodeSize = byteCount;
          std::array<std::uint64_t, 4> hashWords{};
          std::memcpy(hashWords.data(), context.sha256.data(),
                      context.sha256.size());
          BridgeVgpuXenosLastUcodeHash0 = hashWords[0];
          BridgeVgpuXenosLastUcodeHash1 = hashWords[1];
          BridgeVgpuXenosLastUcodeHash2 = hashWords[2];
          BridgeVgpuXenosLastUcodeHash3 = hashWords[3];
          CaptureXenosUcode(bytes, byteCount, context);
        } else {
          BridgeVgpuXenosUcodeCaptureFailure = ERROR_INVALID_DATA;
        }
      } else {
        BridgeVgpuXenosUcodeCaptureFailure = ERROR_INVALID_DATA;
      }
    } else {
      BridgeVgpuXenosUcodeCaptureFailure = ERROR_INVALID_ADDRESS;
    }
  } else {
    BridgeVgpuXenosUcodeCaptureFailure = ERROR_INVALID_PARAMETER;
  }

  const auto native = g_nativeTranslateXenosShader;
  const auto result =
      native == nullptr
          ? 0
          : native(parameter1, stage, shaderSource, parameter4, parameter5,
                   parameter6, parameter7, parameter8, parameter9, parameter10,
                   parameter11, parameter12, parameter13);
  g_xenosTranslateContext = previousContext;
  return result;
}

bool VgpuCompileHlslHook(const void *const source,
                         const std::uint64_t sourceSize,
                         const wchar_t *const targetProfile,
                         const wchar_t *const entryPoint,
                         void *const parameter5, void *const parameter6,
                         void *const parameter7,
                         void *const parameter8) noexcept {
  const auto compileSequence =
      g_shaderCompileCount.fetch_add(1, std::memory_order_relaxed) + 1;
  BridgeVgpuShaderCompileCount = compileSequence;
  CaptureGeneratedShaderArtifact(targetProfile, compileSequence, L"original",
                                 L"hlsl", source, sourceSize);
  CaptureXenosShaderMapping(source, sourceSize, targetProfile, compileSequence);

  const auto sourceFitsHost =
      sourceSize <= (std::numeric_limits<std::size_t>::max)();
  const auto hostSourceSize =
      sourceFitsHost ? static_cast<std::size_t>(sourceSize) : 0;
  const auto isVertexShader =
      sourceFitsHost &&
      xeo3::vgpu::detail::IsGeneratedVertexShader(source, hostSourceSize);
  bool isAc6Pso537Shader = false;
  if (sourceFitsHost && BridgeVgpuVposScaleFixEnabled != 0 &&
      BridgeVgpuVposSceneHalfWidthUvEnabled != 0) {
    std::array<std::uint8_t, 32> digest{};
    if (!xeo3::vgpu::detail::HashBytesSha256(source, hostSourceSize, digest)) {
      BridgeVgpuVposFixFailure = ERROR_INVALID_DATA;
      EmitPatchEvent("vpos_scene_shader_hash_failure", BridgeVgpuVposFixFailure,
                     compileSequence);
    } else {
      isAc6Pso537Shader = xeo3::vgpu::detail::MatchesAc6Pso537ShaderFingerprint(
          hostSourceSize, digest);
    }
  }
  bool isAc6WaveBallotShader = false;
  if (sourceFitsHost && BridgeVgpuWaveBallotFixEnabled != 0) {
    std::array<std::uint8_t, 32> digest{};
    if (!xeo3::vgpu::detail::HashBytesSha256(source, hostSourceSize, digest)) {
      BridgeVgpuWaveBallotFixFailure = ERROR_INVALID_DATA;
      EmitPatchEvent("wave_ballot_shader_hash_failure",
                     BridgeVgpuWaveBallotFixFailure, compileSequence);
    } else if (xeo3::vgpu::detail::MatchesAc6Pso533PixelShaderFingerprint(
                   hostSourceSize, digest) ||
               xeo3::vgpu::detail::MatchesAc6Pso540VertexShaderFingerprint(
                   hostSourceSize, digest)) {
      isAc6WaveBallotShader = true;
      const auto count = g_waveBallotFingerprintMatchCount.fetch_add(
                             1, std::memory_order_relaxed) +
                         1;
      BridgeVgpuWaveBallotFingerprintMatchCount = count;
      EmitPatchEvent("wave_ballot_shader_fingerprint_match",
                     static_cast<std::uint32_t>(hostSourceSize), count);
    }
  }
  bool isAc6ExposureShader = false;
  if (sourceFitsHost && BridgeVgpuExposureFixEnabled != 0) {
    std::array<std::uint8_t, 32> digest{};
    if (!xeo3::vgpu::detail::HashBytesSha256(source, hostSourceSize, digest)) {
      BridgeVgpuExposureFixFailure = ERROR_INVALID_DATA;
      EmitPatchEvent("exposure_shader_hash_failure",
                     BridgeVgpuExposureFixFailure, compileSequence);
    } else if (xeo3::vgpu::detail::MatchesAc6ExposureShaderFingerprint(
                   hostSourceSize, digest)) {
      isAc6ExposureShader = true;
      const auto count = g_exposureFingerprintMatchCount.fetch_add(
                             1, std::memory_order_relaxed) +
                         1;
      BridgeVgpuExposureFingerprintMatchCount = count;
      EmitPatchEvent("exposure_shader_fingerprint_match",
                     static_cast<std::uint32_t>(hostSourceSize), count);
    }
  }
  bool isAc6SkyRestartShader = false;
  bool isAc6TerrainFanRestartShader = false;
  bool isAc6AircraftRestartShader = false;
  bool isAc6ShadowRestartShader = false;
  if (sourceFitsHost) {
    std::array<std::uint8_t, 32> digest{};
    if (!xeo3::vgpu::detail::HashBytesSha256(source, hostSourceSize, digest)) {
      BridgeVgpuIndexFixFailure = ERROR_INVALID_DATA;
      EmitPatchEvent("index_restart_shader_hash_failure",
                     BridgeVgpuIndexFixFailure, compileSequence);
    } else {
      isAc6SkyRestartShader =
          xeo3::vgpu::detail::MatchesAc6SkyRestartShaderFingerprint(
              hostSourceSize, digest);
      isAc6TerrainFanRestartShader =
          xeo3::vgpu::detail::MatchesAc6TerrainFanRestartShaderFingerprint(
              hostSourceSize, digest);
      isAc6AircraftRestartShader =
          xeo3::vgpu::detail::MatchesAc6AircraftRestartShaderFingerprint(
              hostSourceSize, digest);
      isAc6ShadowRestartShader =
          xeo3::vgpu::detail::MatchesAc6ShadowRestartShaderFingerprint(
              hostSourceSize, digest);
    }
  }
  const bool isAc6RestartIndexedShader =
      isAc6SkyRestartShader || isAc6TerrainFanRestartShader ||
      isAc6AircraftRestartShader || isAc6ShadowRestartShader;
  if (isVertexShader) {
    const auto count =
        g_vertexShaderCompileCount.fetch_add(1, std::memory_order_relaxed) + 1;
    BridgeVgpuVertexShaderCompileCount = count;
    BridgeVgpuLastVertexShaderSize =
        sourceSize > UINT32_MAX ? UINT32_MAX
                                : static_cast<std::uint32_t>(sourceSize);
    CaptureGeneratedVertexShader(source, sourceSize, count);
  }

  const void *compileSource = source;
  std::uint64_t compileSourceSize = sourceSize;
  std::string waveBallotPatchedSource;
  std::uint32_t waveBallotPatchCount = 0;
  std::string reciprocalPatchedSource;
  std::uint32_t reciprocalPatchCount = 0;
  std::string vposPatchedSource;
  std::string vposSceneUvPatchedSource;
  std::uint32_t vposPatchCount = 0;
  std::string toneMapPatchedSource;
  std::uint32_t toneMapPatchCount = 0;
  std::string exposurePatchedSource;
  std::uint32_t exposurePatchCount = 0;
  std::string indexPatchedSource;
  std::uint32_t indexPatchCount = 0;
  std::string groundPatchedSource;
  std::uint32_t groundPatchCount = 0;
  if (sourceFitsHost) {
    if (isAc6WaveBallotShader) {
      if (!xeo3::vgpu::detail::PatchAc6WaveBallots(
              compileSource, static_cast<std::size_t>(compileSourceSize),
              waveBallotPatchedSource, waveBallotPatchCount) ||
          waveBallotPatchCount != 2) {
        BridgeVgpuWaveBallotFixFailure = ERROR_INVALID_DATA;
        EmitPatchEvent("wave_ballot_fix_failure",
                       BridgeVgpuWaveBallotFixFailure, compileSequence);
      } else {
        compileSource = waveBallotPatchedSource.data();
        compileSourceSize = waveBallotPatchedSource.size();
        const auto shaderCount = g_waveBallotFixShaderCount.fetch_add(
                                     1, std::memory_order_relaxed) +
                                 1;
        const auto siteCount = g_waveBallotFixSiteCount.fetch_add(
                                   waveBallotPatchCount,
                                   std::memory_order_relaxed) +
                               waveBallotPatchCount;
        BridgeVgpuWaveBallotFixShaderCount = shaderCount;
        BridgeVgpuWaveBallotFixSiteCount = siteCount;
        BridgeVgpuWaveBallotFixFailure = ERROR_SUCCESS;
        EmitPatchEvent("wave_ballot_fix_shader", waveBallotPatchCount,
                       shaderCount);
      }
    }

    if (BridgeVgpuReciprocalFixEnabled != 0) {
      if (!xeo3::vgpu::detail::PatchXenosScalarReciprocals(
              compileSource, static_cast<std::size_t>(compileSourceSize),
              reciprocalPatchedSource,
              reciprocalPatchCount)) {
        BridgeVgpuReciprocalFixFailure = ERROR_INVALID_DATA;
        EmitPatchEvent("reciprocal_fix_failure",
                       BridgeVgpuReciprocalFixFailure);
      } else if (reciprocalPatchCount != 0) {
        compileSource = reciprocalPatchedSource.data();
        compileSourceSize = reciprocalPatchedSource.size();
        const auto shaderCount =
            g_reciprocalFixShaderCount.fetch_add(1, std::memory_order_relaxed) +
            1;
        const auto instructionCount =
            g_reciprocalFixInstructionCount.fetch_add(
                reciprocalPatchCount, std::memory_order_relaxed) +
            reciprocalPatchCount;
        BridgeVgpuReciprocalFixShaderCount = shaderCount;
        BridgeVgpuReciprocalFixInstructionCount = instructionCount;
        if (shaderCount <= 64 || (shaderCount & (shaderCount - 1)) == 0) {
          EmitPatchEvent("reciprocal_fix_shader", reciprocalPatchCount,
                         shaderCount);
        }
      }
    }

    if (BridgeVgpuVposScaleFixEnabled != 0) {
      if (!xeo3::vgpu::detail::PatchAc6ScreenSpaceVposScale(
              compileSource, static_cast<std::size_t>(compileSourceSize),
              vposPatchedSource, vposPatchCount)) {
        BridgeVgpuVposFixFailure = ERROR_INVALID_DATA;
        EmitPatchEvent("vpos_scale_fix_failure", BridgeVgpuVposFixFailure);
      } else if (vposPatchCount != 0) {
        compileSource = vposPatchedSource.data();
        compileSourceSize = vposPatchedSource.size();
        if (isAc6Pso537Shader && BridgeVgpuVposSceneHalfWidthUvEnabled != 0) {
          std::uint32_t sceneUvPatchCount = 0;
          if (!xeo3::vgpu::detail::PatchAc6Pso537HalfWidthUv(
                  compileSource, static_cast<std::size_t>(compileSourceSize),
                  vposSceneUvPatchedSource, sceneUvPatchCount) ||
              sceneUvPatchCount != 1) {
            BridgeVgpuVposFixFailure = ERROR_INVALID_DATA;
            EmitPatchEvent("vpos_scene_half_width_uv_failure",
                           BridgeVgpuVposFixFailure, compileSequence);
          } else {
            compileSource = vposSceneUvPatchedSource.data();
            compileSourceSize = vposSceneUvPatchedSource.size();
            vposPatchCount += sceneUvPatchCount;
            EmitPatchEvent("vpos_scene_half_width_uv_shader", sceneUvPatchCount,
                           compileSequence);
          }
        }
        const auto shaderCount =
            g_vposFixShaderCount.fetch_add(1, std::memory_order_relaxed) + 1;
        const auto siteCount = g_vposFixSiteCount.fetch_add(
                                   vposPatchCount, std::memory_order_relaxed) +
                               vposPatchCount;
        BridgeVgpuVposFixShaderCount = shaderCount;
        BridgeVgpuVposFixSiteCount = siteCount;
        if (shaderCount <= 64 || (shaderCount & (shaderCount - 1)) == 0) {
          EmitPatchEvent("vpos_scale_fix_shader", vposPatchCount, shaderCount);
        }
      }
    }

    if (BridgeVgpuToneMapFixEnabled != 0) {
      if (!xeo3::vgpu::detail::PatchAc6ToneMapInterpolant(
              compileSource, static_cast<std::size_t>(compileSourceSize),
              toneMapPatchedSource, toneMapPatchCount)) {
        BridgeVgpuVposFixFailure = ERROR_INVALID_DATA;
        EmitPatchEvent("tone_map_interpolant_fix_failure",
                       BridgeVgpuVposFixFailure);
      } else if (toneMapPatchCount != 0) {
        compileSource = toneMapPatchedSource.data();
        compileSourceSize = toneMapPatchedSource.size();
        const auto shaderCount =
            g_vposFixShaderCount.fetch_add(1, std::memory_order_relaxed) + 1;
        const auto siteCount =
            g_vposFixSiteCount.fetch_add(toneMapPatchCount,
                                         std::memory_order_relaxed) +
            toneMapPatchCount;
        BridgeVgpuVposFixShaderCount = shaderCount;
        BridgeVgpuVposFixSiteCount = siteCount;
        EmitPatchEvent("tone_map_interpolant_fix_shader", toneMapPatchCount,
                       shaderCount);
      }
    }

    if (isAc6ExposureShader) {
      if (!xeo3::vgpu::detail::PatchAc6ExposureSample(
              compileSource, static_cast<std::size_t>(compileSourceSize),
              exposurePatchedSource, exposurePatchCount) ||
          exposurePatchCount != 1) {
        BridgeVgpuExposureFixFailure = ERROR_INVALID_DATA;
        EmitPatchEvent("exposure_sample_guard_failure",
                       BridgeVgpuExposureFixFailure, compileSequence);
      } else {
        compileSource = exposurePatchedSource.data();
        compileSourceSize = exposurePatchedSource.size();
        const auto shaderCount =
            g_exposureFixShaderCount.fetch_add(1, std::memory_order_relaxed) +
            1;
        const auto siteCount =
            g_exposureFixSiteCount.fetch_add(exposurePatchCount,
                                             std::memory_order_relaxed) +
            exposurePatchCount;
        BridgeVgpuExposureFixShaderCount = shaderCount;
        BridgeVgpuExposureFixSiteCount = siteCount;
        BridgeVgpuExposureFixFailure = ERROR_SUCCESS;
        EmitPatchEvent("exposure_sample_guard_shader", exposurePatchCount,
                       shaderCount);
      }
    }

    if (!xeo3::vgpu::detail::PatchXenosIndexBufferSemantics(
            compileSource, static_cast<std::size_t>(compileSourceSize),
            indexPatchedSource, indexPatchCount, isAc6RestartIndexedShader,
            isAc6ShadowRestartShader ? 128U : 64U)) {
      BridgeVgpuIndexFixFailure = ERROR_INVALID_DATA;
      EmitPatchEvent("index_fix_failure", BridgeVgpuIndexFixFailure);
    } else if (indexPatchCount != 0) {
      compileSource = indexPatchedSource.data();
      compileSourceSize = indexPatchedSource.size();
      const auto shaderCount =
          g_indexFixShaderCount.fetch_add(1, std::memory_order_relaxed) + 1;
      const auto siteCount = g_indexFixSiteCount.fetch_add(
                                 indexPatchCount, std::memory_order_relaxed) +
                             indexPatchCount;
      BridgeVgpuIndexFixShaderCount = shaderCount;
      BridgeVgpuIndexFixSiteCount = siteCount;
      if (shaderCount <= 64 || (shaderCount & (shaderCount - 1)) == 0) {
        EmitPatchEvent("index_fix_shader", indexPatchCount, shaderCount);
      }
      if (isAc6RestartIndexedShader) {
        EmitPatchEvent(isAc6TerrainFanRestartShader
                           ? "index_restart_fan_shader"
                           : (isAc6AircraftRestartShader
                                  ? "index_restart_aircraft_shader"
                                  : (isAc6ShadowRestartShader
                                         ? "index_restart_shadow_shader"
                                         : "index_restart_strip_shader")),
                       indexPatchCount, compileSequence);
      }
      if (isAc6AircraftRestartShader) {
        BridgeVgpuAircraftRestartShaderCount =
            g_aircraftRestartShaderCount.fetch_add(1, std::memory_order_relaxed) +
            1;
      }
      if (isAc6ShadowRestartShader) {
        BridgeVgpuShadowRestartShaderCount =
            g_shadowRestartShaderCount.fetch_add(1, std::memory_order_relaxed) +
            1;
      }
    }

    if (BridgeVgpuGroundFixEnabled != 0) {
      if (!xeo3::vgpu::detail::PatchAc6GroundFetchIndices(
              compileSource, static_cast<std::size_t>(compileSourceSize),
              groundPatchedSource, groundPatchCount)) {
        BridgeVgpuGroundFixFailure = ERROR_INVALID_DATA;
        EmitPatchEvent("ground_fetch_fix_failure", BridgeVgpuGroundFixFailure);
      } else if (groundPatchCount != 0) {
        compileSource = groundPatchedSource.data();
        compileSourceSize = groundPatchedSource.size();
        const auto shaderCount =
            g_groundFixShaderCount.fetch_add(1, std::memory_order_relaxed) + 1;
        const auto fetchCount =
            g_groundFixFetchCount.fetch_add(groundPatchCount,
                                            std::memory_order_relaxed) +
            groundPatchCount;
        BridgeVgpuGroundFixShaderCount = shaderCount;
        BridgeVgpuGroundFixFetchCount = fetchCount;
        if (shaderCount <= 64 || (shaderCount & (shaderCount - 1)) == 0) {
          EmitPatchEvent("ground_fetch_fix_shader", groundPatchCount,
                         shaderCount);
        }
      }
    }
  }

  CaptureGeneratedShaderArtifact(targetProfile, compileSequence, L"compiled",
                                 L"hlsl", compileSource, compileSourceSize);

  const auto native = g_nativeCompileHlsl;
  if (native == nullptr) {
    return false;
  }
  const auto compiled =
      native(compileSource, compileSourceSize, targetProfile, entryPoint,
             parameter5, parameter6, parameter7, parameter8);
  if (compiled && parameter8 != nullptr) {
    auto *const shaderBlob = *static_cast<IDxcBlob **>(parameter8);
    if (shaderBlob != nullptr) {
      const auto compiledSize = shaderBlob->GetBufferSize();
      BridgeVgpuLastCompiledShaderSize =
          compiledSize > UINT32_MAX ? UINT32_MAX
                                    : static_cast<std::uint32_t>(compiledSize);
      CaptureGeneratedShaderArtifact(
          targetProfile, compileSequence, L"compiled", L"dxil",
          shaderBlob->GetBufferPointer(), compiledSize);
    }
  }
  return compiled;
}

std::uint32_t VgpuFetchTableHook(void *cache, std::uint64_t *outputGpuAddress,
                                 const void *source,
                                 const std::uint32_t entryCount,
                                 void *allocatorContext) noexcept {
  if (entryCount <= xeo3::vgpu::kNativeFetchTableCapacity) {
    const auto native = g_nativeFetchTable;
    return native == nullptr ? 0
                             : native(cache, outputGpuAddress, source,
                                      entryCount, allocatorContext);
  }

  BridgeVgpuLastFetchCount = entryCount;
  if (entryCount > xeo3::vgpu::kXenosFetchTableCapacity ||
      g_moduleBase == nullptr) {
    SetStatus(PatchStatus::InvalidExtendedCall);
    EmitPatchEvent("extended_fetch_failure", entryCount);
    return 0;
  }

  auto allocate = reinterpret_cast<xeo3::vgpu::AllocateFetchTable>(
      g_moduleBase + kAllocatorRva);
  void *uploadInterface = nullptr;
  std::memcpy(&uploadInterface, g_moduleBase + kUploadInterfacePointerRva,
              sizeof(uploadInterface));
  if (uploadInterface == nullptr) {
    SetStatus(PatchStatus::InvalidExtendedCall);
    EmitPatchEvent("extended_fetch_failure", entryCount);
    return 0;
  }

  void **uploadVtable = nullptr;
  std::memcpy(&uploadVtable, uploadInterface, sizeof(uploadVtable));
  if (uploadVtable == nullptr) {
    SetStatus(PatchStatus::InvalidExtendedCall);
    EmitPatchEvent("extended_fetch_failure", entryCount);
    return 0;
  }

  xeo3::vgpu::UploadFetchTable upload = nullptr;
  std::memcpy(&upload,
              reinterpret_cast<const std::uint8_t *>(uploadVtable) +
                  kUploadMethodVtableOffset,
              sizeof(upload));
  if (upload == nullptr) {
    SetStatus(PatchStatus::InvalidExtendedCall);
    EmitPatchEvent("extended_fetch_failure", entryCount);
    return 0;
  }

  const xeo3::vgpu::ExtendedFetchTableCallbacks callbacks{
      allocate,
      upload,
  };
  const auto count =
      g_extendedFetchCount.fetch_add(1, std::memory_order_relaxed) + 1;
  BridgeVgpuExtendedFetchCount = count;
  EmitPatchEvent("extended_fetch_begin", entryCount, count);
  const auto result = xeo3::vgpu::BuildExtendedFetchTable(
      cache, outputGpuAddress, source, entryCount, allocatorContext,
      uploadInterface, callbacks);
  if (result == 0) {
    SetStatus(PatchStatus::InvalidExtendedCall);
    EmitPatchEvent("extended_fetch_failure", entryCount);
    return 0;
  }

  SetStatus(PatchStatus::Installed);
  if (count <= 64 || (count & (count - 1)) == 0) {
    EmitPatchEvent("extended_fetch", entryCount, count);
  }
  return result;
}

bool TryConsumeTextureEndianFixBudget() noexcept {
  auto *const budget =
      reinterpret_cast<volatile LONG *>(&BridgeVgpuTextureEndianFixBudget);
  LONG current = InterlockedCompareExchange(budget, 0, 0);
  while (current != 0) {
    const auto next =
        static_cast<LONG>(static_cast<std::uint32_t>(current) - 1U);
    const auto observed = InterlockedCompareExchange(budget, next, current);
    if (observed == current) {
      return true;
    }
    current = observed;
  }
  return false;
}

bool PatchTextureTransferConstants(
    std::array<std::uint32_t, 6> &transferConstants,
    const std::uint64_t transferCallCount) noexcept {
  if (transferConstants[1] != 2) {
    return false;
  }

  const auto endian2CallCount =
      g_textureEndian2CallCount.fetch_add(1, std::memory_order_relaxed) + 1;
  BridgeVgpuTextureEndian2CallCount = endian2CallCount;
  BridgeVgpuLastTextureEndianOriginal = transferConstants[1];
  BridgeVgpuLastTextureEndian2Parameter9 = transferConstants[0];
  BridgeVgpuLastTextureEndian2Parameter13 = transferConstants[2];
  BridgeVgpuLastTextureEndian2Parameter14 = transferConstants[3];
  BridgeVgpuLastTextureEndian2Parameter15 = transferConstants[4];
  BridgeVgpuLastTextureEndian2Parameter16 = transferConstants[5];
  if (endian2CallCount <= 16 ||
      (endian2CallCount & (endian2CallCount - 1)) == 0) {
    EmitPatchEvent("texture_transfer_endian2", transferConstants[0],
                   (static_cast<std::uint64_t>(transferConstants[4]) << 32) |
                       transferConstants[5]);
  }

  const auto unpackKind =
      xeo3::vgpu::detail::ClassifyAc6TextureUnpackTransfer(transferConstants);
  if (unpackKind == xeo3::vgpu::Ac6TextureUnpackKind::None) {
    return false;
  }

  const auto signatureMatchCount = g_textureEndianSignatureMatchCount.fetch_add(
                                       1, std::memory_order_relaxed) +
                                   1;
  BridgeVgpuTextureEndianSignatureMatchCount = signatureMatchCount;
  BridgeVgpuTextureEndianLastMatchCall = transferCallCount;

  auto patchedConstants = transferConstants;
  if (BridgeVgpuTextureEndianFixEnabled == 0 ||
      !xeo3::vgpu::detail::PatchAc6TextureUnpackEndian(
          patchedConstants, BridgeVgpuTextureEndianReplacement) ||
      !TryConsumeTextureEndianFixBudget()) {
    return false;
  }

  transferConstants = patchedConstants;

  BridgeVgpuLastTextureEndianReplacement = transferConstants[1];
  BridgeVgpuTextureEndianLastPatchedCall = transferCallCount;
  const auto count =
      g_textureEndianFixCount.fetch_add(1, std::memory_order_relaxed) + 1;
  BridgeVgpuTextureEndianFixCount = count;
  if (unpackKind == xeo3::vgpu::Ac6TextureUnpackKind::TargetPreview) {
    const auto previewCount = g_texturePreviewEndianFixCount.fetch_add(
                                  1, std::memory_order_relaxed) +
                              1;
    BridgeVgpuTexturePreviewEndianFixCount = previewCount;
    if (previewCount <= 16 || (previewCount & (previewCount - 1)) == 0) {
      EmitPatchEvent("texture_preview_unpack_endian_fix", transferConstants[5],
                     previewCount);
    }
  }
  if (count <= 64 || (count & (count - 1)) == 0) {
    EmitPatchEvent("texture_unpack_endian_fix",
                   BridgeVgpuLastTextureEndianOriginal, count);
  }
  return true;
}

void RecordEdramTransferConstants(
    const std::uint64_t context, const std::size_t sourceSize,
    const std::uint64_t constantUploadCallCount,
    const std::array<std::uint32_t, 16> &constants) noexcept {
  const auto kind =
      xeo3::vgpu::detail::ClassifyAc6EdramTransferConstants(constants);
  if (kind == xeo3::vgpu::Ac6EdramConstantKind::None) {
    return;
  }

  const auto candidateCount =
      g_edramConstantCandidateCount.fetch_add(1, std::memory_order_relaxed) + 1;
  BridgeVgpuEdramConstantCandidateCount = candidateCount;
  if (kind == xeo3::vgpu::Ac6EdramConstantKind::Load) {
    BridgeVgpuEdramLoadConstantCount =
        g_edramLoadConstantCount.fetch_add(1, std::memory_order_relaxed) + 1;
  } else if (kind == xeo3::vgpu::Ac6EdramConstantKind::Scale) {
    BridgeVgpuEdramScaleConstantCount =
        g_edramScaleConstantCount.fetch_add(1, std::memory_order_relaxed) + 1;
  }

  if (g_edramConstantSnapshotWriter.test_and_set(std::memory_order_acquire)) {
    return;
  }

  const auto packed =
      xeo3::vgpu::detail::PackAc6EdramTransferConstants(constants);
  const auto beginSequence =
      g_edramConstantSnapshotSequence.fetch_add(1, std::memory_order_acq_rel) +
      1;
  BridgeVgpuEdramConstantSnapshotSequence = beginSequence;
  std::atomic_thread_fence(std::memory_order_release);
  BridgeVgpuEdramConstantLastCall = constantUploadCallCount;
  BridgeVgpuEdramConstantLastContext = context;
  BridgeVgpuEdramConstantLastSize = static_cast<std::uint64_t>(sourceSize);
  BridgeVgpuEdramConstantLastKind = static_cast<std::uint32_t>(kind);
  BridgeVgpuEdramConstantData0 = packed[0];
  BridgeVgpuEdramConstantData1 = packed[1];
  BridgeVgpuEdramConstantData2 = packed[2];
  BridgeVgpuEdramConstantData3 = packed[3];
  BridgeVgpuEdramConstantData4 = packed[4];
  BridgeVgpuEdramConstantData5 = packed[5];
  BridgeVgpuEdramConstantData6 = packed[6];
  BridgeVgpuEdramConstantData7 = packed[7];
  std::atomic_thread_fence(std::memory_order_release);
  const auto endSequence =
      g_edramConstantSnapshotSequence.fetch_add(1, std::memory_order_release) +
      1;
  BridgeVgpuEdramConstantSnapshotSequence = endSequence;
  g_edramConstantSnapshotWriter.clear(std::memory_order_release);

  if (candidateCount <= 16 || (candidateCount & (candidateCount - 1)) == 0) {
    EmitPatchEvent("edram_transfer_constants", static_cast<std::uint32_t>(kind),
                   candidateCount);
  }
}

void VgpuTextureTransferHook(
    const std::uint64_t parameter1, const std::uint64_t parameter2,
    const std::uint64_t parameter3, const std::uint32_t parameter4,
    const std::uint32_t parameter5, const std::uint64_t *const parameter6,
    const std::uint64_t parameter7, const std::uint64_t parameter8,
    const std::uint32_t parameter9, std::int32_t parameter10,
    const std::uint32_t parameter11, const std::uint64_t *const parameter12,
    const std::uint32_t parameter13, const std::uint32_t parameter14,
    const std::uint32_t parameter15, const std::uint32_t parameter16,
    const std::uint32_t parameter17, const std::uint32_t parameter18,
    const std::uint32_t parameter19, const std::uint32_t parameter20,
    const std::uint64_t parameter21, const std::uint64_t parameter22) noexcept {
  const auto transferCallCount =
      g_textureTransferCallCount.fetch_add(1, std::memory_order_relaxed) + 1;
  BridgeVgpuTextureTransferCallCount = transferCallCount;

  const std::uint64_t *nativeParameter6 = parameter6;
  std::array<std::uint64_t, 2> parameter6Copy{};
  if (parameter6 != nullptr && parameter12 != nullptr) {
    // VGPUDX12 concatenates p6[0..3] and p12[0..1] into CB0 DWORDs 0..5.
    std::array<std::uint32_t, 6> transferConstants{};
    std::memcpy(transferConstants.data(), parameter6,
                sizeof(std::uint32_t) * 4);
    std::memcpy(transferConstants.data() + 4, parameter12,
                sizeof(std::uint32_t) * 2);
    if (PatchTextureTransferConstants(transferConstants, transferCallCount)) {
      std::memcpy(parameter6Copy.data(), transferConstants.data(),
                  sizeof(parameter6Copy));
      nativeParameter6 = parameter6Copy.data();
    }
  }

  const auto native = g_nativeTextureTransfer;
  if (native == nullptr) {
    return;
  }
  native(parameter1, parameter2, parameter3, parameter4, parameter5,
         nativeParameter6, parameter7, parameter8, parameter9, parameter10,
         parameter11, parameter12, parameter13, parameter14, parameter15,
         parameter16, parameter17, parameter18, parameter19, parameter20,
         parameter21, parameter22);
}

void VgpuStructuredTextureTransferHook(
    const std::uint64_t parameter1, const std::uint64_t *const parameter2,
    const std::uint64_t parameter3, const std::uint64_t parameter4,
    const std::int64_t parameter5, const std::int64_t parameter6,
    const std::int64_t parameter7, const std::uint32_t parameter8,
    const std::uint32_t parameter9, const std::uint32_t parameter10,
    const std::uint64_t parameter11, const std::uint64_t parameter12) noexcept {
  const auto transferCallCount =
      g_textureTransferCallCount.fetch_add(1, std::memory_order_relaxed) + 1;
  BridgeVgpuTextureTransferCallCount = transferCallCount;

  const std::uint64_t *nativeParameter2 = parameter2;
  std::array<std::uint64_t, 6> parameter2Copy{};
  if (parameter2 != nullptr) {
    std::memcpy(parameter2Copy.data(), parameter2, sizeof(parameter2Copy));
    auto transferConstants =
        xeo3::vgpu::detail::ExtractStructuredTextureTransferConstants(
            parameter2Copy.data());
    if (PatchTextureTransferConstants(transferConstants, transferCallCount)) {
      std::memcpy(reinterpret_cast<std::uint8_t *>(parameter2Copy.data()) +
                      sizeof(std::uint32_t),
                  &transferConstants[1], sizeof(transferConstants[1]));
      nativeParameter2 = parameter2Copy.data();
    }
  }

  const auto native = g_nativeStructuredTextureTransfer;
  if (native == nullptr) {
    return;
  }
  native(parameter1, nativeParameter2, parameter3, parameter4, parameter5,
         parameter6, parameter7, parameter8, parameter9, parameter10,
         parameter11, parameter12);
}

void VgpuConstantUploadHook(const std::uint64_t context,
                            const void *const source,
                            const std::size_t sourceSize) noexcept {
  const auto transferCallCount =
      g_textureTransferCallCount.fetch_add(1, std::memory_order_relaxed) + 1;
  BridgeVgpuTextureTransferCallCount = transferCallCount;
  const auto constantUploadCallCount =
      g_constantUploadCallCount.fetch_add(1, std::memory_order_relaxed) + 1;
  BridgeVgpuConstantUploadCallCount = constantUploadCallCount;
  if (sourceSize == 0x80) {
    BridgeVgpuConstantUpload128Count =
        g_constantUpload128Count.fetch_add(1, std::memory_order_relaxed) + 1;
  }
  RecordConstantUploadContext(context, sourceSize);
  RecordAc6Pso341UploadContext(context, sourceSize);

  if (source != nullptr &&
      sourceSize >= sizeof(std::array<std::uint32_t, 16>)) {
    std::array<std::uint32_t, 16> edramConstants{};
    std::memcpy(edramConstants.data(), source, sizeof(edramConstants));
    RecordEdramTransferConstants(context, sourceSize, constantUploadCallCount,
                                 edramConstants);
  }

  const void *nativeSource = source;
  std::array<std::uint8_t, xeo3::vgpu::kAc6Pso341TaskBufferSize>
      transfer341SourceCopy{};
  if (source != nullptr && sourceSize == transfer341SourceCopy.size()) {
    const auto candidateCount = g_transfer341WidthCandidateCount.fetch_add(
                                    1, std::memory_order_relaxed) +
                                1;
    BridgeVgpuTransfer341WidthCandidateCount = candidateCount;

    const auto state =
        xeo3::vgpu::detail::ClassifyAc6Pso341TaskWidth(source, sourceSize);
    if (state != xeo3::vgpu::Ac6Pso341TaskWidthState::NotCandidate) {
      const auto matchCount = g_transfer341WidthSignatureMatchCount.fetch_add(
                                  1, std::memory_order_relaxed) +
                              1;
      BridgeVgpuTransfer341WidthSignatureMatchCount = matchCount;
      BridgeVgpuTransfer341WidthLastCall = constantUploadCallCount;
      BridgeVgpuTransfer341WidthLastContext = context;
      BridgeVgpuTransfer341WidthLastSourceSize = sourceSize;
      std::uint32_t packedDimensions = 0;
      std::memcpy(&packedDimensions,
                  static_cast<const std::uint8_t *>(source) +
                      sizeof(std::uint32_t) * 2,
                  sizeof(packedDimensions));
      BridgeVgpuTransfer341WidthLastOriginalPackedDimensions = packedDimensions;
      BridgeVgpuTransfer341WidthLastReplacementPackedDimensions =
          packedDimensions;

      if (state == xeo3::vgpu::Ac6Pso341TaskWidthState::Broken &&
          BridgeVgpuTransfer341WidthFixEnabled != 0) {
        std::memcpy(transfer341SourceCopy.data(), source,
                    transfer341SourceCopy.size());
        std::uint32_t originalPackedDimensions = 0;
        std::uint32_t replacementPackedDimensions = 0;
        if (xeo3::vgpu::detail::PatchAc6Pso341TaskWidth(
                transfer341SourceCopy.data(), transfer341SourceCopy.size(),
                originalPackedDimensions, replacementPackedDimensions)) {
          nativeSource = transfer341SourceCopy.data();
          BridgeVgpuTransfer341WidthLastOriginalPackedDimensions =
              originalPackedDimensions;
          BridgeVgpuTransfer341WidthLastReplacementPackedDimensions =
              replacementPackedDimensions;
          const auto patchCount = g_transfer341WidthPatchCount.fetch_add(
                                      1, std::memory_order_relaxed) +
                                  1;
          BridgeVgpuTransfer341WidthPatchCount = patchCount;
          if (patchCount <= 16 || (patchCount & (patchCount - 1)) == 0) {
            EmitPatchEvent("transfer341_width_fix", replacementPackedDimensions,
                           patchCount);
          }
        } else {
          BridgeVgpuTransfer341WidthFailureCount =
              g_transfer341WidthFailureCount.fetch_add(
                  1, std::memory_order_relaxed) +
              1;
        }
      }
    }
  }

  std::array<std::uint8_t, 0x80> sourceCopy{};
  if (source != nullptr && sourceSize == sourceCopy.size()) {
    std::array<std::uint32_t, 6> transferConstants{};
    std::memcpy(transferConstants.data(), source, sizeof(transferConstants));
    if (PatchTextureTransferConstants(transferConstants, transferCallCount)) {
      std::memcpy(sourceCopy.data(), source, sourceCopy.size());
      std::memcpy(sourceCopy.data() + sizeof(std::uint32_t),
                  &transferConstants[1], sizeof(transferConstants[1]));
      nativeSource = sourceCopy.data();
    }
  }

  if (BridgeVgpuG2HTraceEnabled != 0 && sourceSize == 0x80) {
    xeo3::vgpu::RecordG2HTrace(g_moduleBase, _AddressOfReturnAddress(), context,
                              source, nativeSource, sourceSize);
  }
  const auto native = g_nativeConstantUpload;
  if (native != nullptr) {
    native(context, nativeSource, sourceSize);
  }
}

bool InstallShaderCompileHook(std::uint8_t *const moduleBase) noexcept {
  auto *const target = moduleBase + kShaderCompileRva;
  if (!xeo3::vgpu::detail::HasExpectedShaderCompilePrologue(
          target, xeo3::vgpu::kShaderCompileDetourSize)) {
    SetStatus(PatchStatus::ShaderCompilePrologueMismatch);
    return false;
  }

  constexpr std::size_t kTrampolineCapacity = 64;
  auto *const trampoline = static_cast<std::uint8_t *>(VirtualAlloc(
      nullptr, kTrampolineCapacity, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  if (trampoline == nullptr) {
    SetStatus(PatchStatus::ShaderCompileTrampolineAllocationFailure);
    return false;
  }

  std::memcpy(trampoline, kExpectedShaderCompilePrologue.data(),
              kExpectedShaderCompilePrologue.size());
  const auto returnJump = xeo3::vgpu::detail::EncodeAbsoluteJump(
      target + xeo3::vgpu::kShaderCompileDetourSize);
  std::memcpy(trampoline + xeo3::vgpu::kShaderCompileDetourSize,
              returnJump.data(), returnJump.size());

  DWORD trampolineOldProtection = 0;
  if (!VirtualProtect(trampoline, kTrampolineCapacity, PAGE_EXECUTE_READ,
                      &trampolineOldProtection)) {
    VirtualFree(trampoline, 0, MEM_RELEASE);
    SetStatus(PatchStatus::ShaderCompileTrampolineProtectionFailure);
    return false;
  }
  FlushInstructionCache(GetCurrentProcess(), trampoline,
                        xeo3::vgpu::kShaderCompileDetourSize +
                            returnJump.size());

  const auto detour = xeo3::vgpu::detail::EncodeShaderCompileJump(
      reinterpret_cast<const void *>(&VgpuCompileHlslHook));
  g_nativeCompileHlsl = reinterpret_cast<NativeCompileHlsl>(trampoline);
  g_shaderCompileTrampoline = trampoline;
  g_installedShaderCompileDetour = detour;

  DWORD targetOldProtection = 0;
  if (!VirtualProtect(target, xeo3::vgpu::kShaderCompileDetourSize,
                      PAGE_EXECUTE_READWRITE, &targetOldProtection)) {
    g_nativeCompileHlsl = nullptr;
    g_shaderCompileTrampoline = nullptr;
    VirtualFree(trampoline, 0, MEM_RELEASE);
    SetStatus(PatchStatus::ShaderCompileTargetProtectionFailure);
    return false;
  }

  std::memcpy(target, detour.data(), detour.size());
  FlushInstructionCache(GetCurrentProcess(), target, detour.size());

  DWORD ignoredProtection = 0;
  if (!VirtualProtect(target, xeo3::vgpu::kShaderCompileDetourSize,
                      targetOldProtection, &ignoredProtection)) {
    std::memcpy(target, kExpectedShaderCompilePrologue.data(),
                kExpectedShaderCompilePrologue.size());
    FlushInstructionCache(GetCurrentProcess(), target,
                          kExpectedShaderCompilePrologue.size());
    VirtualProtect(target, xeo3::vgpu::kShaderCompileDetourSize,
                   targetOldProtection, &ignoredProtection);
    g_nativeCompileHlsl = nullptr;
    g_shaderCompileTrampoline = nullptr;
    VirtualFree(trampoline, 0, MEM_RELEASE);
    SetStatus(PatchStatus::ShaderCompileTargetProtectionRestoreFailure);
    return false;
  }

  g_shaderCompileTarget = target;
  return true;
}

bool RemoveShaderCompileHook() noexcept {
  auto *const target = g_shaderCompileTarget;
  if (target == nullptr) {
    return true;
  }
  if (!std::equal(g_installedShaderCompileDetour.begin(),
                  g_installedShaderCompileDetour.end(), target)) {
    SetStatus(PatchStatus::ShaderCompileDetourChanged);
    return false;
  }

  DWORD oldProtection = 0;
  if (!VirtualProtect(target, xeo3::vgpu::kShaderCompileDetourSize,
                      PAGE_EXECUTE_READWRITE, &oldProtection)) {
    SetStatus(PatchStatus::ShaderCompileTargetProtectionFailure);
    return false;
  }
  std::memcpy(target, kExpectedShaderCompilePrologue.data(),
              kExpectedShaderCompilePrologue.size());
  FlushInstructionCache(GetCurrentProcess(), target,
                        kExpectedShaderCompilePrologue.size());
  DWORD ignoredProtection = 0;
  const auto restoredProtection =
      VirtualProtect(target, xeo3::vgpu::kShaderCompileDetourSize,
                     oldProtection, &ignoredProtection);

  auto *const trampoline = g_shaderCompileTrampoline;
  g_shaderCompileTarget = nullptr;
  g_shaderCompileTrampoline = nullptr;
  g_nativeCompileHlsl = nullptr;
  if (trampoline != nullptr) {
    VirtualFree(trampoline, 0, MEM_RELEASE);
  }
  if (!restoredProtection) {
    SetStatus(PatchStatus::ShaderCompileTargetProtectionRestoreFailure);
    return false;
  }
  return true;
}

bool InstallXenosTranslateHook(std::uint8_t *const moduleBase) noexcept {
  auto *const target = moduleBase + kXenosTranslateRva;
  if (!xeo3::vgpu::detail::HasExpectedXenosTranslatePrologue(
          target, xeo3::vgpu::kXenosTranslateDetourSize)) {
    SetStatus(PatchStatus::XenosTranslatePrologueMismatch);
    BridgeVgpuXenosTranslateHookFailure =
        static_cast<std::uint32_t>(PatchStatus::XenosTranslatePrologueMismatch);
    return false;
  }

  constexpr std::size_t kTrampolineCapacity = 64;
  auto *const trampoline = static_cast<std::uint8_t *>(VirtualAlloc(
      nullptr, kTrampolineCapacity, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  if (trampoline == nullptr) {
    SetStatus(PatchStatus::XenosTranslateTrampolineAllocationFailure);
    BridgeVgpuXenosTranslateHookFailure = static_cast<std::uint32_t>(
        PatchStatus::XenosTranslateTrampolineAllocationFailure);
    return false;
  }

  std::memcpy(trampoline, kExpectedXenosTranslatePrologue.data(),
              kExpectedXenosTranslatePrologue.size());
  const auto returnJump = xeo3::vgpu::detail::EncodeAbsoluteJump(
      target + xeo3::vgpu::kXenosTranslateDetourSize);
  std::memcpy(trampoline + xeo3::vgpu::kXenosTranslateDetourSize,
              returnJump.data(), returnJump.size());

  DWORD trampolineOldProtection = 0;
  if (!VirtualProtect(trampoline, kTrampolineCapacity, PAGE_EXECUTE_READ,
                      &trampolineOldProtection)) {
    VirtualFree(trampoline, 0, MEM_RELEASE);
    SetStatus(PatchStatus::XenosTranslateTrampolineProtectionFailure);
    BridgeVgpuXenosTranslateHookFailure = static_cast<std::uint32_t>(
        PatchStatus::XenosTranslateTrampolineProtectionFailure);
    return false;
  }
  FlushInstructionCache(GetCurrentProcess(), trampoline,
                        xeo3::vgpu::kXenosTranslateDetourSize +
                            returnJump.size());

  const auto detour = xeo3::vgpu::detail::EncodeXenosTranslateJump(
      reinterpret_cast<const void *>(&VgpuTranslateXenosShaderHook));
  g_nativeTranslateXenosShader =
      reinterpret_cast<NativeTranslateXenosShader>(trampoline);
  g_xenosTranslateTrampoline = trampoline;
  g_installedXenosTranslateDetour = detour;

  DWORD targetOldProtection = 0;
  if (!VirtualProtect(target, xeo3::vgpu::kXenosTranslateDetourSize,
                      PAGE_EXECUTE_READWRITE, &targetOldProtection)) {
    g_nativeTranslateXenosShader = nullptr;
    g_xenosTranslateTrampoline = nullptr;
    VirtualFree(trampoline, 0, MEM_RELEASE);
    SetStatus(PatchStatus::XenosTranslateTargetProtectionFailure);
    BridgeVgpuXenosTranslateHookFailure = static_cast<std::uint32_t>(
        PatchStatus::XenosTranslateTargetProtectionFailure);
    return false;
  }

  std::memcpy(target, detour.data(), detour.size());
  FlushInstructionCache(GetCurrentProcess(), target, detour.size());

  DWORD ignoredProtection = 0;
  if (!VirtualProtect(target, xeo3::vgpu::kXenosTranslateDetourSize,
                      targetOldProtection, &ignoredProtection)) {
    std::memcpy(target, kExpectedXenosTranslatePrologue.data(),
                kExpectedXenosTranslatePrologue.size());
    FlushInstructionCache(GetCurrentProcess(), target,
                          kExpectedXenosTranslatePrologue.size());
    VirtualProtect(target, xeo3::vgpu::kXenosTranslateDetourSize,
                   targetOldProtection, &ignoredProtection);
    g_nativeTranslateXenosShader = nullptr;
    g_xenosTranslateTrampoline = nullptr;
    VirtualFree(trampoline, 0, MEM_RELEASE);
    SetStatus(PatchStatus::XenosTranslateTargetProtectionRestoreFailure);
    BridgeVgpuXenosTranslateHookFailure = static_cast<std::uint32_t>(
        PatchStatus::XenosTranslateTargetProtectionRestoreFailure);
    return false;
  }

  g_xenosTranslateTarget = target;
  BridgeVgpuXenosTranslateHookInstalled = 1;
  BridgeVgpuXenosTranslateHookFailure = ERROR_SUCCESS;
  return true;
}

bool RemoveXenosTranslateHook() noexcept {
  auto *const target = g_xenosTranslateTarget;
  if (target == nullptr) {
    BridgeVgpuXenosTranslateHookInstalled = 0;
    return true;
  }
  if (!std::equal(g_installedXenosTranslateDetour.begin(),
                  g_installedXenosTranslateDetour.end(), target)) {
    SetStatus(PatchStatus::XenosTranslateDetourChanged);
    BridgeVgpuXenosTranslateHookFailure =
        static_cast<std::uint32_t>(PatchStatus::XenosTranslateDetourChanged);
    return false;
  }

  DWORD oldProtection = 0;
  if (!VirtualProtect(target, xeo3::vgpu::kXenosTranslateDetourSize,
                      PAGE_EXECUTE_READWRITE, &oldProtection)) {
    SetStatus(PatchStatus::XenosTranslateTargetProtectionFailure);
    BridgeVgpuXenosTranslateHookFailure = static_cast<std::uint32_t>(
        PatchStatus::XenosTranslateTargetProtectionFailure);
    return false;
  }
  std::memcpy(target, kExpectedXenosTranslatePrologue.data(),
              kExpectedXenosTranslatePrologue.size());
  FlushInstructionCache(GetCurrentProcess(), target,
                        kExpectedXenosTranslatePrologue.size());
  DWORD ignoredProtection = 0;
  const auto restoredProtection =
      VirtualProtect(target, xeo3::vgpu::kXenosTranslateDetourSize,
                     oldProtection, &ignoredProtection);

  auto *const trampoline = g_xenosTranslateTrampoline;
  g_xenosTranslateTarget = nullptr;
  g_xenosTranslateTrampoline = nullptr;
  g_nativeTranslateXenosShader = nullptr;
  BridgeVgpuXenosTranslateHookInstalled = 0;
  if (trampoline != nullptr) {
    VirtualFree(trampoline, 0, MEM_RELEASE);
  }
  if (!restoredProtection) {
    SetStatus(PatchStatus::XenosTranslateTargetProtectionRestoreFailure);
    BridgeVgpuXenosTranslateHookFailure = static_cast<std::uint32_t>(
        PatchStatus::XenosTranslateTargetProtectionRestoreFailure);
    return false;
  }
  BridgeVgpuXenosTranslateHookFailure = ERROR_SUCCESS;
  return true;
}

bool InstallTextureTransferHook(std::uint8_t *const moduleBase) noexcept {
  auto *const target = moduleBase + kTextureTransferRva;
  if (!xeo3::vgpu::detail::HasExpectedTextureTransferPrologue(
          target, xeo3::vgpu::kTextureTransferDetourSize)) {
    SetStatus(PatchStatus::TextureTransferPrologueMismatch);
    return false;
  }

  constexpr std::size_t kTrampolineCapacity = 64;
  auto *const trampoline = static_cast<std::uint8_t *>(VirtualAlloc(
      nullptr, kTrampolineCapacity, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  if (trampoline == nullptr) {
    SetStatus(PatchStatus::TextureTransferTrampolineAllocationFailure);
    return false;
  }

  std::memcpy(trampoline, kExpectedTextureTransferPrologue.data(),
              kExpectedTextureTransferPrologue.size());
  const auto returnJump = xeo3::vgpu::detail::EncodeAbsoluteJump(
      target + xeo3::vgpu::kTextureTransferDetourSize);
  std::memcpy(trampoline + xeo3::vgpu::kTextureTransferDetourSize,
              returnJump.data(), returnJump.size());

  DWORD trampolineOldProtection = 0;
  if (!VirtualProtect(trampoline, kTrampolineCapacity, PAGE_EXECUTE_READ,
                      &trampolineOldProtection)) {
    VirtualFree(trampoline, 0, MEM_RELEASE);
    SetStatus(PatchStatus::TextureTransferTrampolineProtectionFailure);
    return false;
  }
  FlushInstructionCache(GetCurrentProcess(), trampoline,
                        xeo3::vgpu::kTextureTransferDetourSize +
                            returnJump.size());

  const auto detour = xeo3::vgpu::detail::EncodeTextureTransferJump(
      reinterpret_cast<const void *>(&VgpuTextureTransferHook));
  g_nativeTextureTransfer = reinterpret_cast<NativeTextureTransfer>(trampoline);
  g_textureTransferTrampoline = trampoline;
  g_installedTextureTransferDetour = detour;

  DWORD targetOldProtection = 0;
  if (!VirtualProtect(target, xeo3::vgpu::kTextureTransferDetourSize,
                      PAGE_EXECUTE_READWRITE, &targetOldProtection)) {
    g_nativeTextureTransfer = nullptr;
    g_textureTransferTrampoline = nullptr;
    VirtualFree(trampoline, 0, MEM_RELEASE);
    SetStatus(PatchStatus::TextureTransferTargetProtectionFailure);
    return false;
  }

  std::memcpy(target, detour.data(), detour.size());
  FlushInstructionCache(GetCurrentProcess(), target, detour.size());

  DWORD ignoredProtection = 0;
  if (!VirtualProtect(target, xeo3::vgpu::kTextureTransferDetourSize,
                      targetOldProtection, &ignoredProtection)) {
    std::memcpy(target, kExpectedTextureTransferPrologue.data(),
                kExpectedTextureTransferPrologue.size());
    FlushInstructionCache(GetCurrentProcess(), target,
                          kExpectedTextureTransferPrologue.size());
    VirtualProtect(target, xeo3::vgpu::kTextureTransferDetourSize,
                   targetOldProtection, &ignoredProtection);
    g_nativeTextureTransfer = nullptr;
    g_textureTransferTrampoline = nullptr;
    VirtualFree(trampoline, 0, MEM_RELEASE);
    SetStatus(PatchStatus::TextureTransferTargetProtectionRestoreFailure);
    return false;
  }

  g_textureTransferTarget = target;
  return true;
}

bool RemoveTextureTransferHook() noexcept {
  auto *const target = g_textureTransferTarget;
  if (target == nullptr) {
    return true;
  }
  if (!std::equal(g_installedTextureTransferDetour.begin(),
                  g_installedTextureTransferDetour.end(), target)) {
    SetStatus(PatchStatus::TextureTransferDetourChanged);
    return false;
  }

  DWORD oldProtection = 0;
  if (!VirtualProtect(target, xeo3::vgpu::kTextureTransferDetourSize,
                      PAGE_EXECUTE_READWRITE, &oldProtection)) {
    SetStatus(PatchStatus::TextureTransferTargetProtectionFailure);
    return false;
  }
  std::memcpy(target, kExpectedTextureTransferPrologue.data(),
              kExpectedTextureTransferPrologue.size());
  FlushInstructionCache(GetCurrentProcess(), target,
                        kExpectedTextureTransferPrologue.size());
  DWORD ignoredProtection = 0;
  const auto restoredProtection =
      VirtualProtect(target, xeo3::vgpu::kTextureTransferDetourSize,
                     oldProtection, &ignoredProtection);

  auto *const trampoline = g_textureTransferTrampoline;
  g_textureTransferTarget = nullptr;
  g_textureTransferTrampoline = nullptr;
  g_nativeTextureTransfer = nullptr;
  if (trampoline != nullptr) {
    VirtualFree(trampoline, 0, MEM_RELEASE);
  }
  if (!restoredProtection) {
    SetStatus(PatchStatus::TextureTransferTargetProtectionRestoreFailure);
    return false;
  }
  return true;
}

bool InstallStructuredTextureTransferHook(
    std::uint8_t *const moduleBase) noexcept {
  auto *const target = moduleBase + kStructuredTextureTransferRva;
  if (!xeo3::vgpu::detail::HasExpectedStructuredTextureTransferPrologue(
          target, xeo3::vgpu::kStructuredTextureTransferDetourSize)) {
    SetStatus(PatchStatus::StructuredTextureTransferPrologueMismatch);
    return false;
  }

  constexpr std::size_t kTrampolineCapacity = 64;
  auto *const trampoline = static_cast<std::uint8_t *>(VirtualAlloc(
      nullptr, kTrampolineCapacity, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  if (trampoline == nullptr) {
    SetStatus(
        PatchStatus::StructuredTextureTransferTrampolineAllocationFailure);
    return false;
  }

  std::memcpy(trampoline, kExpectedStructuredTextureTransferPrologue.data(),
              kExpectedStructuredTextureTransferPrologue.size());
  const auto returnJump = xeo3::vgpu::detail::EncodeAbsoluteJump(
      target + xeo3::vgpu::kStructuredTextureTransferDetourSize);
  std::memcpy(trampoline + xeo3::vgpu::kStructuredTextureTransferDetourSize,
              returnJump.data(), returnJump.size());

  DWORD trampolineOldProtection = 0;
  if (!VirtualProtect(trampoline, kTrampolineCapacity, PAGE_EXECUTE_READ,
                      &trampolineOldProtection)) {
    VirtualFree(trampoline, 0, MEM_RELEASE);
    SetStatus(
        PatchStatus::StructuredTextureTransferTrampolineProtectionFailure);
    return false;
  }
  FlushInstructionCache(GetCurrentProcess(), trampoline,
                        xeo3::vgpu::kStructuredTextureTransferDetourSize +
                            returnJump.size());

  const auto detour = xeo3::vgpu::detail::EncodeStructuredTextureTransferJump(
      reinterpret_cast<const void *>(&VgpuStructuredTextureTransferHook));
  g_nativeStructuredTextureTransfer =
      reinterpret_cast<NativeStructuredTextureTransfer>(trampoline);
  g_structuredTextureTransferTrampoline = trampoline;
  g_installedStructuredTextureTransferDetour = detour;

  DWORD targetOldProtection = 0;
  if (!VirtualProtect(target, xeo3::vgpu::kStructuredTextureTransferDetourSize,
                      PAGE_EXECUTE_READWRITE, &targetOldProtection)) {
    g_nativeStructuredTextureTransfer = nullptr;
    g_structuredTextureTransferTrampoline = nullptr;
    VirtualFree(trampoline, 0, MEM_RELEASE);
    SetStatus(PatchStatus::StructuredTextureTransferTargetProtectionFailure);
    return false;
  }

  std::memcpy(target, detour.data(), detour.size());
  FlushInstructionCache(GetCurrentProcess(), target, detour.size());

  DWORD ignoredProtection = 0;
  if (!VirtualProtect(target, xeo3::vgpu::kStructuredTextureTransferDetourSize,
                      targetOldProtection, &ignoredProtection)) {
    std::memcpy(target, kExpectedStructuredTextureTransferPrologue.data(),
                kExpectedStructuredTextureTransferPrologue.size());
    FlushInstructionCache(GetCurrentProcess(), target,
                          kExpectedStructuredTextureTransferPrologue.size());
    VirtualProtect(target, xeo3::vgpu::kStructuredTextureTransferDetourSize,
                   targetOldProtection, &ignoredProtection);
    g_nativeStructuredTextureTransfer = nullptr;
    g_structuredTextureTransferTrampoline = nullptr;
    VirtualFree(trampoline, 0, MEM_RELEASE);
    SetStatus(
        PatchStatus::StructuredTextureTransferTargetProtectionRestoreFailure);
    return false;
  }

  g_structuredTextureTransferTarget = target;
  return true;
}

bool RemoveStructuredTextureTransferHook() noexcept {
  auto *const target = g_structuredTextureTransferTarget;
  if (target == nullptr) {
    return true;
  }
  if (!std::equal(g_installedStructuredTextureTransferDetour.begin(),
                  g_installedStructuredTextureTransferDetour.end(), target)) {
    SetStatus(PatchStatus::StructuredTextureTransferDetourChanged);
    return false;
  }

  DWORD oldProtection = 0;
  if (!VirtualProtect(target, xeo3::vgpu::kStructuredTextureTransferDetourSize,
                      PAGE_EXECUTE_READWRITE, &oldProtection)) {
    SetStatus(PatchStatus::StructuredTextureTransferTargetProtectionFailure);
    return false;
  }
  std::memcpy(target, kExpectedStructuredTextureTransferPrologue.data(),
              kExpectedStructuredTextureTransferPrologue.size());
  FlushInstructionCache(GetCurrentProcess(), target,
                        kExpectedStructuredTextureTransferPrologue.size());
  DWORD ignoredProtection = 0;
  const auto restoredProtection =
      VirtualProtect(target, xeo3::vgpu::kStructuredTextureTransferDetourSize,
                     oldProtection, &ignoredProtection);

  auto *const trampoline = g_structuredTextureTransferTrampoline;
  g_structuredTextureTransferTarget = nullptr;
  g_structuredTextureTransferTrampoline = nullptr;
  g_nativeStructuredTextureTransfer = nullptr;
  if (trampoline != nullptr) {
    VirtualFree(trampoline, 0, MEM_RELEASE);
  }
  if (!restoredProtection) {
    SetStatus(
        PatchStatus::StructuredTextureTransferTargetProtectionRestoreFailure);
    return false;
  }
  return true;
}

bool InstallConstantUploadHook(std::uint8_t *const moduleBase) noexcept {
  auto *const target = moduleBase + kConstantUploadRva;
  if (!xeo3::vgpu::detail::HasExpectedConstantUploadPrologue(
          target, xeo3::vgpu::kConstantUploadDetourSize)) {
    SetStatus(PatchStatus::ConstantUploadPrologueMismatch);
    return false;
  }

  constexpr std::size_t kTrampolineCapacity = 64;
  auto *const trampoline = static_cast<std::uint8_t *>(VirtualAlloc(
      nullptr, kTrampolineCapacity, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  if (trampoline == nullptr) {
    SetStatus(PatchStatus::ConstantUploadTrampolineAllocationFailure);
    return false;
  }

  std::memcpy(trampoline, kExpectedConstantUploadPrologue.data(),
              kExpectedConstantUploadPrologue.size());
  const auto returnJump = xeo3::vgpu::detail::EncodeAbsoluteJump(
      target + xeo3::vgpu::kConstantUploadDetourSize);
  std::memcpy(trampoline + xeo3::vgpu::kConstantUploadDetourSize,
              returnJump.data(), returnJump.size());

  DWORD trampolineOldProtection = 0;
  if (!VirtualProtect(trampoline, kTrampolineCapacity, PAGE_EXECUTE_READ,
                      &trampolineOldProtection)) {
    VirtualFree(trampoline, 0, MEM_RELEASE);
    SetStatus(PatchStatus::ConstantUploadTrampolineProtectionFailure);
    return false;
  }
  FlushInstructionCache(GetCurrentProcess(), trampoline,
                        xeo3::vgpu::kConstantUploadDetourSize +
                            returnJump.size());

  const auto detour = xeo3::vgpu::detail::EncodeConstantUploadJump(
      reinterpret_cast<const void *>(&VgpuConstantUploadHook));
  g_nativeConstantUpload = reinterpret_cast<NativeConstantUpload>(trampoline);
  g_constantUploadTrampoline = trampoline;
  g_installedConstantUploadDetour = detour;

  DWORD targetOldProtection = 0;
  if (!VirtualProtect(target, xeo3::vgpu::kConstantUploadDetourSize,
                      PAGE_EXECUTE_READWRITE, &targetOldProtection)) {
    g_nativeConstantUpload = nullptr;
    g_constantUploadTrampoline = nullptr;
    VirtualFree(trampoline, 0, MEM_RELEASE);
    SetStatus(PatchStatus::ConstantUploadTargetProtectionFailure);
    return false;
  }

  std::memcpy(target, detour.data(), detour.size());
  FlushInstructionCache(GetCurrentProcess(), target, detour.size());

  DWORD ignoredProtection = 0;
  if (!VirtualProtect(target, xeo3::vgpu::kConstantUploadDetourSize,
                      targetOldProtection, &ignoredProtection)) {
    std::memcpy(target, kExpectedConstantUploadPrologue.data(),
                kExpectedConstantUploadPrologue.size());
    FlushInstructionCache(GetCurrentProcess(), target,
                          kExpectedConstantUploadPrologue.size());
    VirtualProtect(target, xeo3::vgpu::kConstantUploadDetourSize,
                   targetOldProtection, &ignoredProtection);
    g_nativeConstantUpload = nullptr;
    g_constantUploadTrampoline = nullptr;
    VirtualFree(trampoline, 0, MEM_RELEASE);
    SetStatus(PatchStatus::ConstantUploadTargetProtectionRestoreFailure);
    return false;
  }

  g_constantUploadTarget = target;
  return true;
}

bool RemoveConstantUploadHook() noexcept {
  auto *const target = g_constantUploadTarget;
  if (target == nullptr) {
    return true;
  }
  if (!std::equal(g_installedConstantUploadDetour.begin(),
                  g_installedConstantUploadDetour.end(), target)) {
    SetStatus(PatchStatus::ConstantUploadDetourChanged);
    return false;
  }

  DWORD oldProtection = 0;
  if (!VirtualProtect(target, xeo3::vgpu::kConstantUploadDetourSize,
                      PAGE_EXECUTE_READWRITE, &oldProtection)) {
    SetStatus(PatchStatus::ConstantUploadTargetProtectionFailure);
    return false;
  }
  std::memcpy(target, kExpectedConstantUploadPrologue.data(),
              kExpectedConstantUploadPrologue.size());
  FlushInstructionCache(GetCurrentProcess(), target,
                        kExpectedConstantUploadPrologue.size());
  DWORD ignoredProtection = 0;
  const auto restoredProtection =
      VirtualProtect(target, xeo3::vgpu::kConstantUploadDetourSize,
                     oldProtection, &ignoredProtection);

  auto *const trampoline = g_constantUploadTrampoline;
  g_constantUploadTarget = nullptr;
  g_constantUploadTrampoline = nullptr;
  g_nativeConstantUpload = nullptr;
  if (trampoline != nullptr) {
    VirtualFree(trampoline, 0, MEM_RELEASE);
  }
  if (!restoredProtection) {
    SetStatus(PatchStatus::ConstantUploadTargetProtectionRestoreFailure);
    return false;
  }
  return true;
}
} // namespace

extern "C" const void *
VgpuResolveEdramPipelineState(const void *const record,
                              const void *const commandList,
                              const void *const commandContext,
                              const void *const pipelineState) noexcept {
  EnsureGraphicsCommandListHooks(static_cast<ID3D12GraphicsCommandList *>(
      const_cast<void *>(commandList)));
  const auto callCount =
      g_drawRecordCallCount.fetch_add(1, std::memory_order_relaxed) + 1;
  BridgeVgpuDrawRecordCallCount = callCount;

  const void *resolvedPipelineState = pipelineState;
  xeo3::vgpu::DrawRecordSignature observed{};
  if (xeo3::vgpu::detail::ExtractDrawRecordSignature(
          record, xeo3::vgpu::kDrawRecordMinimumSize, observed)) {
    BridgeVgpuDrawRecordLastRecord = reinterpret_cast<std::uintptr_t>(record);
    BridgeVgpuDrawRecordLastCommandList =
        reinterpret_cast<std::uintptr_t>(commandList);
    BridgeVgpuDrawRecordLastCommandContext =
        reinterpret_cast<std::uintptr_t>(commandContext);
    BridgeVgpuDrawRecordLastRootSignature = observed.rootSignature;
    BridgeVgpuDrawRecordLastPipelineState =
        reinterpret_cast<std::uintptr_t>(pipelineState);
    BridgeVgpuDrawRecordLastViewportWidthBits = observed.viewportWidthBits;
    BridgeVgpuDrawRecordLastViewportHeightBits = observed.viewportHeightBits;
    BridgeVgpuDrawRecordLastViewportMinDepthBits =
        observed.viewportMinDepthBits;
    BridgeVgpuDrawRecordLastViewportMaxDepthBits =
        observed.viewportMaxDepthBits;
    BridgeVgpuDrawRecordLastViewportTopLeftXBits =
        observed.viewportTopLeftXBits;
    BridgeVgpuDrawRecordLastViewportTopLeftYBits =
        observed.viewportTopLeftYBits;
    BridgeVgpuDrawRecordLastScissorRight = observed.scissorRight;
    BridgeVgpuDrawRecordLastScissorBottom = observed.scissorBottom;
    BridgeVgpuDrawRecordLastRecordKind = observed.recordKind;
    BridgeVgpuDrawRecordLastVertexCount = observed.vertexCount;
    BridgeVgpuDrawRecordLastStartVertex = observed.startVertex;
    RecordInterestingDraw(observed, reinterpret_cast<std::uintptr_t>(record),
                          reinterpret_cast<std::uintptr_t>(commandList),
                          reinterpret_cast<std::uintptr_t>(commandContext),
                          reinterpret_cast<std::uintptr_t>(pipelineState));
    // The record-level pointer is an AC6/Xenos game PSO. XeO3's faulty
    // EDRAM transfer PSOs are bound later through the host D3D12 command
    // list, where the command-list hook performs the guarded substitution.
    resolvedPipelineState = pipelineState;

    const bool isMsaaViewportCandidate =
        observed.rootSignature != 0 &&
        observed.viewportWidthBits == 0x44A00000U &&
        observed.viewportHeightBits == 0x44340000U &&
        observed.viewportMinDepthBits == 0 &&
        observed.viewportMaxDepthBits == 0x3F800000U &&
        observed.viewportTopLeftXBits == 0 &&
        observed.viewportTopLeftYBits == 0 && observed.scissorRight == 640 &&
        observed.scissorBottom == 720 && observed.recordKind == 0 &&
        observed.vertexCount != 0;
    if (BridgeVgpuMsaaViewportFixEnabled != 0 &&
        isMsaaViewportCandidate) {
      const auto candidateCount = g_msaaViewportCandidateCount.fetch_add(
                                      1, std::memory_order_relaxed) +
                                  1;
      BridgeVgpuMsaaViewportCandidateCount = candidateCount;
      BridgeVgpuMsaaViewportLastPipelineState =
          reinterpret_cast<std::uintptr_t>(pipelineState);
      BridgeVgpuMsaaViewportLastOriginalWidthBits =
          observed.viewportWidthBits;
      BridgeVgpuMsaaViewportLastReplacementWidthBits = 0;
      BridgeVgpuMsaaViewportLastVertexCount = observed.vertexCount;

      ObservedPipelineState observedPipeline{};
      if (!FindObservedPipelineState(pipelineState, observedPipeline)) {
        BridgeVgpuMsaaViewportFixFailure = ERROR_NOT_FOUND;
        if (candidateCount <= 64 ||
            (candidateCount & (candidateCount - 1)) == 0) {
          EmitPatchEvent("msaa_viewport_pipeline_not_found",
                         BridgeVgpuMsaaViewportFixFailure, candidateCount);
        }
      } else {
        std::uint32_t originalWidthBits = 0;
        std::uint32_t replacementWidthBits = 0;
        if (xeo3::vgpu::detail::PatchAc6HalfWidthMsaaViewport(
                const_cast<void *>(record),
                xeo3::vgpu::kDrawRecordMinimumSize,
                observedPipeline.signature, pipelineState, originalWidthBits,
                replacementWidthBits)) {
          const auto fixedCount = g_msaaViewportFixCount.fetch_add(
                                      1, std::memory_order_relaxed) +
                                  1;
          BridgeVgpuMsaaViewportFixCount = fixedCount;
          BridgeVgpuMsaaViewportFixFailure = ERROR_SUCCESS;
          BridgeVgpuMsaaViewportLastOriginalWidthBits = originalWidthBits;
          BridgeVgpuMsaaViewportLastReplacementWidthBits =
              replacementWidthBits;
          if (fixedCount <= 64 || (fixedCount & (fixedCount - 1)) == 0) {
            EmitPatchEvent("msaa_viewport_fixed", replacementWidthBits,
                           fixedCount);
          }
        } else {
          BridgeVgpuMsaaViewportFixFailure = ERROR_INVALID_DATA;
          if (candidateCount <= 64 ||
              (candidateCount & (candidateCount - 1)) == 0) {
            EmitPatchEvent("msaa_viewport_rejected",
                           BridgeVgpuMsaaViewportFixFailure, candidateCount);
          }
        }
      }
    }

    if (IsNativeHalfWidthSmallFullscreenDraw(observed, pipelineState)) {
      const auto hitCount = g_edramRestoreExperimentHitCount.fetch_add(
                                1, std::memory_order_relaxed) +
                            1;
      BridgeVgpuEdramRestoreExperimentHitCount = hitCount;
      const auto candidateId =
          RegisterEdramRestoreExperimentCandidate(pipelineState);
      if (candidateId != 0) {
        BridgeVgpuEdramRestoreExperimentLastCandidate = candidateId;
        const auto selector = BridgeVgpuEdramRestoreExperimentSelector;
        if (selector == candidateId) {
          const auto skipCount = g_edramRestoreExperimentSkipCount.fetch_add(
                                     1, std::memory_order_relaxed) +
                                 1;
          BridgeVgpuEdramRestoreExperimentSkipCount = skipCount;
          if (skipCount <= 64 || (skipCount & (skipCount - 1)) == 0) {
            EmitPatchEvent("edram_restore_experiment_skipped", candidateId,
                           skipCount);
          }
          return nullptr;
        }
      }

      if (BridgeVgpuFullscreenScissorFixEnabled != 0) {
        if (xeo3::vgpu::detail::PatchAc6HalfWidthFullscreenScissor(
                const_cast<void *>(record),
                xeo3::vgpu::kDrawRecordMinimumSize)) {
          const auto fixedCount = g_fullscreenScissorFixCount.fetch_add(
                                      1, std::memory_order_relaxed) +
                                  1;
          BridgeVgpuFullscreenScissorFixCount = fixedCount;
          BridgeVgpuFullscreenScissorFixFailure = ERROR_SUCCESS;
          if (fixedCount <= 64 || (fixedCount & (fixedCount - 1)) == 0) {
            EmitPatchEvent("fullscreen_scissor_fixed", 1280, fixedCount);
          }
        } else {
          BridgeVgpuFullscreenScissorFixFailure = ERROR_INVALID_DATA;
          EmitPatchEvent("fullscreen_scissor_fix_failure",
                         BridgeVgpuFullscreenScissorFixFailure, hitCount);
        }
      }
    }
  }

  if (!xeo3::vgpu::detail::IsAc6CorruptEdramRestoreDrawRecord(
          record, xeo3::vgpu::kDrawRecordMinimumSize, pipelineState)) {
    return resolvedPipelineState;
  }

  const auto candidateCount =
      g_edramRestoreDrawCandidateCount.fetch_add(1, std::memory_order_relaxed) +
      1;
  BridgeVgpuEdramRestoreDrawCandidateCount = candidateCount;
  BridgeVgpuEdramRestoreDrawLastRecord =
      reinterpret_cast<std::uintptr_t>(record);
  BridgeVgpuEdramRestoreDrawLastPipelineState =
      reinterpret_cast<std::uintptr_t>(pipelineState);

  bool matches = false;
  {
    std::scoped_lock lock(g_edramRestoreDrawMutex);
    if (g_edramRestoreDrawPipelineState != pipelineState) {
      g_edramRestoreDrawPipelineState = pipelineState;
      g_edramRestoreDrawPipelineMatches = false;
      BridgeVgpuEdramRestoreDrawGuardFailure = ERROR_SUCCESS;
      BridgeVgpuEdramRestoreDrawLastCachedBlobSize = 0;
      BridgeVgpuEdramRestoreDrawLastCachedBlobHash0 = 0;
      BridgeVgpuEdramRestoreDrawLastCachedBlobHash1 = 0;
      BridgeVgpuEdramRestoreDrawLastCachedBlobHash2 = 0;
      BridgeVgpuEdramRestoreDrawLastCachedBlobHash3 = 0;

      ID3DBlob *cachedBlob = nullptr;
      const auto blobResult =
          static_cast<ID3D12PipelineState *>(const_cast<void *>(pipelineState))
              ->GetCachedBlob(&cachedBlob);
      if (FAILED(blobResult) || cachedBlob == nullptr) {
        BridgeVgpuEdramRestoreDrawGuardFailure = static_cast<std::uint32_t>(
            FAILED(blobResult) ? blobResult : E_POINTER);
        EmitPatchEvent("edram_restore_draw_blob_failure",
                       BridgeVgpuEdramRestoreDrawGuardFailure, candidateCount);
      } else {
        const auto blobSize = cachedBlob->GetBufferSize();
        BridgeVgpuEdramRestoreDrawLastCachedBlobSize = blobSize;
        std::array<std::uint8_t, 32> digest{};
        const auto hashed = xeo3::vgpu::detail::HashBytesSha256(
            cachedBlob->GetBufferPointer(), blobSize, digest);
        cachedBlob->Release();

        if (!hashed) {
          BridgeVgpuEdramRestoreDrawGuardFailure = ERROR_INVALID_DATA;
          EmitPatchEvent("edram_restore_draw_hash_failure",
                         BridgeVgpuEdramRestoreDrawGuardFailure,
                         candidateCount);
        } else {
          std::array<std::uint64_t, 4> digestWords{};
          std::memcpy(digestWords.data(), digest.data(), digest.size());
          BridgeVgpuEdramRestoreDrawLastCachedBlobHash0 = digestWords[0];
          BridgeVgpuEdramRestoreDrawLastCachedBlobHash1 = digestWords[1];
          BridgeVgpuEdramRestoreDrawLastCachedBlobHash2 = digestWords[2];
          BridgeVgpuEdramRestoreDrawLastCachedBlobHash3 = digestWords[3];
          g_edramRestoreDrawPipelineMatches =
              xeo3::vgpu::detail::IsAc6CorruptEdramRestoreCachedBlob(blobSize,
                                                                     digest);
          if (!g_edramRestoreDrawPipelineMatches) {
            const auto mismatchCount =
                g_edramRestoreDrawHashMismatchCount.fetch_add(
                    1, std::memory_order_relaxed) +
                1;
            BridgeVgpuEdramRestoreDrawHashMismatchCount = mismatchCount;
            EmitPatchEvent("edram_restore_draw_hash_mismatch",
                           static_cast<std::uint32_t>(blobSize), mismatchCount);
          } else {
            EmitPatchEvent("edram_restore_draw_match",
                           static_cast<std::uint32_t>(blobSize),
                           candidateCount);
          }
        }
      }
    }
    matches = g_edramRestoreDrawPipelineMatches;
  }

  if (!matches) {
    return resolvedPipelineState;
  }

  const auto skipCount =
      g_edramRestoreDrawSkipCount.fetch_add(1, std::memory_order_relaxed) + 1;
  BridgeVgpuEdramRestoreDrawSkipCount = skipCount;
  if (skipCount <= 64 || (skipCount & (skipCount - 1)) == 0) {
    EmitPatchEvent("edram_restore_draw_skipped", 536, skipCount);
  }
  return nullptr;
}

extern "C" void
VgpuRecordNullPipelineState(const void *const record,
                            const void *const commandList,
                            const void *const commandContext,
                            const void *const cachedPipelineState) noexcept {
  const auto count =
      g_nullPipelineStateSkipCount.fetch_add(1, std::memory_order_relaxed) + 1;
  BridgeVgpuNullPipelineStateSkipCount = count;
  BridgeVgpuNullPipelineStateLastThreadId = GetCurrentThreadId();
  BridgeVgpuNullPipelineStateLastRecord =
      reinterpret_cast<std::uintptr_t>(record);
  BridgeVgpuNullPipelineStateLastCommandList =
      reinterpret_cast<std::uintptr_t>(commandList);
  BridgeVgpuNullPipelineStateLastCommandContext =
      reinterpret_cast<std::uintptr_t>(commandContext);
  BridgeVgpuNullPipelineStateLastCachedPso =
      reinterpret_cast<std::uintptr_t>(cachedPipelineState);

  std::uintptr_t rootSignature = 0;
  std::uint32_t recordKind = 0;
  std::uint32_t vertexCount = 0;
  std::uint32_t startVertex = 0;
  if (record != nullptr) {
    const auto *const bytes = static_cast<const std::uint8_t *>(record);
    std::memcpy(&rootSignature, bytes + 0x08, sizeof(rootSignature));
    std::memcpy(&recordKind, bytes + 0xC8, sizeof(recordKind));
    std::memcpy(&vertexCount, bytes + 0xCC, sizeof(vertexCount));
    std::memcpy(&startVertex, bytes + 0xD0, sizeof(startVertex));
  }
  BridgeVgpuNullPipelineStateLastRootSignature = rootSignature;
  BridgeVgpuNullPipelineStateLastRecordKind = recordKind;
  BridgeVgpuNullPipelineStateLastVertexCount = vertexCount;
  BridgeVgpuNullPipelineStateLastStartVertex = startVertex;
  EmitPatchEvent("null_pipeline_state_draw_skipped", recordKind, count);
}

extern "C" HRESULT STDMETHODCALLTYPE VgpuCreatePlacedResourceLegacyColdBridge(
    ID3D12Device *const device, ID3D12Heap *const heap, const UINT64 heapOffset,
    const D3D12_RESOURCE_DESC *const description,
    const D3D12_RESOURCE_STATES initialState,
    const D3D12_CLEAR_VALUE *const optimizedClearValue, const IID &interfaceId,
    void **const resource) noexcept {
  return VgpuCreatePlacedResourceImpl(
      kColdLegacyPlacedResourceCallRva, device, heap, heapOffset, description,
      initialState, optimizedClearValue, interfaceId, resource);
}

namespace xeo3::vgpu {
std::uint32_t
BuildExtendedFetchTable(void *cache, std::uint64_t *outputGpuAddress,
                        const void *source, const std::uint32_t entryCount,
                        void *allocatorContext, void *uploadInterface,
                        const ExtendedFetchTableCallbacks &callbacks) noexcept {
  if (cache == nullptr || outputGpuAddress == nullptr || source == nullptr ||
      allocatorContext == nullptr || uploadInterface == nullptr ||
      callbacks.allocate == nullptr || callbacks.upload == nullptr ||
      entryCount <= kNativeFetchTableCapacity ||
      entryCount > kXenosFetchTableCapacity) {
    return 0;
  }

  std::array<std::uint64_t, kXenosFetchTableCapacity> entries{};
  const auto *const sourceBytes = static_cast<const std::uint8_t *>(source);
  for (std::uint32_t index = 0; index < entryCount; ++index) {
    std::memcpy(&entries[index], sourceBytes + 0x80 + index * 16,
                sizeof(entries[index]));
  }

  auto *const contextBytes = static_cast<std::uint8_t *>(allocatorContext);
  FetchAllocation allocation{};
  callbacks.allocate(contextBytes + 0x300, &allocation,
                     static_cast<int>(entryCount));
  if (allocation.allocator == nullptr) {
    return 0;
  }

  const auto *const allocatorBytes =
      static_cast<const std::uint8_t *>(allocation.allocator);
  std::uint32_t stride = 0;
  std::uint64_t uploadBase = 0;
  std::uint64_t gpuBase = 0;
  std::memcpy(&stride, allocatorBytes + 0x10, sizeof(stride));
  std::memcpy(&uploadBase, allocatorBytes + 0x18, sizeof(uploadBase));
  std::memcpy(&gpuBase, allocatorBytes + 0x20, sizeof(gpuBase));

  const auto allocationOffset =
      static_cast<std::uint32_t>(allocation.index * stride);
  auto uploadAddress = uploadBase + allocationOffset;
  auto uploadCount = entryCount;
  callbacks.upload(uploadInterface, 1, &uploadAddress, &uploadCount, entryCount,
                   entries.data(), 0, 1);

  auto *const cacheBytes = static_cast<std::uint8_t *>(cache);
  std::memcpy(cacheBytes + 0x1E0, &entryCount, sizeof(entryCount));
  std::memcpy(cacheBytes + 0x1E8, &allocation, sizeof(allocation));
  std::uint64_t generation = 0;
  std::memcpy(&generation, contextBytes + 0x360, sizeof(generation));
  std::memcpy(cacheBytes + 0x1F8, &generation, sizeof(generation));

  *outputGpuAddress = gpuBase + allocationOffset;
  return 1;
}

namespace detail {
bool HashBytesSha256(const void *const bytes, const std::size_t byteCount,
                     std::array<std::uint8_t, 32> &digest) noexcept {
  digest.fill(0);
  if ((bytes == nullptr && byteCount != 0) || byteCount > ULONG_MAX) {
    return false;
  }

  BCRYPT_ALG_HANDLE algorithm = nullptr;
  auto status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM,
                                            nullptr, 0);
  if (!BCRYPT_SUCCESS(status) || algorithm == nullptr) {
    return false;
  }

  auto *const input = const_cast<PUCHAR>(static_cast<const UCHAR *>(bytes));
  status =
      BCryptHash(algorithm, nullptr, 0, input, static_cast<ULONG>(byteCount),
                 digest.data(), static_cast<ULONG>(digest.size()));
  BCryptCloseAlgorithmProvider(algorithm, 0);
  if (!BCRYPT_SUCCESS(status)) {
    digest.fill(0);
    return false;
  }
  return true;
}

bool ExtractGraphicsPipelineStreamSignature(
    const void *const stream, const std::size_t streamSize,
    GraphicsPipelineSignature &signature,
    std::size_t &pixelShaderBytecodeOffset) noexcept {
  signature = {};
  pixelShaderBytecodeOffset = (std::numeric_limits<std::size_t>::max)();
  if (stream == nullptr || streamSize < sizeof(void *) ||
      reinterpret_cast<std::uintptr_t>(stream) % alignof(void *) != 0) {
    return false;
  }

  D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};
  description.SampleMask = UINT_MAX;
  description.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
  description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  description.DSVFormat = DXGI_FORMAT_UNKNOWN;
  description.SampleDesc = {1, 0};
  description.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

  bool hasBlendState = false;
  bool hasRasterizerState = false;
  bool hasDepthStencilState = false;
  bool hasInputLayout = false;
  bool hasRenderTargetFormats = false;
  const auto *const bytes = static_cast<const std::uint8_t *>(stream);
  std::size_t offset = 0;

#define XEO3_READ_PIPELINE_STREAM_VALUE(typeValue, valueType, name)            \
  valueType const *name = nullptr;                                             \
  std::size_t nextOffset = 0;                                                  \
  if (!ReadPipelineStateStreamSubobject<typeValue, valueType>(                 \
          bytes, streamSize, offset, name, nextOffset)) {                      \
    return false;                                                              \
  }                                                                            \
  offset = nextOffset

  while (offset < streamSize) {
    if (sizeof(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE) > streamSize - offset) {
      return false;
    }
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type{};
    std::memcpy(&type, bytes + offset, sizeof(type));
    switch (type) {
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE: {
      XEO3_READ_PIPELINE_STREAM_VALUE(
          D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE,
          ID3D12RootSignature *, value);
      description.pRootSignature = *value;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS: {
      XEO3_READ_PIPELINE_STREAM_VALUE(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS,
                                      D3D12_SHADER_BYTECODE, value);
      description.VS = *value;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS: {
      if (pixelShaderBytecodeOffset !=
          (std::numeric_limits<std::size_t>::max)()) {
        return false;
      }
      XEO3_READ_PIPELINE_STREAM_VALUE(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS,
                                      D3D12_SHADER_BYTECODE, value);
      description.PS = *value;
      pixelShaderBytecodeOffset = static_cast<std::size_t>(
          reinterpret_cast<const std::uint8_t *>(value) - bytes);
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS: {
      XEO3_READ_PIPELINE_STREAM_VALUE(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS,
                                      D3D12_SHADER_BYTECODE, value);
      description.DS = *value;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS: {
      XEO3_READ_PIPELINE_STREAM_VALUE(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS,
                                      D3D12_SHADER_BYTECODE, value);
      description.HS = *value;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS: {
      XEO3_READ_PIPELINE_STREAM_VALUE(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS,
                                      D3D12_SHADER_BYTECODE, value);
      description.GS = *value;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS: {
      XEO3_READ_PIPELINE_STREAM_VALUE(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS,
                                      D3D12_SHADER_BYTECODE, value);
      (void)value;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT: {
      XEO3_READ_PIPELINE_STREAM_VALUE(
          D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT,
          D3D12_STREAM_OUTPUT_DESC, value);
      description.StreamOutput = *value;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND: {
      XEO3_READ_PIPELINE_STREAM_VALUE(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND,
                                      D3D12_BLEND_DESC, value);
      description.BlendState = *value;
      hasBlendState = true;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK: {
      XEO3_READ_PIPELINE_STREAM_VALUE(
          D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK, UINT, value);
      description.SampleMask = *value;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER: {
      XEO3_READ_PIPELINE_STREAM_VALUE(
          D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER, D3D12_RASTERIZER_DESC,
          value);
      description.RasterizerState = *value;
      hasRasterizerState = true;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL: {
      XEO3_READ_PIPELINE_STREAM_VALUE(
          D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL,
          D3D12_DEPTH_STENCIL_DESC, value);
      description.DepthStencilState = *value;
      hasDepthStencilState = true;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT: {
      XEO3_READ_PIPELINE_STREAM_VALUE(
          D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT,
          D3D12_INPUT_LAYOUT_DESC, value);
      description.InputLayout = *value;
      hasInputLayout = true;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE: {
      XEO3_READ_PIPELINE_STREAM_VALUE(
          D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE,
          D3D12_INDEX_BUFFER_STRIP_CUT_VALUE, value);
      description.IBStripCutValue = *value;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY: {
      XEO3_READ_PIPELINE_STREAM_VALUE(
          D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY,
          D3D12_PRIMITIVE_TOPOLOGY_TYPE, value);
      description.PrimitiveTopologyType = *value;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS: {
      XEO3_READ_PIPELINE_STREAM_VALUE(
          D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS,
          D3D12_RT_FORMAT_ARRAY, value);
      if (value->NumRenderTargets > std::size(description.RTVFormats)) {
        return false;
      }
      description.NumRenderTargets = value->NumRenderTargets;
      std::copy_n(value->RTFormats, value->NumRenderTargets,
                  description.RTVFormats);
      hasRenderTargetFormats = true;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT: {
      XEO3_READ_PIPELINE_STREAM_VALUE(
          D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT, DXGI_FORMAT,
          value);
      description.DSVFormat = *value;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC: {
      XEO3_READ_PIPELINE_STREAM_VALUE(
          D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC, DXGI_SAMPLE_DESC,
          value);
      description.SampleDesc = *value;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK: {
      XEO3_READ_PIPELINE_STREAM_VALUE(
          D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK, UINT, value);
      description.NodeMask = *value;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO: {
      XEO3_READ_PIPELINE_STREAM_VALUE(
          D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO,
          D3D12_CACHED_PIPELINE_STATE, value);
      description.CachedPSO = *value;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS: {
      XEO3_READ_PIPELINE_STREAM_VALUE(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS,
                                      D3D12_PIPELINE_STATE_FLAGS, value);
      description.Flags = *value;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1: {
      XEO3_READ_PIPELINE_STREAM_VALUE(
          D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1,
          D3D12_DEPTH_STENCIL_DESC1, value);
      description.DepthStencilState.DepthEnable = value->DepthEnable;
      description.DepthStencilState.DepthWriteMask = value->DepthWriteMask;
      description.DepthStencilState.DepthFunc = value->DepthFunc;
      description.DepthStencilState.StencilEnable = value->StencilEnable;
      description.DepthStencilState.StencilReadMask = value->StencilReadMask;
      description.DepthStencilState.StencilWriteMask = value->StencilWriteMask;
      description.DepthStencilState.FrontFace = value->FrontFace;
      description.DepthStencilState.BackFace = value->BackFace;
      hasDepthStencilState = !value->DepthBoundsTestEnable;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING: {
      XEO3_READ_PIPELINE_STREAM_VALUE(
          D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING,
          D3D12_VIEW_INSTANCING_DESC, value);
      (void)value;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS: {
      XEO3_READ_PIPELINE_STREAM_VALUE(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS,
                                      D3D12_SHADER_BYTECODE, value);
      (void)value;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS: {
      XEO3_READ_PIPELINE_STREAM_VALUE(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS,
                                      D3D12_SHADER_BYTECODE, value);
      (void)value;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL2: {
      XEO3_READ_PIPELINE_STREAM_VALUE(
          D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL2,
          D3D12_DEPTH_STENCIL_DESC2, value);
      description.DepthStencilState.DepthEnable = value->DepthEnable;
      description.DepthStencilState.DepthWriteMask = value->DepthWriteMask;
      description.DepthStencilState.DepthFunc = value->DepthFunc;
      description.DepthStencilState.StencilEnable = value->StencilEnable;
      hasDepthStencilState = !value->DepthBoundsTestEnable;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER1: {
      XEO3_READ_PIPELINE_STREAM_VALUE(
          D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER1,
          D3D12_RASTERIZER_DESC1, value);
      description.RasterizerState.FillMode = value->FillMode;
      description.RasterizerState.CullMode = value->CullMode;
      description.RasterizerState.FrontCounterClockwise =
          value->FrontCounterClockwise;
      description.RasterizerState.DepthBias = 0;
      description.RasterizerState.DepthBiasClamp = value->DepthBiasClamp;
      description.RasterizerState.SlopeScaledDepthBias =
          value->SlopeScaledDepthBias;
      description.RasterizerState.DepthClipEnable = value->DepthClipEnable;
      description.RasterizerState.MultisampleEnable = value->MultisampleEnable;
      description.RasterizerState.AntialiasedLineEnable =
          value->AntialiasedLineEnable;
      description.RasterizerState.ForcedSampleCount = value->ForcedSampleCount;
      description.RasterizerState.ConservativeRaster =
          value->ConservativeRaster;
      hasRasterizerState = value->DepthBias == 0.0F;
      break;
    }
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER2: {
      XEO3_READ_PIPELINE_STREAM_VALUE(
          D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER2,
          D3D12_RASTERIZER_DESC2, value);
      description.RasterizerState.FillMode = value->FillMode;
      description.RasterizerState.CullMode = value->CullMode;
      description.RasterizerState.FrontCounterClockwise =
          value->FrontCounterClockwise;
      description.RasterizerState.DepthBias = 0;
      description.RasterizerState.DepthBiasClamp = value->DepthBiasClamp;
      description.RasterizerState.SlopeScaledDepthBias =
          value->SlopeScaledDepthBias;
      description.RasterizerState.DepthClipEnable = value->DepthClipEnable;
      description.RasterizerState.ForcedSampleCount = value->ForcedSampleCount;
      description.RasterizerState.ConservativeRaster =
          value->ConservativeRaster;
      hasRasterizerState =
          value->DepthBias == 0.0F &&
          value->LineRasterizationMode == D3D12_LINE_RASTERIZATION_MODE_ALIASED;
      break;
    }
    default:
      return false;
    }
  }

#undef XEO3_READ_PIPELINE_STREAM_VALUE

  if (offset != streamSize) {
    return false;
  }
  signature = BuildGraphicsPipelineSignature(description);
  if (!hasBlendState || !hasRasterizerState || !hasDepthStencilState ||
      !hasInputLayout || !hasRenderTargetFormats) {
    signature.hasExpectedFixedState = false;
    signature.hasExpectedEdramScaleFixedState = false;
    signature.hasExpectedEdramLoadFixedState = false;
  }
  return true;
}

bool IsAc6CorruptEdramRestorePipeline(
    const GraphicsPipelineSignature &signature) noexcept {
  const auto hasShaderDigest = [](const std::array<std::uint8_t, 32> &digest) {
    return std::any_of(digest.begin(), digest.end(),
                       [](const std::uint8_t byte) { return byte != 0; });
  };

  // The PIX replay gives us the stable AC6 pipeline fingerprint, but the
  // native XeO3 shader compiler may produce a different valid bytecode blob
  // for the same Xenos shader. Keep the descriptor match strict and use the
  // digest only as a successful-capture guard, rather than pinning a replay's
  // host/compiler-specific SHA-256.
  return signature.vertexShaderSize == 2224 &&
         signature.pixelShaderSize == 2440 &&
         signature.sampleMask == UINT_MAX &&
         signature.primitiveTopologyType ==
             static_cast<std::uint32_t>(
                 D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE) &&
         signature.sampleCount == 2 && signature.sampleQuality == 0 &&
         signature.renderTargetCount == 1 &&
         signature.renderTarget0Format ==
             static_cast<std::uint32_t>(DXGI_FORMAT_R8G8B8A8_UINT) &&
         signature.depthStencilFormat ==
             static_cast<std::uint32_t>(DXGI_FORMAT_UNKNOWN) &&
         signature.inputElementCount == 2 &&
         signature.renderTarget0WriteMask == D3D12_COLOR_WRITE_ENABLE_ALL &&
         signature.hasExpectedInputLayout && signature.hasExpectedFixedState &&
         hasShaderDigest(signature.vertexShaderSha256) &&
         hasShaderDigest(signature.pixelShaderSha256);
}

bool IsAc6Pso535CullPipeline(
    const GraphicsPipelineSignature &signature) noexcept {
  return IsAc6Pso535CullPipelineDescriptor(signature) &&
         signature.vertexShaderSha256 == kAc6Pso535VertexShaderSha256 &&
         signature.pixelShaderSha256 == kAc6Pso535PixelShaderSha256;
}

bool IsAc6Pso535CullPipelineDescriptor(
    const GraphicsPipelineSignature &signature) noexcept {
  return signature.vertexShaderSize == 7840 &&
         signature.pixelShaderSize == 2780 && signature.sampleMask == 15 &&
         signature.primitiveTopologyType ==
             static_cast<std::uint32_t>(
                 D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE) &&
         signature.sampleCount == 2 && signature.sampleQuality == 0 &&
         signature.renderTargetCount == 1 &&
         signature.renderTarget0Format ==
             static_cast<std::uint32_t>(DXGI_FORMAT_R8G8B8A8_UNORM) &&
         signature.depthStencilFormat ==
             static_cast<std::uint32_t>(DXGI_FORMAT_D32_FLOAT_S8X24_UINT) &&
         signature.inputElementCount == 0 &&
         signature.renderTarget0WriteMask == D3D12_COLOR_WRITE_ENABLE_ALL &&
         signature.fillMode ==
             static_cast<std::uint32_t>(D3D12_FILL_MODE_SOLID) &&
         signature.cullMode ==
             static_cast<std::uint32_t>(D3D12_CULL_MODE_BACK) &&
         signature.depthBias == 0 &&
         signature.depthWriteMask ==
             static_cast<std::uint32_t>(D3D12_DEPTH_WRITE_MASK_ZERO) &&
         signature.depthFunc ==
             static_cast<std::uint32_t>(D3D12_COMPARISON_FUNC_GREATER_EQUAL) &&
         signature.frontCounterClockwise && signature.depthClipEnable &&
         signature.multisampleEnable && !signature.antialiasedLineEnable &&
         signature.depthEnable && !signature.stencilEnable &&
         !signature.renderTarget0BlendEnable;
}

bool IsAc6EdramScalePipeline(
    const GraphicsPipelineSignature &signature) noexcept {
  constexpr std::array<std::uint8_t, 32> kExpectedVertexShaderSha256{
      0x7F, 0x3F, 0x8E, 0x0E, 0xEC, 0x40, 0x28, 0xEC, 0xCF, 0xBF, 0x2E,
      0x63, 0x62, 0x1B, 0x28, 0x04, 0x95, 0x28, 0xEC, 0xFA, 0xE2, 0xEC,
      0xEC, 0x50, 0x45, 0xB5, 0xD6, 0x5E, 0x72, 0xCB, 0xA8, 0x1E,
  };
  constexpr std::array<std::uint8_t, 32> kExpectedPixelShaderSha256{
      0x1E, 0x88, 0x74, 0xA8, 0xEE, 0x00, 0x5E, 0x72, 0x4F, 0xB5, 0xC4,
      0x55, 0xE2, 0xF0, 0x35, 0x07, 0xB4, 0x16, 0x59, 0xE6, 0x12, 0x46,
      0x78, 0xE9, 0x41, 0x2C, 0x87, 0x30, 0x5B, 0x49, 0x62, 0x51,
  };

  return IsAc6EdramScalePipelineDescriptor(signature) &&
         signature.vertexShaderSha256 == kExpectedVertexShaderSha256 &&
         signature.pixelShaderSha256 == kExpectedPixelShaderSha256;
}

bool IsAc6EdramScalePipelineDescriptor(
    const GraphicsPipelineSignature &signature) noexcept {
  return signature.vertexShaderSize == 2224 &&
         signature.pixelShaderSize == 2528 &&
         signature.sampleMask == UINT_MAX &&
         signature.primitiveTopologyType ==
             static_cast<std::uint32_t>(
                 D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE) &&
         signature.sampleCount == 4 && signature.sampleQuality == 0 &&
         signature.renderTargetCount == 1 &&
         signature.renderTarget0Format ==
             static_cast<std::uint32_t>(DXGI_FORMAT_R8G8B8A8_UINT) &&
         signature.depthStencilFormat ==
             static_cast<std::uint32_t>(DXGI_FORMAT_UNKNOWN) &&
         signature.inputElementCount == 2 &&
         signature.renderTarget0WriteMask == D3D12_COLOR_WRITE_ENABLE_ALL &&
         signature.hasExpectedInputLayout &&
         signature.hasExpectedEdramScaleFixedState;
}

const void *GetAc6EdramScaleFixPixelShader(std::size_t &byteCount) noexcept {
  byteCount = generated::kAc6EdramScaleFixPixelShaderSize;
  return generated::kAc6EdramScaleFixPixelShader;
}

bool IsAc6EdramLoadPipeline(
    const GraphicsPipelineSignature &signature) noexcept {
  constexpr std::array<std::uint8_t, 32> kExpectedVertexShaderSha256{
      0x7F, 0x3F, 0x8E, 0x0E, 0xEC, 0x40, 0x28, 0xEC, 0xCF, 0xBF, 0x2E,
      0x63, 0x62, 0x1B, 0x28, 0x04, 0x95, 0x28, 0xEC, 0xFA, 0xE2, 0xEC,
      0xEC, 0x50, 0x45, 0xB5, 0xD6, 0x5E, 0x72, 0xCB, 0xA8, 0x1E,
  };
  constexpr std::array<std::uint8_t, 32> kExpectedPixelShaderSha256{
      0x3A, 0x20, 0x6A, 0x6D, 0xC3, 0xF9, 0xAE, 0xEE, 0x03, 0x8F, 0xEE,
      0xFD, 0x23, 0x57, 0x67, 0xCD, 0x40, 0xF9, 0x92, 0xE1, 0x7B, 0xE9,
      0x4B, 0xA2, 0x0B, 0xA8, 0x8F, 0x90, 0x4A, 0x2F, 0x23, 0x81,
  };

  return IsAc6EdramLoadPipelineDescriptor(signature) &&
         signature.vertexShaderSha256 == kExpectedVertexShaderSha256 &&
         signature.pixelShaderSha256 == kExpectedPixelShaderSha256;
}

bool IsAc6EdramLoadPipelineDescriptor(
    const GraphicsPipelineSignature &signature) noexcept {
  return signature.vertexShaderSize == 2224 &&
         signature.pixelShaderSize == 2440 &&
         signature.sampleMask == UINT_MAX &&
         signature.primitiveTopologyType ==
             static_cast<std::uint32_t>(
                 D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE) &&
         signature.sampleCount == 1 && signature.sampleQuality == 0 &&
         signature.renderTargetCount == 1 &&
         signature.renderTarget0Format ==
             static_cast<std::uint32_t>(DXGI_FORMAT_R8G8B8A8_UINT) &&
         signature.depthStencilFormat ==
             static_cast<std::uint32_t>(DXGI_FORMAT_UNKNOWN) &&
         signature.inputElementCount == 2 &&
         signature.renderTarget0WriteMask == D3D12_COLOR_WRITE_ENABLE_ALL &&
         signature.hasExpectedInputLayout &&
         signature.hasExpectedEdramLoadFixedState;
}

const void *GetAc6EdramLoadFixPixelShader(std::size_t &byteCount) noexcept {
  byteCount = generated::kAc6EdramLoadFixPixelShaderSize;
  return generated::kAc6EdramLoadFixPixelShader;
}

const void *GetAc6EdramTransferVertexShader(std::size_t &byteCount) noexcept {
  byteCount = generated::kAc6EdramTransferVertexShaderSize;
  return generated::kAc6EdramTransferVertexShader;
}

const void *GetAc6Pso341WidthFixComputeShader(std::size_t &byteCount) noexcept {
  byteCount = generated::kAc6Pso341WidthFixComputeShaderSize;
  return generated::kAc6Pso341WidthFixComputeShader;
}

bool PatchAc6HalfWidthFullscreenScissor(void *const record,
                                        const std::size_t recordSize) noexcept {
  DrawRecordSignature signature{};
  if (!ExtractDrawRecordSignature(record, recordSize, signature) ||
      signature.rootSignature == 0 ||
      signature.viewportWidthBits != 0x44A00000U ||
      signature.viewportHeightBits != 0x44340000U ||
      signature.viewportMinDepthBits != 0 ||
      signature.viewportMaxDepthBits != 0x3F800000U ||
      signature.viewportTopLeftXBits != 0 ||
      signature.viewportTopLeftYBits != 0 || signature.scissorRight != 640 ||
      signature.scissorBottom != 720 || signature.recordKind != 0 ||
      signature.vertexCount == 0 || signature.vertexCount > 6 ||
      signature.startVertex != 0) {
    return false;
  }

  constexpr std::uint32_t kFullWidthScissor = 1280;
  std::memcpy(static_cast<std::uint8_t *>(record) + 0x30, &kFullWidthScissor,
              sizeof(kFullWidthScissor));
  return true;
}

bool IsAc6HalfWidthMsaaViewport(
    const DrawRecordSignature &drawSignature,
    const GraphicsPipelineSignature &pipelineSignature,
    const void *const pipelineState) noexcept {
  if (pipelineState == nullptr) {
    return false;
  }

  const auto activePipelineState =
      reinterpret_cast<std::uintptr_t>(pipelineState);
  return drawSignature.rootSignature != 0 &&
         (drawSignature.pipelineState == 0 ||
          drawSignature.pipelineState == activePipelineState) &&
         drawSignature.viewportWidthBits == 0x44A00000U &&
         drawSignature.viewportHeightBits == 0x44340000U &&
         drawSignature.viewportMinDepthBits == 0 &&
         drawSignature.viewportMaxDepthBits == 0x3F800000U &&
         drawSignature.viewportTopLeftXBits == 0 &&
         drawSignature.viewportTopLeftYBits == 0 &&
         drawSignature.scissorRight == 640 &&
         drawSignature.scissorBottom == 720 &&
         drawSignature.recordKind == 0 && drawSignature.vertexCount != 0 &&
         pipelineSignature.sampleCount == 2 &&
         pipelineSignature.sampleQuality == 0 &&
         pipelineSignature.renderTargetCount == 1 &&
         pipelineSignature.renderTarget0Format ==
             static_cast<std::uint32_t>(DXGI_FORMAT_R8G8B8A8_UNORM) &&
         pipelineSignature.depthStencilFormat ==
             static_cast<std::uint32_t>(DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
}

bool PatchAc6HalfWidthMsaaViewport(
    void *const record, const std::size_t recordSize,
    const GraphicsPipelineSignature &pipelineSignature,
    const void *const pipelineState, std::uint32_t &originalWidthBits,
    std::uint32_t &replacementWidthBits) noexcept {
  originalWidthBits = 0;
  replacementWidthBits = 0;

  DrawRecordSignature drawSignature{};
  if (!ExtractDrawRecordSignature(record, recordSize, drawSignature) ||
      !IsAc6HalfWidthMsaaViewport(drawSignature, pipelineSignature,
                                  pipelineState)) {
    return false;
  }

  constexpr std::uint32_t kHalfWidthViewportBits = 0x44200000U;
  originalWidthBits = drawSignature.viewportWidthBits;
  replacementWidthBits = kHalfWidthViewportBits;
  std::memcpy(static_cast<std::uint8_t *>(record) + 0x18,
              &kHalfWidthViewportBits, sizeof(kHalfWidthViewportBits));
  return true;
}

bool ExtractDrawRecordSignature(const void *const record,
                                const std::size_t recordSize,
                                DrawRecordSignature &signature) noexcept {
  signature = {};
  if (record == nullptr || recordSize < kDrawRecordMinimumSize) {
    return false;
  }

  const auto *const bytes = static_cast<const std::uint8_t *>(record);
  const auto load32 = [bytes](const std::size_t offset) {
    std::uint32_t value = 0;
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
  };
  const auto loadPointer = [bytes](const std::size_t offset) {
    std::uintptr_t value = 0;
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
  };

  signature.rootSignature = loadPointer(0x08);
  signature.pipelineState = loadPointer(0x10);
  signature.viewportWidthBits = load32(0x18);
  signature.viewportHeightBits = load32(0x1C);
  signature.viewportMinDepthBits = load32(0x20);
  signature.viewportMaxDepthBits = load32(0x24);
  signature.viewportTopLeftXBits = load32(0x28);
  signature.viewportTopLeftYBits = load32(0x2C);
  signature.scissorRight = load32(0x30);
  signature.scissorBottom = load32(0x34);
  signature.recordKind = load32(0xC8);
  signature.vertexCount = load32(0xCC);
  signature.startVertex = load32(0xD0);
  return true;
}

Ac6EdramDrawPipeline
ClassifyAc6EdramDrawPipeline(const DrawRecordSignature &signature,
                             const void *const activePipelineState) noexcept {
  if (activePipelineState == nullptr || signature.rootSignature == 0 ||
      (signature.pipelineState != 0 &&
       signature.pipelineState !=
           reinterpret_cast<std::uintptr_t>(activePipelineState)) ||
      signature.viewportMinDepthBits != 0 ||
      signature.viewportMaxDepthBits != 0x3F800000U ||
      signature.viewportTopLeftXBits != 0 ||
      signature.viewportTopLeftYBits != 0 || signature.recordKind != 0 ||
      signature.vertexCount != 3 || signature.startVertex != 0) {
    return Ac6EdramDrawPipeline::None;
  }

  struct DrawShape {
    std::uint32_t viewportWidthBits;
    std::uint32_t viewportHeightBits;
    std::uint32_t scissorRight;
    std::uint32_t scissorBottom;
  };
  const DrawShape observed{signature.viewportWidthBits,
                           signature.viewportHeightBits, signature.scissorRight,
                           signature.scissorBottom};
  const auto equals = [&observed](const DrawShape &expected) {
    return observed.viewportWidthBits == expected.viewportWidthBits &&
           observed.viewportHeightBits == expected.viewportHeightBits &&
           observed.scissorRight == expected.scissorRight &&
           observed.scissorBottom == expected.scissorBottom;
  };

  constexpr std::array<DrawShape, 2> kScaleShapes{{
      {0x44200000U, 0x43B40000U, 1280, 720},
      {0x43700000U, 0x43100000U, 8192, 8192},
  }};
  if (std::any_of(kScaleShapes.begin(), kScaleShapes.end(), equals)) {
    return Ac6EdramDrawPipeline::Scale;
  }

  constexpr std::array<DrawShape, 11> kLoadShapes{{
      {0x44A00000U, 0x44340000U, 0, 0},
      {0x44A00000U, 0x44340000U, 160, 96},
      {0x44A00000U, 0x44340000U, 640, 360},
      {0x44A00000U, 0x44340000U, 8192, 8192},
      {0x43200000U, 0x42C00000U, 160, 90},
      {0x43200000U, 0x42C00000U, 320, 184},
      {0x43A00000U, 0x43400000U, 208, 144},
      {0x43A00000U, 0x43400000U, 320, 180},
      {0x43A00000U, 0x43400000U, 8, 8},
      {0x43A00000U, 0x43B80000U, 8192, 8192},
      {0x44200000U, 0x43B80000U, 8192, 8192},
  }};
  if (std::any_of(kLoadShapes.begin(), kLoadShapes.end(), equals)) {
    return Ac6EdramDrawPipeline::Load;
  }
  return Ac6EdramDrawPipeline::None;
}

bool IsPixOpaquePipelineBlob(
    const std::size_t blobSize,
    const std::array<std::uint8_t, 32> &digest) noexcept {
  constexpr std::array<std::uint8_t, 32> kPixOpaqueDigest{
      0x13, 0xC9, 0xE5, 0xC8, 0x2B, 0xA9, 0x3E, 0xD0,
      0xC1, 0xBE, 0x12, 0xD4, 0x13, 0x7E, 0xCD, 0x15,
      0xF8, 0xA1, 0x45, 0xE4, 0x4E, 0x02, 0xDC, 0xE5,
      0x20, 0x5D, 0xF2, 0x1A, 0x13, 0xB4, 0xE7, 0xB2};
  return blobSize == 848 && digest == kPixOpaqueDigest;
}

Ac6EdramDrawPipeline ClassifyAc6BoundEdramDraw(
    const DrawRecordSignature &signature, const void *const pipelineState,
    const Ac6EdramBoundEvidence &evidence) noexcept {
  const auto shape = ClassifyAc6EdramDrawPipeline(signature, pipelineState);
  if (shape == Ac6EdramDrawPipeline::None ||
      evidence.renderTargetCount != 1 || evidence.hasDepthStencil ||
      evidence.viewFormat != DXGI_FORMAT_R8G8B8A8_UINT ||
      !evidence.hasConstants || (evidence.rootTableMask & 3) != 3 ||
      evidence.targetWidth < evidence.constants[6] ||
      evidence.targetHeight < evidence.constants[7]) {
    return Ac6EdramDrawPipeline::None;
  }
  const auto kind = ClassifyAc6EdramTransferConstants(evidence.constants);
  if (shape == Ac6EdramDrawPipeline::Load &&
      kind == Ac6EdramConstantKind::Load && evidence.sampleCount == 1 &&
      signature.viewportWidthBits == 0x44A00000U &&
      signature.viewportHeightBits == 0x44340000U)
    return Ac6EdramDrawPipeline::Load;
  if (shape == Ac6EdramDrawPipeline::Scale &&
      kind == Ac6EdramConstantKind::Scale && evidence.sampleCount == 4 &&
      signature.viewportWidthBits == 0x44200000U &&
      signature.viewportHeightBits == 0x43B40000U)
    return Ac6EdramDrawPipeline::Scale;
  return Ac6EdramDrawPipeline::None;
}

Ac6EdramDrawPipeline ClassifyAc6EdramCachedPipelineBlob(
    const std::size_t blobSize,
    const std::array<std::uint8_t, 32> &digest) noexcept {
  constexpr std::size_t kExpectedCachedBlobSize = 954;
  constexpr std::array<std::uint8_t, 32> kLoadCachedBlobSha256{
      0x5E, 0xE6, 0xE2, 0xC4, 0x21, 0xA6, 0xE2, 0xF1, 0x31, 0x17, 0x1E,
      0x1B, 0x2E, 0x1C, 0x2B, 0x5E, 0x6F, 0x77, 0xF2, 0xA1, 0xE3, 0x6B,
      0x25, 0xDD, 0x99, 0x8A, 0x59, 0x12, 0xAF, 0x17, 0x97, 0x11,
  };
  constexpr std::array<std::uint8_t, 32> kScaleCachedBlobSha256{
      0xBC, 0xFC, 0x09, 0x66, 0x3F, 0x3E, 0x30, 0x80, 0x3D, 0x7A, 0xF5,
      0x09, 0xA0, 0xFD, 0x9F, 0xAD, 0xFD, 0x3E, 0x31, 0x70, 0x23, 0xB5,
      0x46, 0xFC, 0x9C, 0x08, 0xA8, 0xEE, 0xAD, 0xDA, 0x71, 0xBE,
  };
  // AMD UMD 32.0.31041.1004 recompiles the same two pinned AC6 transfer
  // descriptors to different cached blobs. The full digests keep this narrow;
  // the surrounding VGPUDX12 image hash gate still pins the host
  // implementation.
  constexpr std::array<std::uint8_t, 32> kCurrentLoadCachedBlobSha256{
      0xC7, 0xCA, 0xC6, 0xC6, 0xB4, 0x75, 0x39, 0x00, 0x00, 0x44, 0x8A,
      0xBE, 0x06, 0x83, 0x15, 0x39, 0x66, 0x87, 0x3E, 0xA7, 0x3F, 0x3F,
      0xC0, 0xCC, 0x49, 0x8B, 0x54, 0x90, 0x97, 0x35, 0xF6, 0x74,
  };
  constexpr std::array<std::uint8_t, 32> kCurrentScaleCachedBlobSha256{
      0x16, 0xE9, 0x02, 0xC9, 0xF0, 0xD5, 0x32, 0xB3, 0x0D, 0x1C, 0xC0,
      0x7E, 0xE2, 0xB7, 0xCD, 0xE5, 0x5F, 0x4F, 0x00, 0x5A, 0xDE, 0x82,
      0x9A, 0x5A, 0x83, 0x6D, 0xB4, 0xFF, 0xF9, 0x98, 0x1D, 0x87,
  };
  if (blobSize != kExpectedCachedBlobSize) {
    return Ac6EdramDrawPipeline::None;
  }
  if (digest == kLoadCachedBlobSha256 ||
      digest == kCurrentLoadCachedBlobSha256) {
    return Ac6EdramDrawPipeline::Load;
  }
  if (digest == kScaleCachedBlobSha256 ||
      digest == kCurrentScaleCachedBlobSha256) {
    return Ac6EdramDrawPipeline::Scale;
  }
  return Ac6EdramDrawPipeline::None;
}

bool IsAc6CorruptEdramRestoreDrawSignature(
    const DrawRecordSignature &signature,
    const void *const pipelineState) noexcept {
  if (pipelineState == nullptr) {
    return false;
  }

  const auto activePipelineState =
      reinterpret_cast<std::uintptr_t>(pipelineState);
  return signature.rootSignature != 0 &&
         (signature.pipelineState == 0 ||
          signature.pipelineState == activePipelineState) &&
         signature.viewportWidthBits == 0x44A00000U &&
         signature.viewportHeightBits == 0x44340000U &&
         signature.viewportMinDepthBits == 0 &&
         signature.viewportMaxDepthBits == 0x3F800000U &&
         signature.viewportTopLeftXBits == 0 &&
         signature.viewportTopLeftYBits == 0 && signature.scissorRight == 640 &&
         signature.scissorBottom == 360 && signature.recordKind == 0 &&
         signature.vertexCount == 3 && signature.startVertex == 0;
}

bool ShouldSuppressAc6HostEdramRestoreDraw(
    const DrawRecordSignature &signature, const void *const pipelineState,
    const Ac6EdramDrawPipeline fingerprintedPipeline,
    const bool enabled) noexcept {
  return enabled && fingerprintedPipeline == Ac6EdramDrawPipeline::Load &&
         IsAc6CorruptEdramRestoreDrawSignature(signature, pipelineState);
}

bool IsAc6CorruptEdramRestoreDrawRecord(
    const void *const record, const std::size_t recordSize,
    const void *const pipelineState) noexcept {
  if (pipelineState == nullptr) {
    return false;
  }

  DrawRecordSignature signature{};
  if (!ExtractDrawRecordSignature(record, recordSize, signature)) {
    return false;
  }

  return IsAc6CorruptEdramRestoreDrawSignature(signature, pipelineState);
}

bool IsAc6CorruptEdramRestoreCachedBlob(
    const std::size_t blobSize,
    const std::array<std::uint8_t, 32> &digest) noexcept {
  constexpr std::array<std::uint8_t, 32> kExpectedDigest{
      0x52, 0x73, 0x70, 0x28, 0xBA, 0xFA, 0x14, 0x4C, 0x68, 0x48, 0x4A,
      0x49, 0x5F, 0x75, 0x72, 0xF1, 0x17, 0x9E, 0xA7, 0xB9, 0xF1, 0x18,
      0x8F, 0xB9, 0x9A, 0xD6, 0xA9, 0x90, 0x61, 0xDD, 0x28, 0xC4,
  };
  return blobSize == 954 && digest == kExpectedDigest;
}

std::array<std::uint8_t, kFetchTableDetourSize>
EncodeAbsoluteJump(const void *target) noexcept {
  std::array<std::uint8_t, kFetchTableDetourSize> jump{};
  jump.fill(0x90);
  jump[0] = 0x48;
  jump[1] = 0xB8;
  const auto address = reinterpret_cast<std::uintptr_t>(target);
  std::memcpy(jump.data() + 2, &address, sizeof(address));
  jump[10] = 0xFF;
  jump[11] = 0xE0;
  return jump;
}

std::array<std::uint8_t, kShaderCompileDetourSize>
EncodeShaderCompileJump(const void *target) noexcept {
  std::array<std::uint8_t, kShaderCompileDetourSize> jump{};
  jump.fill(0x90);
  jump[0] = 0x48;
  jump[1] = 0xB8;
  const auto address = reinterpret_cast<std::uintptr_t>(target);
  std::memcpy(jump.data() + 2, &address, sizeof(address));
  jump[10] = 0xFF;
  jump[11] = 0xE0;
  return jump;
}

std::array<std::uint8_t, kXenosTranslateDetourSize>
EncodeXenosTranslateJump(const void *target) noexcept {
  std::array<std::uint8_t, kXenosTranslateDetourSize> jump{};
  jump.fill(0x90);
  jump[0] = 0x48;
  jump[1] = 0xB8;
  const auto address = reinterpret_cast<std::uintptr_t>(target);
  std::memcpy(jump.data() + 2, &address, sizeof(address));
  jump[10] = 0xFF;
  jump[11] = 0xE0;
  return jump;
}

std::array<std::uint8_t, kTextureTransferDetourSize>
EncodeTextureTransferJump(const void *target) noexcept {
  std::array<std::uint8_t, kTextureTransferDetourSize> jump{};
  jump.fill(0x90);
  jump[0] = 0x48;
  jump[1] = 0xB8;
  const auto address = reinterpret_cast<std::uintptr_t>(target);
  std::memcpy(jump.data() + 2, &address, sizeof(address));
  jump[10] = 0xFF;
  jump[11] = 0xE0;
  return jump;
}

std::array<std::uint8_t, kStructuredTextureTransferDetourSize>
EncodeStructuredTextureTransferJump(const void *target) noexcept {
  std::array<std::uint8_t, kStructuredTextureTransferDetourSize> jump{};
  jump.fill(0x90);
  jump[0] = 0x48;
  jump[1] = 0xB8;
  const auto address = reinterpret_cast<std::uintptr_t>(target);
  std::memcpy(jump.data() + 2, &address, sizeof(address));
  jump[10] = 0xFF;
  jump[11] = 0xE0;
  return jump;
}

std::array<std::uint8_t, kConstantUploadDetourSize>
EncodeConstantUploadJump(const void *target) noexcept {
  std::array<std::uint8_t, kConstantUploadDetourSize> jump{};
  jump.fill(0x90);
  jump[0] = 0x48;
  jump[1] = 0xB8;
  const auto address = reinterpret_cast<std::uintptr_t>(target);
  std::memcpy(jump.data() + 2, &address, sizeof(address));
  jump[10] = 0xFF;
  jump[11] = 0xE0;
  return jump;
}

std::array<std::uint8_t, kNullPipelineStateDetourSize>
EncodeNullPipelineStateJump(const void *target) noexcept {
  std::array<std::uint8_t, kNullPipelineStateDetourSize> jump{};
  jump.fill(0x90);
  jump[0] = 0x48;
  jump[1] = 0xB8;
  const auto address = reinterpret_cast<std::uintptr_t>(target);
  std::memcpy(jump.data() + 2, &address, sizeof(address));
  jump[10] = 0xFF;
  jump[11] = 0xE0;
  return jump;
}

std::array<std::uint8_t, kCallRelaySize>
EncodeAbsoluteCallRelay(const void *target) noexcept {
  std::array<std::uint8_t, kCallRelaySize> relay{};
  relay.fill(0x90);
  relay[0] = 0x48;
  relay[1] = 0xB8;
  const auto address = reinterpret_cast<std::uintptr_t>(target);
  std::memcpy(relay.data() + 2, &address, sizeof(address));
  relay[10] = 0xFF;
  relay[11] = 0xE0;
  return relay;
}

template <std::size_t ByteCount>
bool EncodeRelativeCallBytes(
    const void *const instruction, const void *const target,
    std::array<std::uint8_t, ByteCount> &call) noexcept {
  static_assert(ByteCount >= 5);
  if (instruction == nullptr || target == nullptr) {
    return false;
  }

  const auto sourceAddress = reinterpret_cast<std::uintptr_t>(instruction);
  const auto targetAddress = reinterpret_cast<std::uintptr_t>(target);
  const auto displacement = static_cast<std::int64_t>(targetAddress) -
                            static_cast<std::int64_t>(sourceAddress + 5);
  if (displacement < (std::numeric_limits<std::int32_t>::min)() ||
      displacement > (std::numeric_limits<std::int32_t>::max)()) {
    return false;
  }

  call.fill(0x90);
  call[0] = 0xE8;
  const auto relative = static_cast<std::int32_t>(displacement);
  std::memcpy(call.data() + 1, &relative, sizeof(relative));
  return true;
}

bool EncodeRelativeCall(
    const void *const instruction, const void *const target,
    std::array<std::uint8_t, kCreateHeapCallSize> &call) noexcept {
  return EncodeRelativeCallBytes(instruction, target, call);
}

bool EncodeRelativeCall(
    const void *const instruction, const void *const target,
    std::array<std::uint8_t, kPlacedResourceCallSize> &call) noexcept {
  return EncodeRelativeCallBytes(instruction, target, call);
}

bool HasExpectedFetchTablePrologue(const std::uint8_t *bytes,
                                   const std::size_t byteCount) noexcept {
  return bytes != nullptr && byteCount >= kExpectedPrologue.size() &&
         std::equal(kExpectedPrologue.begin(), kExpectedPrologue.end(), bytes);
}

bool HasExpectedShaderCompilePrologue(const std::uint8_t *bytes,
                                      const std::size_t byteCount) noexcept {
  return bytes != nullptr &&
         byteCount >= kExpectedShaderCompilePrologue.size() &&
         std::equal(kExpectedShaderCompilePrologue.begin(),
                    kExpectedShaderCompilePrologue.end(), bytes);
}

bool HasExpectedXenosTranslatePrologue(const std::uint8_t *bytes,
                                       const std::size_t byteCount) noexcept {
  return bytes != nullptr &&
         byteCount >= kExpectedXenosTranslatePrologue.size() &&
         std::equal(kExpectedXenosTranslatePrologue.begin(),
                    kExpectedXenosTranslatePrologue.end(), bytes);
}

bool HasExpectedTextureTransferPrologue(const std::uint8_t *bytes,
                                        const std::size_t byteCount) noexcept {
  return bytes != nullptr &&
         byteCount >= kExpectedTextureTransferPrologue.size() &&
         std::equal(kExpectedTextureTransferPrologue.begin(),
                    kExpectedTextureTransferPrologue.end(), bytes);
}

bool HasExpectedStructuredTextureTransferPrologue(
    const std::uint8_t *bytes, const std::size_t byteCount) noexcept {
  return bytes != nullptr &&
         byteCount >= kExpectedStructuredTextureTransferPrologue.size() &&
         std::equal(kExpectedStructuredTextureTransferPrologue.begin(),
                    kExpectedStructuredTextureTransferPrologue.end(), bytes);
}

bool HasExpectedConstantUploadPrologue(const std::uint8_t *bytes,
                                       const std::size_t byteCount) noexcept {
  return bytes != nullptr &&
         byteCount >= kExpectedConstantUploadPrologue.size() &&
         std::equal(kExpectedConstantUploadPrologue.begin(),
                    kExpectedConstantUploadPrologue.end(), bytes);
}

bool HasExpectedNullPipelineStateSequence(
    const std::uint8_t *bytes, const std::size_t byteCount) noexcept {
  return bytes != nullptr &&
         byteCount >= kExpectedNullPipelineStateSequence.size() &&
         std::equal(kExpectedNullPipelineStateSequence.begin(),
                    kExpectedNullPipelineStateSequence.end(), bytes);
}

std::array<std::uint32_t, kXenosSamplerAddressModeCount>
GetD3d12SamplerAddressModeTable() noexcept {
  return kFixedSamplerAddressModes;
}

bool HasExpectedSamplerAddressModeTable(const std::uint32_t *const modes,
                                        const std::size_t modeCount) noexcept {
  return modes != nullptr && modeCount >= kExpectedSamplerAddressModes.size() &&
         std::equal(kExpectedSamplerAddressModes.begin(),
                    kExpectedSamplerAddressModes.end(), modes);
}

bool IsGeneratedVertexShader(const void *const source,
                             const std::size_t sourceSize) noexcept {
  if (source == nullptr || sourceSize == 0) {
    return false;
  }
  const std::string_view sourceText(static_cast<const char *>(source),
                                    sourceSize);
  return sourceText.find("xenon_vertex_shader") != std::string_view::npos;
}

bool PatchXenosScalarReciprocals(const void *const source,
                                 const std::size_t sourceSize,
                                 std::string &patchedSource,
                                 std::uint32_t &patchCount) noexcept {
  patchedSource.clear();
  patchCount = 0;
  if (source == nullptr || sourceSize == 0) {
    return false;
  }

  const std::string_view sourceText(static_cast<const char *>(source),
                                    sourceSize);
  const auto isGeneratedShader =
      sourceText.find("xenon_vertex_shader") != std::string_view::npos ||
      sourceText.find("xenon_pixel_shader") != std::string_view::npos;
  if (!isGeneratedShader) {
    return true;
  }

  const auto isIdentifierCharacter = [](const char value) noexcept {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
           (value >= '0' && value <= '9') || value == '_';
  };
  const auto isWhitespace = [](const char value) noexcept {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
  };

  constexpr std::string_view kNativeReciprocal = "rcp";
  constexpr std::string_view kReplacementReciprocal =
      "XeO3Ac6ApproximateReciprocal";
  constexpr std::string_view kRootSignature = "[RootSignature(";
  constexpr std::string_view kHelper =
      "precise float XeO3Ac6ApproximateReciprocal(float operand)\n"
      "{\n"
      "    precise float reciprocal = 1.0f / operand;\n"
      "    uint operandBits = asuint(operand);\n"
      "    uint bumpedBits = asuint(reciprocal) + 1u;\n"
      "    precise float bumped = asfloat(bumpedBits);\n"
      "    bool keepOriginal = ((operandBits & 0x007FFFFFu) == 0u) ||\n"
      "                        ((bumpedBits & 0x7F800000u) == "
      "0x7F800000u);\n"
      "    return keepOriginal ? reciprocal : bumped;\n"
      "}\n\n";

  try {
    std::string replacementBody;
    replacementBody.reserve(sourceSize + 256);
    std::size_t copyCursor = 0;
    std::size_t searchCursor = 0;
    while (searchCursor < sourceText.size()) {
      const auto reciprocal = sourceText.find(kNativeReciprocal, searchCursor);
      if (reciprocal == std::string_view::npos) {
        break;
      }

      const auto hasIdentifierBefore =
          reciprocal != 0 && isIdentifierCharacter(sourceText[reciprocal - 1]);
      auto after = reciprocal + kNativeReciprocal.size();
      const auto hasIdentifierAfter =
          after < sourceText.size() && isIdentifierCharacter(sourceText[after]);
      while (after < sourceText.size() && isWhitespace(sourceText[after])) {
        ++after;
      }
      if (hasIdentifierBefore || hasIdentifierAfter ||
          after >= sourceText.size() || sourceText[after] != '(') {
        searchCursor = reciprocal + kNativeReciprocal.size();
        continue;
      }

      replacementBody.append(sourceText.data() + copyCursor,
                             reciprocal - copyCursor);
      replacementBody.append(kReplacementReciprocal);
      copyCursor = reciprocal + kNativeReciprocal.size();
      searchCursor = copyCursor;
      ++patchCount;
    }

    if (patchCount == 0) {
      return true;
    }
    replacementBody.append(sourceText.data() + copyCursor,
                           sourceText.size() - copyCursor);
    const auto insertion = replacementBody.find(kRootSignature);
    if (insertion == std::string::npos) {
      patchedSource.clear();
      patchCount = 0;
      return false;
    }
    patchedSource.reserve(replacementBody.size() + kHelper.size());
    patchedSource.append(replacementBody.data(), insertion);
    patchedSource.append(kHelper);
    patchedSource.append(replacementBody.data() + insertion,
                         replacementBody.size() - insertion);
    return true;
  } catch (...) {
    patchedSource.clear();
    patchCount = 0;
    return false;
  }
}

bool MatchesAc6Pso533PixelShaderFingerprint(
    const std::size_t sourceSize,
    const std::array<std::uint8_t, 32> &digest) noexcept {
  return sourceSize == kAc6Pso533PixelShaderSourceSize &&
         digest == kAc6Pso533PixelShaderSourceSha256;
}

bool MatchesAc6Pso540VertexShaderFingerprint(
    const std::size_t sourceSize,
    const std::array<std::uint8_t, 32> &digest) noexcept {
  return sourceSize == kAc6Pso540VertexShaderSourceSize &&
         digest == kAc6Pso540VertexShaderSourceSha256;
}

bool PatchAc6WaveBallots(const void *const source,
                         const std::size_t sourceSize,
                         std::string &patchedSource,
                         std::uint32_t &patchCount) noexcept {
  patchedSource.clear();
  patchCount = 0;
  if (source == nullptr || sourceSize == 0) {
    return false;
  }

  const std::string_view sourceText(static_cast<const char *>(source),
                                    sourceSize);
  constexpr std::string_view kPixelEntry = "xenon_pixel_shader";
  constexpr std::string_view kVertexEntry = "xenon_vertex_shader";
  constexpr std::string_view kNativeBallot = "ballot = BallotAll(p,";
  constexpr std::string_view kFixedBallot = "ballot = XeO3Ac6BallotAll(p,";
  constexpr std::string_view kRootSignature = "[RootSignature(";
  constexpr std::string_view kHelper =
      "uint XeO3Ac6BallotAll(uint predicate, uint testValue)\n"
      "{\n"
      "    return WaveActiveAllTrue(predicate == testValue) ? 1u : 0u;\n"
      "}\n\n";

  if (sourceText.find(kPixelEntry) == std::string_view::npos &&
      sourceText.find(kVertexEntry) == std::string_view::npos) {
    return true;
  }

  try {
    std::string replacementBody;
    replacementBody.reserve(sourceSize + kHelper.size() + 32);
    std::size_t copyCursor = 0;
    std::size_t searchCursor = 0;
    while (searchCursor < sourceText.size()) {
      const auto ballot = sourceText.find(kNativeBallot, searchCursor);
      if (ballot == std::string_view::npos) {
        break;
      }
      replacementBody.append(sourceText.data() + copyCursor,
                             ballot - copyCursor);
      replacementBody.append(kFixedBallot);
      copyCursor = ballot + kNativeBallot.size();
      searchCursor = copyCursor;
      ++patchCount;
    }

    if (patchCount == 0) {
      return true;
    }
    if (patchCount != 2) {
      patchCount = 0;
      return false;
    }
    replacementBody.append(sourceText.data() + copyCursor,
                           sourceText.size() - copyCursor);
    const auto insertion = replacementBody.find(kRootSignature);
    if (insertion == std::string::npos) {
      patchCount = 0;
      return false;
    }
    patchedSource.reserve(replacementBody.size() + kHelper.size());
    patchedSource.append(replacementBody.data(), insertion);
    patchedSource.append(kHelper);
    patchedSource.append(replacementBody.data() + insertion,
                         replacementBody.size() - insertion);
    return true;
  } catch (...) {
    patchedSource.clear();
    patchCount = 0;
    return false;
  }
}

bool PatchAc6Pso533WaveBallots(const void *const source,
                               const std::size_t sourceSize,
                               std::string &patchedSource,
                               std::uint32_t &patchCount) noexcept {
  patchedSource.clear();
  patchCount = 0;
  if (source == nullptr || sourceSize == 0) {
    return false;
  }

  const std::string_view sourceText(static_cast<const char *>(source),
                                    sourceSize);
  if (sourceText.find("xenon_pixel_shader") == std::string_view::npos) {
    return true;
  }
  return PatchAc6WaveBallots(source, sourceSize, patchedSource, patchCount);
}

bool PatchAc6ScreenSpaceVposScale(const void *const source,
                                  const std::size_t sourceSize,
                                  std::string &patchedSource,
                                  std::uint32_t &patchCount) noexcept {
  patchedSource.clear();
  patchCount = 0;
  if (source == nullptr || sourceSize == 0) {
    return false;
  }

  const std::string_view sourceText(static_cast<const char *>(source),
                                    sourceSize);
  constexpr std::string_view kCommonHeader = "#include \"common_header.h\"";
  constexpr std::string_view kPixelEntry = "xenon_pixel_shader";
  constexpr std::string_view kGeometryEntry = "void gsmain(";
  constexpr std::string_view kNativeScale = "vpos_Scale";
  constexpr std::string_view kCorrectScale = "(float2(1.0f, 1.0f))";

  // vpos_Scale is the resolution multiplier applied to SV_Position, not the
  // pixel-to-NDC vport_Scale. XeO3 leaves its packed slot zero for this title;
  // AC6 renders at native 1280x720, so its verified value is one. PIX replay
  // shows that aliasing to vport_Scale collapses scene UVs to a single sample.
  if (sourceText.find(kCommonHeader) == std::string_view::npos ||
      (sourceText.find(kPixelEntry) == std::string_view::npos &&
       sourceText.find(kGeometryEntry) == std::string_view::npos)) {
    return true;
  }

  try {
    std::size_t copyCursor = 0;
    std::size_t searchCursor = 0;
    while (searchCursor < sourceText.size()) {
      const auto scale = sourceText.find(kNativeScale, searchCursor);
      if (scale == std::string_view::npos) {
        break;
      }
      const auto isIdentifier = [](const char value) noexcept {
        return (value >= 'a' && value <= 'z') ||
               (value >= 'A' && value <= 'Z') ||
               (value >= '0' && value <= '9') || value == '_';
      };
      const bool validLeft = scale == 0 || !isIdentifier(sourceText[scale - 1]);
      const auto afterScale = scale + kNativeScale.size();
      const bool validRight = afterScale == sourceText.size() ||
                              !isIdentifier(sourceText[afterScale]);
      if (!validLeft || !validRight) {
        searchCursor = afterScale;
        continue;
      }
      patchedSource.append(sourceText.data() + copyCursor, scale - copyCursor);
      patchedSource.append(kCorrectScale);
      copyCursor = afterScale;
      searchCursor = copyCursor;
      ++patchCount;
    }
    if (patchCount == 0) {
      patchedSource.clear();
      return true;
    }
    patchedSource.append(sourceText.data() + copyCursor,
                         sourceText.size() - copyCursor);
    return true;
  } catch (...) {
    patchedSource.clear();
    patchCount = 0;
    return false;
  }
}

bool MatchesAc6Pso537ShaderFingerprint(
    const std::size_t sourceSize,
    const std::array<std::uint8_t, 32> &digest) noexcept {
  return sourceSize == kAc6Pso537ShaderSourceSize &&
         digest == kAc6Pso537ShaderSourceSha256;
}

bool PatchAc6Pso537HalfWidthUv(const void *const source,
                               const std::size_t sourceSize,
                               std::string &patchedSource,
                               std::uint32_t &patchCount) noexcept {
  patchedSource.clear();
  patchCount = 0;
  if (source == nullptr || sourceSize == 0) {
    return false;
  }

  const std::string_view sourceText(static_cast<const char *>(source),
                                    sourceSize);
  constexpr std::string_view kPixelEntry = "xenon_pixel_shader";
  constexpr std::string_view kColorOutput = "float4 oC0 : SV_Target0;";
  constexpr std::string_view kDepthOutput = "float1 oD : SV_Depth;";
  constexpr std::string_view kTexture1 = "Texture2D texOBJ1 : register(t1);";
  constexpr std::string_view kTexture2 = "Texture2D texOBJ2 : register(t2);";
  constexpr std::string_view kNormalizedUv = "gpr1.xy = gpr0.xy * c(255).xy;";
  constexpr std::string_view kHalfWidthUv = "gpr1.xy = gpr0.xy * c(255).xy;\n"
                                            "gpr1.x = gpr1.x * 0.5f;";

  if (sourceText.find(kPixelEntry) == std::string_view::npos ||
      sourceText.find(kColorOutput) == std::string_view::npos ||
      sourceText.find(kDepthOutput) == std::string_view::npos ||
      sourceText.find(kTexture1) == std::string_view::npos ||
      sourceText.find(kTexture2) == std::string_view::npos) {
    return true;
  }

  const auto normalizedUv = sourceText.find(kNormalizedUv);
  if (normalizedUv == std::string_view::npos ||
      sourceText.find(kNormalizedUv, normalizedUv + kNormalizedUv.size()) !=
          std::string_view::npos) {
    return false;
  }

  try {
    patchedSource.reserve(sourceText.size() - kNormalizedUv.size() +
                          kHalfWidthUv.size());
    patchedSource.append(sourceText.data(), normalizedUv);
    patchedSource.append(kHalfWidthUv);
    patchedSource.append(
        sourceText.data() + normalizedUv + kNormalizedUv.size(),
        sourceText.size() - normalizedUv - kNormalizedUv.size());
    patchCount = 1;
    return true;
  } catch (...) {
    patchedSource.clear();
    patchCount = 0;
    return false;
  }
}

bool PatchAc6ToneMapInterpolant(const void *const source,
                                const std::size_t sourceSize,
                                std::string &patchedSource,
                                std::uint32_t &patchCount) noexcept {
  patchedSource.clear();
  patchCount = 0;
  if (source == nullptr || sourceSize == 0) {
    return false;
  }

  const std::string_view sourceText(static_cast<const char *>(source),
                                    sourceSize);
  constexpr std::string_view kPixelEntry = "xenon_pixel_shader";
  constexpr std::string_view kInterpolant = "linear float3 v0 : TEXCOORD0;";
  constexpr std::string_view kTexture0 = "Texture2D texOBJ0 : register(t0);";
  constexpr std::string_view kTexture1 = "Texture2D texOBJ1 : register(t1);";
  constexpr std::string_view kTexture2 = "Texture2D texOBJ2 : register(t2);";
  constexpr std::string_view kInputCopy = "gpr0.xyz = InV.v0.xyz;";
  constexpr std::string_view kAuxiliaryScale =
      "gpr0.xyw = gpr0.xyw * c(100).www;";
  constexpr std::string_view kAuxiliaryBias =
      "gpr0.xyw = mad( gpr2.xyz, c(100).zzz, gpr0.xyw );";
  constexpr std::string_view kNativeComposite =
      "OutV.oC0.xyz = mad( gpr1.xyz, gpr0.zzz, gpr0.xyw );";
  constexpr std::string_view kFixedComposite =
      "OutV.oC0.xyz = mad( gpr1.xyz, float3(1.0f, 1.0f, 1.0f), "
      "gpr0.xyw );";

  if (sourceText.find(kPixelEntry) == std::string_view::npos ||
      sourceText.find(kInterpolant) == std::string_view::npos ||
      sourceText.find(kTexture0) == std::string_view::npos ||
      sourceText.find(kTexture1) == std::string_view::npos ||
      sourceText.find(kTexture2) == std::string_view::npos ||
      sourceText.find(kInputCopy) == std::string_view::npos ||
      sourceText.find(kAuxiliaryScale) == std::string_view::npos ||
      sourceText.find(kAuxiliaryBias) == std::string_view::npos) {
    return true;
  }

  const auto composite = sourceText.find(kNativeComposite);
  if (composite == std::string_view::npos ||
      sourceText.find(kNativeComposite, composite + kNativeComposite.size()) !=
          std::string_view::npos) {
    return composite == std::string_view::npos;
  }

  try {
    patchedSource.reserve(sourceText.size() - kNativeComposite.size() +
                          kFixedComposite.size());
    patchedSource.append(sourceText.data(), composite);
    patchedSource.append(kFixedComposite);
    patchedSource.append(
        sourceText.data() + composite + kNativeComposite.size(),
        sourceText.size() - composite - kNativeComposite.size());
    patchCount = 1;
    return true;
  } catch (...) {
    patchedSource.clear();
    patchCount = 0;
    return false;
  }
}

bool MatchesAc6ExposureShaderFingerprint(
    const std::size_t sourceSize,
    const std::array<std::uint8_t, 32> &digest) noexcept {
  return sourceSize == kAc6ExposureShaderSourceSize &&
         digest == kAc6ExposureShaderSourceSha256;
}

bool MatchesAc6SkyRestartShaderFingerprint(
    const std::size_t sourceSize,
    const std::array<std::uint8_t, 32> &digest) noexcept {
  return sourceSize == kAc6SkyRestartShaderSourceSize &&
         digest == kAc6SkyRestartShaderSourceSha256;
}

bool MatchesAc6TerrainFanRestartShaderFingerprint(
    const std::size_t sourceSize,
    const std::array<std::uint8_t, 32> &digest) noexcept {
  return sourceSize == kAc6TerrainFanRestartShaderSourceSize &&
         digest == kAc6TerrainFanRestartShaderSourceSha256;
}

bool MatchesAc6AircraftRestartShaderFingerprint(
    const std::size_t sourceSize,
    const std::array<std::uint8_t, 32> &digest) noexcept {
  return sourceSize == kAc6AircraftRestartShaderSourceSize &&
         digest == kAc6AircraftRestartShaderSourceSha256;
}

bool MatchesAc6ShadowRestartShaderFingerprint(
    const std::size_t sourceSize,
    const std::array<std::uint8_t, 32> &digest) noexcept {
  return sourceSize == kAc6ShadowRestartShaderSourceSize &&
         digest == kAc6ShadowRestartShaderSourceSha256;
}

Ac6PrimitiveRestartPipeline ClassifyAc6PrimitiveRestartPipeline(
    const GraphicsPipelineSignature &signature) noexcept {
  if (signature.vertexShaderSize == kAc6TerrainFanRestartDxilSize &&
      signature.vertexShaderSha256 == kAc6TerrainFanRestartDxilSha256) {
    return Ac6PrimitiveRestartPipeline::TerrainFan;
  }
  if (signature.vertexShaderSize == kAc6TerrainFanRestartWindowDxilSize &&
      signature.vertexShaderSha256 == kAc6TerrainFanRestartWindowDxilSha256) {
    return Ac6PrimitiveRestartPipeline::TerrainFan;
  }
  if (signature.vertexShaderSize == kAc6TerrainDrawLocalDxilSize &&
      signature.vertexShaderSha256 == kAc6TerrainDrawLocalDxilSha256) {
    return Ac6PrimitiveRestartPipeline::TerrainFan;
  }
  if ((signature.vertexShaderSize == kAc6SkyRestartDxilSize &&
       signature.vertexShaderSha256 == kAc6SkyRestartDxilSha256) ||
      (signature.vertexShaderSize == kAc6SkyDrawLocalDxilSize &&
       signature.vertexShaderSha256 == kAc6SkyDrawLocalDxilSha256) ||
      (signature.vertexShaderSize == 7840 &&
       signature.vertexShaderSha256 == kAc6Pso535VertexShaderSha256)) {
    return Ac6PrimitiveRestartPipeline::SkyStrip;
  }
  return Ac6PrimitiveRestartPipeline::None;
}

bool PatchAc6ExposureSample(const void *const source,
                            const std::size_t sourceSize,
                            std::string &patchedSource,
                            std::uint32_t &patchCount) noexcept {
  patchedSource.clear();
  patchCount = 0;
  if (source == nullptr || sourceSize == 0) {
    return false;
  }

  const std::string_view sourceText(static_cast<const char *>(source),
                                    sourceSize);
  constexpr std::string_view kVertexEntry = "xenon_vertex_shader";
  constexpr std::string_view kExposureTexture =
      "Texture2D texOBJ19 : register(t19);";
  constexpr std::string_view kExposureSample =
      "tmp0.xyzw = texOBJ19.SampleLevel(samp19 , tT.xy, tT.w);";
  constexpr std::string_view kIntegerFetch =
      "tmp1.xyzw = ((asuint(tfpatch[19].w) & 0x4000) != 0);";
  constexpr std::string_view kSampleAssignment = "gpr0.x = tmp0.x;";
  constexpr std::string_view kLowerThreshold =
      "gpr0.y = select( c(106).x > gpr0.x , 1.0f , 0.0f );";
  constexpr std::string_view kUpperThreshold =
      "gpr0.y = select( gpr0.x > c(106).y , 1.0f , 0.0f );";
  constexpr std::string_view kGuardedAssignment =
      "gpr0.x = tmp0.x;\n"
      "gpr0.x = (isfinite(gpr0.x) && gpr0.x > 0.0f && "
      "gpr0.x <= 65536.0f) ? gpr0.x : c(106).y;";

  if (sourceText.find(kVertexEntry) == std::string_view::npos ||
      sourceText.find(kExposureTexture) == std::string_view::npos ||
      sourceText.find(kExposureSample) == std::string_view::npos ||
      sourceText.find(kIntegerFetch) == std::string_view::npos ||
      sourceText.find(kLowerThreshold) == std::string_view::npos ||
      sourceText.find(kUpperThreshold) == std::string_view::npos) {
    return true;
  }

  const auto assignment = sourceText.find(kSampleAssignment);
  if (assignment == std::string_view::npos ||
      sourceText.find(kSampleAssignment,
                      assignment + kSampleAssignment.size()) !=
          std::string_view::npos) {
    return false;
  }

  try {
    patchedSource.reserve(sourceText.size() - kSampleAssignment.size() +
                          kGuardedAssignment.size());
    patchedSource.append(sourceText.data(), assignment);
    patchedSource.append(kGuardedAssignment);
    patchedSource.append(
        sourceText.data() + assignment + kSampleAssignment.size(),
        sourceText.size() - assignment - kSampleAssignment.size());
    patchCount = 1;
    return true;
  } catch (...) {
    patchedSource.clear();
    patchCount = 0;
    return false;
  }
}

bool IsAc6TextureUnpackTransfer(
    const std::array<std::uint32_t, 6> &constants) noexcept {
  return ClassifyAc6TextureUnpackTransfer(constants) !=
         Ac6TextureUnpackKind::None;
}

Ac6TextureUnpackKind ClassifyAc6TextureUnpackTransfer(
    const std::array<std::uint32_t, 6> &constants) noexcept {
  constexpr std::array<std::uint32_t, 6> kAc6FrameUnpackSignature{
      0, 2, 4, 1, 6, 14400,
  };
  // The pinned Mission 01 replay isolates this 208x144 transfer to exactly
  // the target-preview rectangle. Other sizes include texture assets and
  // reflection mip chains; do not generalize the endian correction to them.
  constexpr std::array<std::uint32_t, 6> kAc6TargetPreviewUnpackSignature{
      0, 2, 4, 1, 6, 468,
  };
  if (constants == kAc6FrameUnpackSignature) {
    return Ac6TextureUnpackKind::Frame;
  }
  if (constants == kAc6TargetPreviewUnpackSignature) {
    return Ac6TextureUnpackKind::TargetPreview;
  }
  return Ac6TextureUnpackKind::None;
}

Ac6EdramConstantKind ClassifyAc6EdramTransferConstants(
    const std::array<std::uint32_t, 16> &constants) noexcept {
  constexpr std::array<std::uint32_t, 16> kAc6EdramLoadConstants{
      0, 0, 1280, 720, 0, 0, 1280, 720, 1280, 2048, 1280, 720, 1, 0, 0, 16,
  };
  constexpr std::array<std::uint32_t, 16> kAc6EdramScaleConstants{
      0, 0, 1280, 720, 0, 0, 640, 360, 1280, 2048, 640, 360, 1, 0, 0, 16,
  };
  if (constants == kAc6EdramLoadConstants) {
    return Ac6EdramConstantKind::Load;
  }
  if (constants == kAc6EdramScaleConstants) {
    return Ac6EdramConstantKind::Scale;
  }

  const bool hasAc6SourceGeometry =
      constants[0] == 0 && constants[1] == 0 && constants[2] == 1280 &&
      constants[3] == 720 && constants[8] == 1280 && constants[9] == 2048;
  const bool hasConsistentDestinationGeometry =
      constants[4] == 0 && constants[5] == 0 && constants[6] != 0 &&
      constants[7] != 0 && constants[6] == constants[10] &&
      constants[7] == constants[11];
  const bool hasAc6TransferTail = constants[12] == 1 && constants[13] == 0 &&
                                  constants[14] == 0 && constants[15] == 16;
  return hasAc6SourceGeometry && hasConsistentDestinationGeometry &&
                 hasAc6TransferTail
             ? Ac6EdramConstantKind::Candidate
             : Ac6EdramConstantKind::None;
}

std::array<std::uint64_t, 8> PackAc6EdramTransferConstants(
    const std::array<std::uint32_t, 16> &constants) noexcept {
  std::array<std::uint64_t, 8> packed{};
  for (std::size_t index = 0; index < packed.size(); ++index) {
    packed[index] =
        static_cast<std::uint64_t>(constants[index * 2]) |
        (static_cast<std::uint64_t>(constants[index * 2 + 1]) << 32);
  }
  return packed;
}

std::uint32_t
ComputeAc6EdramScaleAddress(const std::uint32_t x, const std::uint32_t y,
                            const std::uint32_t resolutionDivisor,
                            const std::uint32_t edramBaseTiles,
                            const std::uint32_t sampleIndex,
                            const std::uint32_t edramPitchTiles) noexcept {
  const std::uint32_t divisor = resolutionDivisor == 0 ? 1u : resolutionDivisor;
  const std::uint32_t scaledX = x / divisor;
  const std::uint32_t scaledY = y / divisor;
  const std::uint32_t resolutionSamplePlane =
      ((y % divisor) * divisor + (x % divisor)) * 2621440u;

  const std::uint32_t evenX = scaledX & ~1u;
  const std::uint32_t doubledXParity = (2u * (scaledX - evenX)) & 0x01FFFFFEu;
  const std::uint32_t doubledY = scaledY << 1u;
  std::uint32_t sampleX = doubledXParity + (sampleIndex >> 1u);
  std::uint32_t sampleY = (doubledY & 2u) | (sampleIndex & 1u);
  if ((sampleX - 1u) < 2u) {
    sampleX ^= 3u;
  }
  if ((sampleY - 1u) < 2u) {
    sampleY ^= 3u;
  }

  const std::uint32_t tileX = scaledX / 40u;
  const std::uint32_t tileY = scaledY >> 3u;
  const std::uint32_t withinTileX = evenX % 40u;
  const std::uint32_t withinTileY = (sampleY & 3u) | (doubledY & 12u);
  const std::uint32_t pitch = edramPitchTiles & 0x00FFFFFFu;
  const std::uint32_t tile = (tileY * pitch + edramBaseTiles + tileX) & 2047u;
  return tile * 1280u + 2u * (withinTileY * 40u + withinTileX) + sampleX +
         resolutionSamplePlane;
}

bool PatchAc6TextureUnpackEndian(
    std::array<std::uint32_t, 6> &constants,
    const std::uint32_t replacementEndian) noexcept {
  if (!IsAc6TextureUnpackTransfer(constants) || replacementEndian > 3 ||
      replacementEndian == constants[1]) {
    return false;
  }
  constants[1] = replacementEndian;
  return true;
}

std::array<std::uint32_t, 6> ExtractStructuredTextureTransferConstants(
    const void *const descriptor) noexcept {
  std::array<std::uint32_t, 6> constants{};
  if (descriptor == nullptr) {
    return constants;
  }
  const auto *const descriptorBytes =
      static_cast<const std::uint8_t *>(descriptor);
  std::memcpy(constants.data(), descriptorBytes, sizeof(std::uint32_t) * 4);
  std::memcpy(constants.data() + 4, descriptorBytes + sizeof(std::uint32_t) * 6,
              sizeof(std::uint32_t) * 2);
  return constants;
}

bool ShouldTrackAc6Pso341UploadBuffer(
    const std::uint32_t heapType, const bool isBuffer,
    const std::uint64_t resourceSize) noexcept {
  // PIX used GPU_UPLOAD while the live XeO3 path falls back to UPLOAD on the
  // pinned AMD driver. The exact arena geometry keeps this narrow.
  return (heapType == kD3d12UploadHeapType ||
          heapType == kD3d12GpuUploadHeapType) &&
         isBuffer && resourceSize == kAc6Pso341UploadArenaSize;
}

bool ResolveAc6Pso341UploadOffset(const std::uint64_t gpuBase,
                                  const std::uint32_t stride,
                                  const std::uint32_t slotCount,
                                  const std::uint64_t mappedSpan,
                                  const std::uint64_t gpuAddress,
                                  const std::uint64_t sourceSize,
                                  std::uint64_t &cpuOffset) noexcept {
  cpuOffset = 0;
  if (gpuBase == 0 || stride != kAc6Pso341TaskBufferSize || slotCount == 0 ||
      sourceSize != kAc6Pso341TaskBufferSize || gpuAddress < gpuBase) {
    return false;
  }
  const auto arenaSize = static_cast<std::uint64_t>(stride) * slotCount;
  if (arenaSize != kAc6Pso341UploadArenaSize || mappedSpan < arenaSize) {
    return false;
  }
  const auto offset = gpuAddress - gpuBase;
  if (offset > mappedSpan || sourceSize > mappedSpan - offset) {
    return false;
  }
  cpuOffset = offset;
  return true;
}

bool IsConstantUploadContextGeometryValid(
    const std::uint64_t gpuBase, const std::uint32_t stride,
    const std::uint32_t slotCount, const std::uint64_t mappedSpan,
    const std::uint64_t sourceSize) noexcept {
  if (gpuBase == 0 || stride == 0 || slotCount == 0 || sourceSize == 0 ||
      sourceSize > stride ||
      slotCount > (std::numeric_limits<std::uint64_t>::max)() / stride) {
    return false;
  }
  const auto arenaSize = static_cast<std::uint64_t>(stride) * slotCount;
  return arenaSize >= sourceSize && arenaSize <= mappedSpan;
}

std::uint64_t DecodeAmdConstantBufferGpuAddress(
    const std::array<std::uint64_t, 4> &descriptorWords) noexcept {
  // The pinned AMD UMD stores a CBV base in bits 0..47 of descriptor word 0;
  // bits 48..63 contain descriptor control data. The caller still validates
  // the result against a mapped upload arena and the full AC6 task signature.
  constexpr std::uint64_t kGpuAddressMask = 0x0000FFFFFFFFFFFFULL;
  const auto address = descriptorWords[0] & kGpuAddressMask;
  return address != 0 && (address & 0xFF) == 0 ? address : 0;
}

bool MatchesAc6Pso341ComputeShaderFingerprint(
    const std::size_t shaderSize,
    const std::array<std::uint8_t, 32> &digest) noexcept {
  return shaderSize == kAc6Pso341ComputeShaderSize &&
         digest == kAc6Pso341ComputeShaderSha256;
}

bool MatchesAc6Pso341CachedPipelineBlob(
    const std::size_t blobSize,
    const std::array<std::uint8_t, 32> &digest) noexcept {
  // AMD UMD 32.0.31041.1004 gives replay-created and XeO3-preloaded PSOs
  // different cache provenance. Both fingerprints are stable under the
  // pinned VGPUDX12 and driver profile.
  constexpr std::size_t kExpectedCachedBlobSize = 954;
  constexpr std::array<std::uint8_t, 32> kReplayCachedBlobSha256{
      0x75, 0xC7, 0xF4, 0x4B, 0x78, 0x4A, 0x9F, 0x7A, 0x2D, 0xE0, 0x21,
      0x6A, 0x0F, 0x46, 0x0E, 0xF0, 0x40, 0x7F, 0x4F, 0x5E, 0x83, 0x5F,
      0x6F, 0x86, 0x04, 0xF1, 0xB7, 0x0B, 0xF0, 0xC0, 0x50, 0xCA,
  };
  constexpr std::array<std::uint8_t, 32> kLiveCachedBlobSha256{
      0x2D, 0x34, 0x18, 0x7A, 0x02, 0x0B, 0x37, 0xA6, 0x9B, 0x0B, 0x07,
      0x7B, 0xFC, 0x08, 0x00, 0x5A, 0xD0, 0xA7, 0xA5, 0x30, 0x33, 0x7D,
      0xBA, 0xF4, 0xCF, 0x9C, 0x0E, 0x6B, 0xE7, 0xB3, 0x6A, 0xE4,
  };
  return blobSize == kExpectedCachedBlobSize &&
         (digest == kReplayCachedBlobSha256 || digest == kLiveCachedBlobSha256);
}

Ac6Pso341TaskWidthState
ClassifyAc6Pso341TaskWidth(const void *const source,
                           const std::size_t sourceSize) noexcept {
  if (source == nullptr || sourceSize != kAc6Pso341TaskBufferSize) {
    return Ac6Pso341TaskWidthState::NotCandidate;
  }

  std::array<std::uint32_t, kAc6Pso341TaskBufferSize / sizeof(std::uint32_t)>
      words{};
  std::memcpy(words.data(), source, sourceSize);
  const bool taskHeaderMatches =
      words[0] == 1280 && words[1] == 720 && words[3] == 1 &&
      words[4] == 5120 && words[5] == 0 && words[6] == 0 && words[7] == 0;
  if (!taskHeaderMatches || words[128] != 14400) {
    return Ac6Pso341TaskWidthState::NotCandidate;
  }
  for (std::size_t index = 129; index < 144; ++index) {
    if (words[index] != UINT32_MAX) {
      return Ac6Pso341TaskWidthState::NotCandidate;
    }
  }

  if (words[2] == kAc6Pso341BrokenPackedDimensions) {
    return Ac6Pso341TaskWidthState::Broken;
  }
  if (words[2] == kAc6Pso341CorrectedPackedDimensions) {
    return Ac6Pso341TaskWidthState::Correct;
  }
  return Ac6Pso341TaskWidthState::NotCandidate;
}

bool PatchAc6Pso341TaskWidth(
    void *const source, const std::size_t sourceSize,
    std::uint32_t &originalPackedDimensions,
    std::uint32_t &replacementPackedDimensions) noexcept {
  originalPackedDimensions = 0;
  replacementPackedDimensions = 0;
  if (ClassifyAc6Pso341TaskWidth(source, sourceSize) !=
      Ac6Pso341TaskWidthState::Broken) {
    return false;
  }

  auto *const bytes = static_cast<std::uint8_t *>(source);
  std::memcpy(&originalPackedDimensions, bytes + sizeof(std::uint32_t) * 2,
              sizeof(originalPackedDimensions));
  replacementPackedDimensions = kAc6Pso341CorrectedPackedDimensions;
  std::memcpy(bytes + sizeof(std::uint32_t) * 2, &replacementPackedDimensions,
              sizeof(replacementPackedDimensions));
  return true;
}

bool PatchAc6GroundFetchIndices(const void *const source,
                                const std::size_t sourceSize,
                                std::string &patchedSource,
                                std::uint32_t &patchCount) noexcept {
  patchedSource.clear();
  patchCount = 0;
  if (source == nullptr || sourceSize == 0) {
    return false;
  }

  const std::string_view sourceText(static_cast<const char *>(source),
                                    sourceSize);
  if (sourceText.find("xenon_vertex_shader") == std::string_view::npos) {
    return true;
  }

  const auto isWhitespace = [](const char value) noexcept {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
  };
  const auto isGprComponent = [](const std::string_view value) noexcept {
    if (value.size() < 6 || value[0] != 'g' || value[1] != 'p' ||
        value[2] != 'r') {
      return false;
    }
    std::size_t cursor = 3;
    const auto digitStart = cursor;
    while (cursor < value.size() && value[cursor] >= '0' &&
           value[cursor] <= '9') {
      ++cursor;
    }
    if (cursor == digitStart || cursor + 2 != value.size() ||
        value[cursor] != '.') {
      return false;
    }
    const auto component = value[cursor + 1];
    return component == 'x' || component == 'y' || component == 'z' ||
           component == 'w';
  };

  try {
    constexpr std::string_view kFetchPrefix = "FetchByID_";
    constexpr std::string_view kBiasSuffix = " + 0.00025f)";
    patchedSource.reserve(sourceSize + 256);
    std::size_t copyCursor = 0;
    std::size_t searchCursor = 0;
    while (searchCursor < sourceText.size()) {
      const auto fetch = sourceText.find(kFetchPrefix, searchCursor);
      if (fetch == std::string_view::npos) {
        break;
      }
      const auto openParenthesis = sourceText.find('(', fetch);
      const auto firstComma = openParenthesis == std::string_view::npos
                                  ? std::string_view::npos
                                  : sourceText.find(',', openParenthesis + 1);
      const auto secondComma = firstComma == std::string_view::npos
                                   ? std::string_view::npos
                                   : sourceText.find(',', firstComma + 1);
      if (secondComma == std::string_view::npos) {
        searchCursor = fetch + kFetchPrefix.size();
        continue;
      }

      auto indexBegin = firstComma + 1;
      while (indexBegin < secondComma && isWhitespace(sourceText[indexBegin])) {
        ++indexBegin;
      }
      auto indexEnd = secondComma;
      while (indexEnd > indexBegin && isWhitespace(sourceText[indexEnd - 1])) {
        --indexEnd;
      }
      const auto index = sourceText.substr(indexBegin, indexEnd - indexBegin);
      if (isGprComponent(index)) {
        // Mirrors Xenia Canary's AC6 workaround before HLSL
        // converts the floating vertex-fetch index to uint.
        patchedSource.append(sourceText.data() + copyCursor,
                             indexBegin - copyCursor);
        patchedSource.push_back('(');
        patchedSource.append(index.data(), index.size());
        patchedSource.append(kBiasSuffix);
        copyCursor = indexEnd;
        ++patchCount;
      }
      searchCursor = secondComma + 1;
    }

    if (patchCount != 0) {
      patchedSource.append(sourceText.data() + copyCursor,
                           sourceText.size() - copyCursor);
    }
    return true;
  } catch (...) {
    patchedSource.clear();
    patchCount = 0;
    return false;
  }
}

bool PatchXenosIndexBufferSemantics(
    const void *const source, const std::size_t sourceSize,
    std::string &patchedSource, std::uint32_t &patchCount,
    const bool patchPrimitiveRestartParity,
    const std::uint32_t restartScanLimit) noexcept {
  patchedSource.clear();
  patchCount = 0;
  if (source == nullptr || sourceSize == 0 ||
      (patchPrimitiveRestartParity &&
       (restartScanLimit == 0 || restartScanLimit > 256))) {
    return false;
  }

  const std::string_view sourceText(static_cast<const char *>(source),
                                    sourceSize);
  if (sourceText.find("xenon_vertex_shader") == std::string_view::npos) {
    return true;
  }

  constexpr std::string_view kIndexAssignment =
      "VID = HostToGuestIndex(InV.vID);";
  constexpr std::string_view kAlreadyPatched = "XeO3GuestIndexUses24Bits";
  constexpr std::string_view kPositionMember = "float4 oP : SV_Position;";
  constexpr std::string_view kVertexEntry =
      "OutputType xenon_vertex_shader(InputType InV)";
  constexpr std::string_view kRootSignature = "[RootSignature(";
  constexpr std::string_view kReturn = "return OutV;";
  constexpr std::string_view kClipMember =
      "\n    float XeO3IndexClip : SV_ClipDistance0;";
  constexpr std::string_view kGenericIndexReplacement =
      "const uint XeO3LocalHostIndex = InV.vID;\n"
      "const uint XeO3GuestIndexRaw = "
      "HostToGuestIndex(XeO3LocalHostIndex);\n"
      "const bool XeO3GuestIndexUses24Bits =\n"
      "    IbDescUseIndexBuf(PackedIbDesc) && "
      "IbDescBits32(PackedIbDesc);\n"
      "const uint XeO3GuestIndex24 = "
      "XeO3GuestIndexRaw & 0x00FFFFFFu;\n"
      "const bool XeO3GuestIndexCut = XeO3GuestIndexUses24Bits &&\n"
      "    XeO3GuestIndex24 == 0x00FFFFFFu;\n"
      "const uint XeO3GuestIndexMasked = XeO3GuestIndexUses24Bits ?\n"
      "    XeO3GuestIndex24 : XeO3GuestIndexRaw;\n"
      "VID = int(XeO3GuestIndexCut ? 0u : XeO3GuestIndexMasked);";
  constexpr std::string_view kRestartIndexReplacement =
      "const uint2 XeO3GuestIndexResolved = "
      "XeO3ResolveGuestIndex(InV.vID);\n"
      "const uint XeO3GuestIndexRaw = XeO3GuestIndexResolved.x;\n"
      "const bool XeO3GuestIndexResetTriangle = "
      "XeO3GuestIndexResolved.y != 0u;\n"
      "const bool XeO3GuestIndexUses24Bits =\n"
      "    IbDescUseIndexBuf(PackedIbDesc) && "
      "IbDescBits32(PackedIbDesc);\n"
      "const uint XeO3GuestIndex24 = "
      "XeO3GuestIndexRaw & 0x00FFFFFFu;\n"
      "const bool XeO3GuestIndexCut = XeO3GuestIndexUses24Bits &&\n"
      "    XeO3GuestIndex24 == 0x00FFFFFFu;\n"
      "const bool XeO3GuestIndexRejected = "
      "XeO3GuestIndexResetTriangle || XeO3GuestIndexCut;\n"
      "const uint XeO3GuestIndexMasked = XeO3GuestIndexUses24Bits ?\n"
      "    XeO3GuestIndex24 : XeO3GuestIndexRaw;\n"
      "VID = int(XeO3GuestIndexRejected ? 0u : "
      "XeO3GuestIndexMasked);";
  constexpr std::string_view kGenericClipAssignment =
      "OutV.XeO3IndexClip = XeO3GuestIndexCut ? -1.0f : 0.0f;\n";
  constexpr std::string_view kRestartClipAssignment =
      "OutV.XeO3IndexClip = XeO3GuestIndexRejected ? -1.0f : 0.0f;\n";
  constexpr std::string_view kRestartHelper = R"XEO3(
uint2 XeO3ResolveGuestIndex(const uint hostIndex)
{
    // SV_VertexID excludes DrawInstanced's StartVertexLocation. It is already
    // local to this draw. The generated shader adds the guest vertexOffset
    // once, after index lookup; subtracting it here corrupts nonzero-base draws.
    // https://microsoft.github.io/hlsl-specs/proposals/0015-extended-command-info/
    const uint XeO3LocalHostIndex = hostIndex;
    const uint XeO3PrimitiveType = IbDescPrimType(PackedIbDesc);
    const bool XeO3UsesIndexBuffer = IbDescUseIndexBuf(PackedIbDesc);
    const bool XeO3UsesRestart = IbDescUseResetIdx(PackedIbDesc);
    if (!XeO3UsesIndexBuffer || !XeO3UsesRestart ||
        (XeO3PrimitiveType != 5u && XeO3PrimitiveType != 6u))
    {
        return uint2(HostToGuestIndex(XeO3LocalHostIndex), 0u);
    }

    const uint XeO3PrimitiveIndex = XeO3LocalHostIndex / 3u;
    const uint XeO3VertexInPrimitive = XeO3LocalHostIndex % 3u;
    const uint XeO3ResetMask = IbDescBits32(PackedIbDesc) ?
        0x00FFFFFFu : 0x0000FFFFu;
    const uint XeO3ComparableReset = ResetIndex & XeO3ResetMask;
    uint XeO3SegmentStart = 0u;
    uint XeO3RestartScanCursor = XeO3PrimitiveIndex;
    uint XeO3RestartScanSteps = 0u;
    bool XeO3RestartStateKnown = false;
    [loop]
    while (XeO3RestartScanCursor > 0u && XeO3RestartScanSteps < 64u)
    {
        const uint XeO3PreviousPosition = XeO3RestartScanCursor - 1u;
        const uint XeO3PreviousIndex =
            FetchIndexBuffer(XeO3PreviousPosition) & XeO3ResetMask;
        if (XeO3PreviousIndex == XeO3ComparableReset)
        {
            XeO3SegmentStart = XeO3RestartScanCursor;
            XeO3RestartStateKnown = true;
            break;
        }
        XeO3RestartScanCursor = XeO3PreviousPosition;
        ++XeO3RestartScanSteps;
    }
    if (XeO3RestartScanCursor == 0u)
    {
        XeO3RestartStateKnown = true;
    }
    if (!XeO3RestartStateKnown)
    {
        return uint2(HostToGuestIndex(XeO3LocalHostIndex), 0u);
    }

    uint XeO3IndexPosition0;
    uint XeO3IndexPosition1;
    uint XeO3IndexPosition2;
    if (XeO3PrimitiveType == 5u)
    {
        XeO3IndexPosition0 = XeO3SegmentStart;
        XeO3IndexPosition1 = XeO3PrimitiveIndex + 1u;
        XeO3IndexPosition2 = XeO3PrimitiveIndex + 2u;
    }
    else
    {
        const bool XeO3OddTriangle =
            ((XeO3PrimitiveIndex - XeO3SegmentStart) & 1u) != 0u;
        XeO3IndexPosition0 = XeO3PrimitiveIndex;
        XeO3IndexPosition1 = XeO3PrimitiveIndex +
            (XeO3OddTriangle ? 2u : 1u);
        XeO3IndexPosition2 = XeO3PrimitiveIndex +
            (XeO3OddTriangle ? 1u : 2u);
    }

    const uint XeO3Index0 = FetchIndexBuffer(XeO3IndexPosition0);
    const uint XeO3Index1 = FetchIndexBuffer(XeO3IndexPosition1);
    const uint XeO3Index2 = FetchIndexBuffer(XeO3IndexPosition2);
    const uint XeO3WindowStartIndex =
        FetchIndexBuffer(XeO3PrimitiveIndex);
    const bool XeO3ResetTriangle =
        ((XeO3WindowStartIndex & XeO3ResetMask) == XeO3ComparableReset) ||
        ((XeO3Index0 & XeO3ResetMask) == XeO3ComparableReset) ||
        ((XeO3Index1 & XeO3ResetMask) == XeO3ComparableReset) ||
        ((XeO3Index2 & XeO3ResetMask) == XeO3ComparableReset);
    if (XeO3ResetTriangle)
    {
        return uint2(0u, 1u);
    }
    return uint2(
        XeO3VertexInPrimitive == 0u ? XeO3Index0 :
        (XeO3VertexInPrimitive == 1u ? XeO3Index1 : XeO3Index2),
        0u);
}

)XEO3";

  if (sourceText.find(kAlreadyPatched) != std::string_view::npos ||
      sourceText.find(kIndexAssignment) == std::string_view::npos) {
    return true;
  }

  try {
    patchedSource.assign(sourceText.data(), sourceText.size());

    const auto outputType = patchedSource.find("struct OutputType");
    const auto outputBegin = patchedSource.find('{', outputType);
    const auto outputEnd = patchedSource.find("};", outputBegin);
    const auto positionMember = patchedSource.find(kPositionMember, outputBegin);
    if (outputType == std::string::npos || outputBegin == std::string::npos ||
        outputEnd == std::string::npos || positionMember == std::string::npos ||
        positionMember >= outputEnd) {
      patchedSource.clear();
      return false;
    }
    // Inserting after SV_Position shifts later semantics (notably PSIZE) to
    // different registers than XeO3's unchanged GS input signature. Append
    // after the last existing output member instead, preserving stage linkage.
    const auto lastOutputMember = patchedSource.rfind(';', outputEnd);
    if (lastOutputMember == std::string::npos || lastOutputMember < outputBegin) {
      patchedSource.clear();
      return false;
    }
    patchedSource.insert(lastOutputMember + 1, kClipMember);

    if (patchPrimitiveRestartParity) {
      const auto vertexEntry = patchedSource.find(kVertexEntry);
      if (vertexEntry == std::string::npos) {
        patchedSource.clear();
        return false;
      }
      const auto rootSignature =
          patchedSource.rfind(kRootSignature, vertexEntry);
      const auto helperInsertion =
          rootSignature == std::string::npos ? vertexEntry : rootSignature;
      if (restartScanLimit == 64) {
        patchedSource.insert(helperInsertion, kRestartHelper);
      } else {
        // The captured aircraft shadow mesh contains 105-index segments.
        // Keep the larger budget local to its fingerprinted shader.
        std::string helper(kRestartHelper);
        constexpr std::string_view kSearchBound = "XeO3RestartScanSteps < 64u";
        const auto bound = helper.find(kSearchBound);
        if (bound == std::string::npos) {
          patchedSource.clear();
          return false;
        }
        helper.replace(bound, kSearchBound.size(),
                       "XeO3RestartScanSteps < " +
                           std::to_string(restartScanLimit) + "u");
        patchedSource.insert(helperInsertion, helper);
      }
    }

    const auto indexReplacement = patchPrimitiveRestartParity
                                      ? kRestartIndexReplacement
                                      : kGenericIndexReplacement;
    const auto clipAssignment = patchPrimitiveRestartParity
                                    ? kRestartClipAssignment
                                    : kGenericClipAssignment;

    std::size_t searchCursor = 0;
    while (searchCursor < patchedSource.size()) {
      const auto assignment =
          patchedSource.find(kIndexAssignment, searchCursor);
      if (assignment == std::string::npos) {
        break;
      }
      patchedSource.replace(assignment, kIndexAssignment.size(),
                            indexReplacement);
      searchCursor = assignment + indexReplacement.size();
      ++patchCount;
    }

    const auto returnStatement = patchedSource.find(kReturn, searchCursor);
    if (patchCount == 0 || returnStatement == std::string::npos) {
      patchedSource.clear();
      patchCount = 0;
      return false;
    }
    patchedSource.insert(returnStatement, clipAssignment);
    return true;
  } catch (...) {
    patchedSource.clear();
    patchCount = 0;
    return false;
  }
}

bool HasExpectedTightAlignmentGate(const std::uint8_t *bytes,
                                   const std::size_t byteCount) noexcept {
  return bytes != nullptr && byteCount >= kExpectedTightAlignmentGate.size() &&
         std::equal(kExpectedTightAlignmentGate.begin(),
                    kExpectedTightAlignmentGate.end(), bytes);
}

bool HasExpectedPlacedResourceGate(const std::uint8_t *bytes,
                                   const std::size_t byteCount) noexcept {
  return bytes != nullptr && byteCount >= kExpectedPlacedResourceGate.size() &&
         std::equal(kExpectedPlacedResourceGate.begin(),
                    kExpectedPlacedResourceGate.end(), bytes);
}

bool ShouldRetryInvalidModernBufferAsLegacy(
    const std::uint32_t result, const bool hasResource,
    const std::uint32_t dimension, const std::uint32_t initialLayout,
    const std::uint32_t castableFormatCount) noexcept {
  constexpr std::uint32_t kDxgiErrorInvalidCall = 0x887A0001;
  constexpr std::uint32_t kBufferDimension = 1;
  constexpr std::uint32_t kUndefinedLayout = 0xFFFFFFFF;
  return result == kDxgiErrorInvalidCall && !hasResource &&
         dimension == kBufferDimension && initialLayout == kUndefinedLayout &&
         castableFormatCount == 0;
}

bool DoesPlacedResourceOverflowHeap(
    const std::uint64_t heapSize, const std::uint64_t heapOffset,
    const std::uint64_t allocationSize) noexcept {
  if (heapSize == 0 || allocationSize == 0 ||
      allocationSize == (std::numeric_limits<std::uint64_t>::max)()) {
    return false;
  }
  return heapOffset > heapSize || allocationSize > heapSize - heapOffset;
}

bool ShouldFallbackPlacedResourceToCommitted(
    const std::uint32_t result, const bool hasResource,
    const std::uint64_t heapSize, const std::uint64_t heapOffset,
    const std::uint64_t allocationSize) noexcept {
  constexpr std::uint32_t kFailedResultMask = 0x80000000;
  if ((result & kFailedResultMask) == 0 || hasResource) {
    return false;
  }
  return DoesPlacedResourceOverflowHeap(heapSize, heapOffset, allocationSize);
}

std::uint32_t
GetCommittedResourceHeapFlags(const std::uint32_t sourceHeapFlags) noexcept {
  constexpr std::uint32_t kExplicitHeapResourceCategoryFlags =
      static_cast<std::uint32_t>(D3D12_HEAP_FLAG_DENY_BUFFERS) |
      static_cast<std::uint32_t>(D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES) |
      static_cast<std::uint32_t>(D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES);
  return sourceHeapFlags & ~kExplicitHeapResourceCategoryFlags;
}
} // namespace detail

bool InstallPinnedVgpuPatch(const HashFileSha256 hashFile) noexcept {
  if (g_patchTarget != nullptr) {
    return true;
  }

  const auto module = GetModuleHandleW(L"VGPUDX12.dll");
  if (!ValidatePinnedModule(module, hashFile)) {
    return false;
  }

  auto *const moduleBase = reinterpret_cast<std::uint8_t *>(module);
  auto *const target = moduleBase + kFetchTableRva;
  auto *const shaderCompileTarget = moduleBase + kShaderCompileRva;
  auto *const xenosTranslateTarget = moduleBase + kXenosTranslateRva;
  auto *const textureTransferTarget = moduleBase + kTextureTransferRva;
  auto *const structuredTextureTransferTarget =
      moduleBase + kStructuredTextureTransferRva;
  auto *const constantUploadTarget = moduleBase + kConstantUploadRva;
  auto *const nullPipelineStateTarget = moduleBase + kNullPipelineStateGuardRva;
  auto *const samplerAddressModeTarget = reinterpret_cast<std::uint32_t *>(
      moduleBase + kSamplerAddressModeTableRva);
  auto *const tightAlignmentTarget = moduleBase + kTightAlignmentGateRva;
  auto *const placedResourceTarget = moduleBase + kPlacedResourceGateRva;
  auto *const createHeapCallTarget = moduleBase + kCreateHeapCallRva;
  auto *const modernPlacedResourceCallTarget =
      moduleBase + kModernPlacedResourceCallRva;
  auto *const legacyPlacedResourceCallTarget =
      moduleBase + kLegacyPlacedResourceCallRva;
  auto *const secondaryModernPlacedResourceCallTarget =
      moduleBase + kSecondaryModernPlacedResourceCallRva;
  auto *const coldLegacyPlacedResourceSequenceTarget =
      moduleBase + kColdLegacyPlacedResourceSequenceRva;
  if (!detail::HasExpectedFetchTablePrologue(target, kFetchTableDetourSize)) {
    return FailInstall(PatchStatus::PrologueMismatch);
  }
  if (!detail::HasExpectedShaderCompilePrologue(shaderCompileTarget,
                                                kShaderCompileDetourSize)) {
    return FailInstall(PatchStatus::ShaderCompilePrologueMismatch);
  }
  if (!detail::HasExpectedXenosTranslatePrologue(xenosTranslateTarget,
                                                 kXenosTranslateDetourSize)) {
    BridgeVgpuXenosTranslateHookFailure =
        static_cast<std::uint32_t>(PatchStatus::XenosTranslatePrologueMismatch);
    return FailInstall(PatchStatus::XenosTranslatePrologueMismatch);
  }
  if (!detail::HasExpectedTextureTransferPrologue(textureTransferTarget,
                                                  kTextureTransferDetourSize)) {
    return FailInstall(PatchStatus::TextureTransferPrologueMismatch);
  }
  if (!detail::HasExpectedStructuredTextureTransferPrologue(
          structuredTextureTransferTarget,
          kStructuredTextureTransferDetourSize)) {
    return FailInstall(PatchStatus::StructuredTextureTransferPrologueMismatch);
  }
  if (!detail::HasExpectedConstantUploadPrologue(constantUploadTarget,
                                                 kConstantUploadDetourSize)) {
    return FailInstall(PatchStatus::ConstantUploadPrologueMismatch);
  }
  if (!detail::HasExpectedNullPipelineStateSequence(
          nullPipelineStateTarget, kNullPipelineStateDetourSize)) {
    BridgeVgpuNullPipelineStateGuardFailure = static_cast<std::uint32_t>(
        PatchStatus::NullPipelineStateSequenceMismatch);
    return FailInstall(PatchStatus::NullPipelineStateSequenceMismatch);
  }
  if (!detail::HasExpectedSamplerAddressModeTable(
          samplerAddressModeTarget, kXenosSamplerAddressModeCount)) {
    return FailInstall(PatchStatus::SamplerAddressModeTableMismatch);
  }
  if (!detail::HasExpectedTightAlignmentGate(tightAlignmentTarget,
                                             kTightAlignmentGateSize)) {
    return FailInstall(PatchStatus::TightAlignmentGateMismatch);
  }
  if (!detail::HasExpectedPlacedResourceGate(placedResourceTarget,
                                             kPlacedResourceGateSize)) {
    return FailInstall(PatchStatus::PlacedResourceGateMismatch);
  }
  if (!std::equal(kExpectedCreateHeapCall.begin(),
                  kExpectedCreateHeapCall.end(), createHeapCallTarget)) {
    return FailInstall(PatchStatus::CreateHeapCallMismatch);
  }
  if (!std::equal(kExpectedModernPlacedResourceCall.begin(),
                  kExpectedModernPlacedResourceCall.end(),
                  modernPlacedResourceCallTarget)) {
    return FailInstall(PatchStatus::ModernPlacedResourceCallMismatch);
  }
  if (!std::equal(kExpectedLegacyPlacedResourceCall.begin(),
                  kExpectedLegacyPlacedResourceCall.end(),
                  legacyPlacedResourceCallTarget)) {
    return FailInstall(PatchStatus::LegacyPlacedResourceCallMismatch);
  }
  if (!std::equal(kExpectedSecondaryModernPlacedResourceCall.begin(),
                  kExpectedSecondaryModernPlacedResourceCall.end(),
                  secondaryModernPlacedResourceCallTarget)) {
    return FailInstall(PatchStatus::SecondaryModernPlacedResourceCallMismatch);
  }
  if (!std::equal(kExpectedColdLegacyPlacedResourceSequence.begin(),
                  kExpectedColdLegacyPlacedResourceSequence.end(),
                  coldLegacyPlacedResourceSequenceTarget)) {
    return FailInstall(PatchStatus::ColdLegacyPlacedResourceCallMismatch);
  }

  VirtualAllocationOwner callRelayOwner(
      AllocateCallRelayRegionNear(moduleBase, kCallRelayRegionSize));
  auto *const callRelayRegion =
      static_cast<std::uint8_t *>(callRelayOwner.get());
  if (callRelayRegion == nullptr) {
    return FailInstall(PatchStatus::CallRelayAllocationFailure);
  }

  const std::array<const void *, kCallRelayCount> callRelayTargets{
      reinterpret_cast<const void *>(&VgpuCreateHeapHook),
      reinterpret_cast<const void *>(&VgpuCreatePlacedResource2Hook),
      reinterpret_cast<const void *>(&VgpuCreatePlacedResource2SecondaryHook),
      reinterpret_cast<const void *>(&VgpuCreatePlacedResourceLegacyColdThunk),
      reinterpret_cast<const void *>(&VgpuCreatePlacedResourceHook),
  };
  for (std::size_t index = 0; index < callRelayTargets.size(); ++index) {
    const auto relay = detail::EncodeAbsoluteCallRelay(callRelayTargets[index]);
    std::memcpy(callRelayRegion + index * kCallRelaySize, relay.data(),
                relay.size());
  }
  DWORD callRelayOldProtection = 0;
  if (!VirtualProtect(callRelayRegion, kCallRelayRegionSize, PAGE_EXECUTE_READ,
                      &callRelayOldProtection)) {
    return FailInstall(PatchStatus::CallRelayProtectionFailure);
  }
  FlushInstructionCache(GetCurrentProcess(), callRelayRegion,
                        kCallRelayRegionSize);

  const auto callRelay = [callRelayRegion](const std::size_t index) noexcept {
    return callRelayRegion + index * kCallRelaySize;
  };
  std::array<std::uint8_t, kCreateHeapCallSize> createHeapCall{};
  if (!detail::EncodeRelativeCall(createHeapCallTarget, callRelay(0),
                                  createHeapCall)) {
    return FailInstall(PatchStatus::CallRelayOutOfRange);
  }
  std::array<std::uint8_t, kPlacedResourceCallSize> modernPlacedResourceCall{};
  if (!detail::EncodeRelativeCall(modernPlacedResourceCallTarget, callRelay(1),
                                  modernPlacedResourceCall)) {
    return FailInstall(PatchStatus::CallRelayOutOfRange);
  }
  std::array<std::uint8_t, kCreateHeapCallSize>
      secondaryModernPlacedResourceCall{};
  if (!detail::EncodeRelativeCall(secondaryModernPlacedResourceCallTarget,
                                  callRelay(2),
                                  secondaryModernPlacedResourceCall)) {
    return FailInstall(PatchStatus::CallRelayOutOfRange);
  }
  std::array<std::uint8_t, kCreateHeapCallSize>
      coldLegacyPlacedResourceSequence{};
  if (!detail::EncodeRelativeCall(coldLegacyPlacedResourceSequenceTarget,
                                  callRelay(3),
                                  coldLegacyPlacedResourceSequence)) {
    return FailInstall(PatchStatus::CallRelayOutOfRange);
  }
  std::array<std::uint8_t, kPlacedResourceCallSize> legacyPlacedResourceCall{};
  if (!detail::EncodeRelativeCall(legacyPlacedResourceCallTarget, callRelay(4),
                                  legacyPlacedResourceCall)) {
    return FailInstall(PatchStatus::CallRelayOutOfRange);
  }
  constexpr std::size_t kTrampolineCapacity = 64;
  auto *const trampoline = static_cast<std::uint8_t *>(VirtualAlloc(
      nullptr, kTrampolineCapacity, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  if (trampoline == nullptr) {
    return FailInstall(PatchStatus::TrampolineAllocationFailure);
  }

  std::memcpy(trampoline, kExpectedPrologue.data(), kExpectedPrologue.size());
  const auto returnJump =
      detail::EncodeAbsoluteJump(target + kFetchTableDetourSize);
  std::memcpy(trampoline + kFetchTableDetourSize, returnJump.data(),
              returnJump.size());

  DWORD trampolineOldProtection = 0;
  if (!VirtualProtect(trampoline, kTrampolineCapacity, PAGE_EXECUTE_READ,
                      &trampolineOldProtection)) {
    VirtualFree(trampoline, 0, MEM_RELEASE);
    return FailInstall(PatchStatus::TrampolineProtectionFailure);
  }
  FlushInstructionCache(GetCurrentProcess(), trampoline,
                        kFetchTableDetourSize + returnJump.size());

  if (!WriteInstructionGate(
          tightAlignmentTarget, kDisabledTightAlignmentGate.data(),
          kDisabledTightAlignmentGate.size(),
          PatchStatus::TightAlignmentProtectionFailure,
          PatchStatus::TightAlignmentProtectionRestoreFailure)) {
    VirtualFree(trampoline, 0, MEM_RELEASE);
    EmitPatchEvent("install_failure",
                   static_cast<std::uint32_t>(BridgeVgpuPatchStatus));
    return false;
  }
  if (!WriteInstructionGate(
          createHeapCallTarget, createHeapCall.data(), createHeapCall.size(),
          PatchStatus::CreateHeapCallProtectionFailure,
          PatchStatus::CreateHeapCallProtectionRestoreFailure)) {
    WriteInstructionGate(tightAlignmentTarget,
                         kExpectedTightAlignmentGate.data(),
                         kExpectedTightAlignmentGate.size(),
                         PatchStatus::TightAlignmentProtectionFailure,
                         PatchStatus::TightAlignmentProtectionRestoreFailure);
    VirtualFree(trampoline, 0, MEM_RELEASE);
    EmitPatchEvent("install_failure",
                   static_cast<std::uint32_t>(BridgeVgpuPatchStatus));
    return false;
  }
  if (!WriteInstructionGate(
          modernPlacedResourceCallTarget, modernPlacedResourceCall.data(),
          modernPlacedResourceCall.size(),
          PatchStatus::PlacedResourceCallProtectionFailure,
          PatchStatus::PlacedResourceCallProtectionRestoreFailure)) {
    WriteInstructionGate(createHeapCallTarget, kExpectedCreateHeapCall.data(),
                         kExpectedCreateHeapCall.size(),
                         PatchStatus::CreateHeapCallProtectionFailure,
                         PatchStatus::CreateHeapCallProtectionRestoreFailure);
    WriteInstructionGate(tightAlignmentTarget,
                         kExpectedTightAlignmentGate.data(),
                         kExpectedTightAlignmentGate.size(),
                         PatchStatus::TightAlignmentProtectionFailure,
                         PatchStatus::TightAlignmentProtectionRestoreFailure);
    VirtualFree(trampoline, 0, MEM_RELEASE);
    EmitPatchEvent("install_failure",
                   static_cast<std::uint32_t>(BridgeVgpuPatchStatus));
    return false;
  }
  if (!WriteInstructionGate(
          legacyPlacedResourceCallTarget, legacyPlacedResourceCall.data(),
          legacyPlacedResourceCall.size(),
          PatchStatus::PlacedResourceCallProtectionFailure,
          PatchStatus::PlacedResourceCallProtectionRestoreFailure)) {
    WriteInstructionGate(
        modernPlacedResourceCallTarget,
        kExpectedModernPlacedResourceCall.data(),
        kExpectedModernPlacedResourceCall.size(),
        PatchStatus::PlacedResourceCallProtectionFailure,
        PatchStatus::PlacedResourceCallProtectionRestoreFailure);
    WriteInstructionGate(createHeapCallTarget, kExpectedCreateHeapCall.data(),
                         kExpectedCreateHeapCall.size(),
                         PatchStatus::CreateHeapCallProtectionFailure,
                         PatchStatus::CreateHeapCallProtectionRestoreFailure);
    WriteInstructionGate(tightAlignmentTarget,
                         kExpectedTightAlignmentGate.data(),
                         kExpectedTightAlignmentGate.size(),
                         PatchStatus::TightAlignmentProtectionFailure,
                         PatchStatus::TightAlignmentProtectionRestoreFailure);
    VirtualFree(trampoline, 0, MEM_RELEASE);
    EmitPatchEvent("install_failure",
                   static_cast<std::uint32_t>(BridgeVgpuPatchStatus));
    return false;
  }
  if (!WriteInstructionGate(
          secondaryModernPlacedResourceCallTarget,
          secondaryModernPlacedResourceCall.data(),
          secondaryModernPlacedResourceCall.size(),
          PatchStatus::PlacedResourceCallProtectionFailure,
          PatchStatus::PlacedResourceCallProtectionRestoreFailure)) {
    WriteInstructionGate(
        legacyPlacedResourceCallTarget,
        kExpectedLegacyPlacedResourceCall.data(),
        kExpectedLegacyPlacedResourceCall.size(),
        PatchStatus::PlacedResourceCallProtectionFailure,
        PatchStatus::PlacedResourceCallProtectionRestoreFailure);
    WriteInstructionGate(
        modernPlacedResourceCallTarget,
        kExpectedModernPlacedResourceCall.data(),
        kExpectedModernPlacedResourceCall.size(),
        PatchStatus::PlacedResourceCallProtectionFailure,
        PatchStatus::PlacedResourceCallProtectionRestoreFailure);
    WriteInstructionGate(createHeapCallTarget, kExpectedCreateHeapCall.data(),
                         kExpectedCreateHeapCall.size(),
                         PatchStatus::CreateHeapCallProtectionFailure,
                         PatchStatus::CreateHeapCallProtectionRestoreFailure);
    WriteInstructionGate(tightAlignmentTarget,
                         kExpectedTightAlignmentGate.data(),
                         kExpectedTightAlignmentGate.size(),
                         PatchStatus::TightAlignmentProtectionFailure,
                         PatchStatus::TightAlignmentProtectionRestoreFailure);
    VirtualFree(trampoline, 0, MEM_RELEASE);
    EmitPatchEvent("install_failure",
                   static_cast<std::uint32_t>(BridgeVgpuPatchStatus));
    return false;
  }
  if (!WriteInstructionGate(
          coldLegacyPlacedResourceSequenceTarget,
          coldLegacyPlacedResourceSequence.data(),
          coldLegacyPlacedResourceSequence.size(),
          PatchStatus::PlacedResourceCallProtectionFailure,
          PatchStatus::PlacedResourceCallProtectionRestoreFailure)) {
    WriteInstructionGate(
        secondaryModernPlacedResourceCallTarget,
        kExpectedSecondaryModernPlacedResourceCall.data(),
        kExpectedSecondaryModernPlacedResourceCall.size(),
        PatchStatus::PlacedResourceCallProtectionFailure,
        PatchStatus::PlacedResourceCallProtectionRestoreFailure);
    WriteInstructionGate(
        legacyPlacedResourceCallTarget,
        kExpectedLegacyPlacedResourceCall.data(),
        kExpectedLegacyPlacedResourceCall.size(),
        PatchStatus::PlacedResourceCallProtectionFailure,
        PatchStatus::PlacedResourceCallProtectionRestoreFailure);
    WriteInstructionGate(
        modernPlacedResourceCallTarget,
        kExpectedModernPlacedResourceCall.data(),
        kExpectedModernPlacedResourceCall.size(),
        PatchStatus::PlacedResourceCallProtectionFailure,
        PatchStatus::PlacedResourceCallProtectionRestoreFailure);
    WriteInstructionGate(createHeapCallTarget, kExpectedCreateHeapCall.data(),
                         kExpectedCreateHeapCall.size(),
                         PatchStatus::CreateHeapCallProtectionFailure,
                         PatchStatus::CreateHeapCallProtectionRestoreFailure);
    WriteInstructionGate(tightAlignmentTarget,
                         kExpectedTightAlignmentGate.data(),
                         kExpectedTightAlignmentGate.size(),
                         PatchStatus::TightAlignmentProtectionFailure,
                         PatchStatus::TightAlignmentProtectionRestoreFailure);
    VirtualFree(trampoline, 0, MEM_RELEASE);
    EmitPatchEvent("install_failure",
                   static_cast<std::uint32_t>(BridgeVgpuPatchStatus));
    return false;
  }
  const auto detour = detail::EncodeAbsoluteJump(
      reinterpret_cast<const void *>(&VgpuFetchTableHook));
  g_moduleBase = moduleBase;
  g_nativeFetchTable = reinterpret_cast<NativeFetchTable>(trampoline);
  g_trampoline = trampoline;
  g_installedDetour = detour;

  DWORD targetOldProtection = 0;
  if (!VirtualProtect(target, kFetchTableDetourSize, PAGE_EXECUTE_READWRITE,
                      &targetOldProtection)) {
    g_moduleBase = nullptr;
    g_nativeFetchTable = nullptr;
    g_trampoline = nullptr;
    VirtualFree(trampoline, 0, MEM_RELEASE);
    WriteInstructionGate(
        coldLegacyPlacedResourceSequenceTarget,
        kExpectedColdLegacyPlacedResourceSequence.data(),
        kExpectedColdLegacyPlacedResourceSequence.size(),
        PatchStatus::PlacedResourceCallProtectionFailure,
        PatchStatus::PlacedResourceCallProtectionRestoreFailure);
    WriteInstructionGate(
        secondaryModernPlacedResourceCallTarget,
        kExpectedSecondaryModernPlacedResourceCall.data(),
        kExpectedSecondaryModernPlacedResourceCall.size(),
        PatchStatus::PlacedResourceCallProtectionFailure,
        PatchStatus::PlacedResourceCallProtectionRestoreFailure);
    WriteInstructionGate(
        legacyPlacedResourceCallTarget,
        kExpectedLegacyPlacedResourceCall.data(),
        kExpectedLegacyPlacedResourceCall.size(),
        PatchStatus::PlacedResourceCallProtectionFailure,
        PatchStatus::PlacedResourceCallProtectionRestoreFailure);
    WriteInstructionGate(
        modernPlacedResourceCallTarget,
        kExpectedModernPlacedResourceCall.data(),
        kExpectedModernPlacedResourceCall.size(),
        PatchStatus::PlacedResourceCallProtectionFailure,
        PatchStatus::PlacedResourceCallProtectionRestoreFailure);
    WriteInstructionGate(createHeapCallTarget, kExpectedCreateHeapCall.data(),
                         kExpectedCreateHeapCall.size(),
                         PatchStatus::CreateHeapCallProtectionFailure,
                         PatchStatus::CreateHeapCallProtectionRestoreFailure);
    WriteInstructionGate(tightAlignmentTarget,
                         kExpectedTightAlignmentGate.data(),
                         kExpectedTightAlignmentGate.size(),
                         PatchStatus::TightAlignmentProtectionFailure,
                         PatchStatus::TightAlignmentProtectionRestoreFailure);
    return FailInstall(PatchStatus::TargetProtectionFailure);
  }

  std::memcpy(target, detour.data(), detour.size());
  FlushInstructionCache(GetCurrentProcess(), target, detour.size());

  DWORD ignoredProtection = 0;
  if (!VirtualProtect(target, kFetchTableDetourSize, targetOldProtection,
                      &ignoredProtection)) {
    std::memcpy(target, kExpectedPrologue.data(), kExpectedPrologue.size());
    FlushInstructionCache(GetCurrentProcess(), target,
                          kExpectedPrologue.size());
    VirtualProtect(target, kFetchTableDetourSize, targetOldProtection,
                   &ignoredProtection);
    g_moduleBase = nullptr;
    g_nativeFetchTable = nullptr;
    g_trampoline = nullptr;
    VirtualFree(trampoline, 0, MEM_RELEASE);
    WriteInstructionGate(
        coldLegacyPlacedResourceSequenceTarget,
        kExpectedColdLegacyPlacedResourceSequence.data(),
        kExpectedColdLegacyPlacedResourceSequence.size(),
        PatchStatus::PlacedResourceCallProtectionFailure,
        PatchStatus::PlacedResourceCallProtectionRestoreFailure);
    WriteInstructionGate(
        secondaryModernPlacedResourceCallTarget,
        kExpectedSecondaryModernPlacedResourceCall.data(),
        kExpectedSecondaryModernPlacedResourceCall.size(),
        PatchStatus::PlacedResourceCallProtectionFailure,
        PatchStatus::PlacedResourceCallProtectionRestoreFailure);
    WriteInstructionGate(
        legacyPlacedResourceCallTarget,
        kExpectedLegacyPlacedResourceCall.data(),
        kExpectedLegacyPlacedResourceCall.size(),
        PatchStatus::PlacedResourceCallProtectionFailure,
        PatchStatus::PlacedResourceCallProtectionRestoreFailure);
    WriteInstructionGate(
        modernPlacedResourceCallTarget,
        kExpectedModernPlacedResourceCall.data(),
        kExpectedModernPlacedResourceCall.size(),
        PatchStatus::PlacedResourceCallProtectionFailure,
        PatchStatus::PlacedResourceCallProtectionRestoreFailure);
    WriteInstructionGate(createHeapCallTarget, kExpectedCreateHeapCall.data(),
                         kExpectedCreateHeapCall.size(),
                         PatchStatus::CreateHeapCallProtectionFailure,
                         PatchStatus::CreateHeapCallProtectionRestoreFailure);
    WriteInstructionGate(tightAlignmentTarget,
                         kExpectedTightAlignmentGate.data(),
                         kExpectedTightAlignmentGate.size(),
                         PatchStatus::TightAlignmentProtectionFailure,
                         PatchStatus::TightAlignmentProtectionRestoreFailure);
    return FailInstall(PatchStatus::TargetProtectionRestoreFailure);
  }

  g_patchTarget = target;
  g_tightAlignmentTarget = tightAlignmentTarget;
  g_createHeapCallTarget = createHeapCallTarget;
  g_modernPlacedResourceCallTarget = modernPlacedResourceCallTarget;
  g_legacyPlacedResourceCallTarget = legacyPlacedResourceCallTarget;
  g_secondaryModernPlacedResourceCallTarget =
      secondaryModernPlacedResourceCallTarget;
  g_coldLegacyPlacedResourceSequenceTarget =
      coldLegacyPlacedResourceSequenceTarget;
  g_installedCreateHeapCall = createHeapCall;
  g_installedModernPlacedResourceCall = modernPlacedResourceCall;
  g_installedLegacyPlacedResourceCall = legacyPlacedResourceCall;
  g_installedSecondaryModernPlacedResourceCall =
      secondaryModernPlacedResourceCall;
  g_installedColdLegacyPlacedResourceSequence =
      coldLegacyPlacedResourceSequence;
  g_extendedFetchCount.store(0, std::memory_order_relaxed);
  g_zeroedHeapCount.store(0, std::memory_order_relaxed);
  g_discardedResourceCount.store(0, std::memory_order_relaxed);
  g_placedResourceCallCount.store(0, std::memory_order_relaxed);
  g_legacyFallbackCount.store(0, std::memory_order_relaxed);
  g_committedOverflowFallbackCount.store(0, std::memory_order_relaxed);
  g_shaderCompileCount.store(0, std::memory_order_relaxed);
  g_xenosTranslateCount.store(0, std::memory_order_relaxed);
  g_xenosUcodeCaptureCount.store(0, std::memory_order_relaxed);
  g_vertexShaderCompileCount.store(0, std::memory_order_relaxed);
  g_groundFixShaderCount.store(0, std::memory_order_relaxed);
  g_groundFixFetchCount.store(0, std::memory_order_relaxed);
  g_indexFixShaderCount.store(0, std::memory_order_relaxed);
  g_indexFixSiteCount.store(0, std::memory_order_relaxed);
  g_aircraftRestartShaderCount.store(0, std::memory_order_relaxed);
  g_shadowRestartShaderCount.store(0, std::memory_order_relaxed);
  g_reciprocalFixShaderCount.store(0, std::memory_order_relaxed);
  g_reciprocalFixInstructionCount.store(0, std::memory_order_relaxed);
  g_waveBallotFingerprintMatchCount.store(0, std::memory_order_relaxed);
  g_waveBallotFixShaderCount.store(0, std::memory_order_relaxed);
  g_waveBallotFixSiteCount.store(0, std::memory_order_relaxed);
  g_vposFixShaderCount.store(0, std::memory_order_relaxed);
  g_vposFixSiteCount.store(0, std::memory_order_relaxed);
  g_exposureFingerprintMatchCount.store(0, std::memory_order_relaxed);
  g_exposureFixShaderCount.store(0, std::memory_order_relaxed);
  g_exposureFixSiteCount.store(0, std::memory_order_relaxed);
  g_textureTransferCallCount.store(0, std::memory_order_relaxed);
  g_constantUploadCallCount.store(0, std::memory_order_relaxed);
  g_constantUpload128Count.store(0, std::memory_order_relaxed);
  g_constantUploadContextFailureCount.store(0, std::memory_order_relaxed);
  g_transfer341WidthCandidateCount.store(0, std::memory_order_relaxed);
  g_transfer341WidthSignatureMatchCount.store(0, std::memory_order_relaxed);
  g_transfer341WidthPatchCount.store(0, std::memory_order_relaxed);
  g_transfer341WidthFailureCount.store(0, std::memory_order_relaxed);
  g_transfer341DescriptorCount.store(0, std::memory_order_relaxed);
  g_transfer341BindCount.store(0, std::memory_order_relaxed);
  g_transfer341PipelineBindCount.store(0, std::memory_order_relaxed);
  g_transfer341CachedPsoQueryCount.store(0, std::memory_order_relaxed);
  g_transfer341CachedPsoCacheHitCount.store(0, std::memory_order_relaxed);
  g_transfer341CachedPsoMatchCount.store(0, std::memory_order_relaxed);
  g_transfer341ReplacementCreateCount.store(0, std::memory_order_relaxed);
  g_transfer341ReplacementSubstitutionCount.store(0, std::memory_order_relaxed);
  g_transfer341DescriptorMissCount.store(0, std::memory_order_relaxed);
  g_transfer341GpuAddressMissCount.store(0, std::memory_order_relaxed);
  g_transfer341ArenaCandidateCount.store(0, std::memory_order_relaxed);
  g_transfer341CommittedArenaCandidateCount.store(0, std::memory_order_relaxed);
  g_transfer341UploadContextHitCount.store(0, std::memory_order_relaxed);
  g_edramConstantCandidateCount.store(0, std::memory_order_relaxed);
  g_edramLoadConstantCount.store(0, std::memory_order_relaxed);
  g_edramScaleConstantCount.store(0, std::memory_order_relaxed);
  g_edramConstantSnapshotSequence.store(0, std::memory_order_relaxed);
  g_edramConstantSnapshotWriter.clear(std::memory_order_relaxed);
  g_textureEndian2CallCount.store(0, std::memory_order_relaxed);
  g_textureEndianFixCount.store(0, std::memory_order_relaxed);
  g_texturePreviewEndianFixCount.store(0, std::memory_order_relaxed);
  g_textureEndianSignatureMatchCount.store(0, std::memory_order_relaxed);
  g_pipelineStateCreateCount.store(0, std::memory_order_relaxed);
  g_pipelineStateCreateFailureCount.store(0, std::memory_order_relaxed);
  g_computePipelineStateCreateCount.store(0, std::memory_order_relaxed);
  g_computePipelineStateFingerprintMatchCount.store(0,
                                                    std::memory_order_relaxed);
  g_pipelineStreamCreateCount.store(0, std::memory_order_relaxed);
  g_pipelineStreamParseCount.store(0, std::memory_order_relaxed);
  g_pipelineStreamParseFailureCount.store(0, std::memory_order_relaxed);
  g_suppressedEdramRestorePsoCount.store(0, std::memory_order_relaxed);
  g_pso535CullCandidateCount.store(0, std::memory_order_relaxed);
  g_pso535CullFingerprintMatchCount.store(0, std::memory_order_relaxed);
  g_pso535CullFixPipelineCount.store(0, std::memory_order_relaxed);
  g_edramScaleCandidateCount.store(0, std::memory_order_relaxed);
  g_edramScaleFingerprintMatchCount.store(0, std::memory_order_relaxed);
  g_edramScaleFixPipelineCount.store(0, std::memory_order_relaxed);
  g_edramLoadCandidateCount.store(0, std::memory_order_relaxed);
  g_edramLoadFingerprintMatchCount.store(0, std::memory_order_relaxed);
  g_edramLoadFixPipelineCount.store(0, std::memory_order_relaxed);
  g_edramScaleDrawMatchCount.store(0, std::memory_order_relaxed);
  g_edramScaleDrawSubstitutionCount.store(0, std::memory_order_relaxed);
  g_edramLoadDrawMatchCount.store(0, std::memory_order_relaxed);
  g_edramLoadDrawSubstitutionCount.store(0, std::memory_order_relaxed);
  g_edramLoadScissorOverrideCount.store(0, std::memory_order_relaxed);
  g_edramDrawRootSignatureMismatchCount.store(0, std::memory_order_relaxed);
  g_edramDrawFingerprintCandidateCount.store(0, std::memory_order_relaxed);
  g_pixEdramBoundMatchCount.store(0, std::memory_order_relaxed);
  ResetG2HTrace();
  g_pixEdramBoundRejectCount.store(0, std::memory_order_relaxed);
  g_pixDescriptorCopyCount.store(0, std::memory_order_relaxed);
  g_pixConstantCopyCount.store(0, std::memory_order_relaxed);
  g_edramDrawFingerprintQueryCount.store(0, std::memory_order_relaxed);
  g_edramDrawFingerprintCacheHitCount.store(0, std::memory_order_relaxed);
  g_edramDrawFingerprintCacheOverflowCount.store(0, std::memory_order_relaxed);
  g_hostDrawCallCount.store(0, std::memory_order_relaxed);
  g_restartTerrainPipelineCount.store(0, std::memory_order_relaxed);
  g_restartSkyPipelineCount.store(0, std::memory_order_relaxed);
  g_restartPipelineOverflowCount.store(0, std::memory_order_relaxed);
  g_restartTerrainDrawCount.store(0, std::memory_order_relaxed);
  g_restartSkyDrawCount.store(0, std::memory_order_relaxed);
  g_restartTerrainStartVertexZeroCount.store(0, std::memory_order_relaxed);
  g_restartTerrainStartVertexNonZeroCount.store(0,
                                                std::memory_order_relaxed);
  g_restartSkyStartVertexZeroCount.store(0, std::memory_order_relaxed);
  g_restartSkyStartVertexNonZeroCount.store(0, std::memory_order_relaxed);
  g_restartTerrainMaxStartVertex.store(0, std::memory_order_relaxed);
  g_restartSkyMaxStartVertex.store(0, std::memory_order_relaxed);
  g_restartConstantResolveCount.store(0, std::memory_order_relaxed);
  g_restartConstantResolveFailureCount.store(0, std::memory_order_relaxed);
  g_restartStartMatchesVertexOffsetCount.store(0, std::memory_order_relaxed);
  g_restartStartMismatchesVertexOffsetCount.store(0,
                                                  std::memory_order_relaxed);
  g_hostCommandListResetCount.store(0, std::memory_order_relaxed);
  g_hostTransferDrawCount.store(0, std::memory_order_relaxed);
  g_hostEdramRestoreDrawCandidateCount.store(0, std::memory_order_relaxed);
  g_hostEdramRestoreDrawSkipCount.store(0, std::memory_order_relaxed);
  g_hostSetDescriptorHeapsCount.store(0, std::memory_order_relaxed);
  g_hostSetGraphicsRootDescriptorTableCount.store(0, std::memory_order_relaxed);
  g_fullscreenScissorFixCount.store(0, std::memory_order_relaxed);
  g_msaaViewportCandidateCount.store(0, std::memory_order_relaxed);
  g_msaaViewportFixCount.store(0, std::memory_order_relaxed);
  g_edramRestoreDrawCandidateCount.store(0, std::memory_order_relaxed);
  g_edramRestoreDrawSkipCount.store(0, std::memory_order_relaxed);
  g_edramRestoreDrawHashMismatchCount.store(0, std::memory_order_relaxed);
  g_edramRestoreExperimentHitCount.store(0, std::memory_order_relaxed);
  g_edramRestoreExperimentSkipCount.store(0, std::memory_order_relaxed);
  g_drawRecordCallCount.store(0, std::memory_order_relaxed);
  g_drawRecordInterestingCount.store(0, std::memory_order_relaxed);
  g_drawRecordUniqueCount.store(0, std::memory_order_relaxed);
  g_nullPipelineStateSkipCount.store(0, std::memory_order_relaxed);
  {
    std::scoped_lock lock(g_edramRestoreDrawMutex);
    g_edramRestoreDrawPipelineState = nullptr;
    g_edramRestoreDrawPipelineMatches = false;
  }
  {
    std::scoped_lock lock(g_edramDrawReplacementMutex);
    ResetEdramDrawReplacementsLocked();
  }
  {
    std::scoped_lock lock(g_drawRecordTraceMutex);
    g_observedDrawRecords = {};
    g_observedDrawRecordCount = 0;
  }
  {
    std::scoped_lock lock(g_pipelineObservationMutex);
    g_observedPipelineStates = {};
    g_observedPipelineStateCount = 0;
    g_edramRestoreExperimentCandidates = {};
    g_edramRestoreExperimentCandidateCount = 0;
  }
  for (auto &slot : g_restartTerrainPipelineStates) {
    slot.store(nullptr, std::memory_order_relaxed);
  }
  for (auto &slot : g_restartSkyPipelineStates) {
    slot.store(nullptr, std::memory_order_relaxed);
  }
  BridgeVgpuExtendedFetchCount = 0;
  BridgeVgpuLastFetchCount = 0;
  BridgeVgpuZeroedHeapCount = 0;
  BridgeVgpuLastHeapFlags = 0;
  BridgeVgpuLastResourceFlags = 0;
  BridgeVgpuDiscardedResourceCount = 0;
  BridgeVgpuDiscardFailure = 0;
  BridgeVgpuPlacedResourceCallCount = 0;
  BridgeVgpuLastPlacedResourceSite = 0;
  BridgeVgpuLastPlacedResourceFailure = 0;
  BridgeVgpuLegacyFallbackCount = 0;
  BridgeVgpuLegacyFallbackFailure = 0;
  BridgeVgpuCommittedOverflowFallbackCount = 0;
  BridgeVgpuCommittedOverflowFallbackFailure = 0;
  BridgeVgpuLastCommittedHeapFlags = 0;
  BridgeVgpuLastHeapType = 0;
  BridgeVgpuLastHeapCpuPageProperty = 0;
  BridgeVgpuLastHeapMemoryPoolPreference = 0;
  BridgeVgpuLastHeapOffset = 0;
  BridgeVgpuLastHeapSize = 0;
  BridgeVgpuLastAllocationSize = 0;
  BridgeVgpuShaderCompileCount = 0;
  BridgeVgpuLastCompiledShaderSize = 0;
  BridgeVgpuShaderCaptureFailure = 0;
  BridgeVgpuXenosTranslateHookInstalled = 0;
  BridgeVgpuXenosTranslateHookFailure = 0;
  BridgeVgpuXenosTranslateCount = 0;
  BridgeVgpuXenosUcodeCaptureCount = 0;
  BridgeVgpuXenosUcodeCaptureFailure = 0;
  BridgeVgpuXenosShaderMapFailure = 0;
  BridgeVgpuXenosLastStage = 0;
  BridgeVgpuXenosLastUcodeSize = 0;
  BridgeVgpuXenosLastUcodeHash0 = 0;
  BridgeVgpuXenosLastUcodeHash1 = 0;
  BridgeVgpuXenosLastUcodeHash2 = 0;
  BridgeVgpuXenosLastUcodeHash3 = 0;
  BridgeVgpuVertexShaderCompileCount = 0;
  BridgeVgpuLastVertexShaderSize = 0;
  BridgeVgpuVertexShaderCaptureFailure = 0;
  BridgeVgpuGroundFixShaderCount = 0;
  BridgeVgpuGroundFixFetchCount = 0;
  BridgeVgpuGroundFixFailure = 0;
  // Keep the currently deployed title-specific fetch bias until the corrected
  // ballot path is also validated in a live XeO3 run.
  BridgeVgpuGroundFixEnabled = 1;
  BridgeVgpuIndexFixShaderCount = 0;
  BridgeVgpuIndexFixSiteCount = 0;
  BridgeVgpuIndexFixFailure = 0;
  BridgeVgpuAircraftRestartShaderCount = 0;
  BridgeVgpuShadowRestartShaderCount = 0;
  BridgeVgpuReciprocalFixShaderCount = 0;
  BridgeVgpuReciprocalFixInstructionCount = 0;
  BridgeVgpuReciprocalFixFailure = 0;
  // PIX A/B runs showed Xenia's reciprocal approximation did not affect the
  // terrain corruption, so retain it as an opt-in diagnostic only.
  BridgeVgpuReciprocalFixEnabled = 0;
  BridgeVgpuWaveBallotFingerprintMatchCount = 0;
  BridgeVgpuWaveBallotFixShaderCount = 0;
  BridgeVgpuWaveBallotFixSiteCount = 0;
  BridgeVgpuWaveBallotFixFailure = 0;
  BridgeVgpuWaveBallotFixEnabled = 1;
  BridgeVgpuVposFixShaderCount = 0;
  BridgeVgpuVposFixSiteCount = 0;
  BridgeVgpuVposFixFailure = 0;
  BridgeVgpuVposScaleFixEnabled = 1;
  BridgeVgpuVposSceneHalfWidthUvEnabled = 1;
  BridgeVgpuToneMapFixEnabled = 0;
  BridgeVgpuExposureFixEnabled = 1;
  BridgeVgpuExposureFingerprintMatchCount = 0;
  BridgeVgpuExposureFixShaderCount = 0;
  BridgeVgpuExposureFixSiteCount = 0;
  BridgeVgpuExposureFixFailure = 0;
  BridgeVgpuTextureEndianFixEnabled = 1;
  BridgeVgpuTextureEndianFixBudget = UINT32_MAX;
  BridgeVgpuTextureEndianReplacement = 0;
  BridgeVgpuTextureEndianFixCount = 0;
  BridgeVgpuTexturePreviewEndianFixCount = 0;
  BridgeVgpuTextureEndianSignatureMatchCount = 0;
  BridgeVgpuTextureEndianLastMatchCall = 0;
  BridgeVgpuTextureEndianLastPatchedCall = 0;
  BridgeVgpuTextureTransferCallCount = 0;
  BridgeVgpuConstantUploadCallCount = 0;
  BridgeVgpuConstantUpload128Count = 0;
  BridgeVgpuConstantUploadContextCount = 0;
  BridgeVgpuConstantUploadContextFailureCount = 0;
  BridgeVgpuConstantUploadLastContext = 0;
  BridgeVgpuConstantUploadLastCpuBase = 0;
  BridgeVgpuConstantUploadLastGpuBase = 0;
  BridgeVgpuConstantUploadLastStride = 0;
  BridgeVgpuConstantUploadLastSlotCount = 0;
  BridgeVgpuConstantUploadLastMappedSpan = 0;
  BridgeVgpuTransfer341WidthFixEnabled = 1;
  BridgeVgpuTransfer341WidthCandidateCount = 0;
  BridgeVgpuTransfer341WidthSignatureMatchCount = 0;
  BridgeVgpuTransfer341WidthPatchCount = 0;
  BridgeVgpuTransfer341WidthFailureCount = 0;
  BridgeVgpuTransfer341MappedBufferCount = 0;
  BridgeVgpuTransfer341ArenaCandidateCount = 0;
  BridgeVgpuTransfer341CommittedArenaCandidateCount = 0;
  BridgeVgpuTransfer341LastArenaHeapType = 0;
  BridgeVgpuTransfer341LastArenaMapResult = 0;
  BridgeVgpuTransfer341UploadContextCount = 0;
  BridgeVgpuTransfer341UploadContextHitCount = 0;
  BridgeVgpuTransfer341LastUploadContext = 0;
  BridgeVgpuTransfer341LastUploadCpuBase = 0;
  BridgeVgpuTransfer341LastUploadGpuBase = 0;
  BridgeVgpuTransfer341LastUploadStride = 0;
  BridgeVgpuTransfer341LastUploadSlotCount = 0;
  BridgeVgpuTransfer341LastUploadMappedSpan = 0;
  BridgeVgpuTransfer341LastResolvedCpuAddress = 0;
  BridgeVgpuTransfer341DescriptorCount = 0;
  BridgeVgpuTransfer341BindCount = 0;
  BridgeVgpuTransfer341DescriptorMissCount = 0;
  BridgeVgpuTransfer341GpuAddressMissCount = 0;
  BridgeVgpuTransfer341LastGpuAddress = 0;
  BridgeVgpuTransfer341LastMappedGpuBase = 0;
  BridgeVgpuTransfer341LastMappedBufferSize = 0;
  BridgeVgpuTransfer341WidthLastCall = 0;
  BridgeVgpuTransfer341WidthLastContext = 0;
  BridgeVgpuTransfer341WidthLastSourceSize = 0;
  BridgeVgpuTransfer341WidthLastOriginalPackedDimensions = 0;
  BridgeVgpuTransfer341WidthLastReplacementPackedDimensions = 0;
  BridgeVgpuEdramConstantCandidateCount = 0;
  BridgeVgpuEdramLoadConstantCount = 0;
  BridgeVgpuEdramScaleConstantCount = 0;
  BridgeVgpuEdramConstantSnapshotSequence = 0;
  BridgeVgpuEdramConstantLastCall = 0;
  BridgeVgpuEdramConstantLastContext = 0;
  BridgeVgpuEdramConstantLastSize = 0;
  BridgeVgpuEdramConstantLastKind = 0;
  BridgeVgpuEdramConstantData0 = 0;
  BridgeVgpuEdramConstantData1 = 0;
  BridgeVgpuEdramConstantData2 = 0;
  BridgeVgpuEdramConstantData3 = 0;
  BridgeVgpuEdramConstantData4 = 0;
  BridgeVgpuEdramConstantData5 = 0;
  BridgeVgpuEdramConstantData6 = 0;
  BridgeVgpuEdramConstantData7 = 0;
  BridgeVgpuTextureEndian2CallCount = 0;
  BridgeVgpuLastTextureEndianOriginal = 0;
  BridgeVgpuLastTextureEndianReplacement = 0;
  BridgeVgpuLastTextureEndian2Parameter9 = 0;
  BridgeVgpuLastTextureEndian2Parameter13 = 0;
  BridgeVgpuLastTextureEndian2Parameter14 = 0;
  BridgeVgpuLastTextureEndian2Parameter15 = 0;
  BridgeVgpuLastTextureEndian2Parameter16 = 0;
  BridgeVgpuPipelineStateHookInstalled = 0;
  BridgeVgpuPipelineStateHookFailure = 0;
  BridgeVgpuPipelineStateCreateCount = 0;
  BridgeVgpuComputePipelineStateHookInstalled = 0;
  BridgeVgpuComputePipelineStateHookFailure = 0;
  BridgeVgpuComputePipelineStateCreateCount = 0;
  BridgeVgpuComputePipelineStateFingerprintMatchCount = 0;
  BridgeVgpuComputePipelineStateLastShaderSize = 0;
  BridgeVgpuComputePipelineStateLastShaderHash0 = 0;
  BridgeVgpuComputePipelineStateLastShaderHash1 = 0;
  BridgeVgpuComputePipelineStateLastShaderHash2 = 0;
  BridgeVgpuComputePipelineStateLastShaderHash3 = 0;
  BridgeVgpuComputePipelineStateLastCreateResult = 0;
  BridgeVgpuComputePipelineStateLastCreateOutput = 0;
  BridgeVgpuTransfer341PipelineState = 0;
  BridgeVgpuTransfer341PipelineBindCount = 0;
  BridgeVgpuTransfer341CachedPsoQueryCount = 0;
  BridgeVgpuTransfer341CachedPsoCacheHitCount = 0;
  BridgeVgpuTransfer341CachedPsoMatchCount = 0;
  BridgeVgpuTransfer341LastCachedPso = 0;
  BridgeVgpuTransfer341LastCachedBlobSize = 0;
  BridgeVgpuTransfer341LastCachedBlobHash0 = 0;
  BridgeVgpuTransfer341LastCachedBlobHash1 = 0;
  BridgeVgpuTransfer341LastCachedBlobHash2 = 0;
  BridgeVgpuTransfer341LastCachedBlobHash3 = 0;
  BridgeVgpuTransfer341LastCachedBlobResult = 0;
  BridgeVgpuTransfer341FingerprintDumpFailure = 0;
  BridgeVgpuTransfer341PipelineReplacementEnabled = 0;
  BridgeVgpuTransfer341ReplacementPipelineState = 0;
  BridgeVgpuTransfer341ReplacementCreateCount = 0;
  BridgeVgpuTransfer341ReplacementSubstitutionCount = 0;
  BridgeVgpuTransfer341ReplacementFailure = 0;
  BridgeVgpuTransfer341LastComputeRootSignature = 0;
  BridgeVgpuTransfer341LastCpuDescriptor = 0;
  BridgeVgpuTransfer341LastDescriptor0 = 0;
  BridgeVgpuTransfer341LastDescriptor1 = 0;
  BridgeVgpuTransfer341LastDescriptor2 = 0;
  BridgeVgpuTransfer341LastDescriptor3 = 0;
  BridgeVgpuTransfer341LastTaskCpuDescriptor = 0;
  BridgeVgpuTransfer341LastTaskDescriptor0 = 0;
  BridgeVgpuTransfer341LastTaskDescriptor1 = 0;
  BridgeVgpuTransfer341LastTaskDescriptor2 = 0;
  BridgeVgpuTransfer341LastTaskDescriptor3 = 0;
  BridgeVgpuTransfer341LastTaskGpuAddress = 0;
  BridgeVgpuPipelineStreamHookInstalled = 0;
  BridgeVgpuPipelineStreamHookFailure = 0;
  BridgeVgpuPipelineStreamCreateCount = 0;
  BridgeVgpuPipelineStreamParseCount = 0;
  BridgeVgpuPipelineStreamParseFailureCount = 0;
  BridgeVgpuPipelineStreamLastCreateResult = 0;
  BridgeVgpuPipelineStreamLastCreateOutput = 0;
  BridgeVgpuSuppressedEdramRestorePsoCount = 0;
  BridgeVgpuEdramRestoreFingerprintCount = 0;
  BridgeVgpuPso535CullFixEnabled = 0;
  BridgeVgpuPso535CullCandidateCount = 0;
  BridgeVgpuPso535CullFingerprintMatchCount = 0;
  BridgeVgpuPso535CullFixPipelineCount = 0;
  BridgeVgpuPso535CullFixFailure = 0;
  BridgeVgpuPso535CullLastOriginalMode = 0;
  BridgeVgpuPso535CullLastReplacementMode = 0;
  BridgeVgpuEdramScaleFixEnabled = 1;
  BridgeVgpuEdramScaleCandidateCount = 0;
  BridgeVgpuEdramScaleFingerprintMatchCount = 0;
  BridgeVgpuEdramScaleFixPipelineCount = 0;
  BridgeVgpuEdramScaleFixFailure = 0;
  BridgeVgpuEdramScaleCandidateVsHash0 = 0;
  BridgeVgpuEdramScaleCandidateVsHash1 = 0;
  BridgeVgpuEdramScaleCandidateVsHash2 = 0;
  BridgeVgpuEdramScaleCandidateVsHash3 = 0;
  BridgeVgpuEdramScaleCandidatePsHash0 = 0;
  BridgeVgpuEdramScaleCandidatePsHash1 = 0;
  BridgeVgpuEdramScaleCandidatePsHash2 = 0;
  BridgeVgpuEdramScaleCandidatePsHash3 = 0;
  BridgeVgpuEdramLoadFixEnabled = 1;
  BridgeVgpuEdramLoadCandidateCount = 0;
  BridgeVgpuEdramLoadFingerprintMatchCount = 0;
  BridgeVgpuEdramLoadFixPipelineCount = 0;
  BridgeVgpuEdramLoadFixFailure = 0;
  BridgeVgpuEdramLoadCandidateVsHash0 = 0;
  BridgeVgpuEdramLoadCandidateVsHash1 = 0;
  BridgeVgpuEdramLoadCandidateVsHash2 = 0;
  BridgeVgpuEdramLoadCandidateVsHash3 = 0;
  BridgeVgpuEdramLoadCandidatePsHash0 = 0;
  BridgeVgpuEdramLoadCandidatePsHash1 = 0;
  BridgeVgpuEdramLoadCandidatePsHash2 = 0;
  BridgeVgpuEdramLoadCandidatePsHash3 = 0;
  BridgeVgpuEdramScaleDrawMatchCount = 0;
  BridgeVgpuEdramScaleDrawSubstitutionCount = 0;
  BridgeVgpuEdramScaleDrawOriginalPipelineState = 0;
  BridgeVgpuEdramScaleDrawReplacementPipelineState = 0;
  BridgeVgpuEdramLoadDrawMatchCount = 0;
  BridgeVgpuEdramLoadDrawSubstitutionCount = 0;
  BridgeVgpuEdramLoadScissorOverrideCount = 0;
  BridgeVgpuEdramLoadDrawOriginalPipelineState = 0;
  BridgeVgpuEdramLoadDrawReplacementPipelineState = 0;
  BridgeVgpuEdramDrawRootSignature = 0;
  BridgeVgpuPixEdramBoundMatchCount = 0;
  BridgeVgpuPixEdramBoundRejectCount = 0;
  BridgeVgpuPixEdramResolveFailure = 0;
  BridgeVgpuRtvHookInstalled = 0;
  BridgeVgpuRtvHookFailure = 0;
  BridgeVgpuPixDescriptorCopyCount = 0;
  BridgeVgpuPixConstantCopyCount = 0;
  BridgeVgpuPixDescriptorHookFailure = 0;
  BridgeVgpuEdramDrawRootSignatureMismatchCount = 0;
  BridgeVgpuEdramDrawFingerprintCandidateCount = 0;
  BridgeVgpuEdramDrawFingerprintQueryCount = 0;
  BridgeVgpuEdramDrawFingerprintCacheHitCount = 0;
  BridgeVgpuEdramDrawFingerprintCacheOverflowCount = 0;
  BridgeVgpuEdramDrawLastFingerprintPipelineState = 0;
  BridgeVgpuEdramDrawLastFingerprintBlobSize = 0;
  BridgeVgpuEdramDrawLastFingerprintHash0 = 0;
  BridgeVgpuEdramDrawLastFingerprintHash1 = 0;
  BridgeVgpuEdramDrawLastFingerprintHash2 = 0;
  BridgeVgpuEdramDrawLastFingerprintHash3 = 0;
  BridgeVgpuEdramDrawLastFingerprintResult = 0;
  BridgeVgpuEdramDrawLastFingerprintClassification = 0;
  BridgeVgpuEdramDrawFingerprintDumpEnabled = 0;
  BridgeVgpuEdramDrawFingerprintDumpFailure = 0;
  BridgeVgpuHostCommandListHookCount = 0;
  BridgeVgpuHostCommandListHookFailure = 0;
  BridgeVgpuHostCommandListResetCount = 0;
  BridgeVgpuHostCommandListResetLastInitialPipelineState = 0;
  BridgeVgpuHostCommandListResetLastResult = 0;
  BridgeVgpuHostDrawCallCount = 0;
  BridgeVgpuRestartTerrainPipelineCount = 0;
  BridgeVgpuRestartSkyPipelineCount = 0;
  BridgeVgpuRestartPipelineOverflowCount = 0;
  BridgeVgpuRestartTerrainDrawCount = 0;
  BridgeVgpuRestartSkyDrawCount = 0;
  BridgeVgpuRestartTerrainStartVertexZeroCount = 0;
  BridgeVgpuRestartTerrainStartVertexNonZeroCount = 0;
  BridgeVgpuRestartSkyStartVertexZeroCount = 0;
  BridgeVgpuRestartSkyStartVertexNonZeroCount = 0;
  BridgeVgpuRestartTerrainLastVertexCount = 0;
  BridgeVgpuRestartTerrainLastStartVertex = 0;
  BridgeVgpuRestartTerrainMaxStartVertex = 0;
  BridgeVgpuRestartSkyLastVertexCount = 0;
  BridgeVgpuRestartSkyLastStartVertex = 0;
  BridgeVgpuRestartSkyMaxStartVertex = 0;
  BridgeVgpuRestartLastClassification = 0;
  BridgeVgpuRestartLastPipelineState = 0;
  BridgeVgpuRestartLastInstanceCount = 0;
  BridgeVgpuRestartLastStartInstance = 0;
  BridgeVgpuRestartLastThreadId = 0;
  BridgeVgpuRestartLastDrawCall = 0;
  BridgeVgpuRestartConstantResolveCount = 0;
  BridgeVgpuRestartConstantResolveFailureCount = 0;
  BridgeVgpuRestartLastResolveFailure = 0;
  BridgeVgpuRestartLastRootDescriptorTable = 0;
  BridgeVgpuRestartLastDescriptorHeapGpuStart = 0;
  BridgeVgpuRestartLastDescriptorHeapCpuStart = 0;
  BridgeVgpuRestartLastDescriptorHeapByteSpan = 0;
  BridgeVgpuRestartLastDescriptorHeapIncrement = 0;
  BridgeVgpuRestartLastCpuDescriptor = 0;
  BridgeVgpuRestartLastDescriptorWord0 = 0;
  BridgeVgpuRestartLastDescriptorWord1 = 0;
  BridgeVgpuRestartLastDecodedGpuAddress = 0;
  BridgeVgpuRestartLastUploadContextKind = 0;
  BridgeVgpuRestartStartMatchesVertexOffsetCount = 0;
  BridgeVgpuRestartStartMismatchesVertexOffsetCount = 0;
  BridgeVgpuRestartLastConstantGpuAddress = 0;
  BridgeVgpuRestartLastConstantCpuAddress = 0;
  BridgeVgpuRestartLastVertexOffsetBits = 0;
  BridgeVgpuRestartLastUseIndexBuffer = 0;
  BridgeVgpuRestartLastIndexCount = 0;
  BridgeVgpuRestartLastVfetchEndianness = 0;
  BridgeVgpuRestartLastPackedIbDesc = 0;
  BridgeVgpuRestartLastResetIndex = 0;
  BridgeVgpuRestartLastIbBase = 0;
  BridgeVgpuRestartTerrainLastVertexOffsetBits = 0;
  BridgeVgpuRestartTerrainLastIndexCount = 0;
  BridgeVgpuRestartTerrainLastPackedIbDesc = 0;
  BridgeVgpuRestartSkyLastVertexOffsetBits = 0;
  BridgeVgpuRestartSkyLastIndexCount = 0;
  BridgeVgpuRestartSkyLastPackedIbDesc = 0;
  BridgeVgpuHostTransferDrawCount = 0;
  BridgeVgpuHostTransferExperimentSelector = 0;
  BridgeVgpuHostTransferLastCandidate = 0;
  BridgeVgpuHostTransferLastClassification = 0;
  BridgeVgpuHostTransferLastPipelineState = 0;
  BridgeVgpuHostTransferLastRootSignature = 0;
  BridgeVgpuHostSetDescriptorHeapsCount = 0;
  BridgeVgpuHostSetGraphicsRootDescriptorTableCount = 0;
  BridgeVgpuHostTransferLastDescriptorHeapCount = 0;
  BridgeVgpuHostTransferLastDescriptorHeap0 = 0;
  BridgeVgpuHostTransferLastDescriptorHeap1 = 0;
  BridgeVgpuHostTransferLastDescriptorHeap0GpuStart = 0;
  BridgeVgpuHostTransferLastDescriptorHeap1GpuStart = 0;
  BridgeVgpuHostTransferLastRootDescriptorTableMask = 0;
  BridgeVgpuHostTransferLastRootDescriptorTable0 = 0;
  BridgeVgpuHostTransferLastRootDescriptorTable1 = 0;
  BridgeVgpuHostEdramRestoreDrawSkipEnabled = 0;
  BridgeVgpuHostEdramRestoreDrawCandidateCount = 0;
  BridgeVgpuHostEdramRestoreDrawSkipCount = 0;
  BridgeVgpuHostEdramRestoreDrawLastDrawCall = 0;
  BridgeVgpuHostEdramRestoreDrawLastPipelineState = 0;
  BridgeVgpuHostEdramRestoreDrawLastRootSignature = 0;
  BridgeVgpuFullscreenScissorFixEnabled = 0;
  BridgeVgpuFullscreenScissorFixCount = 0;
  BridgeVgpuFullscreenScissorFixFailure = 0;
  BridgeVgpuMsaaViewportFixEnabled = 1;
  BridgeVgpuMsaaViewportCandidateCount = 0;
  BridgeVgpuMsaaViewportFixCount = 0;
  BridgeVgpuMsaaViewportFixFailure = 0;
  BridgeVgpuMsaaViewportLastPipelineState = 0;
  BridgeVgpuMsaaViewportLastOriginalWidthBits = 0;
  BridgeVgpuMsaaViewportLastReplacementWidthBits = 0;
  BridgeVgpuMsaaViewportLastVertexCount = 0;
  BridgeVgpuEdramRestoreDrawCandidateCount = 0;
  BridgeVgpuEdramRestoreDrawSkipCount = 0;
  BridgeVgpuEdramRestoreDrawHashMismatchCount = 0;
  BridgeVgpuEdramRestoreDrawGuardFailure = 0;
  BridgeVgpuEdramRestoreDrawLastRecord = 0;
  BridgeVgpuEdramRestoreDrawLastPipelineState = 0;
  BridgeVgpuEdramRestoreDrawLastCachedBlobSize = 0;
  BridgeVgpuEdramRestoreDrawLastCachedBlobHash0 = 0;
  BridgeVgpuEdramRestoreDrawLastCachedBlobHash1 = 0;
  BridgeVgpuEdramRestoreDrawLastCachedBlobHash2 = 0;
  BridgeVgpuEdramRestoreDrawLastCachedBlobHash3 = 0;
  BridgeVgpuEdramRestoreExperimentSelector = 0;
  BridgeVgpuEdramRestoreExperimentCandidateCount = 0;
  BridgeVgpuEdramRestoreExperimentLastCandidate = 0;
  BridgeVgpuEdramRestoreExperimentHitCount = 0;
  BridgeVgpuEdramRestoreExperimentSkipCount = 0;
  BridgeVgpuEdramRestoreExperimentLastCreateCount = 0;
  BridgeVgpuDrawRecordCallCount = 0;
  BridgeVgpuDrawRecordInterestingCount = 0;
  BridgeVgpuDrawRecordUniqueCount = 0;
  BridgeVgpuDrawRecordLastRecord = 0;
  BridgeVgpuDrawRecordLastCommandList = 0;
  BridgeVgpuDrawRecordLastCommandContext = 0;
  BridgeVgpuDrawRecordLastRootSignature = 0;
  BridgeVgpuDrawRecordLastPipelineState = 0;
  BridgeVgpuDrawRecordLastViewportWidthBits = 0;
  BridgeVgpuDrawRecordLastViewportHeightBits = 0;
  BridgeVgpuDrawRecordLastViewportMinDepthBits = 0;
  BridgeVgpuDrawRecordLastViewportMaxDepthBits = 0;
  BridgeVgpuDrawRecordLastViewportTopLeftXBits = 0;
  BridgeVgpuDrawRecordLastViewportTopLeftYBits = 0;
  BridgeVgpuDrawRecordLastScissorRight = 0;
  BridgeVgpuDrawRecordLastScissorBottom = 0;
  BridgeVgpuDrawRecordLastRecordKind = 0;
  BridgeVgpuDrawRecordLastVertexCount = 0;
  BridgeVgpuDrawRecordLastStartVertex = 0;
  BridgeVgpuPipelineLastVertexShaderSize = 0;
  BridgeVgpuPipelineLastPixelShaderSize = 0;
  BridgeVgpuPipelineLastSampleCount = 0;
  BridgeVgpuPipelineLastRenderTargetFormat = 0;
  BridgeVgpuPipelineLastInputElementCount = 0;
  BridgeVgpuPipelineLastExpectedInputLayout = 0;
  BridgeVgpuPipelineLastExpectedFixedState = 0;
  BridgeVgpuPipelineStateCreateFailureCount = 0;
  BridgeVgpuPipelineStateLastCreateResult = 0;
  BridgeVgpuPipelineStateLastCreateOutput = 0;
  BridgeVgpuPipelineStateLastFailureResult = 0;
  BridgeVgpuPipelineStateLastFailureCreateSequence = 0;
  BridgeVgpuNullPipelineStateGuardInstalled = 0;
  BridgeVgpuNullPipelineStateGuardFailure = 0;
  BridgeVgpuNullPipelineStateSkipCount = 0;
  BridgeVgpuNullPipelineStateLastThreadId = 0;
  BridgeVgpuNullPipelineStateLastRecord = 0;
  BridgeVgpuNullPipelineStateLastCommandList = 0;
  BridgeVgpuNullPipelineStateLastCommandContext = 0;
  BridgeVgpuNullPipelineStateLastCachedPso = 0;
  BridgeVgpuNullPipelineStateLastRootSignature = 0;
  BridgeVgpuNullPipelineStateLastRecordKind = 0;
  BridgeVgpuNullPipelineStateLastVertexCount = 0;
  BridgeVgpuNullPipelineStateLastStartVertex = 0;

  const auto nullPipelineStateDetour = detail::EncodeNullPipelineStateJump(
      reinterpret_cast<const void *>(&VgpuNullPipelineStateGuardThunk));
  VgpuNullPipelineStateNormalTarget = reinterpret_cast<std::uintptr_t>(
      moduleBase + kNullPipelineStateNormalRva);
  VgpuNullPipelineStateSkipTarget =
      reinterpret_cast<std::uintptr_t>(moduleBase + kNullPipelineStateSkipRva);
  if (!WriteInstructionGate(
          nullPipelineStateTarget, nullPipelineStateDetour.data(),
          nullPipelineStateDetour.size(),
          PatchStatus::NullPipelineStateProtectionFailure,
          PatchStatus::NullPipelineStateProtectionRestoreFailure)) {
    const auto failure = static_cast<PatchStatus>(BridgeVgpuPatchStatus);
    BridgeVgpuNullPipelineStateGuardFailure =
        static_cast<std::uint32_t>(failure);
    if (std::equal(nullPipelineStateDetour.begin(),
                   nullPipelineStateDetour.end(), nullPipelineStateTarget)) {
      g_installedNullPipelineStateDetour = nullPipelineStateDetour;
      g_nullPipelineStateTarget = nullPipelineStateTarget;
      BridgeVgpuNullPipelineStateGuardInstalled = 1;
    } else {
      VgpuNullPipelineStateNormalTarget = 0;
      VgpuNullPipelineStateSkipTarget = 0;
    }
    RemovePinnedVgpuPatch();
    return FailInstall(failure);
  }
  g_installedNullPipelineStateDetour = nullPipelineStateDetour;
  g_nullPipelineStateTarget = nullPipelineStateTarget;
  BridgeVgpuNullPipelineStateGuardInstalled = 1;
  BridgeVgpuNullPipelineStateGuardFailure = ERROR_SUCCESS;

  if (!InstallShaderCompileHook(moduleBase)) {
    const auto failure = static_cast<PatchStatus>(BridgeVgpuPatchStatus);
    RemovePinnedVgpuPatch();
    return FailInstall(failure);
  }
  if (!InstallXenosTranslateHook(moduleBase)) {
    const auto failure = static_cast<PatchStatus>(BridgeVgpuPatchStatus);
    RemovePinnedVgpuPatch();
    return FailInstall(failure);
  }
  if (!InstallTextureTransferHook(moduleBase)) {
    const auto failure = static_cast<PatchStatus>(BridgeVgpuPatchStatus);
    RemovePinnedVgpuPatch();
    return FailInstall(failure);
  }
  if (!InstallStructuredTextureTransferHook(moduleBase)) {
    const auto failure = static_cast<PatchStatus>(BridgeVgpuPatchStatus);
    RemovePinnedVgpuPatch();
    return FailInstall(failure);
  }
  if (!InstallConstantUploadHook(moduleBase)) {
    const auto failure = static_cast<PatchStatus>(BridgeVgpuPatchStatus);
    RemovePinnedVgpuPatch();
    return FailInstall(failure);
  }
  if (!WriteSamplerAddressModeTable(samplerAddressModeTarget,
                                    kFixedSamplerAddressModes)) {
    const auto failure = static_cast<PatchStatus>(BridgeVgpuPatchStatus);
    if (std::equal(kFixedSamplerAddressModes.begin(),
                   kFixedSamplerAddressModes.end(), samplerAddressModeTarget)) {
      WriteSamplerAddressModeTable(samplerAddressModeTarget,
                                   kExpectedSamplerAddressModes);
    }
    RemovePinnedVgpuPatch();
    return FailInstall(failure);
  }
  g_samplerAddressModeTarget = samplerAddressModeTarget;
  g_callRelayRegion = callRelayOwner.release();
  SetStatus(PatchStatus::Installed);
  EmitPatchEvent("install", static_cast<std::uint32_t>(kFetchTableRva));
  EmitPatchEvent("tight_alignment_disabled",
                 static_cast<std::uint32_t>(kTightAlignmentGateRva));
  EmitPatchEvent("modern_placed_resource_preserved",
                 static_cast<std::uint32_t>(kPlacedResourceGateRva));
  EmitPatchEvent("create_heap_zeroing_enabled",
                 static_cast<std::uint32_t>(kCreateHeapCallRva));
  EmitPatchEvent("placed_resource_discard_enabled",
                 static_cast<std::uint32_t>(kModernPlacedResourceCallRva));
  EmitPatchEvent("legacy_resource_discard_enabled",
                 static_cast<std::uint32_t>(kLegacyPlacedResourceCallRva));
  EmitPatchEvent(
      "secondary_placed_resource_discard_enabled",
      static_cast<std::uint32_t>(kSecondaryModernPlacedResourceCallRva));
  EmitPatchEvent("cold_legacy_resource_discard_enabled",
                 static_cast<std::uint32_t>(kColdLegacyPlacedResourceCallRva));
  EmitPatchEvent("vertex_shader_capture_enabled",
                 static_cast<std::uint32_t>(kShaderCompileRva));
  EmitPatchEvent("xenos_ucode_capture_enabled",
                 static_cast<std::uint32_t>(kXenosTranslateRva));
  EmitPatchEvent(BridgeVgpuReciprocalFixEnabled != 0
                     ? "scalar_reciprocal_fix_enabled"
                     : "scalar_reciprocal_fix_disabled",
                 static_cast<std::uint32_t>(kShaderCompileRva));
  EmitPatchEvent(BridgeVgpuWaveBallotFixEnabled != 0
                     ? "wave_ballot_fix_enabled"
                     : "wave_ballot_fix_disabled",
                 static_cast<std::uint32_t>(kShaderCompileRva));
  EmitPatchEvent(BridgeVgpuPso535CullFixEnabled != 0
                     ? "pso535_cull_fix_enabled"
                     : "pso535_cull_fix_disabled",
                 static_cast<std::uint32_t>(
                     kCreateGraphicsPipelineStateVtableIndex));
  EmitPatchEvent(BridgeVgpuGroundFixEnabled != 0 ? "ground_fetch_fix_enabled"
                                                 : "ground_fetch_fix_disabled",
                 static_cast<std::uint32_t>(kShaderCompileRva));
  EmitPatchEvent("screen_space_vpos_fix_enabled",
                 static_cast<std::uint32_t>(kShaderCompileRva));
  EmitPatchEvent("exposure_sample_guard_enabled",
                 static_cast<std::uint32_t>(kShaderCompileRva));
  EmitPatchEvent("null_pipeline_state_guard_enabled",
                 static_cast<std::uint32_t>(kNullPipelineStateGuardRva));
  EmitPatchEvent("texture_unpack_endian_fix_configured",
                 BridgeVgpuTextureEndianFixEnabled,
                 BridgeVgpuTextureEndianFixBudget);
  EmitPatchEvent("sampler_address_modes_fixed",
                 static_cast<std::uint32_t>(kSamplerAddressModeTableRva), 3);
  return true;
}

void RemovePinnedVgpuPatch() noexcept {
  RemoveGraphicsCommandListHooks();
  RemoveRenderTargetViewHook();
  RemovePixDescriptorHooks();
  RemoveConstantBufferViewHook();
  RemovePipelineStateStreamHook();
  RemoveComputePipelineStateHook();
  RemoveGraphicsPipelineStateHook();
  ReleaseMappedUploadBuffers();
  auto *const target = g_patchTarget;
  if (target == nullptr) {
    return;
  }
  auto *const samplerAddressModeTarget = g_samplerAddressModeTarget;
  auto *const tightAlignmentTarget = g_tightAlignmentTarget;
  auto *const createHeapCallTarget = g_createHeapCallTarget;
  auto *const modernPlacedResourceCallTarget = g_modernPlacedResourceCallTarget;
  auto *const legacyPlacedResourceCallTarget = g_legacyPlacedResourceCallTarget;
  auto *const secondaryModernPlacedResourceCallTarget =
      g_secondaryModernPlacedResourceCallTarget;
  auto *const coldLegacyPlacedResourceSequenceTarget =
      g_coldLegacyPlacedResourceSequenceTarget;
  auto *const nullPipelineStateTarget = g_nullPipelineStateTarget;
  auto *const callRelayRegion = g_callRelayRegion;

  if (!std::equal(g_installedDetour.begin(), g_installedDetour.end(), target)) {
    SetStatus(PatchStatus::DetourChanged);
    EmitPatchEvent("remove_failure", 0);
    return;
  }
  if (nullPipelineStateTarget != nullptr &&
      !std::equal(g_installedNullPipelineStateDetour.begin(),
                  g_installedNullPipelineStateDetour.end(),
                  nullPipelineStateTarget)) {
    SetStatus(PatchStatus::NullPipelineStateDetourChanged);
    BridgeVgpuNullPipelineStateGuardFailure =
        static_cast<std::uint32_t>(PatchStatus::NullPipelineStateDetourChanged);
    EmitPatchEvent("remove_failure",
                   static_cast<std::uint32_t>(kNullPipelineStateGuardRva));
    return;
  }
  if (tightAlignmentTarget == nullptr ||
      !std::equal(kDisabledTightAlignmentGate.begin(),
                  kDisabledTightAlignmentGate.end(), tightAlignmentTarget)) {
    SetStatus(PatchStatus::TightAlignmentGateChanged);
    EmitPatchEvent("remove_failure", 0);
    return;
  }
  if (createHeapCallTarget == nullptr ||
      !std::equal(g_installedCreateHeapCall.begin(),
                  g_installedCreateHeapCall.end(), createHeapCallTarget)) {
    SetStatus(PatchStatus::CreateHeapCallChanged);
    EmitPatchEvent("remove_failure", 0);
    return;
  }
  if (modernPlacedResourceCallTarget == nullptr ||
      !std::equal(g_installedModernPlacedResourceCall.begin(),
                  g_installedModernPlacedResourceCall.end(),
                  modernPlacedResourceCallTarget)) {
    SetStatus(PatchStatus::PlacedResourceCallChanged);
    EmitPatchEvent("remove_failure",
                   static_cast<std::uint32_t>(kModernPlacedResourceCallRva));
    return;
  }
  if (secondaryModernPlacedResourceCallTarget == nullptr ||
      !std::equal(g_installedSecondaryModernPlacedResourceCall.begin(),
                  g_installedSecondaryModernPlacedResourceCall.end(),
                  secondaryModernPlacedResourceCallTarget)) {
    SetStatus(PatchStatus::PlacedResourceCallChanged);
    EmitPatchEvent(
        "remove_failure",
        static_cast<std::uint32_t>(kSecondaryModernPlacedResourceCallRva));
    return;
  }
  if (coldLegacyPlacedResourceSequenceTarget == nullptr ||
      !std::equal(g_installedColdLegacyPlacedResourceSequence.begin(),
                  g_installedColdLegacyPlacedResourceSequence.end(),
                  coldLegacyPlacedResourceSequenceTarget)) {
    SetStatus(PatchStatus::PlacedResourceCallChanged);
    EmitPatchEvent("remove_failure", static_cast<std::uint32_t>(
                                         kColdLegacyPlacedResourceCallRva));
    return;
  }
  if (legacyPlacedResourceCallTarget == nullptr ||
      !std::equal(g_installedLegacyPlacedResourceCall.begin(),
                  g_installedLegacyPlacedResourceCall.end(),
                  legacyPlacedResourceCallTarget)) {
    SetStatus(PatchStatus::PlacedResourceCallChanged);
    EmitPatchEvent("remove_failure",
                   static_cast<std::uint32_t>(kLegacyPlacedResourceCallRva));
    return;
  }
  if (samplerAddressModeTarget != nullptr &&
      !std::equal(kFixedSamplerAddressModes.begin(),
                  kFixedSamplerAddressModes.end(), samplerAddressModeTarget)) {
    SetStatus(PatchStatus::SamplerAddressModeTableChanged);
    EmitPatchEvent("remove_failure",
                   static_cast<std::uint32_t>(kSamplerAddressModeTableRva));
    return;
  }
  if (!RemoveTextureTransferHook()) {
    EmitPatchEvent("remove_failure",
                   static_cast<std::uint32_t>(kTextureTransferRva));
    return;
  }
  if (!RemoveStructuredTextureTransferHook()) {
    EmitPatchEvent("remove_failure",
                   static_cast<std::uint32_t>(kStructuredTextureTransferRva));
    return;
  }
  if (!RemoveConstantUploadHook()) {
    EmitPatchEvent("remove_failure",
                   static_cast<std::uint32_t>(kConstantUploadRva));
    return;
  }
  if (!RemoveXenosTranslateHook()) {
    EmitPatchEvent("remove_failure",
                   static_cast<std::uint32_t>(kXenosTranslateRva));
    return;
  }
  if (!RemoveShaderCompileHook()) {
    EmitPatchEvent("remove_failure",
                   static_cast<std::uint32_t>(kShaderCompileRva));
    return;
  }

  const auto reinstallColdLegacy = [&]() noexcept {
    WriteInstructionGate(
        coldLegacyPlacedResourceSequenceTarget,
        g_installedColdLegacyPlacedResourceSequence.data(),
        g_installedColdLegacyPlacedResourceSequence.size(),
        PatchStatus::PlacedResourceCallProtectionFailure,
        PatchStatus::PlacedResourceCallProtectionRestoreFailure);
  };
  const auto reinstallSecondaryModern = [&]() noexcept {
    WriteInstructionGate(
        secondaryModernPlacedResourceCallTarget,
        g_installedSecondaryModernPlacedResourceCall.data(),
        g_installedSecondaryModernPlacedResourceCall.size(),
        PatchStatus::PlacedResourceCallProtectionFailure,
        PatchStatus::PlacedResourceCallProtectionRestoreFailure);
  };
  const auto reinstallLegacy = [&]() noexcept {
    WriteInstructionGate(
        legacyPlacedResourceCallTarget,
        g_installedLegacyPlacedResourceCall.data(),
        g_installedLegacyPlacedResourceCall.size(),
        PatchStatus::PlacedResourceCallProtectionFailure,
        PatchStatus::PlacedResourceCallProtectionRestoreFailure);
  };
  const auto reinstallModern = [&]() noexcept {
    WriteInstructionGate(
        modernPlacedResourceCallTarget,
        g_installedModernPlacedResourceCall.data(),
        g_installedModernPlacedResourceCall.size(),
        PatchStatus::PlacedResourceCallProtectionFailure,
        PatchStatus::PlacedResourceCallProtectionRestoreFailure);
  };

  if (!WriteInstructionGate(
          coldLegacyPlacedResourceSequenceTarget,
          kExpectedColdLegacyPlacedResourceSequence.data(),
          kExpectedColdLegacyPlacedResourceSequence.size(),
          PatchStatus::PlacedResourceCallProtectionFailure,
          PatchStatus::PlacedResourceCallProtectionRestoreFailure)) {
    EmitPatchEvent("remove_failure", 0);
    return;
  }
  if (!WriteInstructionGate(
          secondaryModernPlacedResourceCallTarget,
          kExpectedSecondaryModernPlacedResourceCall.data(),
          kExpectedSecondaryModernPlacedResourceCall.size(),
          PatchStatus::PlacedResourceCallProtectionFailure,
          PatchStatus::PlacedResourceCallProtectionRestoreFailure)) {
    reinstallColdLegacy();
    EmitPatchEvent("remove_failure", 0);
    return;
  }
  if (!WriteInstructionGate(
          legacyPlacedResourceCallTarget,
          kExpectedLegacyPlacedResourceCall.data(),
          kExpectedLegacyPlacedResourceCall.size(),
          PatchStatus::PlacedResourceCallProtectionFailure,
          PatchStatus::PlacedResourceCallProtectionRestoreFailure)) {
    reinstallSecondaryModern();
    reinstallColdLegacy();
    EmitPatchEvent("remove_failure", 0);
    return;
  }
  if (!WriteInstructionGate(
          modernPlacedResourceCallTarget,
          kExpectedModernPlacedResourceCall.data(),
          kExpectedModernPlacedResourceCall.size(),
          PatchStatus::PlacedResourceCallProtectionFailure,
          PatchStatus::PlacedResourceCallProtectionRestoreFailure)) {
    reinstallLegacy();
    reinstallSecondaryModern();
    reinstallColdLegacy();
    EmitPatchEvent("remove_failure", 0);
    return;
  }
  if (!WriteInstructionGate(
          createHeapCallTarget, kExpectedCreateHeapCall.data(),
          kExpectedCreateHeapCall.size(),
          PatchStatus::CreateHeapCallProtectionFailure,
          PatchStatus::CreateHeapCallProtectionRestoreFailure)) {
    reinstallModern();
    reinstallLegacy();
    reinstallSecondaryModern();
    reinstallColdLegacy();
    EmitPatchEvent("remove_failure", 0);
    return;
  }

  if (nullPipelineStateTarget != nullptr &&
      !WriteInstructionGate(
          nullPipelineStateTarget, kExpectedNullPipelineStateSequence.data(),
          kExpectedNullPipelineStateSequence.size(),
          PatchStatus::NullPipelineStateProtectionFailure,
          PatchStatus::NullPipelineStateProtectionRestoreFailure)) {
    BridgeVgpuNullPipelineStateGuardFailure = BridgeVgpuPatchStatus;
    EmitPatchEvent("remove_failure",
                   static_cast<std::uint32_t>(kNullPipelineStateGuardRva));
    return;
  }
  g_nullPipelineStateTarget = nullptr;
  VgpuNullPipelineStateNormalTarget = 0;
  VgpuNullPipelineStateSkipTarget = 0;
  BridgeVgpuNullPipelineStateGuardInstalled = 0;
  BridgeVgpuNullPipelineStateGuardFailure = ERROR_SUCCESS;

  DWORD oldProtection = 0;
  if (!VirtualProtect(target, kFetchTableDetourSize, PAGE_EXECUTE_READWRITE,
                      &oldProtection)) {
    SetStatus(PatchStatus::TargetProtectionFailure);
    EmitPatchEvent("remove_failure", 0);
    return;
  }

  std::memcpy(target, kExpectedPrologue.data(), kExpectedPrologue.size());
  FlushInstructionCache(GetCurrentProcess(), target, kExpectedPrologue.size());
  DWORD ignoredProtection = 0;
  VirtualProtect(target, kFetchTableDetourSize, oldProtection,
                 &ignoredProtection);

  if (!WriteInstructionGate(
          tightAlignmentTarget, kExpectedTightAlignmentGate.data(),
          kExpectedTightAlignmentGate.size(),
          PatchStatus::TightAlignmentProtectionFailure,
          PatchStatus::TightAlignmentProtectionRestoreFailure)) {
    EmitPatchEvent("remove_failure", 0);
    return;
  }
  if (samplerAddressModeTarget != nullptr &&
      !WriteSamplerAddressModeTable(samplerAddressModeTarget,
                                    kExpectedSamplerAddressModes)) {
    EmitPatchEvent("remove_failure",
                   static_cast<std::uint32_t>(kSamplerAddressModeTableRva));
    return;
  }

  {
    std::scoped_lock lock(g_discardMutex);
    ResetDiscardContextLocked();
  }
  {
    std::scoped_lock lock(g_edramRestoreDrawMutex);
    g_edramRestoreDrawPipelineState = nullptr;
    g_edramRestoreDrawPipelineMatches = false;
  }
  {
    std::scoped_lock lock(g_edramDrawReplacementMutex);
    ResetEdramDrawReplacementsLocked();
  }
  for (auto &slot : g_restartTerrainPipelineStates) {
    slot.store(nullptr, std::memory_order_release);
  }
  for (auto &slot : g_restartSkyPipelineStates) {
    slot.store(nullptr, std::memory_order_release);
  }

  auto *const trampoline = g_trampoline;
  g_patchTarget = nullptr;
  g_samplerAddressModeTarget = nullptr;
  g_tightAlignmentTarget = nullptr;
  g_createHeapCallTarget = nullptr;
  g_modernPlacedResourceCallTarget = nullptr;
  g_legacyPlacedResourceCallTarget = nullptr;
  g_secondaryModernPlacedResourceCallTarget = nullptr;
  g_coldLegacyPlacedResourceSequenceTarget = nullptr;
  g_nullPipelineStateTarget = nullptr;
  g_callRelayRegion = nullptr;
  g_moduleBase = nullptr;
  g_nativeFetchTable = nullptr;
  g_trampoline = nullptr;
  if (trampoline != nullptr) {
    VirtualFree(trampoline, 0, MEM_RELEASE);
  }
  if (callRelayRegion != nullptr) {
    VirtualFree(callRelayRegion, 0, MEM_RELEASE);
  }
  SetStatus(PatchStatus::Removed);
  EmitPatchEvent("remove", 0);
}
} // namespace xeo3::vgpu

#undef XEO3_VGPU_EXPORT
