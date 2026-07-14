#!/usr/bin/env bash
set -euo pipefail

# Applies local Android support to the Tempest submodule, which is fetched
# fresh from upstream and must stay untouched in git (never `git add` inside
# lib/Tempest). Idempotent and CRLF-tolerant, mirroring ios/patches/apply-patches.sh.
# Fails loudly if a patch does not apply, so CI never silently produces a
# broken binary.

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

SYS="$ROOT/lib/Tempest/Engine/system/systemapi.cpp"
VK="$ROOT/lib/Tempest/Engine/gapi/vulkanapi.cpp"
SW="$ROOT/lib/Tempest/Engine/gapi/vulkan/vswapchain.cpp"
CM="$ROOT/lib/Tempest/Engine/CMakeLists.txt"
API_H="$ROOT/lib/Tempest/Engine/system/api/androidapi.h"
API_CPP="$ROOT/lib/Tempest/Engine/system/api/androidapi.cpp"

for f in "$SYS" "$VK" "$SW" "$CM"; do
  if [ ! -f "$f" ]; then
    echo "ERROR: not found: $f" >&2
    exit 1
  fi
done

# --------------------------------------------------------------------------
# (a) systemapi.cpp — #include the new backend header, and dispatch to it
#     from SystemApi::inst() on __ANDROID__.
# --------------------------------------------------------------------------

if grep -q '#include "api/androidapi.h"' "$SYS"; then
  echo "skip: systemapi.cpp androidapi.h include (already patched)"
else
  perl -0777 -pi -e \
    's/(#include "api\/iosapi\.h"\r?\n)/${1}#if defined(__ANDROID__)\n#include "api\/androidapi.h"\n#endif\n/' \
    "$SYS"
  if grep -q '#include "api/androidapi.h"' "$SYS"; then
    echo "patched: systemapi.cpp androidapi.h include"
  else
    echo "ERROR: failed to patch systemapi.cpp include (pattern not found)" >&2
    exit 1
  fi
fi

if grep -q 'static AndroidApi api;' "$SYS"; then
  echo "skip: systemapi.cpp AndroidApi dispatch (already patched)"
else
  perl -0777 -pi -e \
    's/(SystemApi& SystemApi::inst\(\) \{\r?\n)(\s*)#ifdef __WINDOWS__/${1}${2}#if defined(__ANDROID__)\n${2}static AndroidApi api;\n${2}#elif defined(__WINDOWS__)/' \
    "$SYS"
  if grep -q 'static AndroidApi api;' "$SYS"; then
    echo "patched: systemapi.cpp AndroidApi dispatch"
  else
    echo "ERROR: failed to patch systemapi.cpp inst() dispatch (pattern not found)" >&2
    exit 1
  fi
fi

# --------------------------------------------------------------------------
# (b) vulkanapi.cpp — VK_KHR_android_surface instance extension.
# --------------------------------------------------------------------------

if grep -q 'VK_KHR_ANDROID_SURFACE_EXTENSION_NAME' "$VK"; then
  echo "skip: vulkanapi.cpp android surface extension (already patched)"
else
  perl -0777 -pi -e \
    's/(#define VK_KHR_XLIB_SURFACE_EXTENSION_NAME  "VK_KHR_xlib_surface"\r?\n)/${1}#define VK_KHR_ANDROID_SURFACE_EXTENSION_NAME "VK_KHR_android_surface"\n/' \
    "$VK"
  perl -0777 -pi -e \
    's/(#define SURFACE_EXTENSION_NAME VK_KHR_XLIB_SURFACE_EXTENSION_NAME\r?\n)#endif/${1}#elif defined(__ANDROID__)\n#define SURFACE_EXTENSION_NAME VK_KHR_ANDROID_SURFACE_EXTENSION_NAME\n#endif/' \
    "$VK"
  if grep -q 'VK_KHR_ANDROID_SURFACE_EXTENSION_NAME' "$VK"; then
    echo "patched: vulkanapi.cpp android surface extension"
  else
    echo "ERROR: failed to patch vulkanapi.cpp surface extension (pattern not found)" >&2
    exit 1
  fi
fi

# --------------------------------------------------------------------------
# (c) vswapchain.cpp — VK_USE_PLATFORM_ANDROID_KHR include block,
#     createSurface(), and checkPresentSupport() branches.
# --------------------------------------------------------------------------

if grep -q 'VK_USE_PLATFORM_ANDROID_KHR' "$SW"; then
  echo "skip: vswapchain.cpp android WSI include (already patched)"
else
  perl -0777 -pi -e \
    's/(#  include <vulkan\/vulkan_xlib\.h>\r?\n#  undef Always\r?\n#  undef None\r?\n)#else/${1}#elif defined(__ANDROID__)\n#  define VK_USE_PLATFORM_ANDROID_KHR\n#  include <android\/native_window.h>\n#  include <vulkan\/vulkan_android.h>\n#else/' \
    "$SW"
  if grep -q 'VK_USE_PLATFORM_ANDROID_KHR' "$SW"; then
    echo "patched: vswapchain.cpp android WSI include"
  else
    echo "ERROR: failed to patch vswapchain.cpp WSI include block (pattern not found)" >&2
    exit 1
  fi
fi

if grep -q 'all queues that support graphics support present' "$SW"; then
  echo "skip: vswapchain.cpp checkPresentSupport (already patched)"
else
  perl -0777 -pi -e \
    's/(presentSupport = vkGetPhysicalDeviceXlibPresentationSupportKHR\(device,queueFamilyIndex,dpy,visualId\)!=VK_FALSE;\r?\n\s*\}\r?\n)#else/${1}#elif defined(__ANDROID__)\n  const bool presentSupport = true; \/\/ Android: all queues that support graphics support present to a surface\n#else/' \
    "$SW"
  if grep -q 'all queues that support graphics support present' "$SW"; then
    echo "patched: vswapchain.cpp checkPresentSupport"
  else
    echo "ERROR: failed to patch vswapchain.cpp checkPresentSupport (pattern not found)" >&2
    exit 1
  fi
fi

if grep -q 'VkAndroidSurfaceCreateInfoKHR' "$SW"; then
  echo "skip: vswapchain.cpp createSurface (already patched)"
else
  perl -0777 -pi -e \
    's/(if\(vkCreateXlibSurfaceKHR\(instance, &createInfo, nullptr, &ret\)!=VK_SUCCESS\)\r?\n\s*throw std::system_error\(Tempest::GraphicsErrc::NoDevice\);\r?\n)#else/${1}#elif defined(__ANDROID__)\n  VkAndroidSurfaceCreateInfoKHR createInfo = {};\n  createInfo.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;\n  createInfo.window = reinterpret_cast<ANativeWindow*>(hwnd);\n  if(vkCreateAndroidSurfaceKHR(instance, &createInfo, nullptr, &ret)!=VK_SUCCESS)\n    throw std::system_error(Tempest::GraphicsErrc::NoDevice);\n#else/' \
    "$SW"
  if grep -q 'VkAndroidSurfaceCreateInfoKHR' "$SW"; then
    echo "patched: vswapchain.cpp createSurface"
  else
    echo "ERROR: failed to patch vswapchain.cpp createSurface (pattern not found)" >&2
    exit 1
  fi
fi

# --------------------------------------------------------------------------
# (d) Tempest CMakeLists.txt — Android Vulkan lib + game-activity link.
# --------------------------------------------------------------------------

if grep -q 'find_library(VULKAN_LIB vulkan)' "$CM"; then
  echo "skip: Tempest CMakeLists.txt android vulkan lib (already patched)"
else
  perl -0777 -pi -e \
    's/(  if\("\$\{CMAKE_SIZEOF_VOID_P\}" EQUAL "8"\)\r?\n    target_link_directories\(\$\{PROJECT_NAME\} PRIVATE "\$ENV\{VULKAN_SDK\}\/lib"\)\r?\n  else\(\)\r?\n    target_link_directories\(\$\{PROJECT_NAME\} PRIVATE "\$ENV\{VULKAN_SDK\}\/Lib32"\)\r?\n  endif\(\)\r?\n\r?\n  target_include_directories\(\$\{PROJECT_NAME\} PRIVATE "\$ENV\{VULKAN_SDK\}\/include"\)\r?\n  if\(WIN32\)\r?\n    target_link_libraries\(\$\{PROJECT_NAME\} PRIVATE vulkan-1\)\r?\n  else\(\)\r?\n    target_link_libraries\(\$\{PROJECT_NAME\} PRIVATE vulkan\)\r?\n  endif\(\)\r?\n)/  if(NOT ANDROID)\n    if(\"\${CMAKE_SIZEOF_VOID_P}\" EQUAL \"8\")\n      target_link_directories(\${PROJECT_NAME} PRIVATE \"\$ENV{VULKAN_SDK}\/lib\")\n    else()\n      target_link_directories(\${PROJECT_NAME} PRIVATE \"\$ENV{VULKAN_SDK}\/Lib32\")\n    endif()\n\n    target_include_directories(\${PROJECT_NAME} PRIVATE \"\$ENV{VULKAN_SDK}\/include\")\n  endif()\n\n  if(ANDROID)\n    find_library(VULKAN_LIB vulkan)\n    target_link_libraries(\${PROJECT_NAME} PRIVATE \${VULKAN_LIB})\n  elseif(WIN32)\n    target_link_libraries(\${PROJECT_NAME} PRIVATE vulkan-1)\n  else()\n    target_link_libraries(\${PROJECT_NAME} PRIVATE vulkan)\n  endif()\n/' \
    "$CM"
  if grep -q 'find_library(VULKAN_LIB vulkan)' "$CM"; then
    echo "patched: Tempest CMakeLists.txt android vulkan lib"
  else
    echo "ERROR: failed to patch Tempest CMakeLists.txt vulkan block (pattern not found)" >&2
    exit 1
  fi
fi

if grep -q 'game-activity::game-activity_static android log' "$CM"; then
  echo "skip: Tempest CMakeLists.txt android link section (already patched)"
else
  perl -0777 -pi -e \
    's/(target_link_libraries\(\$\{PROJECT_NAME\} PRIVATE "-framework AppKit" "-framework QuartzCore" "-framework Metal"\)\r?\n)elseif\(UNIX\)/${1}elseif(ANDROID)\n  find_package(game-activity REQUIRED CONFIG)\n  target_link_libraries(\${PROJECT_NAME} PRIVATE game-activity::game-activity_static android log)\nelseif(UNIX)/' \
    "$CM"
  if grep -q 'game-activity::game-activity_static android log' "$CM"; then
    echo "patched: Tempest CMakeLists.txt android link section"
  else
    echo "ERROR: failed to patch Tempest CMakeLists.txt link section (pattern not found)" >&2
    exit 1
  fi
fi

# --------------------------------------------------------------------------
# (e) New files: the AndroidApi backend itself. These are wholly-owned by
#     this script (not upstream files being edited), so they are rewritten
#     unconditionally each run -- that is idempotent by construction.
# --------------------------------------------------------------------------

cat > "$API_H" <<'EOF'
#pragma once
#include <Tempest/SystemApi>
struct android_app;
struct ANativeWindow;

namespace Tempest {
class AndroidApi final: SystemApi {
  public:
    using SystemApi::dispatchRender;
    static void  setAndroidApp(android_app* app);
  private:
    AndroidApi();
    Window*  implCreateWindow(Tempest::Window* owner, uint32_t width, uint32_t height) override;
    Window*  implCreateWindow(Tempest::Window* owner, ShowMode sm) override;
    void     implDestroyWindow(Window* w) override;
    void     implExit() override;
    Rect     implWindowClientRect(SystemApi::Window* w) override;
    bool     implSetAsFullscreen(SystemApi::Window* w, bool fullScreen) override;
    bool     implIsFullscreen(SystemApi::Window* w) override;
    void     implSetCursorPosition(SystemApi::Window* w, int x, int y) override;
    void     implShowCursor(SystemApi::Window* w, CursorShape cursor) override;
    bool     implIsRunning() override;
    int      implExec(AppCallBack& cb) override;
    void     implProcessEvents(AppCallBack& cb) override;
    void     implSetWindowTitle(SystemApi::Window* w, const char* utf8) override;
  friend class SystemApi;
  };
}
EOF
echo "wrote: lib/Tempest/Engine/system/api/androidapi.h"

cat > "$API_CPP" <<'EOF'
#include "androidapi.h"

#include <Tempest/Platform>
#include <Tempest/Log>

#if defined(__ANDROID__)

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <android/native_window.h>
#include <android/looper.h>
#include <android/input.h>

#include <atomic>
#include <thread>

#include <Tempest/Window>
#include <Tempest/Event>

using namespace Tempest;

namespace {
android_app*        g_app     = nullptr;
Tempest::Window*    g_owner   = nullptr;
std::atomic_bool    g_running {true};
std::atomic_bool    g_windowChanged {false};

// Runs on the glue thread, synchronously, from inside ALooper_pollAll() (via
// android_poll_source::process). Keep this to bookkeeping only -- the actual
// SystemApi:: dispatch happens from AndroidApi::implProcessEvents (a member
// function, so it has access to the protected dispatch* statics), mirroring
// how iosapi.mm defers Resize dispatch out of the UIKit callback and into
// implProcessEvents.
void handleAppCmd(android_app* app, int32_t cmd) {
  (void)app;
  switch(cmd) {
    case APP_CMD_INIT_WINDOW:
    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_TERM_WINDOW:
      g_windowChanged.store(true);
      break;
    case APP_CMD_DESTROY:
      g_running.store(false);
      break;
    default:
      break;
    }
  }
}

void AndroidApi::setAndroidApp(android_app* app) {
  g_app = app;
  if(g_app!=nullptr)
    g_app->onAppCmd = &handleAppCmd;
  }

AndroidApi::AndroidApi() {
  // Block-pump the glue's event loop until the first ANativeWindow arrives.
  // VSwapchain captures SystemApi::Window* (== ANativeWindow*) once, at
  // construction time (see vswapchain.cpp createSurface), so the window must
  // already exist before Tempest::Window (and therefore Swapchain) is
  // constructed in main_android.cpp. android_main owns this thread outright
  // (unlike iOS, which has to hijack UIKit's run loop via a fiber trick), so
  // blocking here is safe and simple.
  if(g_app==nullptr)
    return;
  while(g_app->window==nullptr && g_app->destroyRequested==0) {
    int                   events = 0;
    android_poll_source*  source = nullptr;
    int id = ALooper_pollAll(-1, nullptr, &events, reinterpret_cast<void**>(&source));
    if(id>=0 && source!=nullptr)
      source->process(g_app, source);
    }
  }

SystemApi::Window* AndroidApi::implCreateWindow(Tempest::Window* owner, uint32_t /*width*/, uint32_t /*height*/) {
  g_owner = owner;
  return reinterpret_cast<SystemApi::Window*>(g_app!=nullptr ? g_app->window : nullptr);
  }

SystemApi::Window* AndroidApi::implCreateWindow(Tempest::Window* owner, SystemApi::ShowMode /*sm*/) {
  g_owner = owner;
  return reinterpret_cast<SystemApi::Window*>(g_app!=nullptr ? g_app->window : nullptr);
  }

void AndroidApi::implDestroyWindow(SystemApi::Window* /*w*/) {
  g_owner = nullptr;
  }

void AndroidApi::implExit() {
  g_running.store(false);
  if(g_app!=nullptr)
    g_app->destroyRequested = 1;
  }

Tempest::Rect AndroidApi::implWindowClientRect(SystemApi::Window* w) {
  auto wnd = reinterpret_cast<ANativeWindow*>(w);
  if(wnd==nullptr)
    return Rect(0,0,0,0);
  return Rect(0, 0, ANativeWindow_getWidth(wnd), ANativeWindow_getHeight(wnd));
  }

bool AndroidApi::implSetAsFullscreen(SystemApi::Window* /*w*/, bool /*fullScreen*/) {
  return true; // the Activity theme is already fullscreen/edge-to-edge
  }

bool AndroidApi::implIsFullscreen(SystemApi::Window* /*w*/) {
  return true;
  }

void AndroidApi::implSetCursorPosition(SystemApi::Window* /*w*/, int /*x*/, int /*y*/) {
  }

void AndroidApi::implShowCursor(SystemApi::Window* /*w*/, CursorShape /*cursor*/) {
  }

bool AndroidApi::implIsRunning() {
  return g_running.load();
  }

int AndroidApi::implExec(AppCallBack& cb) {
  while(implIsRunning()) {
    implProcessEvents(cb);
    if(!cb.onTimer())
      std::this_thread::yield();
    }
  return 0;
  }

void AndroidApi::implProcessEvents(AppCallBack& /*cb*/) {
  if(g_app==nullptr)
    return;

  // Drain the glue's command queue non-blocking once a window+owner exist,
  // otherwise block until something happens (there is nothing to render yet).
  int                  events  = 0;
  android_poll_source* source  = nullptr;
  int timeoutMillis = (g_owner!=nullptr && g_app->window!=nullptr) ? 0 : -1;
  int id = ALooper_pollAll(timeoutMillis, nullptr, &events, reinterpret_cast<void**>(&source));
  while(id>=0) {
    if(source!=nullptr)
      source->process(g_app, source);
    if(g_app->destroyRequested!=0) {
      g_running.store(false);
      return;
      }
    id = ALooper_pollAll(0, nullptr, &events, reinterpret_cast<void**>(&source));
    }

  if(g_windowChanged.exchange(false) && g_owner!=nullptr && g_app->window!=nullptr) {
    SizeEvent e(ANativeWindow_getWidth(g_app->window), ANativeWindow_getHeight(g_app->window));
    dispatchResize(*g_owner, e);
    }

  // Drain per-frame touch input and forward it as mouse events, mirroring
  // how iosapi.mm maps UITouch -> dispatchMouse* (touchesBegan/Moved/Ended
  // each synthesize a MouseEvent at the touch position and dispatch it
  // directly -- there is no separate "hover" move sent before a down, so we
  // don't synthesize one here either). Coordinates from
  // GameActivityPointerAxes are already in surface pixels -- the same space
  // as ANativeWindow_getWidth/Height -- so unlike iOS (which multiplies
  // touch points by contentScaleFactor) no extra DPI scaling is needed here.
  android_input_buffer* ib = android_app_swap_input_buffers(g_app);
  if(ib!=nullptr) {
    for(uint64_t i=0; i<ib->motionEventsCount; ++i) {
      const GameActivityMotionEvent& m = ib->motionEvents[i];
      if(m.pointerCount==0 || g_owner==nullptr)
        continue;

      const int32_t action = m.action & AMOTION_EVENT_ACTION_MASK;
      Event::Type   type    = Event::NoEvent;
      if(action==AMOTION_EVENT_ACTION_DOWN)
        type = Event::MouseDown;
      else if(action==AMOTION_EVENT_ACTION_UP || action==AMOTION_EVENT_ACTION_CANCEL)
        type = Event::MouseUp; // cancelled gesture -> release, same as iosapi.mm's touchesCancelled -> touchesEnded
      else if(action==AMOTION_EVENT_ACTION_MOVE)
        type = Event::MouseMove;
      else
        continue;

      const float x   = GameActivityPointerAxes_getX(&m.pointers[0]);
      const float y   = GameActivityPointerAxes_getY(&m.pointers[0]);
      const int   pid = m.pointers[0].id; // touch-point id, not to be confused with the outer ALooper `id`

      MouseEvent e(int(x), int(y), Event::ButtonLeft, Event::M_NoModifier, 0, pid, type);
      if(type==Event::MouseDown)
        dispatchMouseDown(*g_owner, e);
      else if(type==Event::MouseUp)
        dispatchMouseUp(*g_owner, e);
      else if(type==Event::MouseMove)
        dispatchMouseMove(*g_owner, e);
      }
    android_app_clear_motion_events(ib);
    }

  if(g_owner!=nullptr && g_app->window!=nullptr)
    dispatchRender(*g_owner);
  }

void AndroidApi::implSetWindowTitle(SystemApi::Window* /*w*/, const char* /*utf8*/) {
  }

#endif
EOF
echo "wrote: lib/Tempest/Engine/system/api/androidapi.cpp"

echo "apply-patches.sh: done"
