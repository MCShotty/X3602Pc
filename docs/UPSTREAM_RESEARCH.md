# Pinned upstream research sources

These are source-only checkpoint patches, not upstream releases or proof of
correct AC6 gameplay. Local generated shader code, game data, traces, and binaries
are deliberately excluded. See [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).

| Project | Source | Pinned revision | Tracked changes |
| --- | --- | --- | --- |
| XenonRecomp | https://github.com/hedge-dev/XenonRecomp | `ddd128bcca99fe8bfbb99bea583c972351fa6ace` | `patches/xenonrecomp/0001` through `0007` |
| Xenia | https://github.com/xenia-project/xenia | `95a5c3ee250f80c3b9d139658649d9ffb6db3eec` | `patches/xenia/0001-ac6-research-tooling.patch` |
| XenosRecomp | https://github.com/hedge-dev/XenosRecomp | `990d03b28a27b50277ee5d8d942e1c5f873869d1` | `patches/xenosrecomp/0001-ac6-shader-analysis-cli.patch` |

## Existing local copies

- `third_party/XenonRecomp` is the pristine recursive submodule. Use
  `tools/build-xenonrecomp.ps1` for the disposable patched build.
- `.tools/upstream/xenia` is the modified research tree preserved by the Xenia
  patch. `.tools/xenia-reference`, `.tools/xenia-source`, and `.tools/xenia-src`
  were clean at the same pin when this checkpoint was prepared.
- `.tools/upstream/XenosRecomp` is the clean pinned reference.
  `.tools/work/XenosRecomp-ac6` is the modified CLI tree preserved by its patch.
- `.tools/xenia-canary-source` is a separate clean comparison at
  `8f55b4abf70d5041e2e9da65f8b2adcce002b115`. It is not the semantic baseline.

Do not edit a reference copy in place or create another development workspace.
These existing directories are all within the canonical checkout; none needs to
be deleted or moved. Never commit the complete `.tools` trees.

## Xenia patch scope

- Keep the portable XTR frame-trace path available when Graphics Tools exposes
  `IDXGraphicsAnalysis`, rather than diverting that request into PIX.
- Add an opt-in, default-off `ac6_ground_fix` vertex-fetch index bias. This is an
  explicitly title-specific experiment and changes translated shader behavior.
- Extend offline trace dumping with draw/texture/vertex metadata and selected
  local AC6 resolve/terrain dumps for cross-emulator comparison.
- Adapt the local build helper for Python 3.14 command-line handling and the
  installed Visual Studio version's existing Premake project format.

The diagnostic dumper contains capture-specific addresses, shader hashes, and
command numbers. It is not a generic validated export API. Its output files must
remain ignored even when written into the repository root.

## XenosRecomp patch scope and limitations

- `--index`: shader container/microcode hashes and source locations in a TSV.
- `--hlsl-dir`: per-shader HLSL files for local comparison.
- `--raw-ucode`: wrap raw microcode in synthetic metadata, then emit HLSL with
  size bounds and Windows exception reporting.

These options were preserved from the working research copy, not polished for
upstream submission. Synthetic raw-shader metadata is an assumption. The
directory mode skips shaders on native exceptions, so success does not imply
complete coverage. Its exception branch retains the allocated recompiler; input
file handling and container bounds also need the deferred detailed review.
Do not silently reinterpret these limitations as successful shader validation.

## Reproduce without modifying clean references

Make disposable copies under `.tools/work`, check out the exact pin, initialize
its recursive submodules, and apply the corresponding patch. From the repository
root, read-only checks against the existing clean references are:

```powershell
$researchRoot = (Get-Location).Path
git -C .tools/xenia-reference rev-parse HEAD
git -C .tools/xenia-reference apply --check "$researchRoot/patches/xenia/0001-ac6-research-tooling.patch"
git -C .tools/upstream/XenosRecomp rev-parse HEAD
git -C .tools/upstream/XenosRecomp apply --check "$researchRoot/patches/xenosrecomp/0001-ac6-shader-analysis-cli.patch"
```

Run `git apply` without `--check` only in the disposable copy. Review all compiler
and diagnostic output. Patch applicability is a reproducibility check, not a
replacement for building/testing the fork or validating gameplay.
