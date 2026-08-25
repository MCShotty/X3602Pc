# X3602Pc

Experimental Ace Combat 6 AOT bridge for Microsoft's Windows XeO3 Xbox 360
backward-compatibility runtime. The project uses a pinned XenonRecomp build for
PowerPC translation while retaining XeO3 for kernel services, guest scheduling,
storage, audio, input, and the Xenos-to-D3D12 path.

The pinned base-disc build currently boots and reaches Mission 01. Gameplay
rendering, performance, and long-run stability remain active research work; this
is not a general-purpose Xbox 360 emulator.

## Repository policy

This repository contains only original bridge code, configuration, analysis
metadata, tests, and reproducible patches. It does not contain Xbox game data,
Microsoft binaries, decrypted executables, extracted shaders, keys, or ownership
bypasses. Those inputs remain local and must come from software you own and a
legitimate XeO3/Gaming Services installation.

Pinned title input:

- AC6 `default.xex` SHA-256:
  `6EEFBA42CDFE9121207E534D8D290009C98B1A8C60AE5334A33A4F15167CBBBC`
- Image base: `0x82000000`
- Entry point: `0x821F5ED0`
- Image size: `0x00AA0000`

## Layout

- `src/xeo3_bridge`: XeO3 ABI bridge, runtime state synchronization, kernel
  continuations, diagnostics, and narrowly scoped VGPU experiments.
- `src/xeo3_contract`: minimal ABI probe DLL.
- `configs/ac6`: pinned XenonRecomp analysis and title configuration.
- `profiles/xeo3`: hash-gated XeO3 host profiles.
- `patches/xenonrecomp`: reproducible patches applied to a disposable copy of
  the pinned upstream submodule.
- `tests`: native contract, state, dispatch, synchronization, and VGPU tests.
- `tools`: build, analysis, deployment, capture, and debugger automation.

## Build

Requirements are Windows 11, CMake 3.26+, Ninja, LLVM/`clang-cl`, Visual Studio
Build Tools with MASM, and the Windows 11 SDK. Clone recursively so the pinned
XenonRecomp submodule is present:

```powershell
git clone --recurse-submodules https://github.com/MCShotty/X3602Pc.git
cd X3602Pc
```

Build and test both the pinned upstream tools and the patched recompiler:

```powershell
.\tools\build-xenonrecomp.ps1 -Pristine
.\tools\build-xenonrecomp.ps1
```

Generate the AC6 AOT sources and build the bridge using a local, hash-matched
disc executable:

```powershell
.\tools\analyze-ac6.ps1 -XexPath 'D:\XeO3AC6DVD\default.xex'
.\tools\run-xenonrecomp.ps1 -XexPath 'D:\XeO3AC6DVD\default.xex'
.\tools\build-ac6-aot.ps1 -XexPath 'D:\XeO3AC6DVD\default.xex'
```

The probe can be built independently:

```powershell
.\tools\build-probe.ps1 -XexPath 'D:\XeO3AC6DVD\default.xex'
```

The VGPU patch tests and final AOT DLL also require two hash-pinned shader
artifacts extracted from a local PIX capture. CMake skips those private targets
when both artifacts are absent, so a clean source checkout can still configure
and build the probe and public unit tests. Generated code, captures, build output,
game assets, and Microsoft runtime files are intentionally ignored by Git.

## Status

This is pre-release preservation research. Expect title-specific assumptions,
strict host hashes, diagnostic instrumentation, and spectacularly unhelpful
failure modes when Microsoft updates the runtime.
