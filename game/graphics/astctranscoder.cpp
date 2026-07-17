#include "astctranscoder.h"

#if defined(HAS_ASTCENC)

#include <Tempest/Log>

#include <astcenc.h>

#include <sys/stat.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#include "gothic.h"
#include "resources.h"

using namespace Tempest;

namespace {

// Bump to invalidate every cached file at once, e.g. after touching encoder settings.
const uint32_t kCacheVersion = 1;
const uint32_t kCacheMagic   = 0x43545341u; // 'A','S','T','C' on little-endian

#pragma pack(push,1)
struct CacheHeader {
  uint32_t magic;
  uint32_t version;
  uint64_t srcSize;   // VDF entry size: game data replaced => re-encode
  uint32_t w;
  uint32_t h;
  uint32_t mipCnt;
  uint64_t dataSz;
  };
#pragma pack(pop)

struct Stats {
  std::atomic<uint32_t> hit    {0};
  std::atomic<uint32_t> encoded{0};
  std::atomic<uint32_t> failed {0};
  std::atomic<uint64_t> astcSz {0};
  std::atomic<uint64_t> rgbaSz {0};
  std::atomic<uint64_t> texels {0};
  std::atomic<uint64_t> encMs  {0};
  };
Stats stats;

std::mutex       ctxSync;
astcenc_context* ctx     = nullptr;
bool             ctxInit = false;

const std::string& cacheDir() {
  static const std::string dir = []() -> std::string {
    auto s = Gothic::settingsGetS("INTERNAL","astcCacheDir");
    std::string d = s.empty() ? std::string("astc") : std::string(s);
    // Android chdir()s to /sdcard/OpenGothic (main_android.cpp), where the game already
    // writes log.txt and Gothic.ini, so a relative default lands beside them and survives
    // an APK reinstall.
    ::mkdir(d.c_str(), 0775);
    return d;
    }();
  return dir;
  }

std::string cachePath(std::string_view name) {
  std::string p = cacheDir();
  p += '/';
  for(char c:name) {
    const bool safe = (c>='A' && c<='Z') || (c>='a' && c<='z') ||
                      (c>='0' && c<='9') || c=='.' || c=='-' || c=='_';
    p += safe ? c : '_';
    }
  p += ".astc";
  return p;
  }

// Bytes of one ASTC 4x4 mip level: 16 bytes per 4x4 block, partial blocks rounded up.
// This is exactly the block grid Pixmap::blockCount and vulkanapi.cpp assume for DXT.
uint64_t astcSize(uint32_t w, uint32_t h) {
  return uint64_t((w+3)/4) * uint64_t((h+3)/4) * 16u;
  }

// Bytes the RGBA8 path allocated on the GPU for the same texture: Device::texture()
// decompresses to RGBA8 and asks for mipCount(w,h) levels, which the GPU then generates.
// Mirrors device.cpp:15-23 so the "before" number is arithmetic on real dimensions
// rather than a share of a total-GPU-memory counter.
uint64_t rgba8ChainSize(uint32_t w, uint32_t h) {
  uint32_t n = 1, s = std::max(w,h);
  while(s>1) {
    ++n;
    s = s/2;
    }
  uint64_t sz = 0;
  for(uint32_t i=0; i<n; ++i)
    sz += uint64_t(std::max<uint32_t>(1,w>>i)) * uint64_t(std::max<uint32_t>(1,h>>i)) * 4u;
  return sz;
  }

// Call with ctxSync held.
astcenc_context* context() {
  if(ctxInit)
    return ctx;
  ctxInit = true;

  astcenc_config cfg = {};
  // LDR, not LDR_SRGB: the format we map to is VK_FORMAT_ASTC_4x4_UNORM_BLOCK, and the
  // path being replaced produced VK_FORMAT_R8G8B8A8_UNORM. Both are unorm, so the encoder
  // must weigh error linearly too or the swap would shift every texture's gamma.
  auto e = astcenc_config_init(ASTCENC_PRF_LDR, 4,4,1, ASTCENC_PRE_FAST,
                               ASTCENC_FLG_SELF_DECOMPRESS_ONLY, &cfg);
  if(e!=ASTCENC_SUCCESS) {
    Log::e("[astc] config_init: ", astcenc_get_error_string(e));
    return nullptr;
    }
  // thread_count=1: Resources::sync is a global recursive_mutex held across the whole
  // texture load, so there is no concurrency here to hand the encoder.
  e = astcenc_context_alloc(&cfg, 1, &ctx, nullptr);
  if(e!=ASTCENC_SUCCESS) {
    Log::e("[astc] context_alloc: ", astcenc_get_error_string(e));
    ctx = nullptr;
    return nullptr;
    }
  return ctx;
  }

Pixmap readCache(const std::string& path, uint64_t srcSize) {
  FILE* f = std::fopen(path.c_str(),"rb");
  if(f==nullptr)
    return Pixmap();

  CacheHeader h = {};
  if(std::fread(&h,1,sizeof(h),f)!=sizeof(h) ||
     h.magic!=kCacheMagic || h.version!=kCacheVersion || h.srcSize!=srcSize ||
     h.w==0 || h.h==0 || h.mipCnt==0 || h.dataSz==0 || h.dataSz>(256u<<20)) {
    std::fclose(f);
    return Pixmap();
    }

  std::vector<uint8_t> data(size_t(h.dataSz));
  const bool ok = std::fread(data.data(),1,data.size(),f)==data.size();
  std::fclose(f);
  if(!ok)
    return Pixmap();

  try {
    return Pixmap(data.data(), data.size(), h.w, h.h, h.mipCnt, TextureFormat::ASTC4x4);
    }
  catch(...) {
    return Pixmap();
    }
  }

void writeCache(const std::string& path, const CacheHeader& h, const std::vector<uint8_t>& data) {
  const std::string tmp = path + ".tmp";
  FILE* f = std::fopen(tmp.c_str(),"wb");
  if(f==nullptr)
    return;
  const bool ok = std::fwrite(&h,1,sizeof(h),f)==sizeof(h) &&
                  std::fwrite(data.data(),1,data.size(),f)==data.size();
  std::fclose(f);
  // rename() is atomic on POSIX: killing the process mid-encode can leave a .tmp behind,
  // but never a truncated file that the next run would trust as a valid cache entry.
  if(!ok || std::rename(tmp.c_str(),path.c_str())!=0)
    std::remove(tmp.c_str());
  }

} // namespace

bool AstcTranscoder::enabled() {
  static const bool en = []() -> bool {
    auto& p    = Resources::device().properties();
    const bool dxt  = p.hasSamplerFormat(TextureFormat::DXT1);
    const bool astc = p.hasSamplerFormat(TextureFormat::ASTC4x4);
    Log::i("[astc] caps: DXT1=",int(dxt)," ASTC4x4=",int(astc),
           " -> transcoder ",int(!dxt && astc));
    return !dxt && astc;
    }();
  return en;
  }

Tempest::Pixmap AstcTranscoder::transcode(std::string_view name, const zenkit::Texture& tex, uint64_t srcSize) {
  const uint32_t w0 = std::max<uint32_t>(1,tex.mipmap_width(0));
  const uint32_t h0 = std::max<uint32_t>(1,tex.mipmap_height(0));

  // vulkanapi.cpp walks exactly pm.mipCount() levels, and an image cannot have more levels
  // than its dimensions allow. Nothing validates the count stored in the file, and zenkit's
  // mipmap_width() is a bare `_m_width >> level` -- a bogus count would mean out-of-range
  // levels and a >>32 shift. The DXT path only trusts this count on BC-capable desktop GPUs;
  // this is the first path to trust it on mobile, so clamp instead. Mirrors device.cpp:15-23.
  uint32_t maxMips = 1;
  for(uint32_t s=std::max(w0,h0); s>1; s=s/2)
    ++maxMips;
  const uint32_t mips = std::min(tex.mipmaps(), maxMips);

  // A single-mip source is a trap rather than a win: Device::texture() only keeps a
  // compressed pixmap when (!mips || pm.mipCount()>1), so a 1-mip ASTC pixmap loaded with
  // forceMips would be handed to Pixmap(pm,RGBA8) -- back into the very decompression this
  // exists to avoid, and into the ASTC guard that throws there. Such textures are UI-sized.
  if(mips<=1)
    return Pixmap();

  const std::string path = cachePath(name);
  {
    Pixmap pm = readCache(path,srcSize);
    if(!pm.isEmpty()) {
      stats.hit.fetch_add(1);
      stats.astcSz.fetch_add(pm.dataSize());
      stats.rgbaSz.fetch_add(rgba8ChainSize(pm.w(),pm.h()));
      return pm;
      }
  }

  std::lock_guard<std::mutex> guard(ctxSync);
  astcenc_context* c = context();
  if(c==nullptr)
    return Pixmap();

  uint64_t total = 0;
  for(uint32_t l=0; l<mips; ++l)
    total += astcSize(std::max<uint32_t>(1,tex.mipmap_width(l)),
                      std::max<uint32_t>(1,tex.mipmap_height(l)));

  std::vector<uint8_t> out(size_t(total));
  // Straight RGBA even for DXT1: Gothic's DXT1 can carry 1-bit punch-through alpha, and an
  // RGB1 swizzle would silently flatten it. Costs some rate; cannot lose data.
  const astcenc_swizzle swz = {ASTCENC_SWZ_R,ASTCENC_SWZ_G,ASTCENC_SWZ_B,ASTCENC_SWZ_A};

  const auto t0     = std::chrono::steady_clock::now();
  size_t     off    = 0;
  uint64_t   texels = 0;
  for(uint32_t l=0; l<mips; ++l) {
    const uint32_t mw = std::max<uint32_t>(1,tex.mipmap_width(l));
    const uint32_t mh = std::max<uint32_t>(1,tex.mipmap_height(l));

    std::vector<uint8_t> rgba;
    try {
      rgba = tex.as_rgba8(l);
      }
    catch(...) {
      stats.failed.fetch_add(1);
      return Pixmap();
      }
    if(rgba.size() < size_t(mw)*size_t(mh)*4u) {
      stats.failed.fetch_add(1);
      return Pixmap();
      }

    astcenc_image img = {};
    img.dim_x     = mw;
    img.dim_y     = mh;
    img.dim_z     = 1;
    img.data_type = ASTCENC_TYPE_U8;
    void* slice[1] = { rgba.data() };
    img.data      = slice;

    const size_t len = size_t(astcSize(mw,mh));
    auto e = astcenc_compress_image(c,&img,&swz,out.data()+off,len,0);
    astcenc_compress_reset(c);
    if(e!=ASTCENC_SUCCESS) {
      Log::e("[astc] compress ",name,": ",astcenc_get_error_string(e));
      stats.failed.fetch_add(1);
      return Pixmap();
      }

    off    += len;
    texels += uint64_t(mw)*uint64_t(mh);
    }
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now()-t0).count();

  CacheHeader h = {};
  h.magic   = kCacheMagic;
  h.version = kCacheVersion;
  h.srcSize = srcSize;
  h.w       = w0;
  h.h       = h0;
  h.mipCnt  = mips;
  h.dataSz  = total;
  writeCache(path,h,out);

  stats.encoded.fetch_add(1);
  stats.encMs .fetch_add(uint64_t(ms));
  stats.texels.fetch_add(texels);
  stats.astcSz.fetch_add(total);
  stats.rgbaSz.fetch_add(rgba8ChainSize(h.w,h.h));

  try {
    return Pixmap(out.data(), out.size(), h.w, h.h, mips, TextureFormat::ASTC4x4);
    }
  catch(...) {
    return Pixmap();
    }
  }

void AstcTranscoder::logStats() {
  const uint32_t hit  = stats.hit    .load();
  const uint32_t enc  = stats.encoded.load();
  const uint32_t fail = stats.failed .load();
  if(hit==0 && enc==0 && fail==0)
    return;

  const uint64_t astcB = stats.astcSz.load();
  const uint64_t rgbaB = stats.rgbaSz.load();
  const uint64_t px    = stats.texels.load();
  const uint64_t ms    = stats.encMs .load();

  // The texture-only subtotal, which is the only thing this transcoder controls. Total
  // GPU counters (GL mtrack) also carry render targets, geometry and the swapchain, none
  // of which shrink -- comparing against those would judge a working transcoder a failure.
  Log::i("[astc] textures: ",enc," encoded, ",hit," cache hits, ",fail," failed");
  Log::i("[astc] texel bytes: astc=",int(astcB>>20),"MiB vs rgba8=",int(rgbaB>>20),"MiB",
         " (ratio x",(astcB>0 ? double(rgbaB)/double(astcB) : 0.0),")");
  Log::i("[astc] encoded ",double(px)/1000000.0," Mpx in ",double(ms)/1000.0," s");
  }

#endif
