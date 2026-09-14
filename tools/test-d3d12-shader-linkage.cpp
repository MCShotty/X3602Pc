// Offline shader linkage check. WARP only: no window, queue, or GPU submission.
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <fstream>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {
std::vector<std::uint8_t> ReadBytes(const wchar_t *path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file || file.tellg() <= 0 || file.tellg() > 64 * 1024 * 1024) {
    return {};
  }
  const auto size = static_cast<std::size_t>(file.tellg());
  std::vector<std::uint8_t> bytes(size);
  file.seekg(0);
  file.read(reinterpret_cast<char *>(bytes.data()),
            static_cast<std::streamsize>(size));
  return file ? bytes : std::vector<std::uint8_t>{};
}

bool Check(HRESULT result, const char *operation) {
  std::printf("%s=0x%08X\n", operation, static_cast<unsigned>(result));
  return SUCCEEDED(result);
}

void PrintMessages(ID3D12Device *device) {
  ComPtr<ID3D12InfoQueue> queue;
  if (FAILED(device->QueryInterface(IID_PPV_ARGS(&queue)))) {
    std::puts("D3D12 info queue unavailable");
    return;
  }
  const auto count = queue->GetNumStoredMessagesAllowedByRetrievalFilter();
  for (UINT64 index = 0; index < count; ++index) {
    SIZE_T size = 0;
    if (FAILED(queue->GetMessage(index, nullptr, &size)) || size == 0 ||
        size > 65536) {
      continue;
    }
    std::vector<std::uint8_t> storage(size);
    auto *message = reinterpret_cast<D3D12_MESSAGE *>(storage.data());
    if (SUCCEEDED(queue->GetMessage(index, message, &size))) {
      std::printf("message[%u] severity=%u: %s\n",
                  static_cast<unsigned>(message->ID),
                  static_cast<unsigned>(message->Severity),
                  message->pDescription);
    }
  }
}
} // namespace

int wmain(int argc, wchar_t **argv) {
  if (argc != 5) {
    std::puts("usage: test-d3d12-shader-linkage VS.dxil GS.dxil|- PS.dxil "
              "point|triangle");
    return 2;
  }
  const bool hasGeometry = std::wcscmp(argv[2], L"-") != 0;
  const auto vertex = ReadBytes(argv[1]);
  const auto geometry = hasGeometry ? ReadBytes(argv[2])
                                    : std::vector<std::uint8_t>{};
  const auto pixel = ReadBytes(argv[3]);
  const bool point = std::wcscmp(argv[4], L"point") == 0;
  if (vertex.empty() || pixel.empty() || (hasGeometry && geometry.empty()) ||
      (!point && std::wcscmp(argv[4], L"triangle") != 0)) {
    std::puts("Invalid input files or topology");
    return 2;
  }

  ComPtr<ID3D12Debug> debug;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
    debug->EnableDebugLayer();
  } else {
    std::puts("D3D12 debug layer unavailable; HRESULT remains authoritative");
  }
  ComPtr<IDXGIFactory4> factory;
  ComPtr<IDXGIAdapter> warp;
  ComPtr<ID3D12Device> device;
  if (!Check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "factory") ||
      !Check(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)), "warp") ||
      !Check(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_12_0,
                               IID_PPV_ARGS(&device)), "device")) {
    return 3;
  }
  ComPtr<ID3DBlob> rootBlob;
  ComPtr<ID3D12RootSignature> root;
  if (!Check(D3DGetBlobPart(pixel.data(), pixel.size(), D3D_BLOB_ROOT_SIGNATURE,
                             0, &rootBlob), "extract_root") ||
      !Check(device->CreateRootSignature(0, rootBlob->GetBufferPointer(),
                                           rootBlob->GetBufferSize(),
                                           IID_PPV_ARGS(&root)), "root")) {
    PrintMessages(device.Get());
    return 3;
  }

  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = root.Get();
  desc.VS = {vertex.data(), vertex.size()};
  desc.PS = {pixel.data(), pixel.size()};
  if (hasGeometry) {
    desc.GS = {geometry.data(), geometry.size()};
  }
  desc.SampleMask = UINT_MAX;
  desc.SampleDesc.Count = 1;
  desc.PrimitiveTopologyType = point ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT
                                      : D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  desc.NumRenderTargets = 1;
  desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.DSVFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
  desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  desc.RasterizerState.DepthClipEnable = TRUE;
  desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
  desc.DepthStencilState.FrontFace = {D3D12_STENCIL_OP_KEEP,
      D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP,
      D3D12_COMPARISON_FUNC_ALWAYS};
  desc.DepthStencilState.BackFace = desc.DepthStencilState.FrontFace;
  for (auto &target : desc.BlendState.RenderTarget) {
    target.SrcBlend = D3D12_BLEND_ONE;
    target.DestBlend = D3D12_BLEND_ZERO;
    target.BlendOp = D3D12_BLEND_OP_ADD;
    target.SrcBlendAlpha = D3D12_BLEND_ONE;
    target.DestBlendAlpha = D3D12_BLEND_ZERO;
    target.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    target.LogicOp = D3D12_LOGIC_OP_NOOP;
    target.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  }
  ComPtr<ID3D12PipelineState> pipeline;
  const auto result = device->CreateGraphicsPipelineState(&desc,
                                                          IID_PPV_ARGS(&pipeline));
  Check(result, "pipeline");
  PrintMessages(device.Get());
  return SUCCEEDED(result) && pipeline != nullptr ? 0 : 1;
}
