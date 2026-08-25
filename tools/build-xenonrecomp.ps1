param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'RelWithDebInfo',
    [switch]$Pristine
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
. (Join-Path $PSScriptRoot 'enter-native-build-env.ps1')

$pristineSourceRoot = Join-Path $repoRoot 'third_party\XenonRecomp'
$expectedRevision = 'ddd128bcca99fe8bfbb99bea583c972351fa6ace'
$actualRevision = (git -C $pristineSourceRoot rev-parse HEAD).Trim()
if ($actualRevision -ne $expectedRevision) {
    throw "XenonRecomp revision mismatch: expected $expectedRevision, found $actualRevision"
}

$prepared = if ($Pristine) {
    $null
} else {
    & (Join-Path $PSScriptRoot 'prepare-xenonrecomp.ps1')
}
$sourceRoot = if ($Pristine) {
    $pristineSourceRoot
} else {
    $prepared.Destination
}
$buildRoot = if ($Pristine) {
    Join-Path $repoRoot 'build\xenonrecomp'
} else {
    Join-Path $repoRoot "build\xenonrecomp-patched-$($prepared.PatchSeries.Substring(0, 12))"
}

if (-not (Test-Path -LiteralPath $sourceRoot)) {
    throw "XenonRecomp source directory is missing: $sourceRoot"
}

cmake `
    -S $sourceRoot `
    -B $buildRoot `
    -G Ninja `
    "-DCMAKE_BUILD_TYPE=$Configuration" `
    "-DCMAKE_C_COMPILER:FILEPATH=$env:XEO3_CLANG_CL" `
    "-DCMAKE_CXX_COMPILER:FILEPATH=$env:XEO3_CLANG_CL" `
    '-DFMT_DOC=OFF' `
    '-DFMT_TEST=OFF' `
    '-DXXHASH_BUILD_XXHSUM=OFF'

if ($LASTEXITCODE -ne 0) {
    throw "XenonRecomp CMake configuration failed with exit code $LASTEXITCODE"
}

cmake --build $buildRoot --config $Configuration
if ($LASTEXITCODE -ne 0) {
    throw "XenonRecomp build failed with exit code $LASTEXITCODE"
}

ctest --test-dir $buildRoot --build-config $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) {
    throw "XenonRecomp tests failed with exit code $LASTEXITCODE"
}

$analyse = Join-Path $buildRoot 'XenonAnalyse\XenonAnalyse.exe'
$recomp = Join-Path $buildRoot 'XenonRecomp\XenonRecomp.exe'
foreach ($tool in @($analyse, $recomp)) {
    if (-not (Test-Path -LiteralPath $tool)) {
        throw "Expected build output is missing: $tool"
    }
}

[pscustomobject]@{
    XenonRecompRevision = $actualRevision
    SourceKind = if ($Pristine) { 'pristine' } else { 'patched' }
    SourceRoot = $sourceRoot
    Configuration = $Configuration
    XenonAnalyse = $analyse
    XenonRecomp = $recomp
    ClangCl = $env:XEO3_CLANG_CL
    Ninja = $env:XEO3_NINJA
    Ml64 = $env:XEO3_ML64
} | Format-List
