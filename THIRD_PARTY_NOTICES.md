# Third-party source notices

This document records attribution for source included or adapted in this
research repository. It does not change the licenses of upstream projects or
grant rights to Microsoft/game inputs, which are not included.

## Xenia

Source: https://github.com/xenia-project/xenia, pinned at
`95a5c3ee250f80c3b9d139658649d9ffb6db3eec`.

- `tools/decode-xenia-tiled-rgba8.cpp` adapts the tiled-address functions from
  `src/xenia/gpu/shaders/texture_address.xesli`.
  Copyright 2022 Ben Vanik. All rights reserved.
- `patches/xenia/0001-ac6-research-tooling.patch` contains modifications and
  upstream context from the Xenia source tree. Existing file notices apply.

The Xenia BSD license text is retained in [licenses/Xenia.txt](licenses/Xenia.txt).

## XenonRecomp and XenosRecomp

Sources: https://github.com/hedge-dev/XenonRecomp and
https://github.com/hedge-dev/XenosRecomp.
Copyright (c) 2025 hedge-dev and contributors.

The `patches/xenonrecomp` and `patches/xenosrecomp` directories contain changes
and context from their respective pinned source trees. Both use the MIT license,
retained in [licenses/hedge-dev-MIT.txt](licenses/hedge-dev-MIT.txt).
The XenonRecomp submodule also retains its original license and dependency
notices. Refer to each upstream tree for its transitive dependency licenses.
