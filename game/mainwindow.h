#pragma once

#include "camera.h"
#include "resources.h"

#include <Tempest/Window>
#include <Tempest/VulkanApi>
#include <Tempest/Device>
#include <Tempest/VectorImage>
#include <Tempest/Event>
#include <Tempest/Pixmap>
#include <Tempest/Sprite>
#include <Tempest/Font>
#include <Tempest/TextureAtlas>
#include <Tempest/Timer>

#include <vector>
#include <optional>
#include <string>
#include <thread>

#include "world/world.h"
#include "world/focus.h"
#include "game/playercontrol.h"
#include "graphics/rendererios.h"
#include "ui/dialogmenu.h"
#include "ui/inventorymenu.h"
#include "ui/chapterscreen.h"
#include "ui/documentmenu.h"
#include "ui/videowidget.h"
#include "ui/menuroot.h"
#include "ui/consolewidget.h"
#include "ui/touchinput.h"
#include "ui/gamepadinput.h"

#include "utils/keycodec.h"
#include "utils/safearea.h"
#include "resources.h"

class MenuRoot;
class GameSession;
class Interactive;

class MainWindow : public Tempest::Window {
  public:
    explicit MainWindow(Tempest::Device& device);
    ~MainWindow() override;

    float uiScale() const;

    // UI hooks shared by GamepadInput / TouchInput.
    PadCtx padContext() const;                 // which context the pad routes to
    void   dispatchKey(Tempest::KeyEvent& e);  // route a complete synthetic key tap
    void   uiAction(KeyCodec::Action a);       // window-level Escape/Inventory/Log/Status

#if defined(__MOBILE_PLATFORM__)
    // Touch-overlay -> gamepad ring and inventory navigation bridges.
    bool padRingOpen() const;
    void padOpenWeaponsRing();
    void padOpenItemRing();
    void padRingAim(float nx, float ny);
    void padRingCommit();
    void padRingCancel();
    void padPaintRing(Tempest::PaintEvent& e);
    void padOpenMap();
    bool padCharacterPageActive() const;
    bool padCharacterNavigationActive() const;
    void padCycleCharacterPage(int direction);
    void padInventoryCategory(int direction);
    std::optional<size_t> padInventorySelectedItem();
    bool padVideoActive() const;               // an intro/cutscene bink is playing
    void padSkipVideo();                        // skip it (mirrors desktop Esc)
#endif

  private:
    void paintEvent     (Tempest::PaintEvent& event) override;
    void resizeEvent    (Tempest::SizeEvent & event) override;

    void mouseDownEvent (Tempest::MouseEvent& event) override;
    void mouseUpEvent   (Tempest::MouseEvent& event) override;
    void mouseDragEvent (Tempest::MouseEvent& event) override;
    void mouseMoveEvent (Tempest::MouseEvent& event) override;
    void mouseWheelEvent(Tempest::MouseEvent& event) override;

    void keyDownEvent   (Tempest::KeyEvent&   event) override;
    void keyRepeatEvent (Tempest::KeyEvent&   event) override;
    void keyUpEvent     (Tempest::KeyEvent&   event) override;

    void focusEvent     (Tempest::FocusEvent&  event) override;

    void paintFocus     (Tempest::Painter& p, const Focus& fc, const Tempest::Matrix4x4& vp);
    void paintFocus     (Tempest::Painter& p, Tempest::Rect rect);

    void drawBar(Tempest::Painter& p, const Tempest::Texture2d *bar, int x, int y, float v, Tempest::AlignFlag flg);
#if defined(__MOBILE_PLATFORM__)
    void drawPadHints(Tempest::Painter& p, float scale);
#endif
    void drawMsg(Tempest::Painter& p);
    void drawProgress(Tempest::Painter& p, int x, int y, int w, int h, float v);
    void drawLoading (Tempest::Painter& p,int x,int y,int w,int h);
    void drawSaving  (Tempest::Painter& p);
    void drawSaving  (Tempest::Painter& p, const Tempest::Texture2d& back, int w, int h, float scale);

    void startGame(std::string_view slot);
    void loadGame (std::string_view slot);
    void saveGame (std::string_view slot, std::string_view name);
#if defined(__IOS__)
    void processPendingSave();
    void startPendingSave(Tempest::Pixmap&& preview);
    void startPendingSave();
#endif

    void onVideo(std::string_view fname);
    void onStartLoading();
    void onWorldLoaded();
    void onSessionExit();
    void onBenchmarkFinished();
    void setGameImpl(std::unique_ptr<GameSession>&& w);
    void clearInput();
    void setFullscreen(bool fs);
    bool rendererOperational();
    void pumpLoadingAfterRendererFailure();

    void processMouse(Tempest::MouseEvent& event, bool enable);
    void tickMouse(uint64_t dt);
    void onSettings();

    void setupUi();

    void render() override;

    uint64_t tick();
    void     updateAnimation(uint64_t dt);
    void     tickCamera(uint64_t dt);
    void     isDialogClosed(bool& ret);

#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
    void        logMemorySnapshot(const char* event);
    void        processMemoryEvents();
    const char* perfScene() const;
    void        resetPerfWindow(uint64_t nowUs);
    void        beginPerfFrame(uint64_t nowUs);
    void        submitPerfFrame(uint64_t nowUs);
    void        flushPerfWindow(uint64_t nowUs, bool force);
#endif

    template<Tempest::KeyEvent::KeyType k>
    void     onMarvinKey();

    Camera::Mode solveCameraMode() const;

    enum RuntimeMode : uint8_t {
      R_Normal,
      R_Suspended,
      R_Step,
      };

    Tempest::TextureAtlas atlas;
    Tempest::Font         font;
    RendererIOS           renderer;

    Tempest::VectorImage  uiLayer, numOverlay;

    Tempest::Texture2d        background;
    const Tempest::Texture2d* loadBox=nullptr;
    const Tempest::Texture2d* loadVal=nullptr;

    const Tempest::Texture2d* barBack=nullptr;
    const Tempest::Texture2d* barHp  =nullptr;
    const Tempest::Texture2d* barMisc=nullptr;
    const Tempest::Texture2d* barMana=nullptr;

    const Tempest::Texture2d* focusImg=nullptr;

    const Tempest::Texture2d* saveback=nullptr;

#if defined(__IOS__)
    struct PendingSave final {
      enum class Stage : uint8_t {
        None,
        CaptureRequested,
        AwaitingGpu,
        ReadyCpu,
        };

      Stage               stage = Stage::None;
      std::string         slot;
      std::string         name;
      Tempest::Pixmap     preview;

      bool active() const { return stage!=Stage::None; }
      } pendingSave;
#endif

    bool                      mouseP[Tempest::MouseEvent::ButtonBack]={};

    KeyCodec                  keycodec;

    MenuRoot                  rootMenu;
    VideoWidget               video;
    InventoryMenu             inventory;
    DialogMenu                dialogs;
    DocumentMenu              document;
    ChapterScreen             chapter;
    ConsoleWidget             console;
    PlayerControl             player;
#if defined(__MOBILE_PLATFORM__)
    TouchInput                mobileUi;
    GamepadInput              gamepad;
    PadCtx                    lastPadCtx   = PadCtx::Loading;
    uint64_t                  padHintUntil = 0;   // controls-help auto-hide time
    int                       lastPlayerHp = -1;  // for damage haptics
#endif
    RuntimeMode               runtimeMode = R_Normal;
    bool                      rendererFailureReported = false;
    bool                      rendererFailureSettled  = false;
    SafeArea::Insets          safeArea;           // display cutouts, px; zero off-iOS

    Tempest::Widget*          uiKeyUp=nullptr;
    Tempest::Point            dMouse;
    uint64_t                  lastTick=0;

#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
    struct PerfWindow final {
      std::vector<uint32_t> frameUs;
      std::vector<uint32_t> tickUs;
      std::vector<uint32_t> animationUs;
      std::vector<uint32_t> poseRefreshUs;
      uint64_t              startedUs       = 0;
      uint64_t              lastSubmittedUs = 0;
      size_t                framesStarted   = 0;
      size_t                framesSubmitted = 0;
      size_t                fenceMisses     = 0;
      const char*           scene           = "startup";
      } perfWindow;
#endif

    Tempest::Shortcut         funcKey[11];
    Tempest::Shortcut         displayPos;

    struct BenchmarkData {
      std::vector<uint64_t> low1procent;
      size_t                numFrames = 0;
      double                fpsSum = 0;
      void                  push(uint64_t t);
      void                  clear();
      };
    struct Fps {
      uint64_t dt[10]={};
      double   get() const;
      void     push(uint64_t t);
      };
    Fps           fps;
    BenchmarkData benchmark;
    uint32_t      maxFpsTarget = 0;
    uint64_t      maxFpsInv = 0;
#if defined(__IOS__)
    uint32_t      iosFrameRateTarget = uint32_t(-1);
#endif
  };
