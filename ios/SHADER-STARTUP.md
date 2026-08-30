# iOS shader startup profile

This port keeps the upstream Tempest Metal renderer. It does not import the
unfinished `RendererIOS` experiment. Cold startup is shortened at three safe
boundaries, all of which preserve the normal runtime compiler as a fallback.

## Startup sequence

1. `Shaders::compileKeyShaders()` creates only the Bink and save-thumbnail
   pipelines synchronously.
2. The remaining catalogue compiles on the existing worker future. The first
   menu and swapchain resize do not wait for that future on iOS.
3. Entering or replacing a world still calls `Shaders::waitCompiler()`. A
   compiler exception is propagated there instead of reaching the renderer as
   an empty pipeline.

The iOS catalogue omits groups which cannot be used by the selected startup
configuration: VSM, RTSM, GI, software ray tracing and CMAA2. CMAA2 builds only
the selected preset. Release builds also omit diagnostic ray-query shaders.
Desktop builds retain the upstream full catalogue.

## `OpenGothicStartup.metallib`

The iPhoneOS build creates a small library containing `triangle.vert` and
`downscale.frag`. Each entry point is named from the first 64 bits of the SHA-256
of its complete SPIR-V bytecode. Runtime lookup hashes those same bytes, so two
variants sharing a source filename cannot be confused.

The asset is optional. If the build tools are missing, the file is absent, a
function hash is not present, or Metal rejects the library, Tempest follows its
normal SPIR-V -> MSL -> `newLibrary` path. Repeated SPIR-V modules are also
deduplicated by a 16-entry LRU cache. Compute modules are removed immediately
after their PSO is created, so the cache cannot retain their Metal libraries
through the first world load.

Bink deliberately stays on the runtime path. Its unsized SSBO arrays use
Tempest's buffer-length ABI at Metal slot 29, while the command-line
SPIRV-Cross generator uses a different default. Shipping that function without
matching metadata would be unsafe.

No writable runtime `MTLBinaryArchive` is used. A read-only archive can be
added later only after it is captured and validated on representative devices;
it is an optional pipeline-state optimization, not a replacement for the
shader-library fallback.

## Compatibility and verification

- Deployment target remains iOS 15.0, including iOS 16.4.
- The generator emits a matching iPhoneOS or iPhoneSimulator Metal 2.4 target
  and contains no optional GPU features.
- Verify symbols with `metal-objdump --syms`; entry names must match the SPIR-V
  SHA-256 prefixes.
- A release build must contain `OpenGothicStartup.metallib` in the root of the
  application bundle.
- Device acceptance covers a fresh install, warm relaunch, intro playback,
  first world load and saving a thumbnail.
