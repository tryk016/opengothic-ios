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
AGA="$ROOT/lib/Tempest/Engine/gapi/abstractgraphicsapi.h"
PM="$ROOT/lib/Tempest/Engine/formats/pixmap.cpp"
VD="$ROOT/lib/Tempest/Engine/gapi/vulkan/vdevice.h"
API_H="$ROOT/lib/Tempest/Engine/system/api/androidapi.h"
API_CPP="$ROOT/lib/Tempest/Engine/system/api/androidapi.cpp"

for f in "$SYS" "$VK" "$SW" "$CM" "$AGA" "$PM" "$VD"; do
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
  echo "skip: vulkanapi.cpp android surface extension macro (already patched)"
else
  perl -0777 -pi -e \
    's/(#define VK_KHR_XLIB_SURFACE_EXTENSION_NAME  "VK_KHR_xlib_surface"\r?\n)/${1}#define VK_KHR_ANDROID_SURFACE_EXTENSION_NAME "VK_KHR_android_surface"\n/' \
    "$VK"
  if grep -q 'VK_KHR_ANDROID_SURFACE_EXTENSION_NAME' "$VK"; then
    echo "patched: vulkanapi.cpp android surface extension macro"
  else
    echo "ERROR: failed to patch vulkanapi.cpp surface extension macro (pattern not found)" >&2
    exit 1
  fi
fi

if grep -q '#define SURFACE_EXTENSION_NAME VK_KHR_ANDROID_SURFACE_EXTENSION_NAME' "$VK"; then
  echo "skip: vulkanapi.cpp android SURFACE_EXTENSION_NAME dispatch (already patched)"
else
  perl -0777 -pi -e \
    's/(#define SURFACE_EXTENSION_NAME VK_KHR_XLIB_SURFACE_EXTENSION_NAME\r?\n)#endif/${1}#elif defined(__ANDROID__)\n#define SURFACE_EXTENSION_NAME VK_KHR_ANDROID_SURFACE_EXTENSION_NAME\n#endif/' \
    "$VK"
  if grep -q '#define SURFACE_EXTENSION_NAME VK_KHR_ANDROID_SURFACE_EXTENSION_NAME' "$VK"; then
    echo "patched: vulkanapi.cpp android SURFACE_EXTENSION_NAME dispatch"
  else
    echo "ERROR: failed to patch vulkanapi.cpp SURFACE_EXTENSION_NAME dispatch (pattern not found)" >&2
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
# (c3) vswapchain.cpp — Android orientation fix.
#
# Measured on the target (Mali-G57, portrait-native 800x1340 panel, app locked
# to landscape):
#   currentTransform=2 (ROTATE_90)   supportedTransforms=511 (IDENTITY available)
#   currentExtent=1340x800 (LANDSCAPE)   rectIn=1340x800   chosenExtent=1340x800
#   minImageExtent=1x1   maxImageExtent=4096x4096  (extent is NOT fixed)
#
# So the surface, the swapchain extent, the UI layout and the touch coordinates
# are ALL already in one consistent landscape space; the engine renders a
# correct landscape frame. The only thing wrong is the DECLARATION:
# preTransform=currentTransform tells the presentation engine "the app already
# pre-rotated this image by 90". It did not -- so the compositor skips its own
# rotation and the landscape frame is presented sideways on the portrait panel.
#
# Fix: declare IDENTITY and let the compositor rotate. Costs one composition
# pass, but needs NO projection/viewport/UI/touch remapping (the alternative,
# true app-side pre-rotation, would). Touch then lines up for free, because it
# already matches the un-rotated layout.
# --------------------------------------------------------------------------

if grep -q 'VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR' "$SW"; then
  echo "skip: vswapchain.cpp preTransform identity (already patched)"
else
  perl -0777 -pi -e \
    's/(  createInfo\.preTransform   = swapChainSupport\.capabilities\.currentTransform;\r?\n)/#if defined(__ANDROID__)\n  \/\/ Portrait-native panel => currentTransform is ROTATE_90 while the window is\n  \/\/ landscape. We do NOT pre-rotate our rendering, so declaring currentTransform\n  \/\/ here would make the compositor skip its rotation and present the frame\n  \/\/ sideways. Declare IDENTITY instead and let the compositor rotate.\n  \/\/ NOTE: this deliberately makes preTransform!=currentTransform, so every\n  \/\/ acquire\/present is flagged VK_SUBOPTIMAL_KHR -- handled below.\n  createInfo.preTransform   = (swapChainSupport.capabilities.supportedTransforms \& VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)!=0\n                                ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR\n                                : swapChainSupport.capabilities.currentTransform;\n#else\n${1}#endif\n/' \
    "$SW"
  if grep -q 'VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR' "$SW"; then
    echo "patched: vswapchain.cpp preTransform identity"
  else
    echo "ERROR: failed to patch vswapchain.cpp preTransform (pattern not found)" >&2
    exit 1
  fi
fi

if grep -q 'android-suboptimal-acquire' "$SW"; then
  echo "skip: vswapchain.cpp suboptimal acquire (already patched)"
else
  perl -0777 -pi -e \
    's/(void VSwapchain::acquireNextImage\(\) \{\r?\n  VkResult code = implAcquireNextImage\(\);\r?\n)/${1}\n#if defined(__ANDROID__)\n  \/\/ android-suboptimal-acquire: we create the swapchain with preTransform=IDENTITY\n  \/\/ while the surface reports ROTATE_90 (see createSwapchain), so the driver flags\n  \/\/ every acquire SUBOPTIMAL -- permanently, and by our own choice. Recreating on it\n  \/\/ would rebuild an identically-suboptimal swapchain forever (a hang). Real surface\n  \/\/ changes still arrive as VK_ERROR_OUT_OF_DATE_KHR, and window init\/term\/resize is\n  \/\/ driven explicitly by AndroidApi, so SUBOPTIMAL carries no actionable signal here.\n  if(code==VK_SUBOPTIMAL_KHR)\n    code = VK_SUCCESS;\n#endif\n/' \
    "$SW"
  if grep -q 'android-suboptimal-acquire' "$SW"; then
    echo "patched: vswapchain.cpp suboptimal acquire"
  else
    echo "ERROR: failed to patch vswapchain.cpp acquireNextImage (pattern not found)" >&2
    exit 1
  fi
fi

if grep -q 'android-suboptimal-present' "$SW"; then
  echo "skip: vswapchain.cpp suboptimal present (already patched)"
else
  perl -0777 -pi -e \
    's/(  VkResult code = device\.presentQueue->present\(presentInfo\);\r?\n)/${1}#if defined(__ANDROID__)\n  \/\/ android-suboptimal-present: see android-suboptimal-acquire above -- the\n  \/\/ preTransform=IDENTITY mismatch is deliberate and permanent.\n  if(code==VK_SUBOPTIMAL_KHR)\n    code = VK_SUCCESS;\n#endif\n/' \
    "$SW"
  if grep -q 'android-suboptimal-present' "$SW"; then
    echo "patched: vswapchain.cpp suboptimal present"
  else
    echo "ERROR: failed to patch vswapchain.cpp present (pattern not found)" >&2
    exit 1
  fi
fi

# --------------------------------------------------------------------------
# (f) ASTC4x4 texture format support.
#
# Mali (and every Apple GPU) lacks BC/S3TC, so device.cpp:199 decompresses every
# DXT texture to RGBA8 -- measured 1.38GB GPU on a 3.5GB device, vs 69MB on an
# Adreno that samples DXT natively. ASTC keeps textures compressed at FULL
# resolution (4x smaller than RGBA8), which a mip-cap fundamentally cannot do:
# measured cap=512 saves only 60MB (Gothic2 textures are mostly <=512px) while
# cap=256 saves 500MB but is visibly blocky.
#
# ASTC 4x4 specifically, because it is 16 bytes per 4x4 block -- exactly the
# shape pixmap.cpp and metal/mttexture.cpp already hardcode for DXT3/DXT5 -- so
# NO block-math generalization is needed. 6x6 would compress better but would
# require surgery in the Vulkan AND Metal backends.
#
# The enum entry MUST go last (after RGBA16F, before Last): pixmap.cpp:68 does
# kfrm[uint8_t(frm)-uint8_t(DXT1)] positional arithmetic, which inserting a
# value between DXT1..DXT5 would corrupt.
#
# Nothing more is needed for capability detection: vdevice.cpp:678 probes
# formats generically (for i<Last: nativeFormat(i) -> vkGetPhysicalDeviceFormatProperties,
# and isCompressedFormat(i) -> smpFormat |= 1ull<<i), so hasSamplerFormat(ASTC4x4)
# starts reporting the real driver answer as soon as these patches land.
#
# See docs/superpowers/specs/2026-07-16-astc-transcoder-design.md
# --------------------------------------------------------------------------

if grep -q 'ASTC4x4' "$AGA"; then
  echo "skip: abstractgraphicsapi.h ASTC4x4 (already patched)"
else
  perl -0777 -pi -e 's/(    RGBA16F,\r?\n)(    Last)/${1}    ASTC4x4,\n${2}/' "$AGA"
  perl -0777 -pi -e 's/(      case RGBA16F:     return "RGBA16F";\r?\n)(      case Last:)/${1}      case ASTC4x4:     return "ASTC4x4";\n${2}/' "$AGA"
  perl -0777 -pi -e 's/(    return f==TextureFormat::DXT1 \|\| f==TextureFormat::DXT3 \|\| f==TextureFormat::DXT5;)/    return f==TextureFormat::DXT1 || f==TextureFormat::DXT3 || f==TextureFormat::DXT5 || f==TextureFormat::ASTC4x4;/' "$AGA"
  if [ "$(grep -c 'ASTC4x4' "$AGA")" = "3" ]; then
    echo "patched: abstractgraphicsapi.h ASTC4x4 (enum + formatName + isCompressedFormat)"
  else
    echo "ERROR: failed to patch abstractgraphicsapi.h ASTC4x4 (expected 3 hits, got $(grep -c 'ASTC4x4' "$AGA"))" >&2
    exit 1
  fi
fi

if grep -q 'ASTC4x4' "$PM"; then
  echo "skip: pixmap.cpp ASTC4x4 (already patched)"
else
  perl -0777 -pi -e 's/(    return frm==TextureFormat::DXT1 \|\|\r?\n           frm==TextureFormat::DXT3 \|\|\r?\n           frm==TextureFormat::DXT5;)/    return frm==TextureFormat::DXT1 ||\n           frm==TextureFormat::DXT3 ||\n           frm==TextureFormat::DXT5 ||\n           frm==TextureFormat::ASTC4x4;/' "$PM"
  perl -0777 -pi -e 's/(    case TextureFormat::DXT5:        return 16;\r?\n)/${1}    case TextureFormat::ASTC4x4:     return 16;\n/' "$PM"
  perl -0777 -pi -e 's/(    case TextureFormat::DXT5:        return 4;\r?\n)/${1}    case TextureFormat::ASTC4x4:     return 4;\n/' "$PM"
  if [ "$(grep -c 'ASTC4x4' "$PM")" = "3" ]; then
    echo "patched: pixmap.cpp ASTC4x4 (isCompressed + blockSize=16 + componentCount=4)"
  else
    echo "ERROR: failed to patch pixmap.cpp ASTC4x4 (expected 3 hits, got $(grep -c 'ASTC4x4' "$PM"))" >&2
    exit 1
  fi
fi

if grep -q 'VK_FORMAT_ASTC_4x4_UNORM_BLOCK' "$VD"; then
  echo "skip: vdevice.h ASTC4x4 (already patched)"
else
  perl -0777 -pi -e 's/(    case TextureFormat::RGBA16F:\r?\n      return VK_FORMAT_R16G16B16A16_SFLOAT;\r?\n)(    \})/${1}    case TextureFormat::ASTC4x4:\n      return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;\n${2}/' "$VD"
  if grep -q 'VK_FORMAT_ASTC_4x4_UNORM_BLOCK' "$VD"; then
    echo "patched: vdevice.h ASTC4x4 -> VK_FORMAT_ASTC_4x4_UNORM_BLOCK"
  else
    echo "ERROR: failed to patch vdevice.h ASTC4x4 (pattern not found)" >&2
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
#include <functional>
struct android_app;
struct ANativeWindow;

namespace Tempest {
class AndroidApi final: SystemApi {
  public:
    using SystemApi::dispatchRender;
    static void  setAndroidApp(android_app* app);

    // Lifecycle hook, registered once by main_android.cpp after the initial
    // Device/Swapchain exist. onSurfaceDestroyed runs synchronously from the
    // APP_CMD_TERM_WINDOW handler (before the ANativeWindow is released --
    // see the comment at the call site in androidapi.cpp for why this must
    // not be deferred). onSurfaceCreated runs when a *different* window
    // shows up (resume after backgrounding), and is handed that new window
    // handle so the caller can rebuild its Swapchain (surface included)
    // against it -- Swapchain::reset() alone only rebuilds the images
    // against the OLD, now-dead surface (see vswapchain.cpp).
    static void  setSurfaceCallbacks(std::function<void()> onSurfaceDestroyed,
                                      std::function<void(SystemApi::Window*)> onSurfaceCreated);
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
#include <exception>

#include <Tempest/Window>
#include <Tempest/Event>

using namespace Tempest;

namespace {
android_app*        g_app     = nullptr;
Tempest::Window*    g_owner   = nullptr;
std::atomic_bool    g_running {true};
std::atomic_bool    g_windowChanged {false};

// Set (from handleAppCmd) when APP_CMD_TERM_WINDOW tears down a surface, and
// consumed (cleared) on the next window-changed tick in implProcessEvents.
// Needed because pointer identity alone is an ABA hazard: if the OS hands
// back a new ANativeWindow allocated at the same address as the one that was
// just destroyed, `g_app->window!=g_lastWindow` below would be false and the
// cheap resize path would run against a Swapchain/VkSurfaceKHR still built on
// the dead surface -- reintroducing the SIGSEGV the surface-recreate path
// exists to fix. Forcing the full recreate whenever a teardown happened,
// regardless of pointer equality, closes that hole.
std::atomic_bool    g_wasTerminated {false};

// The ANativeWindow* that the caller's Swapchain/VkSurfaceKHR was (or is
// about to be) built against. Compared on each window-changed tick so a
// same-window resize keeps using the cheap Swapchain::reset() path while an
// actually-different window (background/resume) gets a full surface
// rebuild via g_onSurfaceCreated. Set from setSurfaceCallbacks() and updated
// whenever g_onSurfaceCreated is invoked.
ANativeWindow*      g_lastWindow = nullptr;

std::function<void()>                   g_onSurfaceDestroyed;
std::function<void(SystemApi::Window*)> g_onSurfaceCreated;

// Runs on the glue thread, synchronously, from inside ALooper_pollAll() (via
// android_poll_source::process). Keep this to bookkeeping only -- the actual
// SystemApi:: dispatch happens from AndroidApi::implProcessEvents (a member
// function, so it has access to the protected dispatch* statics), mirroring
// how iosapi.mm defers Resize dispatch out of the UIKit callback and into
// implProcessEvents.
//
// APP_CMD_TERM_WINDOW is the one exception to "defer to implProcessEvents":
// android_native_app_glue's post-exec for this command nulls android_app::window
// and broadcasts a condvar *after* this callback returns -- and the platform's
// onNativeWindowDestroyed (running on the UI thread) blocks on that condvar
// before it actually releases the ANativeWindow. That makes this handler,
// returning, the exact synchronization point gating the real teardown: any
// GPU work still referencing the surface MUST be drained before we return
// here, or the OS can free/disconnect the window's buffer queue while it is
// still in use (this is what "Surface: freeAllBuffers: N buffers were freed
// while being dequeued" in logcat is reporting), which later SIGSEGVs the
// next time the stale surface is touched. So g_onSurfaceDestroyed runs
// synchronously, right here, not via the g_windowChanged flag.
void handleAppCmd(android_app* app, int32_t cmd) {
  (void)app;
  switch(cmd) {
    case APP_CMD_INIT_WINDOW:
    case APP_CMD_WINDOW_RESIZED:
      g_windowChanged.store(true);
      break;
    case APP_CMD_TERM_WINDOW:
      if(g_onSurfaceDestroyed) {
        try {
          g_onSurfaceDestroyed();
          }
        catch(const std::exception& e) {
          Log::e("AndroidApi: onSurfaceDestroyed: ", e.what());
          }
        catch(...) {
          Log::e("AndroidApi: onSurfaceDestroyed: unknown exception");
          }
        }
      g_wasTerminated.store(true); // force a full recreate on the next window-init tick, even if
                                   // the OS reuses this ANativeWindow's address (ABA hazard) --
                                   // see g_wasTerminated's declaration comment above
      g_windowChanged.store(false); // handled synchronously above; nothing left for this transition
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

void AndroidApi::setSurfaceCallbacks(std::function<void()> onSurfaceDestroyed,
                                      std::function<void(SystemApi::Window*)> onSurfaceCreated) {
  g_onSurfaceDestroyed = std::move(onSurfaceDestroyed);
  g_onSurfaceCreated   = std::move(onSurfaceCreated);
  // The window the caller already built its initial Swapchain against is the
  // baseline: only a LATER, different ANativeWindow* should be treated as a
  // "surface changed" event. Also drop any pending g_windowChanged left over
  // from bootstrap (the AndroidApi ctor pumps the very first INIT_WINDOW
  // before this registration can happen), so the first implProcessEvents
  // tick after registration doesn't immediately (and redundantly) re-fire a
  // recreate against the same window it was just built with.
  g_lastWindow = (g_app!=nullptr) ? g_app->window : nullptr;
  g_windowChanged.store(false);
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
    // Consume g_wasTerminated exactly once per tick, unconditionally -- if
    // this were instead inlined as `g_app->window!=g_lastWindow ||
    // g_wasTerminated.exchange(false)`, short-circuit evaluation would skip
    // the exchange whenever the pointer already differed, leaving the flag
    // set for a later, genuinely-same-window resize tick to misfire against.
    const bool wasTerminated = g_wasTerminated.exchange(false);
    if(g_app->window!=g_lastWindow || wasTerminated) {
      // The ANativeWindow itself changed (first bind, or a resume after
      // backgrounding), or the previous surface was just torn down via
      // APP_CMD_TERM_WINDOW (wasTerminated) -- treat the latter as a genuine
      // change too, even if the OS reused the same address for the new
      // window (ABA hazard): Swapchain::reset() only rebuilds the swapchain
      // images against the OLD, cached VkSurfaceKHR/hwnd (see
      // vswapchain.cpp reset()/createSwapchain()), so it cannot recover
      // either case. Hand the new window to the registered callback so it can
      // rebuild the Swapchain (surface included) from scratch, then treat
      // it as the new baseline. A same-window resize with no preceding
      // TERM_WINDOW (below) keeps using the cheap reset() path via
      // dispatchResize, unchanged.
      if(g_onSurfaceCreated) {
        try {
          g_onSurfaceCreated(reinterpret_cast<SystemApi::Window*>(g_app->window));
          }
        catch(const std::exception& e) {
          Log::e("AndroidApi: onSurfaceCreated: ", e.what());
          }
        catch(...) {
          Log::e("AndroidApi: onSurfaceCreated: unknown exception");
          }
        }
      g_lastWindow = g_app->window;
      } else {
      SizeEvent e(ANativeWindow_getWidth(g_app->window), ANativeWindow_getHeight(g_app->window));
      dispatchResize(*g_owner, e);
      }
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
    // Key events aren't processed yet (M1 targets mouse/touch only), but the
    // buffer must still be drained each tick, or hardware key events (incl.
    // the Back button) queue up in the fixed-size buffer and get dropped
    // once it fills.
    android_app_clear_key_events(ib);
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
