#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <cstdint>
#include <cwchar>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

#ifndef D3D12SDK_VERSION
#define D3D12SDK_VERSION 618
#endif

extern "C" {
__declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12SDK_VERSION;
__declspec(dllexport) extern const char *D3D12SDKPath = ".\\D3D12\\";
}

namespace {

std::vector<std::uint8_t> ReadFile(const wchar_t *path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream) {
    return {};
  }
  const auto size = stream.tellg();
  if (size <= 0) {
    return {};
  }
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  stream.seekg(0);
  stream.read(reinterpret_cast<char *>(bytes.data()), size);
  return stream ? bytes : std::vector<std::uint8_t>{};
}

bool WriteFile(const wchar_t *path, const void *data, std::size_t size) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  stream.write(static_cast<const char *>(data),
               static_cast<std::streamsize>(size));
  return stream.good();
}

ComPtr<IDXGIAdapter1> FindHardwareAdapter(IDXGIFactory6 *factory) {
  for (UINT index = 0;; ++index) {
    ComPtr<IDXGIAdapter1> adapter;
    if (factory->EnumAdapterByGpuPreference(
            index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&adapter)) == DXGI_ERROR_NOT_FOUND) {
      break;
    }
    DXGI_ADAPTER_DESC1 description{};
    if (FAILED(adapter->GetDesc1(&description)) ||
        (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) {
      continue;
    }
    if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
                                    __uuidof(ID3D12Device), nullptr))) {
      return adapter;
    }
  }
  return nullptr;
}

} // namespace

int wmain(int argc, wchar_t **argv) {
  if (argc < 4 || argc > 6) {
    std::wcerr << L"usage: dump-d3d12-compute-cached-pso.exe "
                  L"<root-signature.bin> <shader.dxil> <output.cached> "
                  L"[node-mask] [pipeline-flags]\n";
    return 2;
  }

  const bool useEmbeddedRootSignature = std::wcscmp(argv[1], L"-") == 0;
  const auto rootSignatureBytes =
      useEmbeddedRootSignature ? std::vector<std::uint8_t>{}
                               : ReadFile(argv[1]);
  const auto shaderBytes = ReadFile(argv[2]);
  if ((!useEmbeddedRootSignature && rootSignatureBytes.empty()) ||
      shaderBytes.empty()) {
    std::wcerr << L"failed to read the root signature or shader\n";
    return 3;
  }

  ComPtr<IDXGIFactory6> factory;
  auto result = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
  if (FAILED(result)) {
    std::wcerr << L"CreateDXGIFactory2 failed: 0x" << std::hex << result
               << L"\n";
    return 4;
  }
  auto adapter = FindHardwareAdapter(factory.Get());
  if (!adapter) {
    std::wcerr << L"no D3D12 hardware adapter found\n";
    return 5;
  }

  DXGI_ADAPTER_DESC1 adapterDescription{};
  adapter->GetDesc1(&adapterDescription);
  std::wcout << L"adapter: " << adapterDescription.Description << L"\n";

  ComPtr<ID3D12Device> device;
  result = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
                             IID_PPV_ARGS(&device));
  if (FAILED(result)) {
    std::wcerr << L"D3D12CreateDevice failed: 0x" << std::hex << result
               << L"\n";
    return 6;
  }

  ComPtr<ID3D12RootSignature> rootSignature;
  if (!useEmbeddedRootSignature) {
    result = device->CreateRootSignature(
        0, rootSignatureBytes.data(), rootSignatureBytes.size(),
        IID_PPV_ARGS(&rootSignature));
    if (FAILED(result)) {
      std::wcerr << L"CreateRootSignature failed: 0x" << std::hex << result
                 << L"\n";
      return 7;
    }
  }

  D3D12_COMPUTE_PIPELINE_STATE_DESC description{};
  description.pRootSignature = rootSignature.Get();
  description.CS = {shaderBytes.data(), shaderBytes.size()};
  description.NodeMask = argc >= 5 ? std::wcstoul(argv[4], nullptr, 0) : 0;
  description.Flags = static_cast<D3D12_PIPELINE_STATE_FLAGS>(
      argc >= 6 ? std::wcstoul(argv[5], nullptr, 0) : 0);

  ComPtr<ID3D12PipelineState> pipelineState;
  result = device->CreateComputePipelineState(&description,
                                               IID_PPV_ARGS(&pipelineState));
  if (FAILED(result)) {
    std::wcerr << L"CreateComputePipelineState failed: 0x" << std::hex
               << result << L"\n";
    return 8;
  }

  ComPtr<ID3DBlob> cachedBlob;
  result = pipelineState->GetCachedBlob(&cachedBlob);
  if (FAILED(result) || !cachedBlob) {
    std::wcerr << L"GetCachedBlob failed: 0x" << std::hex << result << L"\n";
    return 9;
  }
  if (!WriteFile(argv[3], cachedBlob->GetBufferPointer(),
                 cachedBlob->GetBufferSize())) {
    std::wcerr << L"failed to write cached blob\n";
    return 10;
  }

  std::wcout << L"cached bytes: " << std::dec << cachedBlob->GetBufferSize()
             << L", node mask: " << description.NodeMask << L", flags: 0x"
             << std::hex << static_cast<unsigned>(description.Flags) << L"\n";
  return 0;
}
