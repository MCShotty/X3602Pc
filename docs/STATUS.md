# AC6 checkpoint: 2026-09-14

Runtime work is paused. This checkpoint preserves source and reproducible
research changes, not a finished emulator or a newly validated release.

## What works and what remains

- The pinned base-disc AC6 build boots and reaches Mission 01 gameplay under
  XeO3. Virtual-controller navigation works.
- Recent gameplay was approximately 9-10 FPS. Graphics still show incorrect
  aircraft materials/geometry/shadows and hangar/cinematic color corruption.
  Pause/failure backgrounds remain blue. Recent flight frames did not show the
  earlier quarter-frame artifact, but that is not a universal rendering pass.
- Mission completion, physical-controller/force-feedback validation, and the
  full three-cold/three-warm stability matrix have not passed.
- Recent captured runs had zero recorded unmapped IARs, fatal indirect calls,
  synchronous queue/archive failures, or device-removal reason. This is bounded
  run evidence, not proof that earlier CPU/GPU failure classes are eliminated.

## Recent concrete progress

### PIX now exercises the guarded EDRAM corrections

PIX exposed identical 848-byte cached shader blobs for two different pipelines;
normal runs exposed distinct 954-byte AMD blobs. That invalidated the previous
shader-identity assumption and disabled the intended corrections under capture.

The bridge now uses guarded per-draw evidence for the PIX path: render-target
format, sample count, dimensions, root constant-buffer state, and the exact
16-word EDRAM constant signature. Descriptor copying and SRV/UAV overwrites
update/invalidate a bounded descriptor shadow. Opaque cache blobs alone are not
treated as unique shader identities. The ordinary non-PIX path remains separate.

Retained final telemetry:

| Run | EDRAM load fixes | EDRAM scale fixes | PIX proof matches |
| --- | ---: | ---: | ---: |
| PIX, PID 25964 | 6,765 | 4,308 | 11,073 |
| Normal, PID 36012 | 17,211 | 2,588 | 0 |

The PIX run later reached the game's Mission Failed screen, not an emulator
crash. The exact in-game failure cause was not collected. A saved capture with
"gameplay" in its name contains that failure transition, not stable flight.

### Bounded guest-to-host texture tracing is built, not live-validated

The latest source adds default-off tracing with at most 1,024 records for the
guest-to-host (G2H) texture transfer path. It checks the pinned host prologue/call
sites, original caller context, constant signature, dimensions, and physical
address bounds before reading metadata. It records original/applied endian
values without making an additional texture conversion decision.

Synthetic guard tests passed. The live caller-context interpretation and its
usefulness for the blue pause background still need validation. Do not apply a
blanket final-presentation or shared pixel-shader channel swap: the same shader
is used by smaller post-processing passes with different provenance.

### Source and tooling preserved

The checkpoint includes synchronization/fast-path experiments, register-state
publication, VGPU and descriptor tests, host-profile updates, telemetry/capture
commands, Ghidra helpers, shader correlation, geometry/EDRAM decoding, and PIX
resource/replay tooling. These remain experimental and host/title-specific.

Local Xenia and XenosRecomp changes are preserved as pinned patches described in
[UPSTREAM_RESEARCH.md](UPSTREAM_RESEARCH.md). They are diagnostic tools and
semantic references, not a replacement kernel/GPU runtime integrated into XeO3.

## Validation and deployment boundary

The retained `out/live/g2h-context-build-20260914.log` records all 16 registered
AOT-tree tests passing in 28.02 seconds, followed by a successful DLL build and
loader check: 18,842 mappings and 229 bridge imports. The project builds its
bridge/tests with `/W4 /WX`. Those checks were completed before the pause, not
rerun as part of the source-only publication.

| Artifact | SHA-256 | State at pause |
| --- | --- | --- |
| Lab AOT DLL | `3BA614CCFE4055E9EEAB801DA9B2FB131124C8EFC3BD1976A0827D5BF27D1C2B` | PIX correction deployed and exercised live |
| Lab AOT PDB | `ED83F9DFD8280994AA10384ADE0273828604A0F54F5A47F09CE32A91BCFF6AF8` | Matches deployed checkpoint |
| Newest built AOT DLL | `E7A5AF4C8C1E47A058DD412B83960D3F9EEEF156C1BD4C03C4BE038F10C94876` | Includes G2H context trace; not deployed/live-validated |

The deployed manifest is schema 29 at
`out/manifests/ac6-build-2608-pix-edram-copies-20260914.json`.
The writer has schema 30 changes for the new tracing fields; a new deployment
manifest was not generated before the pause. Raw evidence stays local/ignored.

## Pinned inputs

Use [the 2608 host profile](../profiles/xeo3/2608.3123.1.0-D1578E07.json).
Preserve older profiles as history, not as substitutes for current fingerprints.

| Input | Version / SHA-256 |
| --- | --- |
| Installed package | `2608.3123.1.0` |
| AC6 default.xex | `6EEFBA42CDFE9121207E534D8D290009C98B1A8C60AE5334A33A4F15167CBBBC` |
| Lab Emu.exe | `D1578E07B533E391D8A81C330D5493BA2D45B252490A818EC148DABE1BA24D06` |
| VGPUDX12.dll | `8306B4C06B100CAE18F91DCCD0468C2210CCA11A02D928025DE59BD827610247` |
| Flash kernel | `DA5BE614FB51B5809D70DA073F406F071E5CCB1F8C0EBCD57DAFBAB31B519BDD` |

## Resume

Begin with [the detailed upstream source review](NEXT_STEPS.md). Do not silently
redeploy the newest DLL or resume controller/capture loops during housekeeping.
