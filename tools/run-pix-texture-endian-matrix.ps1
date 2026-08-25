[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string]$ReplayPath,

    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Container })]
    [string]$WorkingDirectory,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [ValidateSet(0, 1, 2, 3)]
    [int[]]$Mode = @(0, 1, 2, 3),

    [ValidateRange(1, [int]::MaxValue)]
    [int]$TextureEvent = 1665,

    [ValidateRange(1, 16384)]
    [int]$Width = 1280,

    [ValidateRange(1, 16384)]
    [int]$Height = 720,

    [ValidateRange(10, 300)]
    [int]$TimeoutSeconds = 120,

    [bool]$UseWarp = $true,

    [string]$Pso537PixelShaderPath = '',

    [string]$Pso475ComputeShaderPath = '',

    [string]$Pso341ComputeShaderPath = '',

    [ValidateRange(0, 8192)]
    [int]$Transfer341RowPitch = 0,

    [ValidateRange(0, 4096)]
    [int]$Transfer341Width = 0
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$replay = (Resolve-Path -LiteralPath $ReplayPath).Path
$workingRoot = (Resolve-Path -LiteralPath $WorkingDirectory).Path
$pso537PixelShader =
    if ([string]::IsNullOrWhiteSpace($Pso537PixelShaderPath)) {
        ''
    } else {
        (Resolve-Path -LiteralPath $Pso537PixelShaderPath).Path
    }
$pso475ComputeShader =
    if ([string]::IsNullOrWhiteSpace($Pso475ComputeShaderPath)) {
        ''
    } else {
        (Resolve-Path -LiteralPath $Pso475ComputeShaderPath).Path
    }
$pso341ComputeShader =
    if ([string]::IsNullOrWhiteSpace($Pso341ComputeShaderPath)) {
        ''
    } else {
        (Resolve-Path -LiteralPath $Pso341ComputeShaderPath).Path
    }
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

$expectedTextureBytes = $Width * $Height * 4
$oldEndianMode = $env:PIX_REPLAY_TEXTURE_ENDIAN_MODE
$oldWarp = $env:PIX_REPLAY_USE_WARP
$oldDumpEvent = $env:PIX_REPLAY_DUMP_TEXTURE_EVENT
$oldDumpPath = $env:PIX_REPLAY_DUMP_TEXTURE_PATH
$oldPso537PixelShader = $env:PIX_REPLAY_PSO537_PS
$oldPso475ComputeShader = $env:PIX_REPLAY_PSO475_CS
$oldPso341ComputeShader = $env:PIX_REPLAY_PSO341_CS
$oldTransfer341RowPitch = $env:PIX_REPLAY_TRANSFER341_ROW_PITCH
$oldTransfer341Width = $env:PIX_REPLAY_TRANSFER341_WIDTH
$results = [Collections.Generic.List[object]]::new()
try {
    $env:PIX_REPLAY_USE_WARP = if ($UseWarp) { '1' } else { $null }
    $env:PIX_REPLAY_DUMP_TEXTURE_EVENT = [string]$TextureEvent
    $env:PIX_REPLAY_PSO537_PS =
        if ($pso537PixelShader) { $pso537PixelShader } else { $null }
    $env:PIX_REPLAY_PSO475_CS =
        if ($pso475ComputeShader) { $pso475ComputeShader } else { $null }
    $env:PIX_REPLAY_PSO341_CS =
        if ($pso341ComputeShader) { $pso341ComputeShader } else { $null }
    $env:PIX_REPLAY_TRANSFER341_ROW_PITCH =
        if ($Transfer341RowPitch -gt 0) {
            [string]$Transfer341RowPitch
        } else {
            $null
        }
    $env:PIX_REPLAY_TRANSFER341_WIDTH =
        if ($Transfer341Width -gt 0) {
            [string]$Transfer341Width
        } else {
            $null
        }

    foreach ($endianMode in $Mode) {
        $rawPath = Join-Path $outputRoot (
            'texture-event{0}-endian-{1}.rgba' -f
            $TextureEvent,
            $endianMode)
        Remove-Item -LiteralPath $rawPath -Force -ErrorAction SilentlyContinue

        $env:PIX_REPLAY_TEXTURE_ENDIAN_MODE = [string]$endianMode
        $env:PIX_REPLAY_DUMP_TEXTURE_PATH = $rawPath
        $process = $null
        try {
            $process = Start-Process `
                -FilePath $replay `
                -WorkingDirectory $workingRoot `
                -WindowStyle Hidden `
                -PassThru

            $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
            while ([DateTime]::UtcNow -lt $deadline) {
                Start-Sleep -Milliseconds 500
                $process.Refresh()
                if ($process.HasExited) {
                    throw (
                        "Replay exited for endian mode {0} with code {1}." -f
                        $endianMode,
                        $process.ExitCode
                    )
                }

                $texture = Get-Item `
                    -LiteralPath $rawPath `
                    -ErrorAction SilentlyContinue
                if ($texture -and $texture.Length -eq $expectedTextureBytes) {
                    break
                }
            }

            $texture = Get-Item `
                -LiteralPath $rawPath `
                -ErrorAction SilentlyContinue
            if (-not $texture -or
                $texture.Length -ne $expectedTextureBytes) {
                throw (
                    "Replay did not write the {0}-byte texture for endian " +
                    "mode {1} within {2} seconds." -f
                    $expectedTextureBytes,
                    $endianMode,
                    $TimeoutSeconds
                )
            }

            $process.Refresh()
            $results.Add([pscustomobject]@{
                Mode = $endianMode
                UseWarp = $UseWarp
                ProcessId = $process.Id
                WorkingSetMB = [Math]::Round(
                    $process.WorkingSet64 / 1MB,
                    1)
                TexturePath = $texture.FullName
                TextureSha256 = (
                    Get-FileHash `
                        -Algorithm SHA256 `
                        -LiteralPath $texture.FullName
                ).Hash
            })
        } finally {
            if ($process -and -not $process.HasExited) {
                Stop-Process `
                    -Id $process.Id `
                    -Force `
                    -ErrorAction SilentlyContinue
                Wait-Process `
                    -Id $process.Id `
                    -ErrorAction SilentlyContinue
            }
        }
    }
} finally {
    $env:PIX_REPLAY_TEXTURE_ENDIAN_MODE = $oldEndianMode
    $env:PIX_REPLAY_USE_WARP = $oldWarp
    $env:PIX_REPLAY_DUMP_TEXTURE_EVENT = $oldDumpEvent
    $env:PIX_REPLAY_DUMP_TEXTURE_PATH = $oldDumpPath
    $env:PIX_REPLAY_PSO537_PS = $oldPso537PixelShader
    $env:PIX_REPLAY_PSO475_CS = $oldPso475ComputeShader
    $env:PIX_REPLAY_PSO341_CS = $oldPso341ComputeShader
    $env:PIX_REPLAY_TRANSFER341_ROW_PITCH = $oldTransfer341RowPitch
    $env:PIX_REPLAY_TRANSFER341_WIDTH = $oldTransfer341Width
}

$manifest = [pscustomobject]@{
    ReplayPath = $replay
    ReplaySha256 = (
        Get-FileHash -Algorithm SHA256 -LiteralPath $replay
    ).Hash
    TextureEvent = $TextureEvent
    Width = $Width
    Height = $Height
    Pso537PixelShaderPath = $pso537PixelShader
    Pso537PixelShaderSha256 =
        if ($pso537PixelShader) {
            (Get-FileHash `
                -Algorithm SHA256 `
                -LiteralPath $pso537PixelShader).Hash
        } else {
            ''
        }
    Pso475ComputeShaderPath = $pso475ComputeShader
    Pso475ComputeShaderSha256 =
        if ($pso475ComputeShader) {
            (Get-FileHash `
                -Algorithm SHA256 `
                -LiteralPath $pso475ComputeShader).Hash
        } else {
            ''
        }
    Pso341ComputeShaderPath = $pso341ComputeShader
    Pso341ComputeShaderSha256 =
        if ($pso341ComputeShader) {
            (Get-FileHash `
                -Algorithm SHA256 `
                -LiteralPath $pso341ComputeShader).Hash
        } else {
            ''
        }
    Transfer341RowPitch = $Transfer341RowPitch
    Transfer341Width = $Transfer341Width
    Results = $results
}
$manifestPath = Join-Path $outputRoot 'manifest.json'
$manifest |
    ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath $manifestPath -Encoding utf8
$manifest
