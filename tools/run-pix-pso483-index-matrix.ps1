[CmdletBinding()]
param(
    [string]$ReplayPath =
        '.\out\pix\ac6-rcp-cold-gameplay-export-build2\Emu.exe',
    [string]$WorkingDirectory =
        '.\out\pix\ac6-rcp-cold-gameplay-export',
    [string]$TypedVertexShaderPath =
        '.\out\pix\pso483-indexfix\vs0046-typed.dxil',
    [string]$OutputDirectory =
        '.\out\pix\pso483-indexfix\replay',
    [ValidateRange(10, 300)]
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
function Resolve-RepoPath([string]$Path, [string]$PathType) {
    $candidate = if ([IO.Path]::IsPathRooted($Path)) {
        $Path
    } else {
        Join-Path $repoRoot $Path
    }
    if (-not (Test-Path -LiteralPath $candidate -PathType $PathType)) {
        throw "Required $PathType path is missing: $candidate"
    }
    return (Resolve-Path -LiteralPath $candidate).Path
}

$replay = Resolve-RepoPath $ReplayPath Leaf
$workingRoot = Resolve-RepoPath $WorkingDirectory Container
$typedVertexShader = Resolve-RepoPath $TypedVertexShaderPath Leaf
$outputRoot = if ([IO.Path]::IsPathRooted($OutputDirectory)) {
    [IO.Path]::GetFullPath($OutputDirectory)
} else {
    [IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDirectory))
}
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null

$oldValues = @{
    PIX_REPLAY_USE_WARP = $env:PIX_REPLAY_USE_WARP
    PIX_REPLAY_PSO483_VS = $env:PIX_REPLAY_PSO483_VS
    PIX_REPLAY_PSO483_RAW_INDEX_SRV =
        $env:PIX_REPLAY_PSO483_RAW_INDEX_SRV
    PIX_REPLAY_DUMP_MSAA_276_EVENT =
        $env:PIX_REPLAY_DUMP_MSAA_276_EVENT
    PIX_REPLAY_DUMP_MSAA_276_PATH =
        $env:PIX_REPLAY_DUMP_MSAA_276_PATH
}
$cases = @(
    [pscustomobject]@{ Name = 'baseline'; VertexShader = $null; RawSrv = $null },
    [pscustomobject]@{
        Name = 'typed-shader'
        VertexShader = $typedVertexShader
        RawSrv = $null
    },
    [pscustomobject]@{ Name = 'raw-srv'; VertexShader = $null; RawSrv = '1' }
)
$expectedBytes = 1280 * 720 * 4
$results = [Collections.Generic.List[object]]::new()

try {
    $env:PIX_REPLAY_USE_WARP = $null
    foreach ($case in $cases) {
        $rawPath = Join-Path $outputRoot ($case.Name + '.rgba')
        Remove-Item -LiteralPath $rawPath -Force -ErrorAction SilentlyContinue
        $env:PIX_REPLAY_PSO483_VS = $case.VertexShader
        $env:PIX_REPLAY_PSO483_RAW_INDEX_SRV = $case.RawSrv
        $env:PIX_REPLAY_DUMP_MSAA_276_EVENT = '853'
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
                        "Replay {0} exited with code {1}." -f
                        $case.Name,
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
                throw "Replay $($case.Name) did not emit the event 853 dump."
            }
            $results.Add([pscustomobject]@{
                Case = $case.Name
                Path = $dump.FullName
                Length = $dump.Length
                Sha256 = (Get-FileHash `
                    -Algorithm SHA256 `
                    -LiteralPath $dump.FullName).Hash
                WorkingSetMB = [Math]::Round(
                    $process.WorkingSet64 / 1MB,
                    1
                )
            })
        } finally {
            if ($process -and -not $process.HasExited) {
                Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
                Wait-Process -Id $process.Id -ErrorAction SilentlyContinue
            }
        }
    }
} finally {
    foreach ($entry in $oldValues.GetEnumerator()) {
        if ($null -eq $entry.Value) {
            Remove-Item `
                -Path "Env:$($entry.Key)" `
                -ErrorAction SilentlyContinue
        } else {
            Set-Item -Path "Env:$($entry.Key)" -Value $entry.Value
        }
    }
}

$manifest = [pscustomobject]@{
    ReplayPath = $replay
    ReplaySha256 = (Get-FileHash `
        -Algorithm SHA256 `
        -LiteralPath $replay).Hash
    TypedVertexShaderPath = $typedVertexShader
    TypedVertexShaderSha256 = (Get-FileHash `
        -Algorithm SHA256 `
        -LiteralPath $typedVertexShader).Hash
    Event = 853
    Results = $results
}
$manifestPath = Join-Path $outputRoot 'manifest.json'
$manifest | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath $manifestPath -Encoding utf8
$manifest
