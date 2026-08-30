#!/usr/bin/env bash
set -euo pipefail

# --- OpenGothic iOS configure script (run on macOS, e.g. a rented cloud Mac) ---
# Requires: Xcode + command line tools, Homebrew.
# For the no-Mac path use the GitHub Actions workflow (.github/workflows/ios.yml).

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build-ios"

echo "==> Checking tools"
command -v cmake            >/dev/null || { echo "Installing cmake";   brew install cmake; }
command -v glslangValidator >/dev/null || { echo "Installing glslang"; brew install glslang; }
command -v spirv-cross      >/dev/null || { echo "Installing spirv-cross"; brew install spirv-cross; }
xcode-select -p             >/dev/null || { echo "Install Xcode + run 'xcode-select --install' first"; exit 1; }

echo "==> Initializing pinned submodules"
git -C "$ROOT" submodule update --init --recursive

echo "==> Configuring (Xcode generator, iOS arm64, deployment 15.0)"
cmake -S "$ROOT" -B "$BUILD" -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0

echo "==> Done. Next:"
echo "    open \"$BUILD/OpenGothic.xcodeproj\""
echo "  Then set signing (personal free team) in Xcode and Run on your device."
