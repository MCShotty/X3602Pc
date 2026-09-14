# Deferred source review and execution handoff

Requested for the next development continuation. This file records the work;
the publication checkpoint does not itself start the review or runtime tests.
Use the exclusive checkout and no subagents, per [AGENTS.md](../AGENTS.md).

## 1. Inventory and read all three source trees systematically

Start with the pinned revisions and local patch inventory in
[UPSTREAM_RESEARCH.md](UPSTREAM_RESEARCH.md). Keep clean references separate from
patched disposable copies, all under this checkout. Do not discard existing
ignored fork work or update pins opportunistically.

Build a source-coverage ledger for Xenia, XenonRecomp, and XenosRecomp. Enumerate
modules/files, their roles, relevant call chains, and reviewed lines. Read the
implementation, supporting types, compiler/runtime assumptions, and tests in
detail. Record any excluded generated/vendor/platform code with a reason.
Do not describe a symbol search or README skim as an exhaustive source review.

For each useful mechanism, record:

- Exact pinned source file, symbol, and line references.
- Current XeO3 bridge equivalent and the concrete mismatch or missing behavior.
- Preconditions, bit-level semantics, ownership, endian/alignment assumptions,
  threading/lifetime rules, and failure behavior.
- Whether we should reuse attributed code, use it as a differential oracle,
  adapt a test/tool, or reject it as incompatible with the XeO3 architecture.
- Required licensing, proposed change size, risk, and a falsifiable test.

## 2. Prioritize the sources that explain our observed failures

### Xenia

- GPU texture/fetch constants, format/swizzle/endian decoding, 2D/3D tiling,
  resolve-to-memory semantics, render-target/EDRAM addressing, MSAA sample layout,
  scaled resolves, texture-cache invalidation, and guest/host aliasing.
- Shader frontend and DXBC translation: vertex-fetch index rounding, register
  export/interpolation, packed formats, integer/float conversion, coordinates,
  clipping, depth, blending, and alpha behavior. Trace the aircraft/terrain data
  through vertex buffers, constants, and draw setup before proposing shader hacks.
- D3D12 command processing, descriptors, barriers, queue/fence/readback behavior,
  trace playback, and the relationship between register state and GPU resources.
- PPC instruction semantics and tests; guest-memory access; CPU context state;
  exception, CR/LR/XER/FPSCR behavior; timebase and synchronization ordering.
  Consult kernel/audio/input code only to clarify the contract retained in XeO3.
- Treat the optional AC6 vertex-index bias as an experiment, not a universal
  instruction-semantic fix. Account for Xenia's own title-specific limitations.

### XenonRecomp

- Analyzer function discovery, compiler helper recognition, branch targets,
  jump tables, address-taken functions, indirect dispatch, and exclusions.
- PPC-to-C++ emission, optimized versus unoptimized state, integer overflow,
  floating-point/VMX semantics, endian/MMIO helpers, exception behavior, and
  asynchronous guest-state publication at every bridge boundary.
- Reconcile the seven existing local patches with the pinned implementation and
  tests. Check thread-local context, re-entry, imports, and host function lookup
  against XeO3's nonstandard entry/exit convention.

### XenosRecomp

- Shader/container decoding, vertex/texture fetch behavior, ALU/control flow,
  exports/interpolators, sampling, formats/endian assumptions, and HLSL generation.
- Compare raw microcode results with Xenia and XeO3 using the same shader/input
  data. Separate source containers from raw microcode: synthesized metadata can
  influence results and is not authoritative title metadata.
- Review the saved CLI patch's bounds/error handling, exception cleanup, and
  evidence completeness before trusting generated indexes or HLSL as an oracle.
- Evaluate reference/test/tool reuse first. Replacing XeO3's GPU with
  XenosRecomp or Xenia is not the current architecture or an automatic next step.

## 3. Turn findings into a prioritized, testable fix plan

First explain the blue pause/failure texture by tracing its producer, memory
bytes, transfer metadata, cache key/invalidation, and consumer descriptor. The
new default-off G2H context tracing has passed synthetic tests only. Validate its
live provenance before using it to choose endian/format corrections. Shared
post-processing shaders and the final swap path are not safe blanket fix points.

Then address aircraft geometry/material corruption and frame-time bottlenecks
using matched-input comparisons and call-chain evidence. Distinguish an incorrect
emulation result from an opt-in speed workaround. Keep successful normal/PIX
EDRAM paths covered by negative and regression tests.

Produce a ranked findings report and implementation plan with explicit gates.
Do not start another sequence of visual tweaks without a bounded hypothesis and
an observable result. Do not hide synchronization bugs with permanent sleeps or
GPU hangs with increased TDR timeouts.

## 4. Resume execution only after the source-led plan

When runtime work is authorized, validate hashes, compare built/deployed DLLs,
generate the matching manifest, and deploy to the existing lab only. Use virtual
input and skip every skippable cutscene. Keep capture helpers bounded and stop
all task-owned emulator/debugger/capture/controller processes after each run.

The final acceptance remains: three cold and three warm menu boots, three clean
Mission 01 starts, Mission 01 completion with virtual and physical input, correct
graphics/HUD/audio/DVD/control behavior, and no unresolved fatal IARs, deadlocks,
archive corruption, or GPU device removal. Physical-controller validation must
be recorded separately; virtual input cannot stand in for it.
