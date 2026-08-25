[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string]$ReplayPath,

    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Container })]
    [string]$WorkingDirectory,

    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string]$PixelShaderPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [int[]]$EventId = @(1667, 2036, 2067),

    [ValidateRange(10, 300)]
    [int]$TimeoutSeconds = 150,

    [bool]$UseWarp = $true,

    [string]$Pso716CstackPath = '',

    [string]$Pso524PixelShaderPath = '',

    [string]$Pso600PixelShaderPath = '',

    [string]$Pso716PixelShaderPath = '',

    [string]$Pso735PixelShaderPath = '',

    [string]$Pso743VertexShaderPath = '',

    [string]$Transfer341Cb0Path = '',

    [string]$Transfer341Cb1Path = '',

    [string]$ExposureUploadPath = '',

    [ValidateRange(0, 8192)]
    [int]$Transfer341RowPitch = 0,

    [ValidateRange(0, 4096)]
    [int]$Transfer341Width = 0,

    [switch]$Pso537DisableDepth,

    [ValidateSet(-1, 0, 1, 2, 3)]
    [int]$TextureEndianMode = -1
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$replay = (Resolve-Path -LiteralPath $ReplayPath).Path
$workingRoot = (Resolve-Path -LiteralPath $WorkingDirectory).Path
$pixelShader = (Resolve-Path -LiteralPath $PixelShaderPath).Path
function Resolve-OptionalFile([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ''
    }
    return (Resolve-Path -LiteralPath $Path).Path
}
$pso524PixelShader = Resolve-OptionalFile $Pso524PixelShaderPath
$pso600PixelShader = Resolve-OptionalFile $Pso600PixelShaderPath
$pso716PixelShader = Resolve-OptionalFile $Pso716PixelShaderPath
$pso735PixelShader = Resolve-OptionalFile $Pso735PixelShaderPath
$pso743VertexShader = Resolve-OptionalFile $Pso743VertexShaderPath
$outputRoot = [IO.Path]::GetFullPath(
    $(if ([IO.Path]::IsPathRooted($OutputDirectory)) {
        $OutputDirectory
    } else {
        Join-Path $repoRoot $OutputDirectory
    })
)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null

$expectedBytes = 1280 * 720 * 4
$oldWarp = $env:PIX_REPLAY_USE_WARP
$oldShader = $env:PIX_REPLAY_PSO537_PS
$oldPso524Shader = $env:PIX_REPLAY_PSO524_PS
$oldPso600Shader = $env:PIX_REPLAY_PSO600_PS
$oldPso716Shader = $env:PIX_REPLAY_PSO716_PS
$oldPso735Shader = $env:PIX_REPLAY_PSO735_PS
$oldPso743Shader = $env:PIX_REPLAY_PSO743_VS
$oldTextureEndianMode = $env:PIX_REPLAY_TEXTURE_ENDIAN_MODE
$oldTransfer341Cb0Path = $env:PIX_REPLAY_DUMP_TRANSFER341_CB0
$oldTransfer341Cb1Path = $env:PIX_REPLAY_DUMP_TRANSFER341_CB1
$oldExposureUploadPath = $env:PIX_REPLAY_DUMP_EXPOSURE_UPLOAD
$oldTransfer341RowPitch = $env:PIX_REPLAY_TRANSFER341_ROW_PITCH
$oldTransfer341Width = $env:PIX_REPLAY_TRANSFER341_WIDTH
$oldPso537DisableDepth = $env:PIX_REPLAY_PSO537_DISABLE_DEPTH
$oldEvent = $env:PIX_REPLAY_DUMP_MSAA_276_EVENT
$oldPath = $env:PIX_REPLAY_DUMP_MSAA_276_PATH
$oldPso716CstackPath = $env:PIX_REPLAY_DUMP_PSO716_CSTACK
$results = [Collections.Generic.List[object]]::new()

try {
    $env:PIX_REPLAY_USE_WARP = if ($UseWarp) { '1' } else { $null }
    $env:PIX_REPLAY_PSO537_PS = $pixelShader
    $env:PIX_REPLAY_PSO524_PS =
        if ($pso524PixelShader) { $pso524PixelShader } else { $null }
    $env:PIX_REPLAY_PSO600_PS =
        if ($pso600PixelShader) { $pso600PixelShader } else { $null }
    $env:PIX_REPLAY_PSO716_PS =
        if ($pso716PixelShader) { $pso716PixelShader } else { $null }
    $env:PIX_REPLAY_PSO735_PS =
        if ($pso735PixelShader) { $pso735PixelShader } else { $null }
    $env:PIX_REPLAY_PSO743_VS =
        if ($pso743VertexShader) { $pso743VertexShader } else { $null }
    $env:PIX_REPLAY_TEXTURE_ENDIAN_MODE =
        if ($TextureEndianMode -ge 0) {
            [string]$TextureEndianMode
        } else {
            $null
        }
    $env:PIX_REPLAY_DUMP_TRANSFER341_CB0 =
        if ([string]::IsNullOrWhiteSpace($Transfer341Cb0Path)) {
            $null
        } else {
            [IO.Path]::GetFullPath($Transfer341Cb0Path)
        }
    $env:PIX_REPLAY_DUMP_TRANSFER341_CB1 =
        if ([string]::IsNullOrWhiteSpace($Transfer341Cb1Path)) {
            $null
        } else {
            [IO.Path]::GetFullPath($Transfer341Cb1Path)
        }
    $env:PIX_REPLAY_DUMP_EXPOSURE_UPLOAD =
        if ([string]::IsNullOrWhiteSpace($ExposureUploadPath)) {
            $null
        } else {
            [IO.Path]::GetFullPath($ExposureUploadPath)
        }
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
    $env:PIX_REPLAY_PSO537_DISABLE_DEPTH =
        if ($Pso537DisableDepth) { '1' } else { $null }
    $env:PIX_REPLAY_DUMP_PSO716_CSTACK =
        if ([string]::IsNullOrWhiteSpace($Pso716CstackPath)) {
            $null
        } else {
            [IO.Path]::GetFullPath($Pso716CstackPath)
        }

    foreach ($event in $EventId) {
        $rawPath = Join-Path $outputRoot (
            'msaa276-event-{0}.rgba' -f $event)
        Remove-Item -LiteralPath $rawPath -Force -ErrorAction SilentlyContinue
        $env:PIX_REPLAY_DUMP_MSAA_276_EVENT = [string]$event
        $env:PIX_REPLAY_DUMP_MSAA_276_PATH = $rawPath

        $process = $null
        try {
            $process = Start-Process `
                -FilePath $replay `
                -WorkingDirectory $workingRoot `
                -WindowStyle Hidden `
                -PassThru
            $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
            do {
                Start-Sleep -Milliseconds 500
                $process.Refresh()
                if ($process.HasExited) {
                    throw (
                        "Replay exited at event {0} with code {1}." -f
                        $event,
                        $process.ExitCode
                    )
                }
                $dump = Get-Item `
                    -LiteralPath $rawPath `
                    -ErrorAction SilentlyContinue
            } while (
                (-not $dump -or $dump.Length -ne $expectedBytes) -and
                [DateTime]::UtcNow -lt $deadline
            )

            if (-not $dump -or $dump.Length -ne $expectedBytes) {
                throw (
                    "Replay did not write the {0}-byte event {1} dump " +
                    "within {2} seconds." -f
                    $expectedBytes,
                    $event,
                    $TimeoutSeconds
                )
            }

            $results.Add([pscustomobject]@{
                Event = $event
                UseWarp = $UseWarp
                Path = $dump.FullName
                Sha256 = (
                    Get-FileHash `
                        -LiteralPath $dump.FullName `
                        -Algorithm SHA256
                ).Hash
                WorkingSetMB = [Math]::Round(
                    $process.WorkingSet64 / 1MB,
                    1)
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
    $env:PIX_REPLAY_USE_WARP = $oldWarp
    $env:PIX_REPLAY_PSO537_PS = $oldShader
    $env:PIX_REPLAY_PSO524_PS = $oldPso524Shader
    $env:PIX_REPLAY_PSO600_PS = $oldPso600Shader
    $env:PIX_REPLAY_PSO716_PS = $oldPso716Shader
    $env:PIX_REPLAY_PSO735_PS = $oldPso735Shader
    $env:PIX_REPLAY_PSO743_VS = $oldPso743Shader
    $env:PIX_REPLAY_TEXTURE_ENDIAN_MODE = $oldTextureEndianMode
    $env:PIX_REPLAY_DUMP_TRANSFER341_CB0 = $oldTransfer341Cb0Path
    $env:PIX_REPLAY_DUMP_TRANSFER341_CB1 = $oldTransfer341Cb1Path
    $env:PIX_REPLAY_DUMP_EXPOSURE_UPLOAD = $oldExposureUploadPath
    $env:PIX_REPLAY_TRANSFER341_ROW_PITCH = $oldTransfer341RowPitch
    $env:PIX_REPLAY_TRANSFER341_WIDTH = $oldTransfer341Width
    $env:PIX_REPLAY_PSO537_DISABLE_DEPTH = $oldPso537DisableDepth
    $env:PIX_REPLAY_DUMP_MSAA_276_EVENT = $oldEvent
    $env:PIX_REPLAY_DUMP_MSAA_276_PATH = $oldPath
    $env:PIX_REPLAY_DUMP_PSO716_CSTACK = $oldPso716CstackPath
}

$manifest = [pscustomobject]@{
    ReplayPath = $replay
    ReplaySha256 = (
        Get-FileHash -LiteralPath $replay -Algorithm SHA256
    ).Hash
    PixelShaderPath = $pixelShader
    PixelShaderSha256 = (
        Get-FileHash -LiteralPath $pixelShader -Algorithm SHA256
    ).Hash
    Pso524PixelShaderPath = $pso524PixelShader
    Pso600PixelShaderPath = $pso600PixelShader
    Pso716PixelShaderPath = $pso716PixelShader
    Pso735PixelShaderPath = $pso735PixelShader
    Pso743VertexShaderPath = $pso743VertexShader
    TextureEndianMode = $TextureEndianMode
    Transfer341RowPitch = $Transfer341RowPitch
    Transfer341Width = $Transfer341Width
    Pso537DisableDepth = [bool]$Pso537DisableDepth
    Width = 1280
    Height = 720
    Results = $results
}
$manifestPath = Join-Path $outputRoot 'manifest.json'
$manifest |
    ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath $manifestPath -Encoding utf8
$manifest
