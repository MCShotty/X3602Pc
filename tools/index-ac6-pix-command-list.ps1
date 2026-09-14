[CmdletBinding()]
param(
    [ValidateRange(1, 100000)]
    [uint32]$CommandListId = 599,
    [string]$CommandListsSource =
        'out\pix\ac6-vpos-exposure-gameplay-export\CommandLists_000.cpp',
    [string]$PsoCorrelation =
        'out\shader-cache\ac6-pix-pso-correlation-31580\pso-shaders.tsv',
    [string]$OutputPath =
        'out\shader-cache\ac6-pix-pso-correlation-31580\command-list-599-draws.tsv'
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Resolve-RepoPath([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) {
        return [IO.Path]::GetFullPath($Path)
    }
    return [IO.Path]::GetFullPath((Join-Path $repoRoot $Path))
}

$sourcePath = (Resolve-Path -LiteralPath (Resolve-RepoPath $CommandListsSource)).Path
$psoPath = (Resolve-Path -LiteralPath (Resolve-RepoPath $PsoCorrelation)).Path
$resultPath = Resolve-RepoPath $OutputPath
[IO.Directory]::CreateDirectory((Split-Path -Parent $resultPath)) | Out-Null

$psoRows = @(Import-Csv -LiteralPath $psoPath -Delimiter "`t")
$psoByKey = @{}
foreach ($row in $psoRows) {
    $psoByKey[('{0}:{1}' -f $row.ApiObjectId, $row.Stage).ToLowerInvariant()] = $row
}

$functionPrefix = "void PopulateCommandList_${CommandListId}_"
$inside = $false
$currentPso = 0
$globalId = $null
$draws = [System.Collections.Generic.List[object]]::new()
foreach ($line in Get-Content -LiteralPath $sourcePath) {
    if ($line.StartsWith($functionPrefix, [StringComparison]::Ordinal)) {
        $inside = $true
        continue
    }
    if ($inside -and $line.StartsWith('void PopulateCommandList_', [StringComparison]::Ordinal) -and
        -not $line.StartsWith($functionPrefix, [StringComparison]::Ordinal)) {
        break
    }
    if (-not $inside) {
        continue
    }
    if ($line -match "GlobalId\s*=\s*(?<id>\d+)") {
        $globalId = [uint32]$Matches.id
        continue
    }
    if ($line -match "GetCommandList\($CommandListId\)->SetPipelineState\(GetPipelineState\((?<pso>\d+)\)\)") {
        $currentPso = [uint32]$Matches.pso
        continue
    }
    if ($line -notmatch "GetCommandList\($CommandListId\)->DrawInstanced\((?<vertices>\d+),\s*(?<instances>\d+),\s*(?<startVertex>\d+),\s*(?<startInstance>\d+)\)") {
        continue
    }

    $vertex = $psoByKey[("${currentPso}:vertex").ToLowerInvariant()]
    $pixel = $psoByKey[("${currentPso}:pixel").ToLowerInvariant()]
    $draws.Add([pscustomobject]@{
        GlobalId = if ($null -ne $globalId) { $globalId } else { '' }
        ApiObjectId = $currentPso
        VertexCount = [uint32]$Matches.vertices
        InstanceCount = [uint32]$Matches.instances
        StartVertex = [uint32]$Matches.startVertex
        StartInstance = [uint32]$Matches.startInstance
        VertexXeniaHash = if ($null -ne $vertex) { $vertex.XeniaHash } else { '' }
        PixelXeniaHash = if ($null -ne $pixel) { $pixel.XeniaHash } else { '' }
        VertexUcodeSha256 = if ($null -ne $vertex) { $vertex.UcodeSha256 } else { '' }
        PixelUcodeSha256 = if ($null -ne $pixel) { $pixel.UcodeSha256 } else { '' }
        VertexXenosRecompHlslPath = if ($null -ne $vertex) { $vertex.XenosRecompHlslPath } else { '' }
        PixelXenosRecompHlslPath = if ($null -ne $pixel) { $pixel.XenosRecompHlslPath } else { '' }
    })
    $globalId = $null
}

$draws | Export-Csv -LiteralPath $resultPath -Delimiter "`t" -NoTypeInformation
[pscustomobject]@{
    CommandListId = $CommandListId
    DrawCount = $draws.Count
    UniquePsos = @($draws.ApiObjectId | Sort-Object -Unique).Count
    OutputPath = $resultPath
}
