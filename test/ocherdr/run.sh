#!/usr/bin/env bash
set -euo pipefail
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
FRAMEWORK="${GHOSTTYKIT_XCFRAMEWORK:-$ROOT/macos/GhosttyKit.xcframework}"
SLICE="$FRAMEWORK/macos-arm64_x86_64"
if [ ! -d "$SLICE" ]; then
  case "$(uname -m)" in
    arm64) SLICE="$FRAMEWORK/macos-arm64" ;;
    x86_64) SLICE="$FRAMEWORK/macos-x86_64" ;;
  esac
fi
ARCHIVE="$SLICE/ghostty-internal.a"
if [ ! -f "$ARCHIVE" ]; then ARCHIVE="$SLICE/libghostty-internal.a"; fi
TEST_DIR="$(mktemp -d)"
trap 'rm -rf "$TEST_DIR"' EXIT
xcrun clang -isysroot "$(xcrun --sdk macosx --show-sdk-path)" -std=c11 -Wall -Wextra -Werror -DGHOSTTY_STATIC \
  -I "$SLICE/Headers" "$ROOT/test/ocherdr/embedding.c" \
  "$ARCHIVE" -lc++ \
  -framework ApplicationServices -framework Carbon -framework Foundation \
  -framework IOSurface -framework Metal -framework QuartzCore \
  -framework UniformTypeIdentifiers -o "$TEST_DIR/embedding"
"$TEST_DIR/embedding"
