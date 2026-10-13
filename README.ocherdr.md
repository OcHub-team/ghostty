# OcHerdr's GhosttyKit integration

This branch tracks upstream `ghostty-org/ghostty` commit
`cc53940b4cf97876e48826e5d825833a77471c89` and adds the embedding APIs needed by
[OcHerdr](https://github.com/OcHub-team/OcHerdr). This is the development tree for
the next Ghostty release, not a released Ghostty 1.4 binary. Upstream currently
identifies the tree as `1.3.2-dev`.

The patch adds manual I/O, a mirror mode which suppresses parser-generated PTY
replies, externally presented Metal frames with explicit leases, exact terminal
grid sizing, viewport row selection, and a surface-specific scrollback byte cap.
Herdr remains the owner of the PTY and terminal protocol. Ordinary Ghostty
surfaces continue to use the upstream process backend and native presenter.

These embedding APIs are derived from manaflow-ai/ghostty's changes:

- `22fa801f88f96fa842e54ecce6c34a5d36003d19`: manual embedded I/O
- `581dbf264f3f7808f414e4a7ecd461b80bbca989`: mirror I/O
- `8c645641a1dd1cbff9aa73189629682c01b1a233`: leased Metal frames and grid sizing
- `fcafac572970e615c9ac5ecd25eaa325cec1f7ca`: rotate acquired frame slots

The lease implementation also keeps tokens distinct across swap-chain
recreation. The C header and static archive must always be used together; this
branch does not promise ABI compatibility with older manaflow binaries.

Build with Zig 0.16.0 and a complete Xcode installation:

```sh
zig build -Demit-exe=false -Demit-macos-app=false -Demit-xcframework=true \
  -Dxcframework-target=universal -Doptimize=ReleaseFast -Dsentry=false
```

The output is `macos/GhosttyKit.xcframework` with both arm64 and x86_64 macOS
architectures. For a faster local iteration, use `-Dxcframework-target=native`.

Run the embedding checks:

```sh
zig test src/renderer/frame_lease.zig
zig build test -Demit-exe=false -Demit-macos-app=false -Demit-xcframework=false \
  -Dtest-filter='frame lease' -Dtest-filter='manual queueWrite' \
  -Dtest-filter='terminal response suppression' -Dtest-filter='Size.screenForGrid' \
  -Dtest-filter='external Metal'
test/ocherdr/run.sh
```

The native integration test requires a Metal-capable macOS host. It verifies a
real surface's grid, UTF-8 output, selection, suppressed PTY replies, encoded
input, two simultaneously held IOSurfaces, out-of-order release and teardown.
All acquired frames must be released before freeing their surface. A frame's
host context is captured at submission, so a host can reject frames from an old
layout without guessing from their dimensions.
