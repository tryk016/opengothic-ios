#if defined(__ANDROID__)
#include <game-activity/native_app_glue/android_native_app_glue.h>

#include <Tempest/Application>
#include <Tempest/Log>
#include <Tempest/VulkanApi>
#include <Tempest/Device>
#include <Tempest/File>   // Tempest::WFile (log.txt), see main.cpp's identical usage

#include <zenkit/Logger.hh>
#include <dmusic.h>

#include <cstring>
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
    // TEMP TEST (revert once memory tuning is verified): "-nomenu" auto-loads
    // Khorinis so world-load memory can be measured without the menu, whose
    // touch is unusable on portrait-native panels until the preTransform fix.
    const char* argv[] = {"opengothic", "-g", "/sdcard/OpenGothic/Gothic2", "-nomenu"};
    int argc = 4;
    CommandLine cmd{argc,argv};

    Tempest::ApiFlags flg = cmd.isValidationMode() ? Tempest::ApiFlags::Validation : Tempest::ApiFlags::NoFlags;
    VulkanApi         api{flg};
    const auto        gpuName = selectDevice(api);
    CrashLog::setGpu(gpuName);

    Tempest::Device   device{api,gpuName};
    CrashLog::setGpu(device.properties().name);

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
