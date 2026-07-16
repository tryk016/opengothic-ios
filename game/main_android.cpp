#if defined(__ANDROID__)
#include <game-activity/native_app_glue/android_native_app_glue.h>

#include <Tempest/Application>
#include <Tempest/Log>
#include <Tempest/VulkanApi>
#include <Tempest/Device>
#include <Tempest/File>   // Tempest::WFile (log.txt), see main.cpp's identical usage

#include <zenkit/Logger.hh>
#include <dmusic.h>

#include <astcenc.h>  // TEMP Phase-1 decision gate (astcBenchmark below)

#include <cstring>
#include <chrono>
#include <vector>
#include <unistd.h>   // ::chdir

#include "system/api/androidapi.h"   // setAndroidApp

#include "utils/crashlog.h"
#include "utils/systemmsg.h"
#include "utils/audiosession.h"
#include "resources.h"
#include "gamemusic.h"
#include "gothic.h"
#include "mainwindow.h"
#include "build.h"
#include "commandline.h"

using namespace Tempest;

struct android_app* g_androidApp = nullptr;

namespace {
// Mirrors main.cpp's selectDevice(): prefer a discrete GPU, else take
// whatever the backend reports first (Android devices normally expose a
// single integrated GPU, so the d[0] fallback is the one that actually fires).
std::string_view selectDevice(const Tempest::AbstractGraphicsApi& api) {
  auto d = api.devices();

  static Tempest::Device::Props p;
  for(auto& i:d)
    if(i.type==Tempest::DeviceType::Discrete) {
      p = i;
      return p.name;
      }
  if(d.size()>0) {
    p = d[0];
    return p.name;
    }
  return "";
  }

// TEMP Phase-1 decision gate: measure real astcenc throughput on this CPU.
// The job size is known from measurement rather than guessed: the 1.38GB of
// RGBA8 that Mali currently holds / 4 bytes-per-pixel = ~345 Mpixels for all of
// Khorinis including mips. This turns the design's "~1-3 min" estimate into a
// fact, and decides on-device encoding vs the offline fallback.
// See docs/superpowers/specs/2026-07-16-astc-transcoder-design.md §5.3/§6.
// Revert once Phase 1 concludes.
void astcBenchmark() {
  const unsigned int dimX = 512, dimY = 512;

  astcenc_config config = {};
  astcenc_error  st = astcenc_config_init(ASTCENC_PRF_LDR, 4, 4, 1, ASTCENC_PRE_FAST, 0, &config);
  if(st!=ASTCENC_SUCCESS) {
    Log::e("[astcdiag] config_init failed: ", astcenc_get_error_string(st));
    return;
    }

  astcenc_context* ctx = nullptr;
  st = astcenc_context_alloc(&config, 1, &ctx);
  if(st!=ASTCENC_SUCCESS) {
    Log::e("[astcdiag] context_alloc failed: ", astcenc_get_error_string(st));
    return;
    }

  // Structured, non-trivial content: a flat image would encode unrealistically
  // fast and pure noise unrealistically slow.
  std::vector<uint8_t> src(size_t(dimX)*size_t(dimY)*4);
  for(size_t i=0; i<src.size(); i+=4) {
    const uint32_t p = uint32_t(i/4);
    const uint32_t x = p % dimX;
    const uint32_t y = p / dimX;
    src[i+0] = uint8_t(x ^ y);
    src[i+1] = uint8_t(x + y);
    src[i+2] = uint8_t(x*3 + y*5);
    src[i+3] = 255;
    }

  uint8_t*      slice = src.data();
  astcenc_image image = {};
  image.dim_x     = dimX;
  image.dim_y     = dimY;
  image.dim_z     = 1;
  image.data_type = ASTCENC_TYPE_U8;
  image.data      = reinterpret_cast<void**>(&slice);

  const astcenc_swizzle swz{ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A};
  const size_t blocks = size_t((dimX+3)/4) * size_t((dimY+3)/4);
  std::vector<uint8_t> dst(blocks*16);   // ASTC 4x4 = 16 bytes per block

  const auto t0 = std::chrono::steady_clock::now();
  st = astcenc_compress_image(ctx, &image, &swz, dst.data(), dst.size(), 0);
  const double ms = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();
  astcenc_context_free(ctx);

  if(st!=ASTCENC_SUCCESS) {
    Log::e("[astcdiag] compress failed: ", astcenc_get_error_string(st));
    return;
    }

  const double mpx       = double(dimX)*double(dimY)/1e6;
  const double mpxPerSec = (ms>0.0) ? mpx/(ms/1000.0) : 0.0;
  const double khorinisS = (mpxPerSec>0.0) ? 345.0/mpxPerSec : -1.0;
  Log::i("[astcdiag] astcenc 512x512 PRE_FAST 1-thread: ", ms, " ms => ", mpxPerSec, " Mpx/s"
         " | Khorinis 345 Mpx => ", khorinisS, " s single-threaded (~", khorinisS/6.0, " s over 6 cores)"
         " | out=", uint32_t(dst.size()/1024), " KiB vs rgba8=", uint32_t(src.size()/1024), " KiB");
  }
}

extern "C" void android_main(struct android_app* app) {
  g_androidApp = app;
  // Ordering invariant (Task 1 review): SystemApi::inst() - i.e. the AndroidApi
  // singleton - is created lazily by the first Tempest::Window it sees, and its
  // constructor blocks polling ALooper until g_androidApp->window != nullptr.
  // setAndroidApp() MUST be called before that first Window is constructed
  // (MainWindow, below), so keep this call first and do not reorder it after
  // any Tempest object that might construct a Window.
  Tempest::AndroidApi::setAndroidApp(app);

  // Task 3: land on the writable data root before anything relative-path
  // based runs (log.txt right below, then Gothic.ini/saves inside the
  // engine). Game data itself is addressed explicitly via the absolute
  // "-g /sdcard/OpenGothic/Gothic2" argv entry below; chdir just makes
  // log.txt/Gothic.ini/saves resolve next to Gothic2/ instead of landing in
  // whatever unspecified (and likely unwritable) CWD the native-glue thread
  // starts with.
  if(::chdir("/sdcard/OpenGothic")!=0)
    Tempest::Log::e("unable to chdir to /sdcard/OpenGothic");

  // --- from here down, mirrors game/main.cpp's bootstrap (mobile path) ---
  try {
    static Tempest::WFile logFile("log.txt");
    Tempest::Log::setOutputCallback([](Tempest::Log::Mode mode, const char* text) {
      logFile.write(text,std::strlen(text));
      logFile.write("\n",1);
      if(mode==Tempest::Log::Error)
        logFile.flush();
      });
    }
  catch(...) {
    Tempest::Log::e("unable to setup logfile - fallback to console log");
    }
  CrashLog::setup();

  zenkit::Logger::set(zenkit::LogLevel::INFO, [] (zenkit::LogLevel lvl, const char* cat, const char* message) {
    (void)cat;
    switch (lvl) {
      case zenkit::LogLevel::ERROR:
        Tempest::Log::e("[zenkit] ", message);
        break;
      case zenkit::LogLevel::WARNING:
        Tempest::Log::e("[zenkit] ", message);
        break;
      case zenkit::LogLevel::INFO:
        Tempest::Log::i("[zenkit] ", message);
        break;
      case zenkit::LogLevel::DEBUG:
      case zenkit::LogLevel::TRACE:
        Tempest::Log::d("[zenkit] ", message); // unused
        break;
      }
    });
  Dm_setLogger(DmLogLevel_INFO, [](void* ctx, DmLogLevel lvl, char const* msg) {
    switch (lvl) {
      case DmLogLevel_FATAL:
      case DmLogLevel_ERROR:
      case DmLogLevel_WARN:
        Tempest::Log::e("[dmusic] ", msg);
        break;
      case DmLogLevel_INFO:
        Tempest::Log::i("[dmusic] ", msg);
        break;
      case DmLogLevel_DEBUG:
      case DmLogLevel_TRACE:
        Tempest::Log::d("[dmusic] ", msg);
        break;
      }
    }, nullptr);

  Tempest::Log::i(appBuild);
  Workers::setThreadName("Main thread");

  try {
    AudioSession::activate();   // no-op off iOS, kept for parity with main.cpp

    // android_main has no argc/argv from the OS. Synthesize the equivalent of
    // a desktop/iOS invocation run with "-g <path>": commandline.cpp parses
    // "-g" by taking the next argv entry verbatim as `gpath`, the game-data
    // root that rootPath()/nestedPath()/validateGothicPath() all key off of
    // (validated by checking <gpath>/Data and <gpath>/_work/Data both
    // exist). Game data is staged on-device at /sdcard/OpenGothic/Gothic2/
    // (Data/, _work/, system/), matching the chdir() above one level up.
    const char* argv[] = {"opengothic", "-g", "/sdcard/OpenGothic/Gothic2"};
    int argc = 3;
    CommandLine cmd{argc,argv};

    Tempest::ApiFlags flg = cmd.isValidationMode() ? Tempest::ApiFlags::Validation : Tempest::ApiFlags::NoFlags;
    VulkanApi         api{flg};
    const auto        gpuName = selectDevice(api);
    CrashLog::setGpu(gpuName);

    Tempest::Device   device{api,gpuName};
    CrashLog::setGpu(device.properties().name);

    // TEMP Phase-1 decision gate: does this GPU sample ASTC4x4, and does it
    // really lack BC? Mali decompressing DXT->RGBA8 is what costs 1.38GB of GPU
    // memory here, and ASTC is the only fix that keeps full resolution. Drives
    // the transcoder go/no-go -- see
    // docs/superpowers/specs/2026-07-16-astc-transcoder-design.md §6.
    // Revert once Phase 1 concludes.
    // NOTE: explicit int() casts -- Tempest::Log has explicit write() overloads
    // for the integer types but none for bool, so a raw bool would rely on the
    // bool->int promotion out-ranking every other overload. Don't make the build
    // depend on that.
    Tempest::Log::i("[astcdiag] caps: DXT1=",   int(device.properties().hasSamplerFormat(Tempest::TextureFormat::DXT1)),
                    " DXT5=",                   int(device.properties().hasSamplerFormat(Tempest::TextureFormat::DXT5)),
                    " ASTC4x4=",                int(device.properties().hasSamplerFormat(Tempest::TextureFormat::ASTC4x4)));
    astcBenchmark(); // TEMP Phase-1 decision gate

    Resources         resources{device};
    Gothic            gothic;
    GameMusic         music;
    gothic.setupGlobalScripts();

    MainWindow        wx(device);

    // Task 5: wire lifecycle hardening so backgrounding/resuming the app
    // does not SIGSEGV. AndroidApi (androidapi.cpp) calls these directly
    // from its APP_CMD_TERM_WINDOW / APP_CMD_INIT_WINDOW handling; see the
    // comments on MainWindow::onSurfaceDestroyed/onSurfaceCreated and in
    // androidapi.cpp for why the destroyed callback in particular must run
    // synchronously rather than being deferred. Register before app.exec()
    // starts the render loop, and after `wx` exists so the callbacks always
    // have a valid MainWindow to call into.
    Tempest::AndroidApi::setSurfaceCallbacks(
      [&wx](){ wx.onSurfaceDestroyed(); },
      [&wx](Tempest::SystemApi::Window* w){ wx.onSurfaceCreated(w); });

    Tempest::Application app;
    app.exec();
    }
  catch(const GothicNotFoundException& e) {
    // Task 3 wires -g /sdcard/OpenGothic/Gothic2 above, so this should now
    // only fire for a genuine problem: data not copied to that path yet,
    // MANAGE_EXTERNAL_STORAGE not granted (validateGothicPath()'s Data/ and
    // _work/Data/ existence checks fail silently under denied storage
    // access), or a relative-path lookup inside CommandLine/ZenKit that
    // doesn't resolve against the absolute -g root. This is thrown from
    // CommandLine, before any Device/Window is created, so it is a clean,
    // controlled shutdown of the glue thread - not a crash-on-launch.
    Tempest::Log::e("fatal: ", e.what());
    SystemMsg::fatal("Gothic II data not found",
                     "Copy your Gothic II: NotR files (Data/, _work/, system/) "
                     "onto the device, then relaunch.");
    }
  catch(const std::exception& e) {
    // Anything else may have happened after the window was created and is
    // being torn down by stack-unwinding; just report and let the glue
    // thread exit (see main.cpp's identical comment for the desktop/iOS case).
    Tempest::Log::e("fatal: ", e.what());
    SystemMsg::fatal("Fatal error", e.what());
    }
  }
#endif
