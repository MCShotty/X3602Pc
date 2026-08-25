param(
    [string]$Destination
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$sourceRoot = Join-Path $repoRoot 'third_party\XenonRecomp'
$expectedRevision = 'ddd128bcca99fe8bfbb99bea583c972351fa6ace'
$actualRevision = (git -C $sourceRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $actualRevision -ne $expectedRevision) {
    throw "XenonRecomp revision mismatch: expected $expectedRevision, found $actualRevision"
}

$sourceChanges = @(git -C $sourceRoot status --porcelain --untracked-files=no)
if ($LASTEXITCODE -ne 0 -or $sourceChanges.Count -ne 0) {
    throw 'The pinned XenonRecomp submodule is not pristine.'
}

$patches = @(Get-ChildItem (Join-Path $repoRoot 'patches\xenonrecomp') -Filter '*.patch' | Sort-Object Name)
if ($patches.Count -eq 0) {
    throw 'No XenonRecomp patches were found.'
}

$patchManifest = @(
    foreach ($patch in $patches) {
        [ordered]@{
            name = $patch.Name
            sha256 = (Get-FileHash -LiteralPath $patch.FullName -Algorithm SHA256).Hash
        }
    }
)
$seriesMaterial = ($patchManifest | ForEach-Object { "$($_.name):$($_.sha256)" }) -join "`n"
$sha256 = [Security.Cryptography.SHA256]::Create()
try {
    $seriesBytes = [Text.Encoding]::UTF8.GetBytes($seriesMaterial)
    $patchSeries = -join ($sha256.ComputeHash($seriesBytes) | ForEach-Object { $_.ToString('x2') })
} finally {
    $sha256.Dispose()
}

$preparationVersion = 2
$defaultLeaf = "XenonRecomp-patched-v$preparationVersion-$($expectedRevision.Substring(0, 8))-$($patchSeries.Substring(0, 12))"
$destinationRoot = if ($Destination) {
    if ([IO.Path]::IsPathRooted($Destination)) {
        [IO.Path]::GetFullPath($Destination)
    } else {
        [IO.Path]::GetFullPath((Join-Path $repoRoot $Destination))
    }
} else {
    Join-Path $repoRoot "build\src\$defaultLeaf"
}

$allowedParent = [IO.Path]::GetFullPath((Join-Path $repoRoot 'build\src'))
$allowedPrefix = $allowedParent.TrimEnd('\') + '\'
if (-not $destinationRoot.StartsWith($allowedPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Disposable source destination must be under $allowedParent"
}
if (-not [IO.Path]::GetFileName($destinationRoot).StartsWith(
        'XenonRecomp-patched-',
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "Unexpected disposable source directory name: $destinationRoot"
}

if (Test-Path -LiteralPath $destinationRoot) {
    $storedRevision = (git -C $destinationRoot config --get xeo3.sourceRevision).Trim()
    $storedPatchSeries = (git -C $destinationRoot config --get xeo3.patchSeries).Trim()
    $destinationChanges = @(git -C $destinationRoot status --porcelain)
    if ($LASTEXITCODE -ne 0 -or
        $storedRevision -ne $actualRevision -or
        $storedPatchSeries -ne $patchSeries -or
        $destinationChanges.Count -ne 0) {
        throw "Existing content-addressed source is not reusable: $destinationRoot"
    }

    return [pscustomobject]@{
        SourceRevision = $actualRevision
        PatchSeries = $patchSeries
        Destination = $destinationRoot
        Reused = $true
        Patches = $patchManifest
    }
}

New-Item -ItemType Directory -Path $destinationRoot -Force | Out-Null
& robocopy.exe `
    $sourceRoot `
    $destinationRoot `
    /E `
    /COPY:DAT `
    /DCOPY:DAT `
    /R:1 `
    /W:1 `
    /NFL `
    /NDL `
    /NJH `
    /NJS `
    /NP `
    /XD .git `
    /XF .git | Out-Null
$robocopyExitCode = $LASTEXITCODE
if ($robocopyExitCode -gt 7) {
    throw "Copying the disposable XenonRecomp source failed with robocopy exit code $robocopyExitCode"
}

git -C $destinationRoot init --quiet
if ($LASTEXITCODE -ne 0) {
    throw 'Failed to initialize the disposable XenonRecomp repository.'
}
git -C $destinationRoot config core.autocrlf true
git -C $destinationRoot config user.name 'XeO3 AC6 Build'
git -C $destinationRoot config user.email 'local-build@invalid'

foreach ($patch in $patches) {
    git -C $destinationRoot apply --whitespace=nowarn -- $patch.FullName
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to apply $($patch.Name)"
    }
}

git -C $destinationRoot add --all
if ($LASTEXITCODE -ne 0) {
    throw 'Failed to stage the prepared XenonRecomp snapshot.'
}
git -C $destinationRoot commit --quiet -m "Pinned XenonRecomp $actualRevision with XeO3 AC6 patches"
if ($LASTEXITCODE -ne 0) {
    throw 'Failed to commit the prepared XenonRecomp snapshot.'
}
git -C $destinationRoot config xeo3.sourceRevision $actualRevision
git -C $destinationRoot config xeo3.patchSeries $patchSeries

[pscustomobject]@{
    SourceRevision = $actualRevision
    PatchSeries = $patchSeries
    Destination = $destinationRoot
    Reused = $false
    Patches = $patchManifest
}
