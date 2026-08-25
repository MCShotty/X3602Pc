param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'RelWithDebInfo',
    [string]$XexPath = 'D:\XeO3AC6DVD\default.xex'
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
. (Join-Path $PSScriptRoot 'enter-native-build-env.ps1')

$buildRoot = Join-Path $repoRoot 'build\probe'
$generatedRoot = Join-Path $buildRoot 'generated'
$expectedXexHash = '6EEFBA42CDFE9121207E534D8D290009C98B1A8C60AE5334A33A4F15167CBBBC'

cmake `
    -S $repoRoot `
    -B $buildRoot `
    -G Ninja `
    "-DCMAKE_BUILD_TYPE=$Configuration" `
    "-DCMAKE_CXX_COMPILER:FILEPATH=$env:XEO3_CLANG_CL" `
    "-DCMAKE_ASM_MASM_COMPILER:FILEPATH=$env:XEO3_ML64" `
    "-DXEO3_ENABLE_PROBE_TESTS:BOOL=ON"

if ($LASTEXITCODE -ne 0) {
    throw "Probe CMake configuration failed with exit code $LASTEXITCODE"
}

cmake --build $buildRoot --config $Configuration --target ac6_xex_symbols
if ($LASTEXITCODE -ne 0) {
    throw "XEX symbol tool build failed with exit code $LASTEXITCODE"
}

$actualXexHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $XexPath).Hash
if ($actualXexHash -ne $expectedXexHash) {
    throw "AC6 XEX hash mismatch: expected $expectedXexHash, found $actualXexHash"
}

New-Item -ItemType Directory -Path $generatedRoot -Force | Out-Null
$symbolTool = Join-Path $buildRoot 'ac6_xex_symbols.exe'
$symbolOutput = Join-Path $generatedRoot 'ac6_imports.tsv'
& $symbolTool $XexPath $symbolOutput
if ($LASTEXITCODE -ne 0) {
    throw "XEX symbol extraction failed with exit code $LASTEXITCODE"
}

$importAddresses = Import-Csv -Delimiter "`t" -LiteralPath $symbolOutput |
    Where-Object kind -eq 'import' |
    Select-Object -ExpandProperty address
$importAddresses = $importAddresses |
    Sort-Object { [Convert]::ToUInt32($_.Substring(2), 16) } -Unique
$addresses = @('0x821F5ED0') + $importAddresses
$addresses = $addresses |
    Sort-Object { [Convert]::ToUInt32($_.Substring(2), 16) } -Unique
$imageBase = [Convert]::ToUInt32('82000000', 16)
$mappingRvas = $addresses | ForEach-Object {
    $absoluteAddress = [Convert]::ToUInt32($_.Substring(2), 16)
    if ($absoluteAddress -lt $imageBase) {
        throw "Mapping address is below the pinned image base: $_"
    }
    '0x{0:X8}' -f ($absoluteAddress - $imageBase)
}

$generatedHeader = Join-Path $generatedRoot 'ac6_probe_mappings.inc'
$builder = New-Object System.Text.StringBuilder
[void]$builder.AppendLine('#pragma once')
[void]$builder.AppendLine('#include <cstdint>')
[void]$builder.AppendLine('namespace xeo3::generated')
[void]$builder.AppendLine('{')
[void]$builder.AppendLine('inline constexpr std::uint32_t kImportGuestAddresses[] =')
[void]$builder.AppendLine('{')
foreach ($address in $importAddresses) {
    [void]$builder.AppendLine("    $($address.ToUpperInvariant())u,")
}
[void]$builder.AppendLine('};')
[void]$builder.AppendLine('inline constexpr std::uint32_t kProbeGuestAddresses[] =')
[void]$builder.AppendLine('{')
foreach ($address in $mappingRvas) {
    [void]$builder.AppendLine("    $($address.ToUpperInvariant())u,")
}
[void]$builder.AppendLine('};')
[void]$builder.AppendLine('}')
[System.IO.File]::WriteAllText(
    $generatedHeader,
    $builder.ToString(),
    (New-Object System.Text.UTF8Encoding($false))
)

cmake `
    --build $buildRoot `
    --config $Configuration `
    --target `
        xeo3_contract_tests `
        xeo3_cpu_state_tests `
        xeo3_fiber_frame_tests `
        xeo3_mmio_endian_tests `
        xeo3_native_dispatch_tests
if ($LASTEXITCODE -ne 0) {
    throw "Probe build failed with exit code $LASTEXITCODE"
}

ctest `
    --test-dir $buildRoot `
    --build-config $Configuration `
    -R '^xeo3_(contract|cpu_state|fiber_frame|mmio_endian|native_dispatch)_tests$' `
    --output-on-failure
if ($LASTEXITCODE -ne 0) {
    throw "Probe tests failed with exit code $LASTEXITCODE"
}

$dll = Join-Path $buildRoot 'xeo3_58f9e24a_5e717488_0670684a_5afca9cb_3f697be6.dll'
$pdb = [System.IO.Path]::ChangeExtension($dll, '.pdb')
foreach ($artifact in @($dll, $pdb)) {
    if (-not (Test-Path -LiteralPath $artifact)) {
        throw "Expected probe artifact is missing: $artifact"
    }
}

[pscustomobject]@{
    Dll = $dll
    DllSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $dll).Hash
    Pdb = $pdb
    MappingCount = $mappingRvas.Count
    ImportCount = $importAddresses.Count
} | Format-List
