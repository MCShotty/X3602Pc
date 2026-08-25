[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string]$ReplayPath,

    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Container })]
    [string]$WorkingDirectory,

    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Container })]
    [string]$VariantDirectory,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [ValidateSet(
        'PIX_REPLAY_PSO497_VS',
        'AC6_PSO620_PS_OVERRIDE',
        'AC6_PSO625_PS_OVERRIDE',
        'AC6_PSO848_PS_OVERRIDE'
    )]
    [string]$OverrideEnvironmentVariable = 'PIX_REPLAY_PSO497_VS',

    [string[]]$VariantName = @(),

    [ValidateRange(1, 30)]
    [int]$RenderDelaySeconds = 2,

    [ValidateRange(512, 16384)]
    [int]$MinimumWorkingSetMB = 3000,

    [ValidateRange(10, 120)]
    [int]$ReadinessTimeoutSeconds = 60
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$captureScript = Join-Path $PSScriptRoot 'capture-process-window.ps1'
$replay = (Resolve-Path -LiteralPath $ReplayPath).Path
$workingRoot = (Resolve-Path -LiteralPath $WorkingDirectory).Path
$variantRoot = (Resolve-Path -LiteralPath $VariantDirectory).Path
$outputRoot = [IO.Path]::GetFullPath(
    $(if ([IO.Path]::IsPathRooted($OutputDirectory)) {
        $OutputDirectory
    } else {
        Join-Path $repoRoot $OutputDirectory
    })
)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null

$processName = [IO.Path]::GetFileNameWithoutExtension($replay)
if (Get-Process -Name $processName -ErrorAction SilentlyContinue) {
    throw "A '$processName' process is already running."
}

$variants = @(
    Get-ChildItem -LiteralPath $variantRoot -Filter '*.dxil' -File |
        Where-Object {
            $VariantName.Count -eq 0 -or
            $VariantName -contains $_.BaseName
        } |
        Sort-Object Name
)
if ($variants.Count -eq 0) {
    throw "No requested DXIL variants were found in '$variantRoot'."
}

$oldWarp = $env:PIX_REPLAY_USE_WARP
$oldOverride = [Environment]::GetEnvironmentVariable(
    $OverrideEnvironmentVariable,
    'Process')
$results = [Collections.Generic.List[object]]::new()
try {
    $env:PIX_REPLAY_USE_WARP = '1'
    foreach ($variant in $variants) {
        [Environment]::SetEnvironmentVariable(
            $OverrideEnvironmentVariable,
            $variant.FullName,
            'Process')
        $process = $null
        $imagePath = Join-Path $outputRoot ($variant.BaseName + '.png')
        try {
            $process = Start-Process `
                -FilePath $replay `
                -WorkingDirectory $workingRoot `
                -PassThru

            $windowFound = $false
            for ($attempt = 0; $attempt -lt 60; ++$attempt) {
                Start-Sleep -Milliseconds 250
                $process.Refresh()
                if ($process.HasExited) {
                    throw (
                        "Replay exited before rendering variant '{0}' " +
                        "with code {1}." -f
                        $variant.BaseName,
                        $process.ExitCode
                    )
                }
                if ($process.MainWindowHandle -ne 0) {
                    $windowFound = $true
                    break
                }
            }
            if (-not $windowFound) {
                throw "Replay window did not appear for '$($variant.BaseName)'."
            }

            $readyDeadline = [DateTime]::UtcNow.AddSeconds(
                $ReadinessTimeoutSeconds)
            do {
                Start-Sleep -Milliseconds 500
                $process.Refresh()
                if ($process.HasExited) {
                    throw (
                        "Replay exited while loading variant '{0}' " +
                        "with code {1}." -f
                        $variant.BaseName,
                        $process.ExitCode
                    )
                }
            } while (
                $process.WorkingSet64 -lt ($MinimumWorkingSetMB * 1MB) -and
                [DateTime]::UtcNow -lt $readyDeadline
            )
            if ($process.WorkingSet64 -lt ($MinimumWorkingSetMB * 1MB)) {
                throw (
                    "Replay did not reach the {0} MB readiness threshold " +
                    "for '{1}'." -f
                    $MinimumWorkingSetMB,
                    $variant.BaseName
                )
            }

            Start-Sleep -Seconds $RenderDelaySeconds
            & powershell.exe `
                -NoProfile `
                -ExecutionPolicy Bypass `
                -File $captureScript `
                -ProcessName $processName `
                -OutputPath $imagePath
            if ($LASTEXITCODE -ne 0) {
                throw "Window capture failed for '$($variant.BaseName)'."
            }
            $results.Add([pscustomobject]@{
                Variant = $variant.BaseName
                OverrideEnvironmentVariable = $OverrideEnvironmentVariable
                DxilSha256 = (
                    Get-FileHash -Algorithm SHA256 -LiteralPath $variant.FullName
                ).Hash
                ReadyWorkingSetMB = [Math]::Round(
                    $process.WorkingSet64 / 1MB,
                    1)
                Screenshot = $imagePath
                ScreenshotSha256 = (
                    Get-FileHash -Algorithm SHA256 -LiteralPath $imagePath
                ).Hash
            })
        } finally {
            if ($process -and -not $process.HasExited) {
                Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
                Wait-Process -Id $process.Id -ErrorAction SilentlyContinue
            }
        }
    }
} finally {
    $env:PIX_REPLAY_USE_WARP = $oldWarp
    [Environment]::SetEnvironmentVariable(
        $OverrideEnvironmentVariable,
        $oldOverride,
        'Process')
}

$manifestPath = Join-Path $outputRoot 'manifest.json'
$results |
    ConvertTo-Json -Depth 3 |
    Set-Content -LiteralPath $manifestPath -Encoding utf8
$results
