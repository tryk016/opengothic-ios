#pragma once

#if defined(HAS_ASTCENC)

#include <Tempest/Pixmap>

#include <zenkit/Texture.hh>

#include <cstdint>
#include <string_view>

// DXT->ASTC 4x4 transcoding with an on-disk cache, for GPUs that cannot sample BC/S3TC.
//
// hasSamplerFormat(DXT1) measured false on both tested mobile Vulkan GPUs (Mali-G57
// and Adreno 619). Older Apple GPUs also lack BC, while A17 Pro and some M-series
// devices support it. When BC is absent, Tempest decompresses DXT textures to
// RGBA8 at full resolution (device.cpp:195-203). The measured texture payload
// was 633 MiB RGBA8 versus 158 MiB ASTC; the separate 1.38GB number was total
// GL mtrack, including non-texture allocations. ASTC 4x4 is 16 bytes per 4x4
// block, so it stays compressed at full resolution, which a mip-cap cannot do.
//
// Gated on GPU capability rather than a GPU name. The current encoder/build path is
// Android-only; future iOS support still needs astcenc linkage and Metal format plumbing.
// See docs/superpowers/specs/2026-07-16-astc-transcoder-design.md
namespace AstcTranscoder {
  // True when the GPU cannot sample DXT1 but can sample ASTC4x4. Latched on first call,
  // so it must not be called before the device exists.
  bool enabled();

  // Returns an ASTC4x4 pixmap with a mip chain, or an empty Pixmap to mean "not for me"
  // (caller then takes the ordinary DXT path). Serves from the disk cache when possible,
  // otherwise encodes and populates it. srcSize is the VDF entry size stored in the
  // current cache header. It is only a weak invalidation key: same-size source changes
  // are not detected until cache hardening adds a content hash.
  Tempest::Pixmap transcode(std::string_view name, const zenkit::Texture& tex, uint64_t srcSize);

  // Emits one [astc] summary line with measured counts, texel bytes and encode time.
  // Called at the end of a world load; safe to call with nothing transcoded.
  void logStats();
  }

#endif
