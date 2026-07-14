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

    // android_main has no argc/argv from the OS. Synthesize the same
    // "no options" argv a normal GUI process gets (just the program name),
    // so CommandLine takes the identical no-flags path main.cpp's mobile
    // targets take. Data root stays default here (Task 3 wires it to
    // /sdcard/OpenGothic): Gothic.ini/Data won't be found yet, so
    // CommandLine throws GothicNotFoundException below - that is the
    // expected, correct outcome for Task 2.
    const char* argv[] = {"opengothic"};
    CommandLine cmd{1,argv};

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
    Tempest::Application app;
    app.exec();
    }
  catch(const GothicNotFoundException& e) {
    // Expected on Task 2: game data isn't wired yet (Task 3 sets the data
    // root). This is thrown from CommandLine, before any Device/Window is
    // created, so it is a clean, controlled shutdown of the glue thread -
    // not a crash-on-launch.
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
