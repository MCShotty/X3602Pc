param(
    [string]$GeneratedRoot
)

$ErrorActionPreference = 'Stop'

if (-not $GeneratedRoot) {
    $repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
    $GeneratedRoot = Join-Path $repoRoot 'out\ac6\generated'
}

$functionMatch = Get-ChildItem -LiteralPath $GeneratedRoot -Filter 'ppc_recomp.*.cpp' |
    Select-String -SimpleMatch 'PPC_FUNC_IMPL(__imp__sub_822BF3F8)' |
    Select-Object -First 1
if (-not $functionMatch) {
    throw 'Generated AC6 code is missing sub_822BF3F8.'
}

$sourcePath = $functionMatch.Path
$sourceText = [IO.File]::ReadAllText($sourcePath)

$earlyExitGuard = (
    "`t// AC6 XeO3: tolerate concurrent teardown of the referenced object.`n" +
    "`tif (ctx.r11.u32 == 0) goto loc_822BF5F0;`n"
)
$removedGuardCount = [regex]::Matches(
    $sourceText,
    [regex]::Escape($earlyExitGuard)
).Count
if ($removedGuardCount -ne 0) {
    $sourceText = $sourceText.Replace($earlyExitGuard, '')
}
if ($removedGuardCount -gt 3) {
    throw "Generated AC6 source contained $removedGuardCount unexpected early-exit guards."
}

$loadSites = @(
    [pscustomobject]@{
        Iar = '0x822BF488'
        Original = 'ctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 28);'
        Replacement = 'ctx.r11.u64 = ctx.r11.u32 == 0 ? 0 : PPC_LOAD_U32(ctx.r11.u32 + 28);'
    },
    [pscustomobject]@{
        Iar = '0x822BF4CC'
        Original = 'ctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 112);'
        Replacement = 'ctx.r11.u64 = ctx.r11.u32 == 0 ? 0 : PPC_LOAD_U32(ctx.r11.u32 + 112);'
    },
    [pscustomobject]@{
        Iar = '0x822BF54C'
        Original = 'temp.u32 = PPC_LOAD_U32(ctx.r11.u32 + 180);'
        Replacement = 'temp.u32 = ctx.r11.u32 == 0 ? 0 : PPC_LOAD_U32(ctx.r11.u32 + 180);'
    },
    [pscustomobject]@{
        Iar = '0x822BF550'
        Original = 'temp.u32 = PPC_LOAD_U32(ctx.r11.u32 + 196);'
        Replacement = 'temp.u32 = ctx.r11.u32 == 0 ? 0 : PPC_LOAD_U32(ctx.r11.u32 + 196);'
    }
)

$patchedCount = 0
$presentCount = 0
foreach ($site in $loadSites) {
    $oldText = "`tPPC_SET_GUEST_IAR($($site.Iar));`n`t$($site.Original)"
    $newText = "`tPPC_SET_GUEST_IAR($($site.Iar));`n`t$($site.Replacement)"

    if ($sourceText.IndexOf($newText, [StringComparison]::Ordinal) -ge 0) {
        ++$presentCount
        continue
    }
    if ($sourceText.IndexOf($oldText, [StringComparison]::Ordinal) -lt 0) {
        throw "Generated AC6 null-safe load site $($site.Iar) did not match the pinned source."
    }
    if ($sourceText.IndexOf(
            $oldText,
            $sourceText.IndexOf($oldText, [StringComparison]::Ordinal) + 1,
            [StringComparison]::Ordinal
        ) -ge 0) {
        throw "Generated AC6 null-safe load site $($site.Iar) is not unique."
    }

    $sourceText = $sourceText.Replace($oldText, $newText)
    ++$patchedCount
}

$lateUpdateGuard = (
    "`tPPC_SET_GUEST_IAR(0x822BF8FC);`n" +
    "`tif (ctx.r11.u32 == 0) goto loc_822BF92C;`n" +
    "`tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 160);"
)
$lateUpdateOriginal = (
    "`tPPC_SET_GUEST_IAR(0x822BF8FC);`n" +
    "`tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 160);"
)
$lateUpdateEpilogue = (
    "`tPPC_SET_GUEST_IAR(0x822BF928);`n" +
    "`tPPC_STORE_U32(ctx.r31.u32 + 208, ctx.r11.u32);`n" +
    "loc_822BF92C:`n" +
    "`t// addi r1,r1,128"
)
$lateUpdateEpilogueOriginal = (
    "`tPPC_SET_GUEST_IAR(0x822BF928);`n" +
    "`tPPC_STORE_U32(ctx.r31.u32 + 208, ctx.r11.u32);`n" +
    "`t// addi r1,r1,128"
)

$lateUpdateGuardPatched = 0
$lateUpdateGuardPresent = 0
if ($sourceText.IndexOf($lateUpdateGuard, [StringComparison]::Ordinal) -ge 0) {
    if ($sourceText.IndexOf(
            $lateUpdateEpilogue,
            [StringComparison]::Ordinal
        ) -lt 0) {
        throw 'Generated AC6 late-update guard is missing its epilogue label.'
    }
    $lateUpdateGuardPresent = 1
} else {
    foreach ($match in @(
        [pscustomobject]@{
            Original = $lateUpdateOriginal
            Replacement = $lateUpdateGuard
            Description = 'null lifetime check'
        },
        [pscustomobject]@{
            Original = $lateUpdateEpilogueOriginal
            Replacement = $lateUpdateEpilogue
            Description = 'shared epilogue label'
        }
    )) {
        if ($sourceText.IndexOf(
                $match.Original,
                [StringComparison]::Ordinal
            ) -lt 0) {
            throw "Generated AC6 late-update $($match.Description) did not match the pinned source."
        }
        $sourceText = $sourceText.Replace(
            $match.Original,
            $match.Replacement)
    }
    $lateUpdateGuardPatched = 1
}

if (
    $removedGuardCount -ne 0 -or
    $patchedCount -ne 0 -or
    $lateUpdateGuardPatched -ne 0
) {
    [IO.File]::WriteAllText(
        $sourcePath,
        $sourceText,
        [Text.UTF8Encoding]::new($false))
}

[pscustomobject]@{
    Source = $sourcePath
    RemovedEarlyExitGuards = $removedGuardCount
    PatchedNullSafeLoads = $patchedCount
    ExistingNullSafeLoads = $presentCount
    PatchedLateUpdateGuard = $lateUpdateGuardPatched
    ExistingLateUpdateGuard = $lateUpdateGuardPresent
}
