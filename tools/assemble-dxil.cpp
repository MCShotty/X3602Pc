#include <Windows.h>
#include <dxcapi.h>
#include <wrl/client.h>

#include <cstdint>
#include <iostream>

using Microsoft::WRL::ComPtr;

namespace {
bool WriteBlob(const wchar_t *const path, IDxcBlob *const blob) noexcept {
  if (path == nullptr || blob == nullptr ||
      blob->GetBufferSize() > static_cast<std::size_t>(MAXDWORD)) {
    return false;
  }

  const auto file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return false;
  }

  DWORD written = 0;
  const auto size = static_cast<DWORD>(blob->GetBufferSize());
  const bool succeeded =
      WriteFile(file, blob->GetBufferPointer(), size, &written, nullptr) !=
          FALSE &&
      written == size;
  CloseHandle(file);
  return succeeded;
}

void PrintErrors(IDxcOperationResult *const result) noexcept {
  ComPtr<IDxcBlobEncoding> errors;
  if (result == nullptr || FAILED(result->GetErrorBuffer(&errors)) ||
      errors == nullptr || errors->GetBufferSize() == 0) {
    return;
  }
  std::cerr.write(static_cast<const char *>(errors->GetBufferPointer()),
                  static_cast<std::streamsize>(errors->GetBufferSize()));
}
} // namespace

int wmain(const int argc, wchar_t **const argv) {
  if (argc != 3 && argc != 4) {
    std::wcerr << L"usage: assemble-dxil <input.ll> <output.dxil> "
                  L"[root-signature.dxbc]\n";
    return 2;
  }

  const auto comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE) {
    std::wcerr << L"CoInitializeEx failed: 0x" << std::hex
               << static_cast<std::uint32_t>(comResult) << L'\n';
    return 3;
  }

  ComPtr<IDxcLibrary> library;
  auto result = DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
  if (FAILED(result) || library == nullptr) {
    std::wcerr << L"DxcCreateInstance(CLSID_DxcLibrary) failed: 0x"
               << std::hex << static_cast<std::uint32_t>(result) << L'\n';
    return 4;
  }

  UINT32 codePage = CP_UTF8;
  ComPtr<IDxcBlobEncoding> source;
  result = library->CreateBlobFromFile(argv[1], &codePage, &source);
  if (FAILED(result) || source == nullptr) {
    std::wcerr << L"CreateBlobFromFile failed: 0x" << std::hex
               << static_cast<std::uint32_t>(result) << L'\n';
    return 5;
  }

  ComPtr<IDxcAssembler> assembler;
  result = DxcCreateInstance(CLSID_DxcAssembler, IID_PPV_ARGS(&assembler));
  if (FAILED(result) || assembler == nullptr) {
    std::wcerr << L"DxcCreateInstance(CLSID_DxcAssembler) failed: 0x"
               << std::hex << static_cast<std::uint32_t>(result) << L'\n';
    return 6;
  }

  ComPtr<IDxcOperationResult> operation;
  result = assembler->AssembleToContainer(source.Get(), &operation);
  if (FAILED(result) || operation == nullptr) {
    std::wcerr << L"AssembleToContainer failed: 0x" << std::hex
               << static_cast<std::uint32_t>(result) << L'\n';
    return 7;
  }

  HRESULT status = E_FAIL;
  result = operation->GetStatus(&status);
  PrintErrors(operation.Get());
  if (FAILED(result) || FAILED(status)) {
    std::wcerr << L"DXIL assembly failed: 0x" << std::hex
               << static_cast<std::uint32_t>(FAILED(result) ? result : status)
               << L'\n';
    return 8;
  }

  ComPtr<IDxcBlob> output;
  result = operation->GetResult(&output);
  if (FAILED(result) || output == nullptr) {
    std::wcerr << L"Retrieving assembled DXIL failed: 0x" << std::hex
               << static_cast<std::uint32_t>(result) << L'\n';
    return 9;
  }

  ComPtr<IDxcBlob> finalCandidate = output;
  if (argc == 4) {
    ComPtr<IDxcBlobEncoding> rootSignatureContainer;
    result = library->CreateBlobFromFile(argv[3], &codePage,
                                         &rootSignatureContainer);
    if (FAILED(result) || rootSignatureContainer == nullptr) {
      std::wcerr << L"Loading root-signature container failed: 0x" << std::hex
                 << static_cast<std::uint32_t>(result) << L'\n';
      return 10;
    }

    ComPtr<IDxcContainerReflection> rootReflection;
    result = DxcCreateInstance(CLSID_DxcContainerReflection,
                               IID_PPV_ARGS(&rootReflection));
    if (FAILED(result) || rootReflection == nullptr ||
        FAILED(result = rootReflection->Load(rootSignatureContainer.Get()))) {
      std::wcerr << L"Loading root-signature reflection failed: 0x" << std::hex
                 << static_cast<std::uint32_t>(result) << L'\n';
      return 11;
    }

    UINT32 rootSignatureIndex = 0;
    ComPtr<IDxcBlob> rootSignaturePart;
    result = rootReflection->FindFirstPartKind(DXC_PART_ROOT_SIGNATURE,
                                               &rootSignatureIndex);
    if (FAILED(result) ||
        FAILED(result = rootReflection->GetPartContent(rootSignatureIndex,
                                                       &rootSignaturePart)) ||
        rootSignaturePart == nullptr) {
      std::wcerr << L"Extracting RTS0 part failed: 0x" << std::hex
                 << static_cast<std::uint32_t>(result) << L'\n';
      return 12;
    }

    ComPtr<IDxcContainerBuilder> builder;
    result = DxcCreateInstance(CLSID_DxcContainerBuilder,
                               IID_PPV_ARGS(&builder));
    if (FAILED(result) || builder == nullptr ||
        FAILED(result = builder->Load(output.Get()))) {
      std::wcerr << L"Loading assembled container failed: 0x" << std::hex
                 << static_cast<std::uint32_t>(result) << L'\n';
      return 13;
    }

    ComPtr<IDxcContainerReflection> outputReflection;
    result = DxcCreateInstance(CLSID_DxcContainerReflection,
                               IID_PPV_ARGS(&outputReflection));
    if (FAILED(result) || outputReflection == nullptr ||
        FAILED(result = outputReflection->Load(output.Get()))) {
      std::wcerr << L"Loading output reflection failed: 0x" << std::hex
                 << static_cast<std::uint32_t>(result) << L'\n';
      return 14;
    }
    UINT32 existingRootSignatureIndex = 0;
    if (SUCCEEDED(outputReflection->FindFirstPartKind(
            DXC_PART_ROOT_SIGNATURE, &existingRootSignatureIndex)) &&
        FAILED(result = builder->RemovePart(DXC_PART_ROOT_SIGNATURE))) {
      std::wcerr << L"Removing existing RTS0 part failed: 0x" << std::hex
                 << static_cast<std::uint32_t>(result) << L'\n';
      return 15;
    }
    result = builder->AddPart(DXC_PART_ROOT_SIGNATURE,
                              rootSignaturePart.Get());
    if (FAILED(result)) {
      std::wcerr << L"Adding RTS0 part failed: 0x" << std::hex
                 << static_cast<std::uint32_t>(result) << L'\n';
      return 16;
    }

    ComPtr<IDxcOperationResult> serializedOperation;
    result = builder->SerializeContainer(&serializedOperation);
    if (FAILED(result) || serializedOperation == nullptr) {
      std::wcerr << L"Serializing rooted container failed: 0x" << std::hex
                 << static_cast<std::uint32_t>(result) << L'\n';
      return 17;
    }
    HRESULT serializedStatus = E_FAIL;
    result = serializedOperation->GetStatus(&serializedStatus);
    PrintErrors(serializedOperation.Get());
    if (FAILED(result) || FAILED(serializedStatus) ||
        FAILED(result = serializedOperation->GetResult(&finalCandidate)) ||
        finalCandidate == nullptr) {
      std::wcerr << L"Building rooted container failed: 0x" << std::hex
                 << static_cast<std::uint32_t>(
                        FAILED(result) ? result : serializedStatus)
                 << L'\n';
      return 18;
    }
  }

  ComPtr<IDxcValidator> validator;
  result = DxcCreateInstance(CLSID_DxcValidator, IID_PPV_ARGS(&validator));
  if (FAILED(result) || validator == nullptr) {
    std::wcerr << L"DxcCreateInstance(CLSID_DxcValidator) failed: 0x"
               << std::hex << static_cast<std::uint32_t>(result) << L'\n';
    return 19;
  }

  ComPtr<IDxcOperationResult> validation;
  result = validator->Validate(finalCandidate.Get(),
                               DxcValidatorFlags_InPlaceEdit, &validation);
  if (FAILED(result) || validation == nullptr) {
    std::wcerr << L"IDxcValidator::Validate failed: 0x" << std::hex
               << static_cast<std::uint32_t>(result) << L'\n';
    return 20;
  }
  status = E_FAIL;
  result = validation->GetStatus(&status);
  PrintErrors(validation.Get());
  if (FAILED(result) || FAILED(status)) {
    std::wcerr << L"DXIL validation failed: 0x" << std::hex
               << static_cast<std::uint32_t>(FAILED(result) ? result : status)
               << L'\n';
    return 21;
  }

  ComPtr<IDxcBlob> validatedOutput;
  result = validation->GetResult(&validatedOutput);
  if (FAILED(result) || validatedOutput == nullptr ||
      !WriteBlob(argv[2], validatedOutput.Get())) {
    std::wcerr << L"Writing assembled DXIL failed: 0x" << std::hex
               << static_cast<std::uint32_t>(
                      FAILED(result) ? result : HRESULT_FROM_WIN32(GetLastError()))
               << L'\n';
    return 22;
  }

  std::wcout << L"assembled_bytes=" << output->GetBufferSize()
             << L" candidate_bytes=" << finalCandidate->GetBufferSize()
             << L" validated_bytes=" << validatedOutput->GetBufferSize()
             << L'\n';
  return 0;
}
