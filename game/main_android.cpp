#if defined(__ANDROID__)
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <Tempest/Application>
#include <Tempest/VulkanApi>
#include <Tempest/Device>
#include <Tempest/Window>
#include <Tempest/Fence>
#include <Tempest/Vec>
#include "system/api/androidapi.h"   // setAndroidApp

using namespace Tempest;

struct android_app* g_androidApp = nullptr;

namespace {
// Task 1 stub: proves NDK + GameActivity + Vulkan surface + AndroidApi event
// pump all work end-to-end by clearing the screen to a solid dark red.
// Task 2 deletes this and boots the real engine (see game/main.cpp).
class ClearWindow : public Window {
  public:
    ClearWindow(Device& d):device(d),swapchain(d,hwnd()){}

    void paintEvent(PaintEvent&) override {}

    void resizeEvent(SizeEvent&) override {
      swapchain.reset();
      update();
      }

    void render() override {
      if(swapchain.w()==0 || swapchain.h()==0)
        return;
      try {
        auto cmd = device.commandBuffer();
        {
        auto enc = cmd.startEncoding(device);
        // minimal clear to dark red (#3a0000) so we can SEE first light on device
        enc.setFramebuffer({{swapchain[swapchain.currentImage()], Vec4(0x3a/255.f,0.f,0.f,1.f), Tempest::Preserve}});
        }
        Fence sync = device.submit(cmd);
        device.present(swapchain);
        sync.wait();
        }
      catch(const Tempest::SwapchainSuboptimal&) {
        swapchain.reset();
        }
      }

    Device&   device;
    Swapchain swapchain;
  };
}

extern "C" void android_main(struct android_app* app) {
  g_androidApp = app;
  Tempest::AndroidApi::setAndroidApp(app);

  static VulkanApi    api;
  static Device       device(api);
  static Application  tempestApp;
  static ClearWindow  w(device);
  tempestApp.exec();
}
#endif
