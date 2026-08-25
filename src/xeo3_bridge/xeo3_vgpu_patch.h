#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace xeo3::vgpu {
constexpr std::uint32_t kNativeFetchTableCapacity = 16;
constexpr std::uint32_t kXenosFetchTableCapacity = 32;
constexpr std::size_t kXenosSamplerAddressModeCount = 8;
constexpr std::size_t kFetchTableDetourSize = 17;
constexpr std::size_t kShaderCompileDetourSize = 16;
constexpr std::size_t kTextureTransferDetourSize = 18;
constexpr std::size_t kStructuredTextureTransferDetourSize = 16;
constexpr std::size_t kConstantUploadDetourSize = 18;
constexpr std::size_t kAc6Pso341TaskBufferSize = 0x300;
constexpr std::uint64_t kAc6Pso341UploadArenaSize = 0xC0000;
constexpr std::size_t kAc6Pso341ComputeShaderSize = 5032;
constexpr std::uint32_t kD3d12UploadHeapType = 2;
constexpr std::uint32_t kD3d12GpuUploadHeapType = 5;
constexpr std::uint32_t kAc6Pso341BrokenPackedDimensions = 0x02D00000;
constexpr std::uint32_t kAc6Pso341CorrectedPackedDimensions = 0x02D00500;
constexpr std::size_t kNullPipelineStateDetourSize = 18;
constexpr std::size_t kTightAlignmentGateSize = 6;
constexpr std::size_t kPlacedResourceGateSize = 6;
constexpr std::size_t kCreateHeapCallSize = 6;
constexpr std::size_t kPlacedResourceCallSize = 7;
constexpr std::size_t kCallRelaySize = 16;
constexpr std::size_t kDrawRecordMinimumSize = 0xD4;

struct FetchAllocation {
  void *allocator;
  std::uint32_t index;
  std::uint32_t end;
};

static_assert(sizeof(FetchAllocation) == 16);
static_assert(offsetof(FetchAllocation, index) == 8);
static_assert(offsetof(FetchAllocation, end) == 12);

using AllocateFetchTable = FetchAllocation *(*)(void *allocator,
                                                FetchAllocation *allocation,
                                                int entryCount);
using UploadFetchTable =
    void (*)(void *uploadInterface, std::uint32_t uploadKind,
             std::uint64_t *uploadAddress, std::uint32_t *uploadCount,
             std::uint32_t entryCount, const std::uint64_t *entries,
             std::uint64_t reserved, std::uint32_t synchronous);

struct ExtendedFetchTableCallbacks {
  AllocateFetchTable allocate;
  UploadFetchTable upload;
};

struct GraphicsPipelineSignature {
  std::array<std::uint8_t, 32> vertexShaderSha256{};
  std::array<std::uint8_t, 32> pixelShaderSha256{};
  std::uint64_t vertexShaderSize = 0;
  std::uint64_t pixelShaderSize = 0;
  std::uint32_t sampleMask = 0;
  std::uint32_t primitiveTopologyType = 0;
  std::uint32_t sampleCount = 0;
  std::uint32_t sampleQuality = 0;
  std::uint32_t renderTargetCount = 0;
  std::uint32_t renderTarget0Format = 0;
  std::uint32_t depthStencilFormat = 0;
  std::uint32_t inputElementCount = 0;
  std::uint32_t renderTarget0WriteMask = 0;
  bool hasExpectedInputLayout = false;
  bool hasExpectedFixedState = false;
  bool hasExpectedEdramScaleFixedState = false;
  bool hasExpectedEdramLoadFixedState = false;
};

struct DrawRecordSignature {
  std::uintptr_t rootSignature = 0;
  std::uintptr_t pipelineState = 0;
  std::uint32_t viewportWidthBits = 0;
  std::uint32_t viewportHeightBits = 0;
  std::uint32_t viewportMinDepthBits = 0;
  std::uint32_t viewportMaxDepthBits = 0;
  std::uint32_t viewportTopLeftXBits = 0;
  std::uint32_t viewportTopLeftYBits = 0;
  std::uint32_t scissorRight = 0;
  std::uint32_t scissorBottom = 0;
  std::uint32_t recordKind = 0;
  std::uint32_t vertexCount = 0;
  std::uint32_t startVertex = 0;
};

enum class Ac6EdramDrawPipeline : std::uint32_t {
  None = 0,
  Scale = 620,
  Load = 523,
};

enum class Ac6EdramConstantKind : std::uint32_t {
  None = 0,
  Candidate = 1,
  Load = 523,
  Scale = 620,
};

enum class Ac6Pso341TaskWidthState : std::uint32_t {
  NotCandidate = 0,
  Broken = 1,
  Correct = 2,
};

std::uint32_t
BuildExtendedFetchTable(void *cache, std::uint64_t *outputGpuAddress,
                        const void *source, std::uint32_t entryCount,
                        void *allocatorContext, void *uploadInterface,
                        const ExtendedFetchTableCallbacks &callbacks) noexcept;

using HashFileSha256 = bool (*)(const wchar_t *path,
                                std::array<std::uint8_t, 32> &digest) noexcept;

bool InstallPinnedVgpuPatch(HashFileSha256 hashFile) noexcept;
void RemovePinnedVgpuPatch() noexcept;

namespace detail {
std::array<std::uint8_t, kFetchTableDetourSize>
EncodeAbsoluteJump(const void *target) noexcept;

std::array<std::uint8_t, kShaderCompileDetourSize>
EncodeShaderCompileJump(const void *target) noexcept;

std::array<std::uint8_t, kTextureTransferDetourSize>
EncodeTextureTransferJump(const void *target) noexcept;

std::array<std::uint8_t, kStructuredTextureTransferDetourSize>
EncodeStructuredTextureTransferJump(const void *target) noexcept;

std::array<std::uint8_t, kConstantUploadDetourSize>
EncodeConstantUploadJump(const void *target) noexcept;

std::array<std::uint8_t, kNullPipelineStateDetourSize>
EncodeNullPipelineStateJump(const void *target) noexcept;

std::array<std::uint8_t, kCallRelaySize>
EncodeAbsoluteCallRelay(const void *target) noexcept;

bool EncodeRelativeCall(
    const void *instruction, const void *target,
    std::array<std::uint8_t, kCreateHeapCallSize> &call) noexcept;

bool EncodeRelativeCall(
    const void *instruction, const void *target,
    std::array<std::uint8_t, kPlacedResourceCallSize> &call) noexcept;

bool HasExpectedFetchTablePrologue(const std::uint8_t *bytes,
                                   std::size_t byteCount) noexcept;

bool HasExpectedShaderCompilePrologue(const std::uint8_t *bytes,
                                      std::size_t byteCount) noexcept;

bool HasExpectedTextureTransferPrologue(const std::uint8_t *bytes,
                                        std::size_t byteCount) noexcept;

bool HasExpectedStructuredTextureTransferPrologue(
    const std::uint8_t *bytes, std::size_t byteCount) noexcept;

bool HasExpectedConstantUploadPrologue(const std::uint8_t *bytes,
                                       std::size_t byteCount) noexcept;

bool HasExpectedNullPipelineStateSequence(const std::uint8_t *bytes,
                                          std::size_t byteCount) noexcept;

std::array<std::uint32_t, kXenosSamplerAddressModeCount>
GetD3d12SamplerAddressModeTable() noexcept;

bool HasExpectedSamplerAddressModeTable(const std::uint32_t *modes,
                                        std::size_t modeCount) noexcept;

bool IsGeneratedVertexShader(const void *source,
                             std::size_t sourceSize) noexcept;

bool PatchXenosScalarReciprocals(const void *source, std::size_t sourceSize,
                                 std::string &patchedSource,
                                 std::uint32_t &patchCount) noexcept;

bool PatchAc6ScreenSpaceVposScale(const void *source, std::size_t sourceSize,
                                  std::string &patchedSource,
                                  std::uint32_t &patchCount) noexcept;

bool MatchesAc6Pso537ShaderFingerprint(
    std::size_t sourceSize,
    const std::array<std::uint8_t, 32> &digest) noexcept;

bool PatchAc6Pso537HalfWidthUv(const void *source, std::size_t sourceSize,
                               std::string &patchedSource,
                               std::uint32_t &patchCount) noexcept;

bool PatchAc6ToneMapInterpolant(const void *source, std::size_t sourceSize,
                                 std::string &patchedSource,
                                 std::uint32_t &patchCount) noexcept;

bool MatchesAc6ExposureShaderFingerprint(
    std::size_t sourceSize,
    const std::array<std::uint8_t, 32> &digest) noexcept;

bool PatchAc6ExposureSample(const void *source, std::size_t sourceSize,
                            std::string &patchedSource,
                            std::uint32_t &patchCount) noexcept;

bool IsAc6TextureUnpackTransfer(
    const std::array<std::uint32_t, 6> &constants) noexcept;

bool PatchAc6TextureUnpackEndian(
    std::array<std::uint32_t, 6> &constants,
    std::uint32_t replacementEndian) noexcept;

std::array<std::uint32_t, 6>
ExtractStructuredTextureTransferConstants(const void *descriptor) noexcept;

Ac6Pso341TaskWidthState ClassifyAc6Pso341TaskWidth(
    const void *source, std::size_t sourceSize) noexcept;

bool ShouldTrackAc6Pso341UploadBuffer(std::uint32_t heapType,
                                      bool isBuffer,
                                      std::uint64_t resourceSize) noexcept;

bool ResolveAc6Pso341UploadOffset(std::uint64_t gpuBase,
                                  std::uint32_t stride,
                                  std::uint32_t slotCount,
                                  std::uint64_t mappedSpan,
                                  std::uint64_t gpuAddress,
                                  std::uint64_t sourceSize,
                                  std::uint64_t &cpuOffset) noexcept;

std::uint64_t DecodeAmdConstantBufferGpuAddress(
    const std::array<std::uint64_t, 4> &descriptorWords) noexcept;

bool MatchesAc6Pso341ComputeShaderFingerprint(
    std::size_t shaderSize,
    const std::array<std::uint8_t, 32> &digest) noexcept;

bool MatchesAc6Pso341CachedPipelineBlob(
    std::size_t blobSize,
    const std::array<std::uint8_t, 32> &digest) noexcept;

bool PatchAc6Pso341TaskWidth(void *source, std::size_t sourceSize,
                             std::uint32_t &originalPackedDimensions,
                             std::uint32_t &replacementPackedDimensions)
    noexcept;

Ac6EdramConstantKind ClassifyAc6EdramTransferConstants(
    const std::array<std::uint32_t, 16> &constants) noexcept;

std::array<std::uint64_t, 8> PackAc6EdramTransferConstants(
    const std::array<std::uint32_t, 16> &constants) noexcept;

bool PatchAc6GroundFetchIndices(const void *source, std::size_t sourceSize,
                                std::string &patchedSource,
                                std::uint32_t &patchCount) noexcept;

bool PatchXenosIndexBufferSemantics(const void *source, std::size_t sourceSize,
                                    std::string &patchedSource,
                                    std::uint32_t &patchCount) noexcept;

bool HashBytesSha256(const void *bytes, std::size_t byteCount,
                    std::array<std::uint8_t, 32> &digest) noexcept;

bool ExtractGraphicsPipelineStreamSignature(
    const void *stream, std::size_t streamSize,
    GraphicsPipelineSignature &signature,
    std::size_t &pixelShaderBytecodeOffset) noexcept;

bool IsAc6CorruptEdramRestorePipeline(
    const GraphicsPipelineSignature &signature) noexcept;

bool IsAc6EdramScalePipeline(
    const GraphicsPipelineSignature &signature) noexcept;

bool IsAc6EdramScalePipelineDescriptor(
    const GraphicsPipelineSignature &signature) noexcept;

const void *GetAc6EdramScaleFixPixelShader(
    std::size_t &byteCount) noexcept;

bool IsAc6EdramLoadPipeline(
    const GraphicsPipelineSignature &signature) noexcept;

bool IsAc6EdramLoadPipelineDescriptor(
    const GraphicsPipelineSignature &signature) noexcept;

const void *GetAc6EdramLoadFixPixelShader(
    std::size_t &byteCount) noexcept;

const void *GetAc6EdramTransferVertexShader(
    std::size_t &byteCount) noexcept;

const void *GetAc6Pso341WidthFixComputeShader(
    std::size_t &byteCount) noexcept;


bool PatchAc6HalfWidthFullscreenScissor(
    void *record, std::size_t recordSize) noexcept;

bool ExtractDrawRecordSignature(const void *record, std::size_t recordSize,
                                DrawRecordSignature &signature) noexcept;

Ac6EdramDrawPipeline ClassifyAc6EdramDrawPipeline(
    const DrawRecordSignature &signature,
    const void *activePipelineState) noexcept;

Ac6EdramDrawPipeline ClassifyAc6EdramCachedPipelineBlob(
    std::size_t blobSize,
    const std::array<std::uint8_t, 32> &digest) noexcept;

bool IsAc6CorruptEdramRestoreDrawSignature(
    const DrawRecordSignature &signature,
    const void *pipelineState) noexcept;

bool ShouldSuppressAc6HostEdramRestoreDraw(
    const DrawRecordSignature &signature, const void *pipelineState,
    Ac6EdramDrawPipeline fingerprintedPipeline, bool enabled) noexcept;

bool IsAc6CorruptEdramRestoreDrawRecord(
    const void *record, std::size_t recordSize,
    const void *pipelineState) noexcept;

bool IsAc6CorruptEdramRestoreCachedBlob(
    std::size_t blobSize,
    const std::array<std::uint8_t, 32> &digest) noexcept;

bool HasExpectedTightAlignmentGate(const std::uint8_t *bytes,
                                   std::size_t byteCount) noexcept;

bool HasExpectedPlacedResourceGate(const std::uint8_t *bytes,
                                   std::size_t byteCount) noexcept;

bool ShouldRetryInvalidModernBufferAsLegacy(
    std::uint32_t result, bool hasResource, std::uint32_t dimension,
    std::uint32_t initialLayout, std::uint32_t castableFormatCount) noexcept;

bool DoesPlacedResourceOverflowHeap(std::uint64_t heapSize,
                                    std::uint64_t heapOffset,
                                    std::uint64_t allocationSize) noexcept;

bool ShouldFallbackPlacedResourceToCommitted(
    std::uint32_t result, bool hasResource, std::uint64_t heapSize,
    std::uint64_t heapOffset, std::uint64_t allocationSize) noexcept;

std::uint32_t
GetCommittedResourceHeapFlags(std::uint32_t sourceHeapFlags) noexcept;
} // namespace detail
} // namespace xeo3::vgpu
