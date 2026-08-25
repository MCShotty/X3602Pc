[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string]$SourcePath,

    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Container })]
    [string]$CommonHeaderDirectory,

    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Container })]
    [string]$PatchConstDirectory,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [string]$DxcPath = (
        'C:\Program Files (x86)\Windows Kits\10\bin\' +
        '10.0.26100.0\x64\dxc.exe'
    )
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

foreach ($path in @(
    $DxcPath,
    (Join-Path $CommonHeaderDirectory 'common_header.h'),
    (Join-Path $PatchConstDirectory 'patch_const.h')
)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required shader-build input is missing: $path"
    }
}

$source = [IO.File]::ReadAllText((Resolve-Path -LiteralPath $SourcePath).Path)
if (-not $source.Contains('OutputType xenon_vertex_shader(InputType InV)')) {
    throw "Input is not the expected generated XeO3 vertex shader: $SourcePath"
}

$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null

$fetchPattern = [regex]::new(
    '(FetchByID_[A-Z0-9_]+\([^,\r\n]+,\s*)' +
    '(gpr\d+\.[xyzw])(\s*,)',
    [Text.RegularExpressions.RegexOptions]::CultureInvariant
)
$moduloStatement = 'gpr0.y = gpr0.z * gpr0.y;'
if ($source.IndexOf($moduloStatement, [StringComparison]::Ordinal) -lt 0) {
    throw "Expected PSO 497 modulo statement is missing from '$SourcePath'."
}

$variants = @(
    [pscustomobject]@{
        Name = 'native'
        Transform = { param($text) $text }
    },
    [pscustomobject]@{
        Name = 'fetch-bias-00025'
        Transform = {
            param($text)
            $fetchPattern.Replace(
                $text,
                '${1}(${2} + 0.00025f)${3}')
        }
    },
    [pscustomobject]@{
        Name = 'fetch-bias-001'
        Transform = {
            param($text)
            $fetchPattern.Replace(
                $text,
                '${1}(${2} + 0.001f)${3}')
        }
    },
    [pscustomobject]@{
        Name = 'fetch-bias-01'
        Transform = {
            param($text)
            $fetchPattern.Replace(
                $text,
                '${1}(${2} + 0.01f)${3}')
        }
    },
    [pscustomobject]@{
        Name = 'fetch-round-nearest'
        Transform = {
            param($text)
            $fetchPattern.Replace(
                $text,
                '${1}floor(${2} + 0.5f)${3}')
        }
    },
    [pscustomobject]@{
        Name = 'modulo-round-nearest'
        Transform = {
            param($text)
            $text.Replace(
                $moduloStatement,
                $moduloStatement +
                "`r`ngpr0.y = floor(gpr0.y + 0.5f);")
        }
    },
    [pscustomobject]@{
        Name = 'modulo-and-fetch-round-nearest'
        Transform = {
            param($text)
            $transformed = $text.Replace(
                $moduloStatement,
                $moduloStatement +
                "`r`ngpr0.y = floor(gpr0.y + 0.5f);")
            $fetchPattern.Replace(
                $transformed,
                '${1}floor(${2} + 0.5f)${3}')
        }
    }
)

$manifest = [Collections.Generic.List[object]]::new()
foreach ($variant in $variants) {
    $variantSource = & $variant.Transform $source
    $sourceOutput = Join-Path $outputRoot ($variant.Name + '.hlsl')
    $dxilOutput = Join-Path $outputRoot ($variant.Name + '.dxil')
    [IO.File]::WriteAllText(
        $sourceOutput,
        $variantSource,
        [Text.UTF8Encoding]::new($false))

    & $DxcPath `
        -T vs_6_0 `
        -E xenon_vertex_shader `
        -O3 `
        -Qstrip_debug `
        -Qstrip_reflect `
        -I $CommonHeaderDirectory `
        -I $PatchConstDirectory `
        -Fo $dxilOutput `
        $sourceOutput
    if ($LASTEXITCODE -ne 0) {
        throw "DXC failed for AC6 PSO 497 variant '$($variant.Name)'."
    }

    $fetchCount = $fetchPattern.Matches($variantSource).Count
    $manifest.Add([pscustomobject]@{
        Name = $variant.Name
        SourcePath = $sourceOutput
        SourceSha256 = (
            Get-FileHash -Algorithm SHA256 -LiteralPath $sourceOutput
        ).Hash
        DxilPath = $dxilOutput
        DxilBytes = (Get-Item -LiteralPath $dxilOutput).Length
        DxilSha256 = (
            Get-FileHash -Algorithm SHA256 -LiteralPath $dxilOutput
        ).Hash
        RemainingRawGprFetches = $fetchCount
    })
}

$manifestPath = Join-Path $outputRoot 'manifest.json'
$manifest |
    ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath $manifestPath -Encoding utf8

[pscustomobject]@{
    SourcePath = (Resolve-Path -LiteralPath $SourcePath).Path
    OutputDirectory = $outputRoot
    VariantCount = $manifest.Count
    ManifestPath = $manifestPath
}
