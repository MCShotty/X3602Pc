# Project working agreement

## Workspace and publication

- The exclusive development checkout on this PC is
  `C:\Users\CaptainMcShotgun\Documents\x3602Pc`.
- Its private GitHub remote is `https://github.com/MCShotty/X3602Pc`.
  Continue on `codex/xeo3-ac6-aot` unless the user requests a different branch.
- Keep source edits, reference checkouts, builds, captures, and analysis under
  this checkout. Put disposable upstream copies under ignored `.tools/` and
  evidence under ignored `out/`; preserve fork edits as tracked patches.
- `D:\Games\AC6 shit\XeO3-AC6-lab` is a runtime deployment destination, not a
  second development workspace. `D:\XeO3AC6DVD` and the original disc image are
  local game inputs. Do not move or delete these directories for housekeeping.
- Do not modify `WindowsApps`, ownership/activation behavior, or DRM checks.
- Publish source, tests, configuration, attributed upstream patches, and curated
  findings only. Never commit game/Microsoft binaries, translated game code,
  extracted shaders, raw GPU/guest-memory data, keys, captures, or local logs.
  Do not force-add ignored research artifacts. Preserve unrelated local files.

## Current handoff

- Runtime testing is paused at this checkpoint. Resume only when requested.
  A request to report status or publish changes is not permission to run AC6.
- Do not use subagents unless the user explicitly reverses this instruction.
- On the next development continuation, first perform the detailed Xenia,
  XenonRecomp, and XenosRecomp source review in `docs/NEXT_STEPS.md`.
- Read `docs/STATUS.md` before relying on earlier observations. Distinguish
  source implementation, successful build/unit tests, deployment, live evidence,
  and gameplay acceptance. None implies the next stage has passed.
- Keep Xbox kernel, GPU, storage, audio, and input services in XeO3. Upstream
  projects are reference/translation tools unless an explicit design decision
  says otherwise. Preserve pinning and all host/fingerprint safety gates.

## Communication

Be cheerful and direct, with brief dry humor when natural. Avoid em dashes.
Report concrete evidence and limitations; do not sell instrumentation progress
as a gameplay fix. When self-testing is authorized, skip skippable cutscenes.
