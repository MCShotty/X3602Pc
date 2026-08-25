param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'RelWithDebInfo',
    [ValidateRange(1, 8)]
    [int]$Parallel = 4
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$generatedRoot = Join-Path $repoRoot 'out\ac6\generated'
$stdoutLog = Join-Path $repoRoot 'out\ac6\xenonrecomp.stdout.log'
$stderrLog = Join-Path $repoRoot 'out\ac6\xenonrecomp.stderr.log'
if (-not (Test-Path -LiteralPath (Join-Path $generatedRoot 'ppc_func_mapping.cpp'))) {
    throw 'AC6 XenonRecomp output is missing. Run tools\run-xenonrecomp.ps1 first.'
}
if ((Get-Item -LiteralPath $stderrLog).Length -ne 0) {
    throw "XenonRecomp stderr is not empty: $stderrLog"
}

$diagnosticPattern =
    'Unrecognized instruction|Unable to decode|RC bit enabled|switch jump table|Unexpected .* instruction|out.of.range'
$diagnostics = @(Select-String -LiteralPath $stdoutLog -Pattern $diagnosticPattern)
if ($diagnostics.Count -ne 0) {
    throw "XenonRecomp diagnostics remain in $stdoutLog"
}
$generatedTraps = @(Get-ChildItem $generatedRoot -Filter 'ppc_recomp.*.cpp' |
    Select-String -Pattern '__builtin_debugtrap\(\)')
if ($generatedTraps.Count -ne 0) {
    throw 'Generated AC6 code still contains debug traps.'
}

. (Join-Path $PSScriptRoot 'enter-native-build-env.ps1')

$buildRoot = Join-Path $repoRoot 'build\ac6-generated'
cmake `
    -S $repoRoot `
    -B $buildRoot `
    -G Ninja `
    "-DCMAKE_BUILD_TYPE=$Configuration" `
    "-DCMAKE_CXX_COMPILER:FILEPATH=$env:XEO3_CLANG_CL" `
    "-DCMAKE_ASM_MASM_COMPILER:FILEPATH=$env:XEO3_ML64" `
    "-DAC6_GENERATED_DIR:PATH=$generatedRoot"
if ($LASTEXITCODE -ne 0) {
    throw "AC6 generated-code CMake configuration failed with exit code $LASTEXITCODE"
}

cmake --build $buildRoot --config $Configuration --target ac6_recompiled_code --parallel $Parallel
if ($LASTEXITCODE -ne 0) {
    throw "AC6 generated-code compilation failed with exit code $LASTEXITCODE"
}

$objectRoot = Join-Path $buildRoot 'CMakeFiles\ac6_recompiled_code.dir'
$objects = @(Get-ChildItem $objectRoot -Recurse -Filter '*.obj')
$expectedObjects = @(Get-ChildItem $generatedRoot -Filter '*.cpp').Count
if ($objects.Count -ne $expectedObjects) {
    throw "Expected $expectedObjects generated objects, found $($objects.Count)"
}

[pscustomobject]@{
    GeneratedSourceCount = $expectedObjects
    GeneratedSourceBytes = (Get-ChildItem $generatedRoot -File | Measure-Object Length -Sum).Sum
    ObjectCount = $objects.Count
    ObjectBytes = ($objects | Measure-Object Length -Sum).Sum
    BuildRoot = $buildRoot
} | Format-List
