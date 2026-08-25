#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "xeo3_bridge/xeo3_vgpu_patch.h"

#include <Windows.h>
#include <d3d12.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#define CHECK(expression)                                                      \
  do {                                                                         \
    if (!(expression)) {                                                       \
      return __LINE__;                                                         \
    }                                                                          \
  } while (false)

namespace {
struct CallbackTrace {
  void *expectedAllocator = nullptr;
  void *expectedUploadInterface = nullptr;
  std::uint32_t allocateCount = 0;
  std::uint32_t uploadCount = 0;
  std::uint64_t uploadAddress = 0;
  std::array<std::uint64_t, xeo3::vgpu::kXenosFetchTableCapacity> entries{};
};

CallbackTrace g_trace;

xeo3::vgpu::FetchAllocation *
FakeAllocate(void *allocator, xeo3::vgpu::FetchAllocation *allocation,
             const int entryCount) {
  if (allocator != g_trace.expectedAllocator || entryCount < 0) {
    return nullptr;
  }
  g_trace.allocateCount = static_cast<std::uint32_t>(entryCount);
  allocation->allocator = allocator;
  allocation->index = 7;
  allocation->end = 7 + static_cast<std::uint32_t>(entryCount);
  return allocation;
}

void FakeUpload(void *uploadInterface, const std::uint32_t uploadKind,
                std::uint64_t *uploadAddress, std::uint32_t *uploadCount,
                const std::uint32_t entryCount, const std::uint64_t *entries,
                const std::uint64_t reserved, const std::uint32_t synchronous) {
  if (uploadInterface != g_trace.expectedUploadInterface || uploadKind != 1 ||
      uploadAddress == nullptr || uploadCount == nullptr ||
      entries == nullptr || reserved != 0 || synchronous != 1) {
    g_trace.uploadCount = UINT32_MAX;
    return;
  }
  g_trace.uploadCount = entryCount;
  g_trace.uploadAddress = *uploadAddress;
  for (std::uint32_t index = 0; index < entryCount; ++index) {
    g_trace.entries[index] = entries[index];
  }
}

template <typename T, std::size_t Size>
void Store(std::array<std::uint8_t, Size> &storage, const std::size_t offset,
           const T &value) {
  std::memcpy(storage.data() + offset, &value, sizeof(value));
}

template <typename T, std::size_t Size>
T Load(const std::array<std::uint8_t, Size> &storage,
       const std::size_t offset) {
  T value{};
  std::memcpy(&value, storage.data() + offset, sizeof(value));
  return value;
}

template <D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type, typename Value>
struct alignas(void *) TestPipelineStateStreamSubobject {
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type = Type;
  Value value{};
};
} // namespace

int main() {
  constexpr std::uint32_t entryCount = 20;
  constexpr std::uint32_t stride = 0x40;
  constexpr std::uint64_t uploadBase = 0x0000012340000000;
  constexpr std::uint64_t gpuBase = 0x0000024680000000;
  constexpr std::uint64_t generation = 0xAABBCCDDEEFF0011;
  constexpr std::uint8_t cacheSentinel = 0xA5;

  std::array<std::uint8_t, 0x210> cache{};
  cache.fill(cacheSentinel);
  std::array<std::uint8_t, 0x380> allocatorContext{};
  Store(allocatorContext, 0x310, stride);
  Store(allocatorContext, 0x318, uploadBase);
  Store(allocatorContext, 0x320, gpuBase);
  Store(allocatorContext, 0x360, generation);

  std::array<std::uint8_t, 0x80 + xeo3::vgpu::kXenosFetchTableCapacity * 16>
      source{};
  for (std::uint32_t index = 0; index < xeo3::vgpu::kXenosFetchTableCapacity;
       ++index) {
    const std::uint64_t value = 0xC0DEC00000000000ULL + index;
    const std::uint64_t ignored = 0xBAD0000000000000ULL + index;
    std::memcpy(source.data() + 0x80 + index * 16, &value, sizeof(value));
    std::memcpy(source.data() + 0x88 + index * 16, &ignored, sizeof(ignored));
  }

  const std::array<std::uint8_t, 16 * sizeof(std::uint64_t)>
      originalNativeCache = [&cache] {
        std::array<std::uint8_t, 16 * sizeof(std::uint64_t)> bytes{};
        std::memcpy(bytes.data(), cache.data() + 0x160, bytes.size());
        return bytes;
      }();

  int uploadInterface = 0;
  g_trace = {};
  g_trace.expectedAllocator = allocatorContext.data() + 0x300;
  g_trace.expectedUploadInterface = &uploadInterface;
  const xeo3::vgpu::ExtendedFetchTableCallbacks callbacks{
      &FakeAllocate,
      &FakeUpload,
  };

  std::uint64_t outputGpuAddress = 0;
  CHECK(xeo3::vgpu::BuildExtendedFetchTable(
            cache.data(), &outputGpuAddress, source.data(), entryCount,
            allocatorContext.data(), &uploadInterface, callbacks) == 1);

  CHECK(g_trace.allocateCount == entryCount);
  CHECK(g_trace.uploadCount == entryCount);
  const auto expectedOffset = static_cast<std::uint32_t>(7 * stride);
  CHECK(g_trace.uploadAddress == uploadBase + expectedOffset);
  CHECK(outputGpuAddress == gpuBase + expectedOffset);
  for (std::uint32_t index = 0; index < entryCount; ++index) {
    CHECK(g_trace.entries[index] == 0xC0DEC00000000000ULL + index);
  }

  CHECK(Load<std::uint32_t>(cache, 0x1E0) == entryCount);
  const auto cachedAllocation = Load<xeo3::vgpu::FetchAllocation>(cache, 0x1E8);
  CHECK(cachedAllocation.allocator == allocatorContext.data() + 0x300);
  CHECK(cachedAllocation.index == 7);
  CHECK(cachedAllocation.end == 7 + entryCount);
  CHECK(Load<std::uint64_t>(cache, 0x1F8) == generation);
  CHECK(std::memcmp(cache.data() + 0x160, originalNativeCache.data(),
                    originalNativeCache.size()) == 0);

  g_trace.allocateCount = 0;
  g_trace.uploadCount = 0;
  outputGpuAddress = 0x1122334455667788;
  CHECK(xeo3::vgpu::BuildExtendedFetchTable(
            cache.data(), &outputGpuAddress, source.data(),
            xeo3::vgpu::kXenosFetchTableCapacity + 1, allocatorContext.data(),
            &uploadInterface, callbacks) == 0);
  CHECK(g_trace.allocateCount == 0);
  CHECK(g_trace.uploadCount == 0);
  CHECK(outputGpuAddress == 0x1122334455667788);

  CHECK(xeo3::vgpu::BuildExtendedFetchTable(
            cache.data(), &outputGpuAddress, source.data(),
            xeo3::vgpu::kNativeFetchTableCapacity, allocatorContext.data(),
            &uploadInterface, callbacks) == 0);

  constexpr auto targetAddress =
      static_cast<std::uintptr_t>(0x1122334455667788);
  const auto jump = xeo3::vgpu::detail::EncodeAbsoluteJump(
      reinterpret_cast<const void *>(targetAddress));
  CHECK(jump[0] == 0x48);
  CHECK(jump[1] == 0xB8);
  CHECK(jump[10] == 0xFF);
  CHECK(jump[11] == 0xE0);
  for (std::size_t index = 12; index < jump.size(); ++index) {
    CHECK(jump[index] == 0x90);
  }
  std::uintptr_t encodedAddress = 0;
  std::memcpy(&encodedAddress, jump.data() + 2, sizeof(encodedAddress));
  CHECK(encodedAddress == targetAddress);

  const auto shaderCompileJump = xeo3::vgpu::detail::EncodeShaderCompileJump(
      reinterpret_cast<const void *>(targetAddress));
  CHECK(shaderCompileJump[0] == 0x48);
  CHECK(shaderCompileJump[1] == 0xB8);
  CHECK(shaderCompileJump[10] == 0xFF);
  CHECK(shaderCompileJump[11] == 0xE0);
  for (std::size_t index = 12; index < shaderCompileJump.size(); ++index) {
    CHECK(shaderCompileJump[index] == 0x90);
  }
  encodedAddress = 0;
  std::memcpy(&encodedAddress, shaderCompileJump.data() + 2,
              sizeof(encodedAddress));
  CHECK(encodedAddress == targetAddress);

  const auto textureTransferJump =
      xeo3::vgpu::detail::EncodeTextureTransferJump(
          reinterpret_cast<const void *>(targetAddress));
  CHECK(textureTransferJump[0] == 0x48);
  CHECK(textureTransferJump[1] == 0xB8);
  CHECK(textureTransferJump[10] == 0xFF);
  CHECK(textureTransferJump[11] == 0xE0);
  for (std::size_t index = 12; index < textureTransferJump.size(); ++index) {
    CHECK(textureTransferJump[index] == 0x90);
  }
  encodedAddress = 0;
  std::memcpy(&encodedAddress, textureTransferJump.data() + 2,
              sizeof(encodedAddress));
  CHECK(encodedAddress == targetAddress);

  const auto structuredTextureTransferJump =
      xeo3::vgpu::detail::EncodeStructuredTextureTransferJump(
          reinterpret_cast<const void *>(targetAddress));
  CHECK(structuredTextureTransferJump[0] == 0x48);
  CHECK(structuredTextureTransferJump[1] == 0xB8);
  CHECK(structuredTextureTransferJump[10] == 0xFF);
  CHECK(structuredTextureTransferJump[11] == 0xE0);
  for (std::size_t index = 12; index < structuredTextureTransferJump.size();
       ++index) {
    CHECK(structuredTextureTransferJump[index] == 0x90);
  }
  encodedAddress = 0;
  std::memcpy(&encodedAddress, structuredTextureTransferJump.data() + 2,
              sizeof(encodedAddress));
  CHECK(encodedAddress == targetAddress);

  const auto constantUploadJump =
      xeo3::vgpu::detail::EncodeConstantUploadJump(
          reinterpret_cast<const void *>(targetAddress));
  CHECK(constantUploadJump[0] == 0x48);
  CHECK(constantUploadJump[1] == 0xB8);
  CHECK(constantUploadJump[10] == 0xFF);
  CHECK(constantUploadJump[11] == 0xE0);
  for (std::size_t index = 12; index < constantUploadJump.size(); ++index) {
    CHECK(constantUploadJump[index] == 0x90);
  }
  encodedAddress = 0;
  std::memcpy(&encodedAddress, constantUploadJump.data() + 2,
              sizeof(encodedAddress));
  CHECK(encodedAddress == targetAddress);

  const auto nullPipelineStateJump =
      xeo3::vgpu::detail::EncodeNullPipelineStateJump(
          reinterpret_cast<const void *>(targetAddress));
  CHECK(nullPipelineStateJump[0] == 0x48);
  CHECK(nullPipelineStateJump[1] == 0xB8);
  CHECK(nullPipelineStateJump[10] == 0xFF);
  CHECK(nullPipelineStateJump[11] == 0xE0);
  for (std::size_t index = 12; index < nullPipelineStateJump.size(); ++index) {
    CHECK(nullPipelineStateJump[index] == 0x90);
  }
  encodedAddress = 0;
  std::memcpy(&encodedAddress, nullPipelineStateJump.data() + 2,
              sizeof(encodedAddress));
  CHECK(encodedAddress == targetAddress);

  const auto callRelay = xeo3::vgpu::detail::EncodeAbsoluteCallRelay(
      reinterpret_cast<const void *>(targetAddress));
  CHECK(callRelay[0] == 0x48);
  CHECK(callRelay[1] == 0xB8);
  CHECK(callRelay[10] == 0xFF);
  CHECK(callRelay[11] == 0xE0);
  for (std::size_t index = 12; index < callRelay.size(); ++index) {
    CHECK(callRelay[index] == 0x90);
  }
  encodedAddress = 0;
  std::memcpy(&encodedAddress, callRelay.data() + 2, sizeof(encodedAddress));
  CHECK(encodedAddress == targetAddress);

  constexpr auto callAddress = static_cast<std::uintptr_t>(0x0000000010000000);
  constexpr auto callTarget = static_cast<std::uintptr_t>(0x0000000010012345);
  std::array<std::uint8_t, xeo3::vgpu::kCreateHeapCallSize> relativeCall{};
  CHECK(xeo3::vgpu::detail::EncodeRelativeCall(
      reinterpret_cast<const void *>(callAddress),
      reinterpret_cast<const void *>(callTarget), relativeCall));
  CHECK(relativeCall[0] == 0xE8);
  CHECK(relativeCall[5] == 0x90);
  std::int32_t callDisplacement = 0;
  std::memcpy(&callDisplacement, relativeCall.data() + 1,
              sizeof(callDisplacement));
  CHECK(callAddress + 5 + static_cast<std::intptr_t>(callDisplacement) ==
        callTarget);
  CHECK(!xeo3::vgpu::detail::EncodeRelativeCall(
      reinterpret_cast<const void *>(static_cast<std::uintptr_t>(0x1000)),
      reinterpret_cast<const void *>(
          static_cast<std::uintptr_t>(0x1'0000'1000ULL)),
      relativeCall));

  std::array<std::uint8_t, xeo3::vgpu::kPlacedResourceCallSize>
      placedResourceCall{};
  CHECK(xeo3::vgpu::detail::EncodeRelativeCall(
      reinterpret_cast<const void *>(callAddress),
      reinterpret_cast<const void *>(callTarget), placedResourceCall));
  CHECK(placedResourceCall[0] == 0xE8);
  CHECK(placedResourceCall[5] == 0x90);
  CHECK(placedResourceCall[6] == 0x90);
  std::int32_t placedResourceDisplacement = 0;
  std::memcpy(&placedResourceDisplacement, placedResourceCall.data() + 1,
              sizeof(placedResourceDisplacement));
  CHECK(callAddress + 5 +
            static_cast<std::intptr_t>(placedResourceDisplacement) ==
        callTarget);

  constexpr std::array<std::uint8_t, 17> expectedPrologue{
      0x48, 0x89, 0x5C, 0x24, 0x10, 0x55, 0x56, 0x57, 0x41,
      0x56, 0x41, 0x57, 0x48, 0x8D, 0x6C, 0x24, 0xD1,
  };
  CHECK(xeo3::vgpu::detail::HasExpectedFetchTablePrologue(
      expectedPrologue.data(), expectedPrologue.size()));
  auto badPrologue = expectedPrologue;
  badPrologue[0] ^= 0xFF;
  CHECK(!xeo3::vgpu::detail::HasExpectedFetchTablePrologue(badPrologue.data(),
                                                           badPrologue.size()));

  constexpr std::array<std::uint8_t, 16> expectedShaderCompilePrologue{
      0x4C, 0x89, 0x4C, 0x24, 0x20, 0x4C, 0x89, 0x44,
      0x24, 0x18, 0x55, 0x53, 0x56, 0x57, 0x41, 0x56,
  };
  CHECK(xeo3::vgpu::detail::HasExpectedShaderCompilePrologue(
      expectedShaderCompilePrologue.data(),
      expectedShaderCompilePrologue.size()));
  auto badShaderCompilePrologue = expectedShaderCompilePrologue;
  badShaderCompilePrologue[15] ^= 0xFF;
  CHECK(!xeo3::vgpu::detail::HasExpectedShaderCompilePrologue(
      badShaderCompilePrologue.data(), badShaderCompilePrologue.size()));

  constexpr std::array<std::uint8_t, 18> expectedTextureTransferPrologue{
      0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55,
      0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0x6C, 0x24, 0xF9,
  };
  CHECK(xeo3::vgpu::detail::HasExpectedTextureTransferPrologue(
      expectedTextureTransferPrologue.data(),
      expectedTextureTransferPrologue.size()));
  auto badTextureTransferPrologue = expectedTextureTransferPrologue;
  badTextureTransferPrologue[17] ^= 0xFF;
  CHECK(!xeo3::vgpu::detail::HasExpectedTextureTransferPrologue(
      badTextureTransferPrologue.data(),
      badTextureTransferPrologue.size()));

  constexpr std::array<std::uint8_t, 16>
      expectedStructuredTextureTransferPrologue{
          0x40, 0x55, 0x56, 0x41, 0x54, 0x41, 0x55, 0x41,
          0x56, 0x41, 0x57, 0x48, 0x8D, 0x6C, 0x24, 0xF8,
      };
  CHECK(xeo3::vgpu::detail::HasExpectedStructuredTextureTransferPrologue(
      expectedStructuredTextureTransferPrologue.data(),
      expectedStructuredTextureTransferPrologue.size()));
  auto badStructuredTextureTransferPrologue =
      expectedStructuredTextureTransferPrologue;
  badStructuredTextureTransferPrologue[15] ^= 0xFF;
  CHECK(!xeo3::vgpu::detail::HasExpectedStructuredTextureTransferPrologue(
      badStructuredTextureTransferPrologue.data(),
      badStructuredTextureTransferPrologue.size()));

  constexpr std::array<std::uint8_t, 18> expectedConstantUploadPrologue{
      0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24,
      0x18, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x49, 0x8B, 0xF8,
  };
  CHECK(xeo3::vgpu::detail::HasExpectedConstantUploadPrologue(
      expectedConstantUploadPrologue.data(),
      expectedConstantUploadPrologue.size()));
  auto badConstantUploadPrologue = expectedConstantUploadPrologue;
  badConstantUploadPrologue[17] ^= 0xFF;
  CHECK(!xeo3::vgpu::detail::HasExpectedConstantUploadPrologue(
      badConstantUploadPrologue.data(), badConstantUploadPrologue.size()));

  constexpr std::array<std::uint8_t, 18>
      expectedNullPipelineStateSequence{
          0x48, 0x89, 0x93, 0x18, 0x10, 0x00, 0x00, 0x8B, 0x03,
          0x48, 0xC1, 0xE0, 0x05, 0x48, 0x8B, 0x4C, 0x18, 0x08,
      };
  CHECK(xeo3::vgpu::detail::HasExpectedNullPipelineStateSequence(
      expectedNullPipelineStateSequence.data(),
      expectedNullPipelineStateSequence.size()));
  auto badNullPipelineStateSequence = expectedNullPipelineStateSequence;
  badNullPipelineStateSequence[17] ^= 0xFF;
  CHECK(!xeo3::vgpu::detail::HasExpectedNullPipelineStateSequence(
      badNullPipelineStateSequence.data(),
      badNullPipelineStateSequence.size()));
  CHECK(!xeo3::vgpu::detail::HasExpectedNullPipelineStateSequence(nullptr, 0));

  constexpr std::array<std::uint32_t, 8> expectedSamplerAddressModes{
      1, 2, 3, 5, 0, 0, 4, 0};
  CHECK(xeo3::vgpu::detail::HasExpectedSamplerAddressModeTable(
      expectedSamplerAddressModes.data(), expectedSamplerAddressModes.size()));
  auto badSamplerAddressModes = expectedSamplerAddressModes;
  badSamplerAddressModes[4] = 3;
  CHECK(!xeo3::vgpu::detail::HasExpectedSamplerAddressModeTable(
      badSamplerAddressModes.data(), badSamplerAddressModes.size()));
  constexpr std::array<std::uint32_t, 8> fixedSamplerAddressModes{1, 2, 3, 5,
                                                                  3, 5, 4, 5};
  CHECK(xeo3::vgpu::detail::GetD3d12SamplerAddressModeTable() ==
        fixedSamplerAddressModes);

  constexpr char vertexShaderSource[] = "void xenon_vertex_shader() { }\n";
  constexpr char pixelShaderSource[] = "void xenon_pixel_shader() { }\n";
  CHECK(xeo3::vgpu::detail::IsGeneratedVertexShader(
      vertexShaderSource, sizeof(vertexShaderSource) - 1));
  CHECK(!xeo3::vgpu::detail::IsGeneratedVertexShader(
      pixelShaderSource, sizeof(pixelShaderSource) - 1));
  CHECK(!xeo3::vgpu::detail::IsGeneratedVertexShader(nullptr, 0));

  constexpr char reciprocalShaderSource[] =
      "float helper_rcp_value(float x) { return x; }\n"
      "[RootSignature(\"RootFlags(0)\")]\n"
      "void xenon_vertex_shader() {\n"
      "  gpr0.x = rcp(gpr1.x);\n"
      "  gpr0.y = rcp (c(100).w);\n"
      "}\n";
  std::string reciprocalPatchedShader;
  std::uint32_t reciprocalPatchCount = 0;
  CHECK(xeo3::vgpu::detail::PatchXenosScalarReciprocals(
      reciprocalShaderSource, sizeof(reciprocalShaderSource) - 1,
      reciprocalPatchedShader, reciprocalPatchCount));
  CHECK(reciprocalPatchCount == 2);
  CHECK(reciprocalPatchedShader.find(
            "precise float XeO3Ac6ApproximateReciprocal(float operand)") !=
        std::string::npos);
  CHECK(reciprocalPatchedShader.find(
            "gpr0.x = XeO3Ac6ApproximateReciprocal(gpr1.x)") !=
        std::string::npos);
  CHECK(reciprocalPatchedShader.find(
            "gpr0.y = XeO3Ac6ApproximateReciprocal (c(100).w)") !=
        std::string::npos);
  CHECK(reciprocalPatchedShader.find("helper_rcp_value") !=
        std::string::npos);
  CHECK(reciprocalPatchedShader.find(
            "uint bumpedBits = asuint(reciprocal) + 1u") !=
        std::string::npos);
  CHECK(reciprocalPatchedShader.find("precise float residual") ==
        std::string::npos);

  constexpr char reciprocalPixelShaderSource[] =
      "[RootSignature(\"RootFlags(0)\")]\n"
      "void xenon_pixel_shader() { gpr0.x = rcp(gpr1.x); }\n";
  CHECK(xeo3::vgpu::detail::PatchXenosScalarReciprocals(
      reciprocalPixelShaderSource,
      sizeof(reciprocalPixelShaderSource) - 1, reciprocalPatchedShader,
      reciprocalPatchCount));
  CHECK(reciprocalPatchCount == 1);
  CHECK(!xeo3::vgpu::detail::PatchXenosScalarReciprocals(
      nullptr, 0, reciprocalPatchedShader, reciprocalPatchCount));

  constexpr char malformedReciprocalShaderSource[] =
      "void xenon_vertex_shader() { gpr0.x = rcp(gpr1.x); }\n";
  CHECK(!xeo3::vgpu::detail::PatchXenosScalarReciprocals(
      malformedReciprocalShaderSource,
      sizeof(malformedReciprocalShaderSource) - 1,
      reciprocalPatchedShader, reciprocalPatchCount));
  CHECK(reciprocalPatchCount == 0);

  constexpr char vposPixelShaderSource[] =
      "#include \"common_header.h\"\n"
      "struct InputType {\n"
      "  float4 v2 : SV_Position;\n"
      "  bool1 v4 : SV_IsFrontFace;\n"
      "};\n"
      "void xenon_pixel_shader(InputType InV) {\n"
      "  gpr0.xy = InV.v2.xy + float2(-0.5,-0.5);\n"
      "  gpr0.xy = gpr0.xy * vpos_Scale.xy;\n"
      "  gpr0.x = select( InV.v4.x == 0.0f , (-gpr0.x) , gpr0.x );\n"
      "}\n";
  std::string patchedVposShader;
  std::uint32_t vposPatchCount = 0;
  CHECK(xeo3::vgpu::detail::PatchAc6ScreenSpaceVposScale(
      vposPixelShaderSource, sizeof(vposPixelShaderSource) - 1,
      patchedVposShader, vposPatchCount));
  CHECK(vposPatchCount == 1);
  CHECK(patchedVposShader.find(
            "gpr0.xy = gpr0.xy * (float2(1.0f, 1.0f)).xy;") !=
        std::string::npos);
  CHECK(patchedVposShader.find("vpos_Scale") == std::string::npos);
  CHECK(patchedVposShader.find("vport_Scale") == std::string::npos);

  constexpr char ac6SceneVposPixelShaderSource[] =
      "#include \"common_header.h\"\n"
      "struct InputType {\n"
      "  float4 v2 : SV_Position;\n"
      "  bool1 v4 : SV_IsFrontFace;\n"
      "};\n"
      "void xenon_pixel_shader(InputType InV) {\n"
      "  gpr0.xy = InV.v2.xy + float2(-0.5,-0.5);\n"
      "  gpr0.xy = gpr0.xy * vpos_Scale.xy;\n"
      "  gpr0.x = select( InV.v4.x == 0.0f , (-gpr0.x) , gpr0.x );\n"
      "  gpr1.xy = gpr0.xy * c(255).xy;\n"
      "  gpr1.xy = abs(gpr0.xy) * c(255).ww;\n"
      "  gpr1.xy = frac( abs(gpr1.xy) );\n"
      "  OutV.oD = gpr1.x;\n"
      "}\n";
  CHECK(xeo3::vgpu::detail::PatchAc6ScreenSpaceVposScale(
      ac6SceneVposPixelShaderSource,
      sizeof(ac6SceneVposPixelShaderSource) - 1, patchedVposShader,
      vposPatchCount));
  CHECK(vposPatchCount == 1);
  CHECK(patchedVposShader.find(
            "gpr0.xy = gpr0.xy * (float2(1.0f, 1.0f)).xy;") !=
        std::string::npos);
  CHECK(patchedVposShader.find("gpr1.x = gpr1.x * 0.5f;") ==
        std::string::npos);
  CHECK(patchedVposShader.find(
            "gpr0.xy = gpr0.xy * float2(0.5f, 1.0f);") ==
        std::string::npos);

  constexpr char malformedAc6SceneVposPixelShaderSource[] =
      "#include \"common_header.h\"\n"
      "struct InputType {\n"
      "  float4 v2 : SV_Position;\n"
      "  bool1 v4 : SV_IsFrontFace;\n"
      "};\n"
      "void xenon_pixel_shader(InputType InV) {\n"
      "  gpr0.xy = InV.v2.xy + float2(-0.5,-0.5);\n"
      "  gpr0.xy = gpr0.xy * vpos_Scale.xy;\n"
      "  gpr0.x = select( InV.v4.x == 0.0f , (-gpr0.x) , gpr0.x );\n"
      "  gpr1.xy = abs(gpr0.xy) * c(255).ww;\n"
      "  gpr1.xy = frac( abs(gpr1.xy) );\n"
      "  OutV.oD = gpr1.x;\n"
      "}\n";
  CHECK(xeo3::vgpu::detail::PatchAc6ScreenSpaceVposScale(
      malformedAc6SceneVposPixelShaderSource,
      sizeof(malformedAc6SceneVposPixelShaderSource) - 1,
      patchedVposShader, vposPatchCount));
  CHECK(vposPatchCount == 1);
  CHECK(patchedVposShader.find("gpr1.x = gpr1.x * 0.5f;") ==
        std::string::npos);

  constexpr char duplicateAc6SceneVposPixelShaderSource[] =
      "#include \"common_header.h\"\n"
      "struct InputType {\n"
      "  float4 v2 : SV_Position;\n"
      "  bool1 v4 : SV_IsFrontFace;\n"
      "};\n"
      "void xenon_pixel_shader(InputType InV) {\n"
      "  gpr0.xy = InV.v2.xy + float2(-0.5,-0.5);\n"
      "  gpr0.xy = gpr0.xy * vpos_Scale.xy;\n"
      "  gpr0.x = select( InV.v4.x == 0.0f , (-gpr0.x) , gpr0.x );\n"
      "  gpr1.xy = gpr0.xy * c(255).xy;\n"
      "  gpr1.xy = gpr0.xy * c(255).xy;\n"
      "  gpr1.xy = abs(gpr0.xy) * c(255).ww;\n"
      "  gpr1.xy = frac( abs(gpr1.xy) );\n"
      "  OutV.oD = gpr1.x;\n"
      "}\n";
  CHECK(xeo3::vgpu::detail::PatchAc6ScreenSpaceVposScale(
      duplicateAc6SceneVposPixelShaderSource,
      sizeof(duplicateAc6SceneVposPixelShaderSource) - 1,
      patchedVposShader, vposPatchCount));
  CHECK(vposPatchCount == 1);
  CHECK(patchedVposShader.find("gpr1.x = gpr1.x * 0.5f;") ==
        std::string::npos);

  constexpr char particleVposPixelShaderSource[] =
      "#include \"common_header.h\"\n"
      "void xenon_pixel_shader(InputType InV) {\n"
      "  gpr1.zw = InV.v3.xy / vpos_Scale.xy;\n"
      "  gpr1.xy = gpr1.xy * vpos_Scale.xy;\n"
      "}\n";
  CHECK(xeo3::vgpu::detail::PatchAc6ScreenSpaceVposScale(
      particleVposPixelShaderSource,
      sizeof(particleVposPixelShaderSource) - 1, patchedVposShader,
      vposPatchCount));
  CHECK(vposPatchCount == 2);
  CHECK(patchedVposShader.find(
            "gpr1.zw = InV.v3.xy / (float2(1.0f, 1.0f)).xy;") !=
        std::string::npos);
  CHECK(patchedVposShader.find(
            "gpr1.xy = gpr1.xy * (float2(1.0f, 1.0f)).xy;") !=
        std::string::npos);

  constexpr char pointSpriteVposGeometryShaderSource[] =
      "#include \"common_header.h\"\n"
      "void gsmain() {\n"
      "  float1 pSZ = oPsz / vpos_Scale.x;\n"
      "  gsoutput.v3 = texcoords * vpos_Scale.xy;\n"
      "}\n";
  CHECK(xeo3::vgpu::detail::PatchAc6ScreenSpaceVposScale(
      pointSpriteVposGeometryShaderSource,
      sizeof(pointSpriteVposGeometryShaderSource) - 1, patchedVposShader,
      vposPatchCount));
  CHECK(vposPatchCount == 2);
  CHECK(patchedVposShader.find(
            "float1 pSZ = oPsz / (float2(1.0f, 1.0f)).x;") !=
        std::string::npos);
  CHECK(patchedVposShader.find(
            "gsoutput.v3 = texcoords * (float2(1.0f, 1.0f)).xy;") !=
        std::string::npos);

  constexpr char identifierBoundaryVposShaderSource[] =
      "#include \"common_header.h\"\n"
      "void xenon_pixel_shader() {\n"
      "  float2 my_vpos_Scale_backup = 0.0f;\n"
      "}\n";
  CHECK(xeo3::vgpu::detail::PatchAc6ScreenSpaceVposScale(
      identifierBoundaryVposShaderSource,
      sizeof(identifierBoundaryVposShaderSource) - 1, patchedVposShader,
      vposPatchCount));
  CHECK(vposPatchCount == 0);
  CHECK(patchedVposShader.empty());

  constexpr char unrelatedVposShaderSource[] =
      "void xenon_pixel_shader() {\n"
      "  gpr0.xy = gpr0.xy * vpos_Scale.xy;\n"
      "}\n";
  CHECK(xeo3::vgpu::detail::PatchAc6ScreenSpaceVposScale(
      unrelatedVposShaderSource, sizeof(unrelatedVposShaderSource) - 1,
      patchedVposShader, vposPatchCount));
  CHECK(vposPatchCount == 0);
  CHECK(patchedVposShader.empty());
  CHECK(!xeo3::vgpu::detail::PatchAc6ScreenSpaceVposScale(
      nullptr, 0, patchedVposShader, vposPatchCount));

  constexpr std::array<std::uint8_t, 32> ac6Pso537ShaderDigest{
      0x10, 0x0A, 0x6B, 0xAA, 0xAC, 0x33, 0x57, 0x53,
      0x0C, 0x7B, 0x69, 0x94, 0xE1, 0xFF, 0xD5, 0x83,
      0x79, 0xB0, 0x77, 0xCD, 0x66, 0xE5, 0x9D, 0xAD,
      0xF6, 0xD2, 0x06, 0x3F, 0xC9, 0x35, 0x11, 0xF9,
  };
  CHECK(xeo3::vgpu::detail::MatchesAc6Pso537ShaderFingerprint(
      3351, ac6Pso537ShaderDigest));
  CHECK(!xeo3::vgpu::detail::MatchesAc6Pso537ShaderFingerprint(
      3350, ac6Pso537ShaderDigest));
  auto wrongPso537ShaderDigest = ac6Pso537ShaderDigest;
  ++wrongPso537ShaderDigest[31];
  CHECK(!xeo3::vgpu::detail::MatchesAc6Pso537ShaderFingerprint(
      3351, wrongPso537ShaderDigest));

  constexpr char ac6Pso537ShaderSource[] =
      "struct OutputType {\n"
      "  float4 oC0 : SV_Target0;\n"
      "  float1 oD : SV_Depth;\n"
      "};\n"
      "Texture2D texOBJ1 : register(t1);\n"
      "Texture2D texOBJ2 : register(t2);\n"
      "void xenon_pixel_shader() {\n"
      "  gpr1.xy = gpr0.xy * c(255).xy;\n"
      "  tT.xy = gpr1.xy;\n"
      "}\n";
  std::string patchedPso537Shader;
  std::uint32_t pso537PatchCount = 0;
  CHECK(xeo3::vgpu::detail::PatchAc6Pso537HalfWidthUv(
      ac6Pso537ShaderSource, sizeof(ac6Pso537ShaderSource) - 1,
      patchedPso537Shader, pso537PatchCount));
  CHECK(pso537PatchCount == 1);
  CHECK(patchedPso537Shader.find(
            "gpr1.xy = gpr0.xy * c(255).xy;\n"
            "gpr1.x = gpr1.x * 0.5f;\n"
            "  tT.xy = gpr1.xy;") != std::string::npos);
  CHECK(!xeo3::vgpu::detail::PatchAc6Pso537HalfWidthUv(
      nullptr, 0, patchedPso537Shader, pso537PatchCount));

  constexpr char ac6ToneMapShaderSource[] =
      "struct InputType {\n"
      "  linear float3 v0 : TEXCOORD0;\n"
      "};\n"
      "Texture2D texOBJ0 : register(t0);\n"
      "Texture2D texOBJ1 : register(t1);\n"
      "Texture2D texOBJ2 : register(t2);\n"
      "void xenon_pixel_shader(InputType InV) {\n"
      "  gpr0.xyz = InV.v0.xyz;\n"
      "  gpr0.xyw = gpr0.xyw * c(100).www;\n"
      "  gpr0.xyw = mad( gpr2.xyz, c(100).zzz, gpr0.xyw );\n"
      "  OutV.oC0.xyz = mad( gpr1.xyz, gpr0.zzz, gpr0.xyw );\n"
      "}\n";
  std::string patchedToneMapShader;
  std::uint32_t toneMapPatchCount = 0;
  CHECK(xeo3::vgpu::detail::PatchAc6ToneMapInterpolant(
      ac6ToneMapShaderSource, sizeof(ac6ToneMapShaderSource) - 1,
      patchedToneMapShader, toneMapPatchCount));
  CHECK(toneMapPatchCount == 1);
  CHECK(patchedToneMapShader.find(
            "OutV.oC0.xyz = mad( gpr1.xyz, "
            "float3(1.0f, 1.0f, 1.0f), gpr0.xyw );") !=
        std::string::npos);
  CHECK(patchedToneMapShader.find(
            "OutV.oC0.xyz = mad( gpr1.xyz, gpr0.zzz, gpr0.xyw );") ==
        std::string::npos);

  constexpr char unrelatedToneMapShaderSource[] =
      "void xenon_pixel_shader() {\n"
      "  OutV.oC0.xyz = mad( gpr1.xyz, gpr0.zzz, gpr0.xyw );\n"
      "}\n";
  CHECK(xeo3::vgpu::detail::PatchAc6ToneMapInterpolant(
      unrelatedToneMapShaderSource,
      sizeof(unrelatedToneMapShaderSource) - 1, patchedToneMapShader,
      toneMapPatchCount));
  CHECK(toneMapPatchCount == 0);
  CHECK(patchedToneMapShader.empty());
  CHECK(!xeo3::vgpu::detail::PatchAc6ToneMapInterpolant(
      nullptr, 0, patchedToneMapShader, toneMapPatchCount));

  constexpr std::array<std::uint8_t, 32> ac6ExposureShaderDigest{
      0x12, 0x97, 0x02, 0x7B, 0x92, 0x53, 0x1D, 0x70, 0xF6, 0x1A, 0x5E,
      0xFD, 0x10, 0xF1, 0xC3, 0x87, 0x67, 0x51, 0x00, 0xE4, 0xB1, 0x0A,
      0x1B, 0x0B, 0x59, 0x0F, 0xDB, 0x41, 0x32, 0x42, 0x7E, 0x09,
  };
  CHECK(xeo3::vgpu::detail::MatchesAc6ExposureShaderFingerprint(
      3317, ac6ExposureShaderDigest));
  CHECK(!xeo3::vgpu::detail::MatchesAc6ExposureShaderFingerprint(
      3316, ac6ExposureShaderDigest));
  auto wrongExposureShaderDigest = ac6ExposureShaderDigest;
  ++wrongExposureShaderDigest[0];
  CHECK(!xeo3::vgpu::detail::MatchesAc6ExposureShaderFingerprint(
      3317, wrongExposureShaderDigest));

  constexpr char ac6ExposureShaderSource[] =
      "Texture2D texOBJ19 : register(t19);\n"
      "void xenon_vertex_shader() {\n"
      "tmp0.xyzw = texOBJ19.SampleLevel(samp19 , tT.xy, tT.w);\n"
      "tmp1.xyzw = ((asuint(tfpatch[19].w) & 0x4000) != 0);\n"
      "gpr0.x = tmp0.x;\n"
      "gpr0.y = select( c(106).x > gpr0.x , 1.0f , 0.0f );\n"
      "gpr0.y = select( gpr0.x > c(106).y , 1.0f , 0.0f );\n"
      "}\n";
  std::string patchedExposureShader;
  std::uint32_t exposurePatchCount = 0;
  CHECK(xeo3::vgpu::detail::PatchAc6ExposureSample(
      ac6ExposureShaderSource, sizeof(ac6ExposureShaderSource) - 1,
      patchedExposureShader, exposurePatchCount));
  CHECK(exposurePatchCount == 1);
  CHECK(patchedExposureShader.find(
            "gpr0.x = (isfinite(gpr0.x) && gpr0.x > 0.0f && "
            "gpr0.x <= 65536.0f) ? gpr0.x : c(106).y;") !=
        std::string::npos);

  constexpr char unrelatedExposureShaderSource[] =
      "void xenon_vertex_shader() {\n"
      "gpr0.x = tmp0.x;\n"
      "}\n";
  CHECK(xeo3::vgpu::detail::PatchAc6ExposureSample(
      unrelatedExposureShaderSource,
      sizeof(unrelatedExposureShaderSource) - 1, patchedExposureShader,
      exposurePatchCount));
  CHECK(exposurePatchCount == 0);
  CHECK(patchedExposureShader.empty());
  CHECK(!xeo3::vgpu::detail::PatchAc6ExposureSample(
      nullptr, 0, patchedExposureShader, exposurePatchCount));

  constexpr std::array<std::uint32_t, 6> textureUnpackSignature{
      0, 2, 4, 1, 6, 14400,
  };
  CHECK(xeo3::vgpu::detail::IsAc6TextureUnpackTransfer(
      textureUnpackSignature));
  auto textureUnpackConstants = textureUnpackSignature;
  CHECK(xeo3::vgpu::detail::PatchAc6TextureUnpackEndian(
      textureUnpackConstants, 0));
  CHECK(textureUnpackConstants ==
        (std::array<std::uint32_t, 6>{0, 0, 4, 1, 6, 14400}));
  for (const auto replacement : {1U, 3U}) {
    auto alternateConstants = textureUnpackSignature;
    CHECK(xeo3::vgpu::detail::PatchAc6TextureUnpackEndian(
        alternateConstants, replacement));
    CHECK(alternateConstants[1] == replacement);
  }
  for (const auto replacement : {2U, 4U}) {
    auto invalidReplacement = textureUnpackSignature;
    CHECK(!xeo3::vgpu::detail::PatchAc6TextureUnpackEndian(
        invalidReplacement, replacement));
    CHECK(invalidReplacement == textureUnpackSignature);
  }
  std::array<std::uint32_t, 12> structuredTextureDescriptor{
      0, 2, 4, 1, 0xAAAAAAAA, 0xBBBBBBBB,
      6, 14400, 0xCCCCCCCC, 0xDDDDDDDD, 0xEEEEEEEE, 0xFFFFFFFF,
  };
  CHECK(xeo3::vgpu::detail::ExtractStructuredTextureTransferConstants(
            structuredTextureDescriptor.data()) == textureUnpackSignature);
  CHECK(xeo3::vgpu::detail::ExtractStructuredTextureTransferConstants(
            nullptr) == (std::array<std::uint32_t, 6>{}));
  for (std::size_t index = 0; index < textureUnpackSignature.size(); ++index) {
    auto nearMiss = textureUnpackSignature;
    ++nearMiss[index];
    const auto original = nearMiss;
    CHECK(!xeo3::vgpu::detail::IsAc6TextureUnpackTransfer(nearMiss));
    CHECK(!xeo3::vgpu::detail::PatchAc6TextureUnpackEndian(nearMiss, 0));
    CHECK(nearMiss == original);
  }

  std::array<std::uint32_t,
             xeo3::vgpu::kAc6Pso341TaskBufferSize / sizeof(std::uint32_t)>
      transfer341TaskBuffer{};
  CHECK(xeo3::vgpu::detail::ShouldTrackAc6Pso341UploadBuffer(
      xeo3::vgpu::kD3d12GpuUploadHeapType, true,
      xeo3::vgpu::kAc6Pso341UploadArenaSize));
  CHECK(xeo3::vgpu::detail::ShouldTrackAc6Pso341UploadBuffer(
      xeo3::vgpu::kD3d12UploadHeapType, true,
      xeo3::vgpu::kAc6Pso341UploadArenaSize));
  CHECK(!xeo3::vgpu::detail::ShouldTrackAc6Pso341UploadBuffer(
      1, true, xeo3::vgpu::kAc6Pso341UploadArenaSize));
  CHECK(!xeo3::vgpu::detail::ShouldTrackAc6Pso341UploadBuffer(
      xeo3::vgpu::kD3d12GpuUploadHeapType, false,
      xeo3::vgpu::kAc6Pso341UploadArenaSize));
  CHECK(!xeo3::vgpu::detail::ShouldTrackAc6Pso341UploadBuffer(
      xeo3::vgpu::kD3d12GpuUploadHeapType, true,
      xeo3::vgpu::kAc6Pso341UploadArenaSize - 1));
  std::uint64_t uploadOffset = UINT64_MAX;
  CHECK(xeo3::vgpu::detail::ResolveAc6Pso341UploadOffset(
      0x2577C0000ULL, 0x300, 0x400, 0xDF0000, 0x2578DFE00ULL,
      xeo3::vgpu::kAc6Pso341TaskBufferSize, uploadOffset));
  CHECK(uploadOffset == 0x11FE00);
  CHECK(!xeo3::vgpu::detail::ResolveAc6Pso341UploadOffset(
      0x2577C0000ULL, 0x200, 0x400, 0xDF0000, 0x2578DFE00ULL,
      xeo3::vgpu::kAc6Pso341TaskBufferSize, uploadOffset));
  CHECK(uploadOffset == 0);
  CHECK(!xeo3::vgpu::detail::ResolveAc6Pso341UploadOffset(
      0x2577C0000ULL, 0x300, 0x400, 0xC0000, 0x2578DFE00ULL,
      xeo3::vgpu::kAc6Pso341TaskBufferSize, uploadOffset));
  CHECK(!xeo3::vgpu::detail::ResolveAc6Pso341UploadOffset(
      0x2577C0000ULL, 0x300, 0x400, 0xC0000, 0x257880000ULL,
      xeo3::vgpu::kAc6Pso341TaskBufferSize, uploadOffset));
  CHECK(!xeo3::vgpu::detail::ResolveAc6Pso341UploadOffset(
      0x2577C0000ULL, 0x300, 0x3FF, 0xDF0000, 0x2578DFE00ULL,
      xeo3::vgpu::kAc6Pso341TaskBufferSize, uploadOffset));
  const std::array<std::uint64_t, 4> liveAmdCbvDescriptor{
      0x00100002578D0E00ULL, 0x11014FAC00000010ULL, 0, 0};
  CHECK(xeo3::vgpu::detail::DecodeAmdConstantBufferGpuAddress(
            liveAmdCbvDescriptor) == 0x000002578D0E00ULL);
  auto unalignedAmdCbvDescriptor = liveAmdCbvDescriptor;
  unalignedAmdCbvDescriptor[0] |= 1;
  CHECK(xeo3::vgpu::detail::DecodeAmdConstantBufferGpuAddress(
            unalignedAmdCbvDescriptor) == 0);
  CHECK(xeo3::vgpu::detail::DecodeAmdConstantBufferGpuAddress({}) == 0);
  constexpr std::array<std::uint8_t, 32> pso341ComputeShaderSha256{
      0xB1, 0x5D, 0x25, 0xCB, 0x2A, 0x20, 0x52, 0xA5,
      0x2D, 0xCB, 0x65, 0x68, 0x9B, 0x1E, 0xD5, 0x4D,
      0x2A, 0xA3, 0x8E, 0x84, 0xF4, 0x0E, 0x75, 0x36,
      0x84, 0x56, 0x1C, 0x10, 0x39, 0xED, 0x4A, 0x54,
  };
  CHECK(xeo3::vgpu::detail::MatchesAc6Pso341ComputeShaderFingerprint(
      xeo3::vgpu::kAc6Pso341ComputeShaderSize,
      pso341ComputeShaderSha256));
  CHECK(!xeo3::vgpu::detail::MatchesAc6Pso341ComputeShaderFingerprint(
      xeo3::vgpu::kAc6Pso341ComputeShaderSize - 1,
      pso341ComputeShaderSha256));
  auto pso341ComputeShaderNearMiss = pso341ComputeShaderSha256;
  pso341ComputeShaderNearMiss[31] ^= 1;
  CHECK(!xeo3::vgpu::detail::MatchesAc6Pso341ComputeShaderFingerprint(
      xeo3::vgpu::kAc6Pso341ComputeShaderSize,
      pso341ComputeShaderNearMiss));
  constexpr std::array<std::uint8_t, 32> pso341CachedBlobSha256{
      0x75, 0xC7, 0xF4, 0x4B, 0x78, 0x4A, 0x9F, 0x7A,
      0x2D, 0xE0, 0x21, 0x6A, 0x0F, 0x46, 0x0E, 0xF0,
      0x40, 0x7F, 0x4F, 0x5E, 0x83, 0x5F, 0x6F, 0x86,
      0x04, 0xF1, 0xB7, 0x0B, 0xF0, 0xC0, 0x50, 0xCA,
  };
  CHECK(xeo3::vgpu::detail::MatchesAc6Pso341CachedPipelineBlob(
      954, pso341CachedBlobSha256));
  constexpr std::array<std::uint8_t, 32> pso341LiveCachedBlobSha256{
      0x2D, 0x34, 0x18, 0x7A, 0x02, 0x0B, 0x37, 0xA6,
      0x9B, 0x0B, 0x07, 0x7B, 0xFC, 0x08, 0x00, 0x5A,
      0xD0, 0xA7, 0xA5, 0x30, 0x33, 0x7D, 0xBA, 0xF4,
      0xCF, 0x9C, 0x0E, 0x6B, 0xE7, 0xB3, 0x6A, 0xE4,
  };
  CHECK(xeo3::vgpu::detail::MatchesAc6Pso341CachedPipelineBlob(
      954, pso341LiveCachedBlobSha256));
  CHECK(!xeo3::vgpu::detail::MatchesAc6Pso341CachedPipelineBlob(
      953, pso341CachedBlobSha256));
  auto pso341CachedBlobNearMiss = pso341CachedBlobSha256;
  pso341CachedBlobNearMiss[31] ^= 1;
  CHECK(!xeo3::vgpu::detail::MatchesAc6Pso341CachedPipelineBlob(
      954, pso341CachedBlobNearMiss));
  transfer341TaskBuffer[0] = 1280;
  transfer341TaskBuffer[1] = 720;
  transfer341TaskBuffer[2] =
      xeo3::vgpu::kAc6Pso341BrokenPackedDimensions;
  transfer341TaskBuffer[3] = 1;
  transfer341TaskBuffer[4] = 5120;
  transfer341TaskBuffer[128] = 14400;
  for (std::size_t index = 129; index < 144; ++index) {
    transfer341TaskBuffer[index] = UINT32_MAX;
  }
  CHECK(xeo3::vgpu::detail::ClassifyAc6Pso341TaskWidth(
            transfer341TaskBuffer.data(), sizeof(transfer341TaskBuffer)) ==
        xeo3::vgpu::Ac6Pso341TaskWidthState::Broken);
  std::uint32_t originalPackedDimensions = 0;
  std::uint32_t replacementPackedDimensions = 0;
  CHECK(xeo3::vgpu::detail::PatchAc6Pso341TaskWidth(
      transfer341TaskBuffer.data(), sizeof(transfer341TaskBuffer),
      originalPackedDimensions, replacementPackedDimensions));
  CHECK(originalPackedDimensions ==
        xeo3::vgpu::kAc6Pso341BrokenPackedDimensions);
  CHECK(replacementPackedDimensions ==
        xeo3::vgpu::kAc6Pso341CorrectedPackedDimensions);
  CHECK((originalPackedDimensions & 0xFFFF0000U) ==
        (replacementPackedDimensions & 0xFFFF0000U));
  CHECK((replacementPackedDimensions & 0xFFFFU) == 1280);
  CHECK(transfer341TaskBuffer[2] == replacementPackedDimensions);
  CHECK(xeo3::vgpu::detail::ClassifyAc6Pso341TaskWidth(
            transfer341TaskBuffer.data(), sizeof(transfer341TaskBuffer)) ==
        xeo3::vgpu::Ac6Pso341TaskWidthState::Correct);
  const auto correctedTransfer341TaskBuffer = transfer341TaskBuffer;
  CHECK(!xeo3::vgpu::detail::PatchAc6Pso341TaskWidth(
      transfer341TaskBuffer.data(), sizeof(transfer341TaskBuffer),
      originalPackedDimensions, replacementPackedDimensions));
  CHECK(transfer341TaskBuffer == correctedTransfer341TaskBuffer);
  CHECK(originalPackedDimensions == 0);
  CHECK(replacementPackedDimensions == 0);

  auto brokenTransfer341TaskBuffer = transfer341TaskBuffer;
  brokenTransfer341TaskBuffer[2] =
      xeo3::vgpu::kAc6Pso341BrokenPackedDimensions;
  constexpr std::array<std::size_t, 8> transfer341GuardedFields{
      0, 1, 3, 4, 5, 6, 7, 128,
  };
  for (const auto index : transfer341GuardedFields) {
    auto nearMiss = brokenTransfer341TaskBuffer;
    ++nearMiss[index];
    CHECK(xeo3::vgpu::detail::ClassifyAc6Pso341TaskWidth(
              nearMiss.data(), sizeof(nearMiss)) ==
          xeo3::vgpu::Ac6Pso341TaskWidthState::NotCandidate);
  }
  for (std::size_t index = 129; index < 144; ++index) {
    auto nearMiss = brokenTransfer341TaskBuffer;
    nearMiss[index] = 0;
    CHECK(xeo3::vgpu::detail::ClassifyAc6Pso341TaskWidth(
              nearMiss.data(), sizeof(nearMiss)) ==
          xeo3::vgpu::Ac6Pso341TaskWidthState::NotCandidate);
  }
  auto invalidPackedDimensions = brokenTransfer341TaskBuffer;
  invalidPackedDimensions[2] = 0x02D004FF;
  CHECK(xeo3::vgpu::detail::ClassifyAc6Pso341TaskWidth(
            invalidPackedDimensions.data(),
            sizeof(invalidPackedDimensions)) ==
        xeo3::vgpu::Ac6Pso341TaskWidthState::NotCandidate);
  CHECK(xeo3::vgpu::detail::ClassifyAc6Pso341TaskWidth(
            brokenTransfer341TaskBuffer.data(),
            sizeof(brokenTransfer341TaskBuffer) - 1) ==
        xeo3::vgpu::Ac6Pso341TaskWidthState::NotCandidate);
  CHECK(xeo3::vgpu::detail::ClassifyAc6Pso341TaskWidth(
            nullptr, xeo3::vgpu::kAc6Pso341TaskBufferSize) ==
        xeo3::vgpu::Ac6Pso341TaskWidthState::NotCandidate);

  constexpr std::array<std::uint32_t, 16> edramLoadConstants{
      0, 0, 1280, 720, 0, 0, 1280, 720,
      1280, 2048, 1280, 720, 1, 0, 0, 16,
  };
  constexpr std::array<std::uint32_t, 16> edramScaleConstants{
      0, 0, 1280, 720, 0, 0, 640, 360,
      1280, 2048, 640, 360, 1, 0, 0, 16,
  };
  CHECK(xeo3::vgpu::detail::ClassifyAc6EdramTransferConstants(
            edramLoadConstants) ==
        xeo3::vgpu::Ac6EdramConstantKind::Load);
  CHECK(xeo3::vgpu::detail::ClassifyAc6EdramTransferConstants(
            edramScaleConstants) ==
        xeo3::vgpu::Ac6EdramConstantKind::Scale);

  auto edramCandidateConstants = edramScaleConstants;
  edramCandidateConstants[6] = 960;
  edramCandidateConstants[7] = 540;
  edramCandidateConstants[10] = 960;
  edramCandidateConstants[11] = 540;
  CHECK(xeo3::vgpu::detail::ClassifyAc6EdramTransferConstants(
            edramCandidateConstants) ==
        xeo3::vgpu::Ac6EdramConstantKind::Candidate);
  auto edramNearMissConstants = edramCandidateConstants;
  edramNearMissConstants[12] = 0;
  CHECK(xeo3::vgpu::detail::ClassifyAc6EdramTransferConstants(
            edramNearMissConstants) ==
        xeo3::vgpu::Ac6EdramConstantKind::None);

  const auto packedEdramConstants =
      xeo3::vgpu::detail::PackAc6EdramTransferConstants(
          edramScaleConstants);
  for (std::size_t index = 0; index < packedEdramConstants.size(); ++index) {
    const auto expected =
        static_cast<std::uint64_t>(edramScaleConstants[index * 2]) |
        (static_cast<std::uint64_t>(edramScaleConstants[index * 2 + 1])
         << 32);
    CHECK(packedEdramConstants[index] == expected);
  }

  constexpr char groundShaderSource[] =
      "OutputType xenon_vertex_shader(InputType InV) {\n"
      "gpr0.xy = FetchByID_FLOAT2(vtxOBJ35, VID, 4, 0, 0).xy;\n"
      "gpr1 = FetchByID_FLOAT4(vtxOBJ36, gpr0.x, 20, 0, 0);\n"
      "gpr2 = FetchByID_FLOAT4(vtxOBJ36, gpr12.w, 20, 4, 0);\n"
      "}\n";
  std::string patchedGroundShader;
  std::uint32_t groundFetchPatchCount = 0;
  CHECK(xeo3::vgpu::detail::PatchAc6GroundFetchIndices(
      groundShaderSource, sizeof(groundShaderSource) - 1, patchedGroundShader,
      groundFetchPatchCount));
  CHECK(groundFetchPatchCount == 2);
  CHECK(patchedGroundShader.find("vtxOBJ35, VID, 4") != std::string::npos);
  CHECK(patchedGroundShader.find("vtxOBJ36, (gpr0.x + 0.00025f), 20") !=
        std::string::npos);
  CHECK(patchedGroundShader.find("vtxOBJ36, (gpr12.w + 0.00025f), 20") !=
        std::string::npos);

  constexpr char alreadyPatchedShaderSource[] =
      "void xenon_vertex_shader() {\n"
      "FetchByID_FLOAT4(vtxOBJ36, (gpr0.x + 0.00025f), 20, 0, 0);\n"
      "}\n";
  CHECK(xeo3::vgpu::detail::PatchAc6GroundFetchIndices(
      alreadyPatchedShaderSource, sizeof(alreadyPatchedShaderSource) - 1,
      patchedGroundShader, groundFetchPatchCount));
  CHECK(groundFetchPatchCount == 0);
  CHECK(patchedGroundShader.empty());
  CHECK(!xeo3::vgpu::detail::PatchAc6GroundFetchIndices(
      nullptr, 0, patchedGroundShader, groundFetchPatchCount));

  constexpr char indexedVertexShaderSource[] =
      "Buffer<float1> vtxOBJ35: register(t35);\n"
      "Buffer<float4> vtxOBJ36: register(t36);\n"
      "struct OutputType {\n"
      "    float4 oP : SV_Position;\n"
      "};\n"
      "OutputType xenon_vertex_shader(InputType InV) {\n"
      "OutputType OutV;\n"
      "int VID = 0;\n"
      "VID = HostToGuestIndex(InV.vID);\n"
      "VID = VID + vertexOffset;\n"
      "gpr1.xy = FetchByID_FLOAT4(vtxOBJ35, VID, 5, 0, 0).xy;\n"
      "gpr0.xyzw = FetchByID_UBYTE4N(vtxOBJ36, VID, 5, 4, 0).zyxw;\n"
      "OutV.oP = float4(0, 0, 0, 1);\n"
      "return OutV;\n"
      "}\n";
  std::string patchedIndexShader;
  std::uint32_t indexPatchCount = 0;
  CHECK(xeo3::vgpu::detail::PatchXenosIndexBufferSemantics(
      indexedVertexShaderSource, sizeof(indexedVertexShaderSource) - 1,
      patchedIndexShader, indexPatchCount));
  CHECK(indexPatchCount == 1);
  CHECK(patchedIndexShader.find(
            "float XeO3IndexClip : SV_ClipDistance0;") != std::string::npos);
  CHECK(patchedIndexShader.find(
            "XeO3GuestIndexRaw & 0x00FFFFFFu") != std::string::npos);
  CHECK(patchedIndexShader.find("IbDescUseResetIdx(PackedIbDesc)") ==
        std::string::npos);
  CHECK(patchedIndexShader.find(
            "XeO3GuestIndex24 == 0x00FFFFFFu") != std::string::npos);
  CHECK(patchedIndexShader.find(
            "VID = int(XeO3GuestIndexCut ? 0u : "
            "XeO3GuestIndexMasked);") != std::string::npos);
  CHECK(patchedIndexShader.find(
            "OutV.XeO3IndexClip = XeO3GuestIndexCut ? -1.0f : 0.0f;") !=
        std::string::npos);
  CHECK(patchedIndexShader.find("VID = HostToGuestIndex(InV.vID);") ==
        std::string::npos);
  CHECK(xeo3::vgpu::detail::PatchXenosIndexBufferSemantics(
      patchedIndexShader.data(), patchedIndexShader.size(), patchedGroundShader,
      indexPatchCount));
  CHECK(indexPatchCount == 0);
  CHECK(patchedGroundShader.empty());

  constexpr char genericIndexedVertexShaderSource[] =
      "Buffer<float1> vtxOBJ35: register(t35);\n"
      "struct OutputType {\n"
      "    float4 oP : SV_Position;\n"
      "};\n"
      "OutputType xenon_vertex_shader(InputType InV) {\n"
      "OutputType OutV;\n"
      "int VID = 0;\n"
      "VID = HostToGuestIndex(InV.vID);\n"
      "VID = VID + vertexOffset;\n"
      "gpr0.xy = FetchByID_FLOAT2(vtxOBJ35, VID, 2, 0, 0).xy;\n"
      "OutV.oP = float4(gpr0.xy, 0, 1);\n"
      "return OutV;\n"
      "}\n";
  CHECK(xeo3::vgpu::detail::PatchXenosIndexBufferSemantics(
      genericIndexedVertexShaderSource,
      sizeof(genericIndexedVertexShaderSource) - 1, patchedIndexShader,
      indexPatchCount));
  CHECK(indexPatchCount == 1);
  CHECK(patchedIndexShader.find("XeO3GuestIndexUses24Bits") !=
        std::string::npos);
  CHECK(patchedIndexShader.find(
            "OutV.XeO3IndexClip = XeO3GuestIndexCut ? -1.0f : 0.0f;") !=
        std::string::npos);

  constexpr char nonIndexedVertexShaderSource[] =
      "struct OutputType { float4 oP : SV_Position; };\n"
      "OutputType xenon_vertex_shader(InputType InV) {\n"
      "OutputType OutV; return OutV; }\n";
  CHECK(xeo3::vgpu::detail::PatchXenosIndexBufferSemantics(
      nonIndexedVertexShaderSource, sizeof(nonIndexedVertexShaderSource) - 1,
      patchedIndexShader, indexPatchCount));
  CHECK(indexPatchCount == 0);
  CHECK(patchedIndexShader.empty());
  CHECK(!xeo3::vgpu::detail::PatchXenosIndexBufferSemantics(
      nullptr, 0, patchedIndexShader, indexPatchCount));

  constexpr char malformedIndexedVertexShaderSource[] =
      "Buffer<float1> vtxOBJ35: register(t35);\n"
      "Buffer<float4> vtxOBJ36: register(t36);\n"
      "OutputType xenon_vertex_shader(InputType InV) {\n"
      "OutputType OutV;\n"
      "int VID = 0;\n"
      "VID = HostToGuestIndex(InV.vID);\n"
      "gpr1.xy = FetchByID_FLOAT4(vtxOBJ35, VID, 5, 0, 0).xy;\n"
      "gpr0.xyzw = FetchByID_UBYTE4N(vtxOBJ36, VID, 5, 4, 0).zyxw;\n"
      "return OutV;\n"
      "}\n";
  CHECK(!xeo3::vgpu::detail::PatchXenosIndexBufferSemantics(
      malformedIndexedVertexShaderSource,
      sizeof(malformedIndexedVertexShaderSource) - 1, patchedIndexShader,
      indexPatchCount));
  CHECK(indexPatchCount == 0);

  constexpr std::array<std::uint8_t, 6> expectedTightAlignmentGate{
      0x0F, 0x85, 0xCA, 0x2D, 0x04, 0x00,
  };
  CHECK(xeo3::vgpu::detail::HasExpectedTightAlignmentGate(
      expectedTightAlignmentGate.data(), expectedTightAlignmentGate.size()));
  auto badTightAlignmentGate = expectedTightAlignmentGate;
  badTightAlignmentGate[5] ^= 0xFF;
  CHECK(!xeo3::vgpu::detail::HasExpectedTightAlignmentGate(
      badTightAlignmentGate.data(), badTightAlignmentGate.size()));

  constexpr std::array<std::uint8_t, 6> expectedPlacedResourceGate{
      0x0F, 0x84, 0x9C, 0x2D, 0x04, 0x00,
  };
  CHECK(xeo3::vgpu::detail::HasExpectedPlacedResourceGate(
      expectedPlacedResourceGate.data(), expectedPlacedResourceGate.size()));
  auto badPlacedResourceGate = expectedPlacedResourceGate;
  badPlacedResourceGate[1] ^= 0xFF;
  CHECK(!xeo3::vgpu::detail::HasExpectedPlacedResourceGate(
      badPlacedResourceGate.data(), badPlacedResourceGate.size()));

  constexpr std::uint32_t dxgiErrorInvalidCall = 0x887A0001;
  constexpr std::uint32_t bufferDimension = 1;
  constexpr std::uint32_t texture2dDimension = 3;
  constexpr std::uint32_t undefinedLayout = 0xFFFFFFFF;
  CHECK(xeo3::vgpu::detail::ShouldRetryInvalidModernBufferAsLegacy(
      dxgiErrorInvalidCall, false, bufferDimension, undefinedLayout, 0));
  CHECK(!xeo3::vgpu::detail::ShouldRetryInvalidModernBufferAsLegacy(
      0, false, bufferDimension, undefinedLayout, 0));
  CHECK(!xeo3::vgpu::detail::ShouldRetryInvalidModernBufferAsLegacy(
      dxgiErrorInvalidCall, true, bufferDimension, undefinedLayout, 0));
  CHECK(!xeo3::vgpu::detail::ShouldRetryInvalidModernBufferAsLegacy(
      dxgiErrorInvalidCall, false, texture2dDimension, undefinedLayout, 0));
  CHECK(!xeo3::vgpu::detail::ShouldRetryInvalidModernBufferAsLegacy(
      dxgiErrorInvalidCall, false, bufferDimension, 0, 0));
  CHECK(!xeo3::vgpu::detail::ShouldRetryInvalidModernBufferAsLegacy(
      dxgiErrorInvalidCall, false, bufferDimension, undefinedLayout, 1));

  constexpr std::uint32_t invalidArgument = 0x80070057;
  constexpr std::uint64_t heapSize = 128ULL * 1024 * 1024;
  constexpr std::uint64_t allocationSize = 22'282'240;
  CHECK(xeo3::vgpu::detail::DoesPlacedResourceOverflowHeap(heapSize, heapSize,
                                                           allocationSize));
  CHECK(xeo3::vgpu::detail::DoesPlacedResourceOverflowHeap(
      heapSize, heapSize - allocationSize + 1, allocationSize));
  CHECK(!xeo3::vgpu::detail::DoesPlacedResourceOverflowHeap(
      heapSize, heapSize - allocationSize, allocationSize));
  CHECK(xeo3::vgpu::detail::DoesPlacedResourceOverflowHeap(
      heapSize, heapSize + 1, allocationSize));
  CHECK(!xeo3::vgpu::detail::DoesPlacedResourceOverflowHeap(heapSize, heapSize,
                                                            0));
  CHECK(!xeo3::vgpu::detail::DoesPlacedResourceOverflowHeap(heapSize, heapSize,
                                                            UINT64_MAX));
  CHECK(!xeo3::vgpu::detail::DoesPlacedResourceOverflowHeap(0, 0,
                                                            allocationSize));
  CHECK(xeo3::vgpu::detail::ShouldFallbackPlacedResourceToCommitted(
      invalidArgument, false, heapSize, heapSize, allocationSize));
  CHECK(xeo3::vgpu::detail::ShouldFallbackPlacedResourceToCommitted(
      invalidArgument, false, heapSize, heapSize - allocationSize + 1,
      allocationSize));
  CHECK(!xeo3::vgpu::detail::ShouldFallbackPlacedResourceToCommitted(
      invalidArgument, false, heapSize, heapSize - allocationSize,
      allocationSize));
  CHECK(!xeo3::vgpu::detail::ShouldFallbackPlacedResourceToCommitted(
      0, false, heapSize, heapSize, allocationSize));
  CHECK(!xeo3::vgpu::detail::ShouldFallbackPlacedResourceToCommitted(
      invalidArgument, true, heapSize, heapSize, allocationSize));
  CHECK(!xeo3::vgpu::detail::ShouldFallbackPlacedResourceToCommitted(
      invalidArgument, false, heapSize, heapSize, 0));
  CHECK(!xeo3::vgpu::detail::ShouldFallbackPlacedResourceToCommitted(
      invalidArgument, false, heapSize, heapSize, UINT64_MAX));

  constexpr std::uint32_t denyBuffers = 0x4;
  constexpr std::uint32_t denyRtDsTextures = 0x40;
  constexpr std::uint32_t denyNonRtDsTextures = 0x80;
  constexpr std::uint32_t createNotZeroed = 0x1000;
  constexpr std::uint32_t allowShaderAtomics = 0x400;
  CHECK(xeo3::vgpu::detail::GetCommittedResourceHeapFlags(
            denyRtDsTextures | denyNonRtDsTextures) == 0);
  CHECK(xeo3::vgpu::detail::GetCommittedResourceHeapFlags(
            denyBuffers | denyRtDsTextures) == 0);
  CHECK(xeo3::vgpu::detail::GetCommittedResourceHeapFlags(
            denyBuffers | denyNonRtDsTextures) == 0);
  CHECK(xeo3::vgpu::detail::GetCommittedResourceHeapFlags(
            denyRtDsTextures | denyNonRtDsTextures | createNotZeroed |
            allowShaderAtomics) == (createNotZeroed | allowShaderAtomics));

  constexpr std::array<std::uint8_t, 32> abcSha256{
      0xBA, 0x78, 0x16, 0xBF, 0x8F, 0x01, 0xCF, 0xEA,
      0x41, 0x41, 0x40, 0xDE, 0x5D, 0xAE, 0x22, 0x23,
      0xB0, 0x03, 0x61, 0xA3, 0x96, 0x17, 0x7A, 0x9C,
      0xB4, 0x10, 0xFF, 0x61, 0xF2, 0x00, 0x15, 0xAD,
  };
  std::array<std::uint8_t, 32> computedSha256{};
  constexpr char sha256Input[] = "abc";
  CHECK(xeo3::vgpu::detail::HashBytesSha256(
      sha256Input, sizeof(sha256Input) - 1, computedSha256));
  CHECK(computedSha256 == abcSha256);
  CHECK(!xeo3::vgpu::detail::HashBytesSha256(
      nullptr, 1, computedSha256));

  using TestVertexShaderSubobject = TestPipelineStateStreamSubobject<
      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS, D3D12_SHADER_BYTECODE>;
  using TestPixelShaderSubobject = TestPipelineStateStreamSubobject<
      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS, D3D12_SHADER_BYTECODE>;
  using TestBlendSubobject = TestPipelineStateStreamSubobject<
      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND, D3D12_BLEND_DESC>;
  using TestRasterizerSubobject = TestPipelineStateStreamSubobject<
      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER,
      D3D12_RASTERIZER_DESC>;
  using TestDepthStencilSubobject = TestPipelineStateStreamSubobject<
      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL,
      D3D12_DEPTH_STENCIL_DESC>;
  using TestInputLayoutSubobject = TestPipelineStateStreamSubobject<
      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT,
      D3D12_INPUT_LAYOUT_DESC>;
  using TestRenderTargetFormatsSubobject = TestPipelineStateStreamSubobject<
      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS,
      D3D12_RT_FORMAT_ARRAY>;
  using TestSampleDescriptionSubobject = TestPipelineStateStreamSubobject<
      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC, DXGI_SAMPLE_DESC>;
  struct alignas(void *) TestPipelineStream {
    TestVertexShaderSubobject vertexShader;
    TestPixelShaderSubobject pixelShader;
    TestBlendSubobject blend;
    TestRasterizerSubobject rasterizer;
    TestDepthStencilSubobject depthStencil;
    TestInputLayoutSubobject inputLayout;
    TestRenderTargetFormatsSubobject renderTargetFormats;
    TestSampleDescriptionSubobject sampleDescription;
  };

  std::array<std::uint8_t, 2224> streamVertexShader{};
  std::array<std::uint8_t, 2440> streamPixelShader{};
  std::array<D3D12_INPUT_ELEMENT_DESC, 2> streamInputElements{
      D3D12_INPUT_ELEMENT_DESC{
          "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      D3D12_INPUT_ELEMENT_DESC{
          "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  };
  TestPipelineStream pipelineStream{};
  pipelineStream.vertexShader.value = {
      streamVertexShader.data(), streamVertexShader.size()};
  pipelineStream.pixelShader.value = {
      streamPixelShader.data(), streamPixelShader.size()};
  auto &streamRenderTarget = pipelineStream.blend.value.RenderTarget[0];
  streamRenderTarget.SrcBlend = D3D12_BLEND_ONE;
  streamRenderTarget.DestBlend = D3D12_BLEND_ZERO;
  streamRenderTarget.BlendOp = D3D12_BLEND_OP_ADD;
  streamRenderTarget.SrcBlendAlpha = D3D12_BLEND_ONE;
  streamRenderTarget.DestBlendAlpha = D3D12_BLEND_ZERO;
  streamRenderTarget.BlendOpAlpha = D3D12_BLEND_OP_ADD;
  streamRenderTarget.LogicOp = D3D12_LOGIC_OP_CLEAR;
  streamRenderTarget.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  pipelineStream.rasterizer.value.FillMode = D3D12_FILL_MODE_SOLID;
  pipelineStream.rasterizer.value.CullMode = D3D12_CULL_MODE_NONE;
  pipelineStream.rasterizer.value.ConservativeRaster =
      D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
  pipelineStream.depthStencil.value.DepthWriteMask =
      D3D12_DEPTH_WRITE_MASK_ZERO;
  pipelineStream.inputLayout.value = {
      streamInputElements.data(),
      static_cast<UINT>(streamInputElements.size())};
  pipelineStream.renderTargetFormats.value.NumRenderTargets = 1;
  pipelineStream.renderTargetFormats.value.RTFormats[0] =
      DXGI_FORMAT_R8G8B8A8_UINT;
  pipelineStream.sampleDescription.value = {1, 0};

  xeo3::vgpu::GraphicsPipelineSignature streamSignature{};
  std::size_t streamPixelShaderOffset = 0;
  CHECK(xeo3::vgpu::detail::ExtractGraphicsPipelineStreamSignature(
      &pipelineStream, sizeof(pipelineStream), streamSignature,
      streamPixelShaderOffset));
  CHECK(streamPixelShaderOffset ==
        offsetof(TestPipelineStream, pixelShader) +
            offsetof(TestPixelShaderSubobject, value));
  CHECK(streamSignature.vertexShaderSize == streamVertexShader.size());
  CHECK(streamSignature.pixelShaderSize == streamPixelShader.size());
  CHECK(streamSignature.sampleCount == 1);
  CHECK(streamSignature.hasExpectedInputLayout);
  CHECK(streamSignature.hasExpectedEdramLoadFixedState);
  CHECK(xeo3::vgpu::detail::IsAc6EdramLoadPipelineDescriptor(
      streamSignature));
  CHECK(!xeo3::vgpu::detail::IsAc6EdramLoadPipeline(streamSignature));
  CHECK(!xeo3::vgpu::detail::ExtractGraphicsPipelineStreamSignature(
      &pipelineStream, sizeof(pipelineStream) - 1, streamSignature,
      streamPixelShaderOffset));

  xeo3::vgpu::GraphicsPipelineSignature restoreSignature{};
  restoreSignature.vertexShaderSha256 = {
      0x7F, 0x3F, 0x8E, 0x0E, 0xEC, 0x40, 0x28, 0xEC,
      0xCF, 0xBF, 0x2E, 0x63, 0x62, 0x1B, 0x28, 0x04,
      0x95, 0x28, 0xEC, 0xFA, 0xE2, 0xEC, 0xEC, 0x50,
      0x45, 0xB5, 0xD6, 0x5E, 0x72, 0xCB, 0xA8, 0x1E,
  };
  restoreSignature.pixelShaderSha256 = {
      0x3A, 0x20, 0x6A, 0x6D, 0xC3, 0xF9, 0xAE, 0xEE,
      0x03, 0x8F, 0xEE, 0xFD, 0x23, 0x57, 0x67, 0xCD,
      0x40, 0xF9, 0x92, 0xE1, 0x7B, 0xE9, 0x4B, 0xA2,
      0x0B, 0xA8, 0x8F, 0x90, 0x4A, 0x2F, 0x23, 0x81,
  };
  restoreSignature.vertexShaderSize = 2224;
  restoreSignature.pixelShaderSize = 2440;
  restoreSignature.sampleMask = UINT32_MAX;
  restoreSignature.primitiveTopologyType = 3;
  restoreSignature.sampleCount = 2;
  restoreSignature.sampleQuality = 0;
  restoreSignature.renderTargetCount = 1;
  restoreSignature.renderTarget0Format = 30;
  restoreSignature.depthStencilFormat = 0;
  restoreSignature.inputElementCount = 2;
  restoreSignature.renderTarget0WriteMask = 15;
  restoreSignature.hasExpectedInputLayout = true;
  restoreSignature.hasExpectedFixedState = true;
  CHECK(xeo3::vgpu::detail::IsAc6CorruptEdramRestorePipeline(
      restoreSignature));

  auto mismatchedRestoreSignature = restoreSignature;
  mismatchedRestoreSignature.pixelShaderSha256[0] ^= 0xFF;
  CHECK(xeo3::vgpu::detail::IsAc6CorruptEdramRestorePipeline(
      mismatchedRestoreSignature));
  mismatchedRestoreSignature = restoreSignature;
  mismatchedRestoreSignature.vertexShaderSha256.fill(0);
  CHECK(!xeo3::vgpu::detail::IsAc6CorruptEdramRestorePipeline(
      mismatchedRestoreSignature));
  mismatchedRestoreSignature = restoreSignature;
  mismatchedRestoreSignature.sampleCount = 1;
  CHECK(!xeo3::vgpu::detail::IsAc6CorruptEdramRestorePipeline(
      mismatchedRestoreSignature));
  mismatchedRestoreSignature = restoreSignature;
  mismatchedRestoreSignature.hasExpectedInputLayout = false;
  CHECK(!xeo3::vgpu::detail::IsAc6CorruptEdramRestorePipeline(
      mismatchedRestoreSignature));

  std::size_t edramScaleShaderSize = 0;
  const auto *const edramScaleShader =
      xeo3::vgpu::detail::GetAc6EdramScaleFixPixelShader(
          edramScaleShaderSize);
  CHECK(edramScaleShader != nullptr);
  CHECK(edramScaleShaderSize == 4456);
  constexpr std::array<std::uint8_t, 32> edramScaleShaderSha256{
      0x7B, 0x95, 0xD5, 0xED, 0x49, 0x02, 0x17, 0xAA,
      0xAB, 0xE0, 0xFC, 0x50, 0xD9, 0xF2, 0x20, 0x5A,
      0x97, 0xB5, 0xB1, 0x5D, 0x0C, 0x93, 0xD6, 0xD6,
      0x8E, 0x73, 0x8B, 0xCD, 0xBE, 0x38, 0x6D, 0x3A,
  };
  CHECK(xeo3::vgpu::detail::HashBytesSha256(
      edramScaleShader, edramScaleShaderSize, computedSha256));
  CHECK(computedSha256 == edramScaleShaderSha256);

  xeo3::vgpu::GraphicsPipelineSignature edramScaleSignature{};
  edramScaleSignature.vertexShaderSha256 = {
      0x7F, 0x3F, 0x8E, 0x0E, 0xEC, 0x40, 0x28, 0xEC,
      0xCF, 0xBF, 0x2E, 0x63, 0x62, 0x1B, 0x28, 0x04,
      0x95, 0x28, 0xEC, 0xFA, 0xE2, 0xEC, 0xEC, 0x50,
      0x45, 0xB5, 0xD6, 0x5E, 0x72, 0xCB, 0xA8, 0x1E,
  };
  edramScaleSignature.pixelShaderSha256 = {
      0x1E, 0x88, 0x74, 0xA8, 0xEE, 0x00, 0x5E, 0x72,
      0x4F, 0xB5, 0xC4, 0x55, 0xE2, 0xF0, 0x35, 0x07,
      0xB4, 0x16, 0x59, 0xE6, 0x12, 0x46, 0x78, 0xE9,
      0x41, 0x2C, 0x87, 0x30, 0x5B, 0x49, 0x62, 0x51,
  };
  edramScaleSignature.vertexShaderSize = 2224;
  edramScaleSignature.pixelShaderSize = 2528;
  edramScaleSignature.sampleMask = UINT32_MAX;
  edramScaleSignature.primitiveTopologyType = 3;
  edramScaleSignature.sampleCount = 4;
  edramScaleSignature.sampleQuality = 0;
  edramScaleSignature.renderTargetCount = 1;
  edramScaleSignature.renderTarget0Format = 30;
  edramScaleSignature.depthStencilFormat = 0;
  edramScaleSignature.inputElementCount = 2;
  edramScaleSignature.renderTarget0WriteMask = 15;
  edramScaleSignature.hasExpectedInputLayout = true;
  edramScaleSignature.hasExpectedEdramScaleFixedState = true;
  CHECK(xeo3::vgpu::detail::IsAc6EdramScalePipeline(
      edramScaleSignature));
  CHECK(xeo3::vgpu::detail::IsAc6EdramScalePipelineDescriptor(
      edramScaleSignature));

  auto mismatchedEdramScaleSignature = edramScaleSignature;
  mismatchedEdramScaleSignature.vertexShaderSha256[0] ^= 0xFF;
  CHECK(!xeo3::vgpu::detail::IsAc6EdramScalePipeline(
      mismatchedEdramScaleSignature));
  CHECK(xeo3::vgpu::detail::IsAc6EdramScalePipelineDescriptor(
      mismatchedEdramScaleSignature));
  mismatchedEdramScaleSignature = edramScaleSignature;
  mismatchedEdramScaleSignature.pixelShaderSha256[31] ^= 0xFF;
  CHECK(!xeo3::vgpu::detail::IsAc6EdramScalePipeline(
      mismatchedEdramScaleSignature));
  mismatchedEdramScaleSignature = edramScaleSignature;
  mismatchedEdramScaleSignature.sampleCount = 2;
  CHECK(!xeo3::vgpu::detail::IsAc6EdramScalePipeline(
      mismatchedEdramScaleSignature));
  mismatchedEdramScaleSignature = edramScaleSignature;
  mismatchedEdramScaleSignature.hasExpectedEdramScaleFixedState = false;
  CHECK(!xeo3::vgpu::detail::IsAc6EdramScalePipeline(
      mismatchedEdramScaleSignature));
  CHECK(!xeo3::vgpu::detail::IsAc6EdramScalePipelineDescriptor(
      mismatchedEdramScaleSignature));
  mismatchedEdramScaleSignature = edramScaleSignature;
  mismatchedEdramScaleSignature.renderTarget0WriteMask = 0;
  CHECK(!xeo3::vgpu::detail::IsAc6EdramScalePipeline(
      mismatchedEdramScaleSignature));

  std::size_t edramLoadShaderSize = 0;
  const auto *const edramLoadShader =
      xeo3::vgpu::detail::GetAc6EdramLoadFixPixelShader(
          edramLoadShaderSize);
  CHECK(edramLoadShader != nullptr);
  CHECK(edramLoadShaderSize == 4200);
  constexpr std::array<std::uint8_t, 32> edramLoadShaderSha256{
      0xB4, 0x9F, 0x8C, 0x14, 0x21, 0x57, 0x21, 0xDF,
      0x22, 0x69, 0x31, 0xCA, 0xF5, 0xE4, 0xC7, 0xFD,
      0xBF, 0xE9, 0xBA, 0xDF, 0xC7, 0x8C, 0xFB, 0xE3,
      0xDB, 0x76, 0xC1, 0xAB, 0x70, 0xD9, 0x7F, 0xB4,
  };
  CHECK(xeo3::vgpu::detail::HashBytesSha256(
      edramLoadShader, edramLoadShaderSize, computedSha256));
  CHECK(computedSha256 == edramLoadShaderSha256);

  std::size_t edramTransferVertexShaderSize = 0;
  const auto *const edramTransferVertexShader =
      xeo3::vgpu::detail::GetAc6EdramTransferVertexShader(
          edramTransferVertexShaderSize);
  CHECK(edramTransferVertexShader != nullptr);
  CHECK(edramTransferVertexShaderSize == 2224);
  constexpr std::array<std::uint8_t, 32>
      edramTransferVertexShaderSha256{
          0x7F, 0x3F, 0x8E, 0x0E, 0xEC, 0x40, 0x28, 0xEC,
          0xCF, 0xBF, 0x2E, 0x63, 0x62, 0x1B, 0x28, 0x04,
          0x95, 0x28, 0xEC, 0xFA, 0xE2, 0xEC, 0xEC, 0x50,
          0x45, 0xB5, 0xD6, 0x5E, 0x72, 0xCB, 0xA8, 0x1E,
      };
  CHECK(xeo3::vgpu::detail::HashBytesSha256(
      edramTransferVertexShader, edramTransferVertexShaderSize,
      computedSha256));
  CHECK(computedSha256 == edramTransferVertexShaderSha256);

  std::size_t pso341WidthFixShaderSize = 0;
  const auto *const pso341WidthFixShader =
      xeo3::vgpu::detail::GetAc6Pso341WidthFixComputeShader(
          pso341WidthFixShaderSize);
  CHECK(pso341WidthFixShader != nullptr);
  CHECK(pso341WidthFixShaderSize == 8708);
  constexpr std::array<std::uint8_t, 32> pso341WidthFixShaderSha256{
      0x15, 0x8B, 0xF9, 0x3B, 0x7D, 0x29, 0x2D, 0xD4,
      0x6E, 0xB7, 0x8E, 0xF2, 0x60, 0x5D, 0xFA, 0xFB,
      0x9D, 0xED, 0xEF, 0x19, 0x71, 0xE3, 0x63, 0x4C,
      0xDB, 0xF9, 0x07, 0xF5, 0x79, 0x4F, 0xEF, 0x38,
  };
  CHECK(xeo3::vgpu::detail::HashBytesSha256(
      pso341WidthFixShader, pso341WidthFixShaderSize, computedSha256));
  CHECK(computedSha256 == pso341WidthFixShaderSha256);

  auto edramLoadSignature = restoreSignature;
  edramLoadSignature.sampleCount = 1;
  edramLoadSignature.hasExpectedFixedState = false;
  edramLoadSignature.hasExpectedEdramLoadFixedState = true;
  CHECK(xeo3::vgpu::detail::IsAc6EdramLoadPipeline(
      edramLoadSignature));
  CHECK(xeo3::vgpu::detail::IsAc6EdramLoadPipelineDescriptor(
      edramLoadSignature));

  auto mismatchedEdramLoadSignature = edramLoadSignature;
  mismatchedEdramLoadSignature.pixelShaderSha256[0] ^= 0xFF;
  CHECK(!xeo3::vgpu::detail::IsAc6EdramLoadPipeline(
      mismatchedEdramLoadSignature));
  CHECK(xeo3::vgpu::detail::IsAc6EdramLoadPipelineDescriptor(
      mismatchedEdramLoadSignature));
  mismatchedEdramLoadSignature = edramLoadSignature;
  mismatchedEdramLoadSignature.sampleCount = 2;
  CHECK(!xeo3::vgpu::detail::IsAc6EdramLoadPipeline(
      mismatchedEdramLoadSignature));
  mismatchedEdramLoadSignature = edramLoadSignature;
  mismatchedEdramLoadSignature.hasExpectedEdramLoadFixedState = false;
  CHECK(!xeo3::vgpu::detail::IsAc6EdramLoadPipelineDescriptor(
      mismatchedEdramLoadSignature));

  std::array<std::uint8_t, xeo3::vgpu::kDrawRecordMinimumSize>
      restoreDrawRecord{};
  const auto restoreRootSignature =
      reinterpret_cast<const void *>(static_cast<std::uintptr_t>(0x12340000));
  const auto restorePipelineState =
      reinterpret_cast<const void *>(static_cast<std::uintptr_t>(0x56780000));
  Store(restoreDrawRecord, 0x08,
        reinterpret_cast<std::uintptr_t>(restoreRootSignature));
  Store(restoreDrawRecord, 0x10,
        reinterpret_cast<std::uintptr_t>(restorePipelineState));
  Store(restoreDrawRecord, 0x18, std::uint32_t{0x44A00000});
  Store(restoreDrawRecord, 0x1C, std::uint32_t{0x44340000});
  Store(restoreDrawRecord, 0x20, std::uint32_t{0});
  Store(restoreDrawRecord, 0x24, std::uint32_t{0x3F800000});
  Store(restoreDrawRecord, 0x28, std::uint32_t{0});
  Store(restoreDrawRecord, 0x2C, std::uint32_t{0});
  Store(restoreDrawRecord, 0x30, std::uint32_t{640});
  Store(restoreDrawRecord, 0x34, std::uint32_t{360});
  Store(restoreDrawRecord, 0xC8, std::uint32_t{0});
  Store(restoreDrawRecord, 0xCC, std::uint32_t{3});
  Store(restoreDrawRecord, 0xD0, std::uint32_t{0});

  auto halfWidthFullscreenRecord = restoreDrawRecord;
  Store(halfWidthFullscreenRecord, 0x34, std::uint32_t{720});
  CHECK(xeo3::vgpu::detail::PatchAc6HalfWidthFullscreenScissor(
      halfWidthFullscreenRecord.data(), halfWidthFullscreenRecord.size()));
  CHECK(Load<std::uint32_t>(halfWidthFullscreenRecord, 0x30) == 1280);
  CHECK(!xeo3::vgpu::detail::PatchAc6HalfWidthFullscreenScissor(
      halfWidthFullscreenRecord.data(), halfWidthFullscreenRecord.size()));
  CHECK(!xeo3::vgpu::detail::PatchAc6HalfWidthFullscreenScissor(
      nullptr, halfWidthFullscreenRecord.size()));
  auto nonFullscreenRecord = restoreDrawRecord;
  Store(nonFullscreenRecord, 0x34, std::uint32_t{720});
  Store(nonFullscreenRecord, 0xD0, std::uint32_t{1});
  CHECK(!xeo3::vgpu::detail::PatchAc6HalfWidthFullscreenScissor(
      nonFullscreenRecord.data(), nonFullscreenRecord.size()));

  xeo3::vgpu::DrawRecordSignature extractedDrawSignature{};
  CHECK(xeo3::vgpu::detail::ExtractDrawRecordSignature(
      restoreDrawRecord.data(), restoreDrawRecord.size(),
      extractedDrawSignature));
  CHECK(extractedDrawSignature.rootSignature ==
        reinterpret_cast<std::uintptr_t>(restoreRootSignature));
  CHECK(extractedDrawSignature.pipelineState ==
        reinterpret_cast<std::uintptr_t>(restorePipelineState));
  CHECK(extractedDrawSignature.viewportWidthBits == 0x44A00000U);
  CHECK(extractedDrawSignature.viewportHeightBits == 0x44340000U);
  CHECK(extractedDrawSignature.viewportMinDepthBits == 0);
  CHECK(extractedDrawSignature.viewportMaxDepthBits == 0x3F800000U);
  CHECK(extractedDrawSignature.viewportTopLeftXBits == 0);
  CHECK(extractedDrawSignature.viewportTopLeftYBits == 0);
  CHECK(extractedDrawSignature.scissorRight == 640);
  CHECK(extractedDrawSignature.scissorBottom == 360);
  CHECK(extractedDrawSignature.recordKind == 0);
  CHECK(extractedDrawSignature.vertexCount == 3);
  CHECK(extractedDrawSignature.startVertex == 0);

  auto scaleDrawSignature = extractedDrawSignature;
  scaleDrawSignature.viewportWidthBits = 0x44200000U;
  scaleDrawSignature.viewportHeightBits = 0x43B40000U;
  scaleDrawSignature.scissorRight = 1280;
  scaleDrawSignature.scissorBottom = 720;
  CHECK(xeo3::vgpu::detail::ClassifyAc6EdramDrawPipeline(
            scaleDrawSignature, restorePipelineState) ==
        xeo3::vgpu::Ac6EdramDrawPipeline::Scale);
  auto pso619DrawSignature = scaleDrawSignature;
  pso619DrawSignature.viewportMaxDepthBits = 0x40000000U;
  CHECK(xeo3::vgpu::detail::ClassifyAc6EdramDrawPipeline(
            pso619DrawSignature, restorePipelineState) ==
        xeo3::vgpu::Ac6EdramDrawPipeline::None);

  constexpr std::array<std::array<std::uint32_t, 4>, 11>
      loadDrawShapes{{
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
  for (const auto &shape : loadDrawShapes) {
    auto loadDrawSignature = extractedDrawSignature;
    loadDrawSignature.viewportWidthBits = shape[0];
    loadDrawSignature.viewportHeightBits = shape[1];
    loadDrawSignature.scissorRight = shape[2];
    loadDrawSignature.scissorBottom = shape[3];
    CHECK(xeo3::vgpu::detail::ClassifyAc6EdramDrawPipeline(
              loadDrawSignature, restorePipelineState) ==
          xeo3::vgpu::Ac6EdramDrawPipeline::Load);
  }
  auto unrelatedDrawSignature = scaleDrawSignature;
  unrelatedDrawSignature.scissorRight = 1279;
  CHECK(xeo3::vgpu::detail::ClassifyAc6EdramDrawPipeline(
            unrelatedDrawSignature, restorePipelineState) ==
        xeo3::vgpu::Ac6EdramDrawPipeline::None);
  CHECK(xeo3::vgpu::detail::ClassifyAc6EdramDrawPipeline(
            scaleDrawSignature, nullptr) ==
        xeo3::vgpu::Ac6EdramDrawPipeline::None);
  CHECK(xeo3::vgpu::detail::ClassifyAc6EdramDrawPipeline(
            scaleDrawSignature,
            reinterpret_cast<const void *>(
                reinterpret_cast<std::uintptr_t>(restorePipelineState) + 8)) ==
        xeo3::vgpu::Ac6EdramDrawPipeline::None);
  CHECK(xeo3::vgpu::detail::IsAc6CorruptEdramRestoreDrawSignature(
      extractedDrawSignature, restorePipelineState));
  CHECK(xeo3::vgpu::detail::ShouldSuppressAc6HostEdramRestoreDraw(
      extractedDrawSignature, restorePipelineState,
      xeo3::vgpu::Ac6EdramDrawPipeline::Load, true));
  CHECK(!xeo3::vgpu::detail::ShouldSuppressAc6HostEdramRestoreDraw(
      extractedDrawSignature, restorePipelineState,
      xeo3::vgpu::Ac6EdramDrawPipeline::Load, false));
  CHECK(!xeo3::vgpu::detail::ShouldSuppressAc6HostEdramRestoreDraw(
      extractedDrawSignature, restorePipelineState,
      xeo3::vgpu::Ac6EdramDrawPipeline::Scale, true));
  CHECK(!xeo3::vgpu::detail::ShouldSuppressAc6HostEdramRestoreDraw(
      extractedDrawSignature, restorePipelineState,
      xeo3::vgpu::Ac6EdramDrawPipeline::None, true));
  auto mismatchedHostRestoreSignature = extractedDrawSignature;
  mismatchedHostRestoreSignature.scissorRight = 1280;
  CHECK(!xeo3::vgpu::detail::ShouldSuppressAc6HostEdramRestoreDraw(
      mismatchedHostRestoreSignature, restorePipelineState,
      xeo3::vgpu::Ac6EdramDrawPipeline::Load, true));
  CHECK(!xeo3::vgpu::detail::ShouldSuppressAc6HostEdramRestoreDraw(
      extractedDrawSignature, nullptr,
      xeo3::vgpu::Ac6EdramDrawPipeline::Load, true));

  constexpr std::array<std::uint8_t, 32> loadCachedBlobSha256{
      0x5E, 0xE6, 0xE2, 0xC4, 0x21, 0xA6, 0xE2, 0xF1,
      0x31, 0x17, 0x1E, 0x1B, 0x2E, 0x1C, 0x2B, 0x5E,
      0x6F, 0x77, 0xF2, 0xA1, 0xE3, 0x6B, 0x25, 0xDD,
      0x99, 0x8A, 0x59, 0x12, 0xAF, 0x17, 0x97, 0x11,
  };
  constexpr std::array<std::uint8_t, 32> scaleCachedBlobSha256{
      0xBC, 0xFC, 0x09, 0x66, 0x3F, 0x3E, 0x30, 0x80,
      0x3D, 0x7A, 0xF5, 0x09, 0xA0, 0xFD, 0x9F, 0xAD,
      0xFD, 0x3E, 0x31, 0x70, 0x23, 0xB5, 0x46, 0xFC,
      0x9C, 0x08, 0xA8, 0xEE, 0xAD, 0xDA, 0x71, 0xBE,
  };
  constexpr std::array<std::uint8_t, 32> currentLoadCachedBlobSha256{
      0xC7, 0xCA, 0xC6, 0xC6, 0xB4, 0x75, 0x39, 0x00,
      0x00, 0x44, 0x8A, 0xBE, 0x06, 0x83, 0x15, 0x39,
      0x66, 0x87, 0x3E, 0xA7, 0x3F, 0x3F, 0xC0, 0xCC,
      0x49, 0x8B, 0x54, 0x90, 0x97, 0x35, 0xF6, 0x74,
  };
  constexpr std::array<std::uint8_t, 32> currentScaleCachedBlobSha256{
      0x16, 0xE9, 0x02, 0xC9, 0xF0, 0xD5, 0x32, 0xB3,
      0x0D, 0x1C, 0xC0, 0x7E, 0xE2, 0xB7, 0xCD, 0xE5,
      0x5F, 0x4F, 0x00, 0x5A, 0xDE, 0x82, 0x9A, 0x5A,
      0x83, 0x6D, 0xB4, 0xFF, 0xF9, 0x98, 0x1D, 0x87,
  };
  CHECK(xeo3::vgpu::detail::ClassifyAc6EdramCachedPipelineBlob(
            954, loadCachedBlobSha256) ==
        xeo3::vgpu::Ac6EdramDrawPipeline::Load);
  CHECK(xeo3::vgpu::detail::ClassifyAc6EdramCachedPipelineBlob(
            954, scaleCachedBlobSha256) ==
        xeo3::vgpu::Ac6EdramDrawPipeline::Scale);
  CHECK(xeo3::vgpu::detail::ClassifyAc6EdramCachedPipelineBlob(
            954, currentLoadCachedBlobSha256) ==
        xeo3::vgpu::Ac6EdramDrawPipeline::Load);
  CHECK(xeo3::vgpu::detail::ClassifyAc6EdramCachedPipelineBlob(
            954, currentScaleCachedBlobSha256) ==
        xeo3::vgpu::Ac6EdramDrawPipeline::Scale);
  CHECK(xeo3::vgpu::detail::ClassifyAc6EdramCachedPipelineBlob(
            953, loadCachedBlobSha256) ==
        xeo3::vgpu::Ac6EdramDrawPipeline::None);
  auto unknownCachedBlobSha256 = loadCachedBlobSha256;
  unknownCachedBlobSha256[31] ^= 1;
  CHECK(xeo3::vgpu::detail::ClassifyAc6EdramCachedPipelineBlob(
            954, unknownCachedBlobSha256) ==
        xeo3::vgpu::Ac6EdramDrawPipeline::None);
  extractedDrawSignature.rootSignature = 1;
  CHECK(!xeo3::vgpu::detail::ExtractDrawRecordSignature(
      nullptr, restoreDrawRecord.size(), extractedDrawSignature));
  CHECK(extractedDrawSignature.rootSignature == 0);
  CHECK(!xeo3::vgpu::detail::ExtractDrawRecordSignature(
      restoreDrawRecord.data(), restoreDrawRecord.size() - 1,
      extractedDrawSignature));
  CHECK(xeo3::vgpu::detail::IsAc6CorruptEdramRestoreDrawRecord(
      restoreDrawRecord.data(), restoreDrawRecord.size(),
      restorePipelineState));
  auto zeroPipelineRestoreDrawRecord = restoreDrawRecord;
  Store(zeroPipelineRestoreDrawRecord, 0x10, std::uintptr_t{0});
  CHECK(xeo3::vgpu::detail::IsAc6CorruptEdramRestoreDrawRecord(
      zeroPipelineRestoreDrawRecord.data(),
      zeroPipelineRestoreDrawRecord.size(), restorePipelineState));
  CHECK(!xeo3::vgpu::detail::IsAc6CorruptEdramRestoreDrawRecord(
      nullptr, restoreDrawRecord.size(), restorePipelineState));
  CHECK(!xeo3::vgpu::detail::IsAc6CorruptEdramRestoreDrawRecord(
      restoreDrawRecord.data(), restoreDrawRecord.size() - 1,
      restorePipelineState));
  CHECK(!xeo3::vgpu::detail::IsAc6CorruptEdramRestoreDrawRecord(
      restoreDrawRecord.data(), restoreDrawRecord.size(), nullptr));
  CHECK(!xeo3::vgpu::detail::IsAc6CorruptEdramRestoreDrawRecord(
      restoreDrawRecord.data(), restoreDrawRecord.size(),
      reinterpret_cast<const void *>(
          reinterpret_cast<std::uintptr_t>(restorePipelineState) + 8)));

  auto mismatchedRestoreDrawRecord = restoreDrawRecord;
  Store(mismatchedRestoreDrawRecord, 0x30, std::uint32_t{1280});
  CHECK(!xeo3::vgpu::detail::IsAc6CorruptEdramRestoreDrawRecord(
      mismatchedRestoreDrawRecord.data(),
      mismatchedRestoreDrawRecord.size(), restorePipelineState));
  mismatchedRestoreDrawRecord = restoreDrawRecord;
  Store(mismatchedRestoreDrawRecord, 0x34, std::uint32_t{720});
  CHECK(!xeo3::vgpu::detail::IsAc6CorruptEdramRestoreDrawRecord(
      mismatchedRestoreDrawRecord.data(),
      mismatchedRestoreDrawRecord.size(), restorePipelineState));
  mismatchedRestoreDrawRecord = restoreDrawRecord;
  Store(mismatchedRestoreDrawRecord, 0xCC, std::uint32_t{4});
  CHECK(!xeo3::vgpu::detail::IsAc6CorruptEdramRestoreDrawRecord(
      mismatchedRestoreDrawRecord.data(),
      mismatchedRestoreDrawRecord.size(), restorePipelineState));

  constexpr std::array<std::uint8_t, 32> restoreCachedBlobSha256{
      0x52, 0x73, 0x70, 0x28, 0xBA, 0xFA, 0x14, 0x4C,
      0x68, 0x48, 0x4A, 0x49, 0x5F, 0x75, 0x72, 0xF1,
      0x17, 0x9E, 0xA7, 0xB9, 0xF1, 0x18, 0x8F, 0xB9,
      0x9A, 0xD6, 0xA9, 0x90, 0x61, 0xDD, 0x28, 0xC4,
  };
  CHECK(xeo3::vgpu::detail::IsAc6CorruptEdramRestoreCachedBlob(
      954, restoreCachedBlobSha256));
  CHECK(!xeo3::vgpu::detail::IsAc6CorruptEdramRestoreCachedBlob(
      953, restoreCachedBlobSha256));
  auto mismatchedRestoreCachedBlobSha256 = restoreCachedBlobSha256;
  mismatchedRestoreCachedBlobSha256[31] ^= 0xFF;
  CHECK(!xeo3::vgpu::detail::IsAc6CorruptEdramRestoreCachedBlob(
      954, mismatchedRestoreCachedBlobSha256));
  return 0;
}
