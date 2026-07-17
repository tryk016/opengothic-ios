#pragma once

#if defined(HAS_ASTCENC)

#include <Tempest/Pixmap>

#include <zenkit/Texture.hh>

#include <cstdint>
#include <string_view>

// DXT->ASTC 4x4 transcoding with an on-disk cache, for GPUs that cannot sample BC/S3TC.
//
// Mobile Vulkan essentially has no BC: hasSamplerFormat(DXT1) measured false on both
// Mali-G57 and Adreno 619, and no Apple GPU has it either. Tempest reacts by decompressing
// every DXT texture to RGBA8 at full resolution (device.cpp:195-203) -- measured 1.38GB of
// GPU memory on a 3.5GB tablet. ASTC 4x4 is 16 bytes per 4x4 block, so it stays compressed
// (4x smaller than RGBA8) at FULL resolution, which is what a mip-cap fundamentally cannot do.
//
// Gated on GPU capability rather than #ifdef, so iOS picks this up unchanged.
// See docs/superpowers/specs/2026-07-16-astc-transcoder-design.md
namespace AstcTranscoder {
  // True when the GPU cannot sample DXT1 but can sample ASTC4x4. Latched on first call,
  // so it must not be called before the device exists.
  bool enabled();

  // Returns an ASTC4x4 pixmap with a mip chain, or an empty Pixmap to mean "not for me"
  // (caller then takes the ordinary DXT path). Serves from the disk cache when possible,
  // otherwise encodes and populates it. srcSize is the VDF entry size, stored in the cache
  // header so replacing game data invalidates stale entries.
  Tempest::Pixmap transcode(std::string_view name, const zenkit::Texture& tex, uint64_t srcSize);

  // Emits one [astc] summary line with measured counts, texel bytes and encode time.
  // Called at the end of a world load; safe to call with nothing transcoded.
  void logStats();
  }

#endif
