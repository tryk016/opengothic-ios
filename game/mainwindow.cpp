#include "mainwindow.h"

#include <Tempest/Except>
#include <Tempest/Painter>

#include <Tempest/Brush>
#include <Tempest/Pen>
#include <Tempest/Layout>
#include <Tempest/Application>
#include <Tempest/Log>

#include "ui/dialogmenu.h"
#include "ui/menuroot.h"
#include "ui/stacklayout.h"
#include "ui/videowidget.h"

#include "utils/mouseutil.h"
#include "utils/string_frm.h"
#include "world/triggers/abstracttrigger.h"
#include "world/objects/npc.h"
#include "world/world.h"
#include "game/serialize.h"
#include "game/globaleffects.h"
#include "utils/gthfont.h"
#include "utils/dbgpainter.h"
#include "utils/gamepad.h"
#include "utils/haptics.h"
#include "utils/exceptiondump.h"
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
#include "utils/memoryinfo.h"
#endif
#include "utils/systemmsg.h"
#include "ui/padglyph.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
#include <chrono>
#include <limits>
#endif

#include "commandline.h"
#include "gothic.h"

using namespace Tempest;

#if defined(__IOS__)
extern "C" void tempestIosSetPreferredFrameRate(int fps);
#endif

#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
namespace {

uint64_t perfNowUs() {
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
  }

uint32_t perfSample(uint64_t value) {
  const uint64_t max = uint64_t(std::numeric_limits<uint32_t>::max());
  return uint32_t(value>max ? max : value);
  }

double percentileMs(const std::vector<uint32_t>& samples, size_t percentile) {
  if(samples.empty())
    return 0.0;
  auto sorted = samples;
  std::sort(sorted.begin(),sorted.end());
  const size_t rank = (sorted.size()*percentile + 99u)/100u;
  const size_t at   = std::min(sorted.size()-1u,rank>0u ? rank-1u : 0u);
  return double(sorted[at])/1000.0;
  }

double memoryMiB(uint64_t bytes, bool valid) {
  return valid ? double(bytes)/(1024.0*1024.0) : -1.0;
  }

}
#endif

MainWindow::MainWindow(Device& device)
  : Window(Maximized),atlas(device),renderer(device,hwnd()),
    rootMenu(keycodec),inventory(keycodec),
    dialogs(inventory),document(keycodec),
    console(*this),player(dialogs,inventory),
#if defined(__MOBILE_PLATFORM__)
    mobileUi(*this,player),
    gamepad(*this,player),
#endif
    runtimeMode(R_Normal) {
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
  MemoryInfo::initialize();
  resetPerfWindow(perfNowUs());
#endif

  Gothic::inst().onSettingsChanged.bind(this,&MainWindow::onSettings);
  onSettings();
  safeArea = SafeArea::insets();

  if(Gothic::inst().version().game==2)
    setWindowTitle("Gothic II"); else
    setWindowTitle("Gothic");

  if(!CommandLine::inst().isWindowMode())
    setFullscreen(true);

  setupUi();

  barBack    = Resources::loadTexture("BAR_BACK.TGA");
  barHp      = Resources::loadTexture("BAR_HEALTH.TGA");
  barMisc    = Resources::loadTexture("BAR_MISC.TGA");
  barMana    = Resources::loadTexture("BAR_MANA.TGA");

  focusImg   = Resources::loadTexture("FOCUS_HIGHLIGHT.TGA");

  loadBox    = Resources::loadTexture("PROGRESS.TGA");
  loadVal    = Resources::loadTexture("PROGRESS_BAR.TGA");
  saveback   = Resources::loadTexture("SAVING.TGA");

  Gothic::inst().onStartGame   .bind(this,&MainWindow::startGame);
  Gothic::inst().onLoadGame    .bind(this,&MainWindow::loadGame);
  Gothic::inst().onSaveGame    .bind(this,&MainWindow::saveGame);

  Gothic::inst().onStartLoading.bind(this,&MainWindow::onStartLoading);
  Gothic::inst().onWorldLoaded .bind(this,&MainWindow::onWorldLoaded);
  Gothic::inst().onSessionExit .bind(this,&MainWindow::onSessionExit);

  Gothic::inst().onVideo       .bind(this,&MainWindow::onVideo);

  Gothic::inst().onBenchmarkFinished.bind(this,&MainWindow::onBenchmarkFinished);

  if(!Gothic::inst().defaultSave().empty()){
    Gothic::inst().load(Gothic::inst().defaultSave());
    rootMenu.popMenu();
    }
  else if(!CommandLine::inst().doStartMenu()) {
    startGame(Gothic::inst().defaultWorld());
    rootMenu.popMenu();
    }
  else {
    rootMenu.processMusicTheme();
    }

  funcKey[2] = Shortcut(*this,Event::M_NoModifier,Event::K_F2);
  funcKey[2].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F2>);

  funcKey[3] = Shortcut(*this,Event::M_NoModifier,Event::K_F3);
  funcKey[3].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F3>);

  funcKey[4] = Shortcut(*this,Event::M_NoModifier,Event::K_F4);
  funcKey[4].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F4>);

  funcKey[5] = Shortcut(*this,Event::M_NoModifier,Event::K_F5);
  funcKey[5].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F5>);

  funcKey[6] = Shortcut(*this,Event::M_NoModifier,Event::K_F6);
  funcKey[6].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F6>);

  funcKey[7] = Shortcut(*this,Event::M_NoModifier,Event::K_F7);
  funcKey[7].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F7>);

  funcKey[9] = Shortcut(*this,Event::M_NoModifier,Event::K_F9);
  funcKey[9].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F9>);

  funcKey[10] = Shortcut(*this,Event::M_NoModifier,Event::K_F10);
  funcKey[10].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F10>);

  displayPos = Shortcut(*this,Event::M_Alt,Event::K_P);
  displayPos.onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_P>);
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
  logMemorySnapshot("main_window_ready");
#endif
  }

MainWindow::~MainWindow() {
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
  flushPerfWindow(perfNowUs(),true);
  logMemorySnapshot("shutdown");
#endif
  GameMusic::inst().stopMusic();
  Gothic::inst().cancelLoading();
  renderer.waitIdle();
  takeWidget(&dialogs);
  takeWidget(&inventory);
  takeWidget(&chapter);
  takeWidget(&document);
  takeWidget(&video);
  takeWidget(&rootMenu);
#if defined(__MOBILE_PLATFORM__)
  takeWidget(&mobileUi);
#endif
  removeAllWidgets();
  // unload
  Gothic::inst().setGame(std::unique_ptr<GameSession>());
  }

float MainWindow::uiScale() const {
  const float base = SystemApi::uiScale(hwnd());
#if defined(__MOBILE_PLATFORM__)
  // High-DPI phones/tablets render the UI at native pixels, but SystemApi
  // reports scale 1.0 on iOS, leaving the fixed-size Gothic UI tiny. Scale up
  // roughly with the framebuffer size so menu/dialogue/subtitle text stays
  // legible (~1x at 1200px, ~2x at 2556px).
  const int   longEdge = (w()>h() ? w() : h());
  float       k        = float(longEdge)/1200.0f;
  if(k<1.0f)
    k = 1.0f;
  return base * k;
#else
  return base;
#endif
  }

void MainWindow::setupUi() {
  setLayout(new StackLayout());
  addWidget(&document);
  addWidget(&dialogs);
  addWidget(&inventory);
  addWidget(&chapter);
  addWidget(&video);
  addWidget(&rootMenu);
#if defined(__MOBILE_PLATFORM__)
  addWidget(&mobileUi);
#endif

  rootMenu.setMainMenu();

  Gothic::inst().onDialogPipe  .bind(&dialogs,&DialogMenu::openPipe);
  Gothic::inst().isNpcInDialogFn = std::bind(&DialogMenu::isNpcInDialog, &dialogs, std::placeholders::_1);

  Gothic::inst().onPrintScreen .bind(&dialogs,&DialogMenu::printScreen);
  Gothic::inst().onPrint       .bind(&dialogs,&DialogMenu::print);

  Gothic::inst().onIntroChapter.bind(&chapter, &ChapterScreen::show);
  Gothic::inst().onShowDocument.bind(&document,&DocumentMenu::show);
  }

void MainWindow::paintEvent(PaintEvent& event) {
  // refresh per painted frame: the ctor may run before the window is laid out
  // (insets read as zero), and a same-size relayout never reaches resizeEvent
  // (Widget::resize early-outs), so this is the only reliable refresh point
  safeArea = SafeArea::insets();

  Painter p(event);
  auto world = Gothic::inst().world();
  auto st    = Gothic::inst().checkLoading();
#if defined(__IOS__)
  const bool preparingSave = pendingSave.active();
#else
  constexpr bool preparingSave = false;
#endif

  if(!Gothic::inst().isInGame() && st==Gothic::LoadState::Idle && background.isEmpty()) {
    background = Resources::loadTextureUncached("STARTSCREEN.TGA");
    }

  if(world==nullptr && !background.isEmpty()) {
    p.setBrush(Color(0.0));
    p.drawRect(0,0,w(),h());

    if(st==Gothic::LoadState::Idle) {
      p.setBrush(Brush(background,Painter::NoBlend));
      p.drawRect(0,0,w(),h(),
                 0,0,background.w(),background.h());
      }
    }

  if(world!=nullptr) {
    world->globalFx()->scrBlend(p,Rect(0,0,w(),h()));
    }

  if(preparingSave || (st!=Gothic::LoadState::Idle && st!=Gothic::LoadState::Finalize)) {
    if(preparingSave || st==Gothic::LoadState::Saving) {
      drawSaving(p);
      } else {
      if(auto back = Gothic::inst().loadingBanner(); back!=nullptr && !back->isEmpty()) {
        p.setBrush(Brush(*back,Painter::NoBlend));
        p.drawRect(0,0,this->w(),this->h(),
                   0,0,back->w(),back->h());
        }
      if(loadBox!=nullptr && !loadBox->isEmpty()) {
        if(Gothic::inst().version().game==1) {
          int lw = int(w()*0.5);
          int lh = int(h()*0.05);
          drawLoading(p,(w()-lw)/2, int(h()*0.75), lw, lh);
          } else {
          drawLoading(p,int(w()*0.92)-loadBox->w(), int(h()*0.12), loadBox->w(),loadBox->h());
          }
        }
      }
    } else {
    if(world!=nullptr && world->view()){
      auto& camera = *Gothic::inst().camera();

      auto vp = camera.viewProj();
      p.setBrush(Color(1.0));

      drawMsg(p);

      auto focus = world->validateFocus(player.focus());
      paintFocus(p,focus,vp);

      if(auto pl = Gothic::inst().player()){
        if (!Gothic::inst().isDesktop()) {
          auto& opt = Gothic::options();
          float hp  = float(pl->attribute(ATR_HITPOINTS))/float(pl->attribute(ATR_HITPOINTSMAX));
          float mp  = float(pl->attribute(ATR_MANA))     /float(pl->attribute(ATR_MANAMAX));

          bool showHealthBar = opt.showHealthBar;
          bool showManaBar   = (opt.showManaBar==2) || (opt.showManaBar==1 && (pl->weaponState()==WeaponState::Mage || inventory.isActive()));
          bool showSwimBar   = (opt.showSwimBar==2) || (opt.showSwimBar==1 && pl->isDive());

          if(showHealthBar)
            drawBar(p,barHp, 10+safeArea.left, h()-10-safeArea.bottom, hp, AlignLeft | AlignBottom);
          if(showManaBar)
            drawBar(p,barMana, w()-10-safeArea.right, h()-10-safeArea.bottom, mp, AlignRight | AlignBottom);
          if(showSwimBar) {
            uint32_t gl = pl->guild();
            auto     v  = float(pl->world().script().guildVal().dive_time[gl]);
            if(v>0) {
              auto t = float(pl->diveTime())/1000.f;
              drawBar(p,barMisc,w()/2,h()-10-safeArea.bottom, (v-t)/(v), AlignHCenter | AlignBottom);
              }
            }
          }
        }
      }
    }

  if(auto c = Gothic::inst().camera()) {
    DbgPainter dbg(p,c->viewProj(),w(),h());
    c->debugDraw(dbg);
    if(world!=nullptr) {
      world->marchPoints(dbg);
      world->marchInteractives(dbg);
      world->view()->dbgLights(dbg);
      world->marchCsCameras(dbg);
      }
    }

  renderer.dbgDraw(p);

  const float scale = Gothic::interfaceScale(this);
#if defined(__MOBILE_PLATFORM__)
  // Context hints are intentionally disabled: Options -> Controls contains
  // the complete controller layout and the transient bar obscured gameplay.
  // drawPadHints(p, scale);
  // The ring is painted by the last overlay widget (TouchInput), after menus
  // and inventory. Painting it here would put assignment mode underneath the
  // inventory widget because parent widgets are dispatched first.
  if(!gamepad.ringOpen() && !inventory.isActive())
    inventory.itemRenderer().reset();   // drop leftover ring icons after close
#endif
  if(Gothic::inst().doFrate() && !Gothic::inst().isDesktop()) {
    char fpsT[64]={};
    std::snprintf(fpsT,sizeof(fpsT),"fps = %.2f",fps.get());

    auto& fnt = Resources::font(scale);
    fnt.drawText(p,5+safeArea.left,fnt.pixelSize()+5+safeArea.top,fpsT);
    }

  if(!Gothic::inst().isDesktop() && world!=nullptr) {
    if(Gothic::inst().doClock()) {
      auto hour = world->time().hour();
      auto min  = world->time().minute();
      auto& fnt = Resources::font(scale);
      string_frm clockT(int(hour),":",int(min));
      fnt.drawText(p,w()-fnt.textSize(clockT).w-5-safeArea.right,fnt.pixelSize()+5+safeArea.top,clockT);
      }

    auto c = Gothic::inst().camera();
    if(Gothic::inst().doVobBox() && c!=nullptr) {
      DbgPainter dbg(p,c->viewProj(),w(),h());
      world->drawVobBoxNpcNear(dbg);
      }

    if(Gothic::inst().doVobRays() && c!=nullptr) {
      DbgPainter dbg(p,c->viewProj(),w(),h());
      player.drawVobRay(dbg);
      }
    }

  if(auto wx = Gothic::inst().worldView()) {
    wx->dbgClusters(p, Vec2(float(w()), float(h())));
    }
  }

void MainWindow::resizeEvent(SizeEvent&) {
  renderer.resize();
  if(auto camera = Gothic::inst().camera()) {
    const auto size = renderer.drawableSize();
    camera->setViewport(uint32_t(size.w),uint32_t(size.h));
    }

  const bool fs = SystemApi::isFullscreen(hwnd());
  auto rect = SystemApi::windowClientRect(hwnd());
  setCursorPosition(rect.w/2,rect.h/2);
  setCursorShape(fs ? CursorShape::Hidden : CursorShape::Arrow);
  dMouse = Point();
  }

void MainWindow::mouseDownEvent(MouseEvent &event) {
  if(event.button<sizeof(mouseP))
    mouseP[event.button]=true;
  auto act     = keycodec.tr(event);
  auto mapping = keycodec.mapping(event);
  player.onKeyPressed(act,KeyEvent::K_NoKey,mapping);
  }

void MainWindow::mouseUpEvent(MouseEvent &event) {
  auto act     = keycodec.tr(event);
  auto mapping = keycodec.mapping(event);
  player.onKeyReleased(act,mapping);
  if(event.button<sizeof(mouseP))
    mouseP[event.button]=false;
  }

void MainWindow::mouseDragEvent(MouseEvent &event) {
  const bool fs = SystemApi::isFullscreen(hwnd());
  if(!mouseP[Event::ButtonLeft] && !fs)
    return;
  if(player.focus().npc && !fs)
    return;
  processMouse(event,true);
  }

void MainWindow::mouseMoveEvent(MouseEvent &event) {
  processMouse(event,SystemApi::isFullscreen(hwnd()));
  }

void MainWindow::processMouse(MouseEvent& event, bool enable) {
  auto center = Point(w()/2,h()/2);
  if(enable && event.pos()!=center && hasFocus()) {
    dMouse += (event.pos()-center);
    setCursorPosition(center);
    }
  }

void MainWindow::tickMouse(uint64_t dt) {
  auto camera = Gothic::inst().camera();
  if(dialogs.hasContent() || Gothic::inst().isPause() || camera==nullptr || camera->isCutscene()) {
    dMouse = Point();
    return;
    }

  const bool enableMouse = Gothic::inst().settingsGetI("GAME","enableMouse");
  if(enableMouse==0) {
    dMouse = Point();
    return;
    }

  if(dMouse==Point())
    return;

  const bool  camLookaroundInverse = Gothic::inst().settingsGetI("GAME","camLookaroundInverse");
  const float mouseSensitivity     = Gothic::inst().settingsGetF("GAME","mouseSensitivity")/MouseUtil::mouseSysSpeed();
  PointF dpScaled = PointF(float(dMouse.x)*mouseSensitivity,float(dMouse.y)*mouseSensitivity);
  dpScaled.x/=float(w());
  dpScaled.y/=float(h());
  if(camLookaroundInverse)
    dpScaled.y *= -1.f;

  static float mul = 270.f;
  dpScaled *= mul;

  static float psMax = 720.f;
  const float  dtF   = float(dt)/1000.f;
  dpScaled.x = std::clamp(dpScaled.x, -(psMax*dtF), psMax*dtF);
  dpScaled.y = std::clamp(dpScaled.y, -(psMax*dtF), psMax*dtF);

  // Log::d("mouse dMouse   = ", dMouse.x,   ", ", dMouse.y);
  // Log::d("mouse dpScaled = ", dpScaled.x, ", ", dpScaled.y);

  camera->onRotateMouse(PointF(dpScaled.y,-dpScaled.x));
  if(!inventory.isActive()) {
    player.onRotateMouse(-dpScaled.x, -dpScaled.y);
    }

  dMouse = Point();
  }

void MainWindow::onSettings() {
  int zMaxFps = 0;
#if defined(__IOS__)
  constexpr int fpsLimits[] = {0,30,60};
  const int fpsMode = std::clamp(Gothic::inst().settingsGetI("ENGINE", "zMaxFpsMode"),0,2);
  zMaxFps = fpsLimits[fpsMode];
#else
  zMaxFps = Gothic::options().fpsLimit;
  if(zMaxFps<=0)
    zMaxFps = Gothic::inst().settingsGetI("ENGINE", "zMaxFps");
#endif
  maxFpsTarget = uint32_t(std::max(zMaxFps,0));
  if(zMaxFps>0)
    maxFpsInv = 1000u/uint64_t(zMaxFps); else
    maxFpsInv = 0;
#if defined(OPENGOTHIC_GPU_EXPERIMENT_DYNAMIC_DRAW_DISTANCE)
  // settingsSetI() emits onSettingsChanged immediately, so rebuilding the
  // projection here makes the stock Draw distance choice live in-game.
  if(auto* camera = Gothic::inst().camera()) {
    const auto size = renderer.drawableSize();
    camera->setViewport(uint32_t(size.w),uint32_t(size.h));
    }
#endif
  }

void MainWindow::mouseWheelEvent(MouseEvent &event) {
  if(auto camera = Gothic::inst().camera())
    camera->changeZoom(event.delta);
  }

void MainWindow::keyDownEvent(KeyEvent &event) {
  if(video.isActive()){
    event.accept();
    video.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&video;
      return;
      }
    }

  if(rootMenu.isActive()) {
    event.accept();
    rootMenu.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&rootMenu;
      return;
      }
    }

  if(chapter.isActive()){
    event.accept();
    chapter.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&chapter;
      return;
      }
    }

  if(document.isActive()){
    event.accept();
    document.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&document;
      return;
      }
    }

  if(dialogs.isActive()){
    event.accept();
    dialogs.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&dialogs;
      return;
      }
    }

  if(inventory.isActive()){
    event.accept();
    inventory.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&inventory;
      return;
      }
    }
  uiKeyUp=nullptr;

  auto act     = keycodec.tr(event);
  auto mapping = keycodec.mapping(event);
  player.onKeyPressed(act,event.key,mapping);

  if(event.key==Event::K_F11) {
    auto pm = renderer.screenshot();
    pm.save("dbg.png");
    }
  event.accept();
  }

void MainWindow::keyRepeatEvent(KeyEvent& event) {
  if(uiKeyUp==&video){
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&rootMenu){
    rootMenu.keyRepeatEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&chapter){
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&document){
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&dialogs){
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&inventory){
    inventory.keyRepeatEvent(event);
    if(event.isAccepted())
      return;
    }
  }

void MainWindow::keyUpEvent(KeyEvent &event) {
  if(uiKeyUp==&video){
    video.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&rootMenu){
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&chapter){
    chapter.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&document){
    document.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&dialogs){
    dialogs.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&inventory){
    inventory.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }


  auto act     = keycodec.tr(event);
  auto mapping = keycodec.mapping(event);

  uiAction(act);
  player.onKeyReleased(act, mapping);
  }

void MainWindow::uiAction(KeyCodec::Action act) {
  std::string_view menuEv;
  if(act==KeyCodec::Escape)
    menuEv = Gothic::inst().menuMain();
  else if(act==KeyCodec::Log)
    menuEv = "MENU_LOG";
  else if(act==KeyCodec::Status)
    menuEv = "MENU_STATUS";

  if(!menuEv.empty()) {
    rootMenu.setMenu(menuEv,act);
    rootMenu.showVersion(act==KeyCodec::Escape);
    if(auto pl = Gothic::inst().player())
      rootMenu.setPlayer(*pl);
    clearInput();
    }
  else if(act==KeyCodec::Inventory && !dialogs.isActive()) {
    if(inventory.isActive()) {
      inventory.close();
      } else {
      auto pl = Gothic::inst().player();
      if(pl!=nullptr)
        inventory.open(*pl);
      }
    clearInput();
    }
  }

PadCtx MainWindow::padContext() const {
  auto& g = Gothic::inst();
  if(g.checkLoading()!=Gothic::LoadState::Idle)
    return PadCtx::Loading;
  // Keep the routing context in the same priority order as keyDownEvent and
  // dispatchKey; overlapping UI must use the mapping of its actual receiver.
  if(video.isActive() || rootMenu.isActive() ||
     chapter.isActive() || document.isActive())
    return PadCtx::Menu;
  // Trade keeps DialogMenu active, but it deliberately ignores input while
  // the inventory owns the interaction. Choose Inventory here so horizontal
  // navigation is available before dispatchKey falls through the dialog.
  if(inventory.isActive())
    return PadCtx::Inventory;
  if(dialogs.isActive())
    return PadCtx::Dialog;
  return PadCtx::World;
  }

#if defined(__MOBILE_PLATFORM__)
bool MainWindow::padRingOpen() const          { return gamepad.ringOpen(); }
void MainWindow::padOpenWeaponsRing()         { gamepad.openWeaponsRing(); }
void MainWindow::padOpenItemRing()            { gamepad.openItemRing(); }
void MainWindow::padRingAim(float nx,float ny){ gamepad.ringAim(nx,ny); }
void MainWindow::padRingCommit()              { gamepad.ringCommit(); }
void MainWindow::padRingCancel()              { gamepad.ringCancel(); }
void MainWindow::padPaintRing(PaintEvent& e)  {
  if(auto* ring=gamepad.activeRing()) {
    Painter p(e);
    ring->paint(p,inventory.itemRenderer(),Gothic::inst().player(),
                w(),h(),Gothic::interfaceScale(this));
    }
  }
void MainWindow::padOpenMap()                 { gamepad.openMap(); }
bool MainWindow::padCharacterPageActive() const {
  return rootMenu.isActive(KeyCodec::Log) || rootMenu.isActive(KeyCodec::Status);
  }
bool MainWindow::padCharacterNavigationActive() const {
  return rootMenu.isRootMenu(KeyCodec::Log) || rootMenu.isRootMenu(KeyCodec::Status);
  }
void MainWindow::padCycleCharacterPage(int direction) {
  if(direction==0)
    return;
  if(rootMenu.isActive(KeyCodec::Log))
    uiAction(KeyCodec::Status);
  else if(rootMenu.isActive(KeyCodec::Status))
    uiAction(KeyCodec::Log);
  }
void MainWindow::padInventoryCategory(int d)  { inventory.moveCategory(d); }
std::optional<size_t> MainWindow::padInventorySelectedItem() {
  return inventory.selectedPlayerItemClass();
  }
bool MainWindow::padVideoActive() const       { return video.isActive(); }
void MainWindow::padSkipVideo()               { video.skip(); }
#endif

void MainWindow::dispatchKey(Tempest::KeyEvent& e) {
  // Synthetic pad/touch input follows the same UI priority and accept/ignore
  // contract as keyDownEvent. Deliver key-up to the widget that accepted the
  // press even if handling key-down changed its active state.
  auto dispatchTap = [&e](auto& widget) {
    e.accept();
    widget.keyDownEvent(e);
    if(!e.isAccepted())
      return false;

    Tempest::KeyEvent up(e.key, e.code, e.modifier, Tempest::Event::KeyUp);
    widget.keyUpEvent(up);
    return true;
    };

  if(video.isActive() && dispatchTap(video))
    return;

  // MenuRoot handles menu actions on key-down and has no public key-up path.
  if(rootMenu.isActive()) {
    e.accept();
    rootMenu.keyDownEvent(e);
    if(e.isAccepted())
      return;
    }

  if(chapter.isActive() && dispatchTap(chapter))
    return;
  if(document.isActive() && dispatchTap(document))
    return;
  if(dialogs.isActive() && dispatchTap(dialogs))
    return;
  if(inventory.isActive() && dispatchTap(inventory))
    return;

  // No UI consumed the synthetic key. Complete the same PlayerControl tap as
  // the regular key-down/key-up path without leaving a held action behind.
  e.accept();
  const auto act     = keycodec.tr(e);
  const auto mapping = keycodec.mapping(e);
  player.onKeyPressed(act, e.key, mapping);
  uiAction(act);
  player.onKeyReleased(act, mapping);
  }

void MainWindow::focusEvent(FocusEvent &event) {
  if(!event.in)
    return;
  dMouse = Point();
  auto center = Point(w()/2,h()/2);
  setCursorPosition(center);
  }

void MainWindow::paintFocus(Painter& p, const Focus& focus, const Matrix4x4& vp) {
  if(!focus || dialogs.isActive())
    return;

  const float scale = Gothic::interfaceScale(this);
  auto        world = Gothic::inst().world();
  auto        pl    = world==nullptr ? nullptr : world->player();
  if(pl==nullptr)
    return;

  auto pw  = 1.f;
  auto pos = focus.displayPosition();
  vp.project(pos.x,pos.y,pos.z,pw);

  if(pw<=0.f)
    return;

  pos /= pw;

  int   ix  = int((0.5f*pos.x+0.5f)*float(w()));
  int   iy  = int((0.5f*pos.y+0.5f)*float(h()));
  auto& fnt = Resources::font(scale);

  auto tsize = fnt.textSize(focus.displayName());
  ix-=tsize.w/2;
  if(iy<tsize.h)
    iy = tsize.h;
  if(iy>h())
    iy = h();
  fnt.drawText(p,ix,iy,focus.displayName());

  if(focus.npc!=nullptr && player.isTargetLocked()) {
    // Lock-on reticle: four corner brackets around the pinned target.
    const int cx = int((0.5f*pos.x+0.5f)*float(w()));
    const int cy = int((0.5f*pos.y+0.5f)*float(h()));
    const int r  = std::max(10,int(18*scale));
    const int t  = std::max(2, int(2*scale));
    const int l  = std::max(4, int(8*scale));
    p.setBrush(Color(1.f,0.43f,0.43f,0.9f));         // (255,110,110) lock tint (spec 5.4)
    p.drawRect(cx-r,   cy-r,   l, t); p.drawRect(cx-r,   cy-r,   t, l); // top-left
    p.drawRect(cx+r-l, cy-r,   l, t); p.drawRect(cx+r-t, cy-r,   t, l); // top-right
    p.drawRect(cx-r,   cy+r-t, l, t); p.drawRect(cx-r,   cy+r-l, t, l); // bottom-left
    p.drawRect(cx+r-l, cy+r-t, l, t); p.drawRect(cx+r-t, cy+r-l, t, l); // bottom-right
    }

  if(focus.npc!=nullptr && !focus.npc->isDead()) {
    float hp = float(focus.npc->attribute(ATR_HITPOINTS))/float(focus.npc->attribute(ATR_HITPOINTSMAX));
    drawBar(p,barHp, w()/2,10, hp, AlignHCenter|AlignTop);
    }

  const int foc = Gothic::settingsGetI("GAME","highlightMeleeFocus");
  if(focus.npc!=nullptr  &&
     (foc==1 || foc==3) &&
     player.isPressed(KeyCodec::ActionGeneric) &&
     pl->weaponState()!=WeaponState::NoWeapon &&
     pl->weaponState()!=WeaponState::Fist) {
    auto tr = vp;
    tr.mul(focus.npc->transform());

    const auto b    = focus.npc->bounds();
    const auto bbox = b.bbox; //focus.npc->bBox();

    Vec3 bx[] = {
      {bbox[0].x, bbox[0].y, bbox[0].z},
      {bbox[1].x, bbox[0].y, bbox[0].z},
      {bbox[1].x, bbox[1].y, bbox[0].z},
      {bbox[0].x, bbox[1].y, bbox[0].z},
      {bbox[0].x, bbox[0].y, bbox[1].z},
      {bbox[1].x, bbox[0].y, bbox[1].z},
      {bbox[1].x, bbox[1].y, bbox[1].z},
      {bbox[0].x, bbox[1].y, bbox[1].z},
      };

    int min[2]={ix,iy-20}, max[2]={ix,iy-20};
    for(int i=0; i<8; ++i) {
      tr.project(bx[i]);
      int x = int((bx[i].x*0.5f+0.5f)*float(w()));
      int y = int((bx[i].y*0.5f+0.5f)*float(h()));
      min[0] = std::min(x,min[0]);
      min[1] = std::min(y,min[1]);
      max[0] = std::max(x,max[0]);
      max[1] = std::max(y,max[1]);
      }

    paintFocus(p,Rect(min[0],min[1],max[0]-min[0],max[1]-min[1]));
    }

  // focusImg
  /*
  if(auto pl = focus.interactive){
    pl->marchInteractives(p,vp,w(),h());
    }*/
  }

void MainWindow::paintFocus(Painter& p, Rect rect) {
  if(focusImg==nullptr)
    return;
  const int w2 = focusImg->w();
  const int h2 = focusImg->h();
  const int w  = w2/2;
  const int h  = h2/2;

  if(rect.w<w) {
    int dw = w-rect.w;
    rect.x -= dw/2;
    rect.w += dw;
    }
  if(rect.h<h) {
    int dh = h-rect.h;
    rect.y -= dh/2;
    rect.h += dh;
    }

  p.setBrush(Brush(*focusImg,Painter::Add));
  p.drawRect(rect.x,         rect.y,         w,h, 0,0, w, h);
  p.drawRect(rect.x+rect.w-w,rect.y,         w,h, w,0, w2,h);
  p.drawRect(rect.x,         rect.y+rect.h-h,w,h, 0,h, w, h2);
  p.drawRect(rect.x+rect.w-w,rect.y+rect.h-h,w,h, w,h, w2,h2);
  }

void MainWindow::drawBar(Painter &p, const Tempest::Texture2d* bar, int x, int y, float v, AlignFlag flg) {
  if(barBack==nullptr || bar==nullptr)
    return;
  const float scale   = Gothic::interfaceScale(this);
  const float destW   = 200.f*scale*float(std::min(w(),800))/800.f;
  const float k       = float(destW)/float(std::max(barBack->w(),1));
  const float destH   = float(barBack->h())*k;
  const float destHin = float(destH)*24.f/32.f;
  //const float destHin = 20;//float(destH)*24.f/32.f;

  v = std::max(0.f,std::min(v,1.f));
  if(flg & AlignRight)
    x-=int(destW);
  else if(flg & AlignHCenter)
    x-=int(destW)/2;
  if(flg & AlignBottom)
    y-=int(destH);

  p.setBrush(*barBack);
  p.drawRect(x,y,int(destW),int(destH), 0,0,barBack->w(),barBack->h());

  int   dy = int(0.5f*(destH-destHin));
  float pd = 9.f*k;
  p.setBrush(*bar);
  p.drawRect(x+int(pd),y+dy,int(float(destW-pd*2)*v),int(destHin),
             0,0,bar->w(),bar->h());
  }

#if defined(__MOBILE_PLATFORM__)
void MainWindow::drawPadHints(Painter& p, float scale) {
  if(Application::tickCount()>=padHintUntil)   // only flashes briefly after a context change
    return;
  if(!Gamepad::poll().connected)               // touch users don't need button hints
    return;

  struct Hint { PadGlyph::Btn b; std::string_view t; };
  std::array<Hint,6> hints{};
  size_t n = 0;
  switch(padContext()) {
    case PadCtx::World:
      hints = {{ {PadGlyph::A,"Action"},{PadGlyph::Y,"Weapon"},{PadGlyph::B,"Jump"},
                 {PadGlyph::RT,"Block"},{PadGlyph::R3,"Lock"},{PadGlyph::RB,"Magic"} }};
      n = 6; break;
    case PadCtx::Dialog:
      hints = {{ {PadGlyph::A,"Select"},{PadGlyph::B,"Skip"},{PadGlyph::DPadUp,"Choose"} }};
      n = 3; break;
    case PadCtx::Menu:
      hints = {{ {PadGlyph::A,"OK"},{PadGlyph::B,"Back"},{PadGlyph::DPadLeft,"Change"} }};
      n = 3; break;
    case PadCtx::Inventory:
      hints = {{ {PadGlyph::A,"Use"},{PadGlyph::B,"Close"},{PadGlyph::DPadUp,"Move"} }};
      n = 3; break;
    case PadCtx::Loading:
      return;
    }
  if(n==0)
    return;

  auto&     fnt = Resources::font(scale);
  const int s   = std::max(16,int(26*scale));
  const int gap = std::max(2, s/6);

  int total = 0;
  for(size_t i=0;i<n;++i)
    total += s + gap + fnt.textSize(hints[i].t).w + gap*2;

  int       x = (w()-total)/2;
  const int y = h() - s - std::max(6,int(10*scale)) - safeArea.bottom;
  for(size_t i=0;i<n;++i)
    x += PadGlyph::drawLabelled(p, fnt, hints[i].b, x, y, s, hints[i].t);
  }
#endif

void MainWindow::drawMsg(Tempest::Painter& p) {
  const float scale   = Gothic::interfaceScale(this);
  const float destW   = 200.f*scale*float(std::min(w(),800))/800.f;
  const float k       = float(destW)/float(std::max(barBack->w(),1));
  const float destH   = float(barBack->h())*k;

  const int y = 10 + int(destH) + 10;
  dialogs.drawMsg(p, y);
  }

void MainWindow::drawProgress(Painter &p, int x, int y, int w, int h, float v) {
  if(v<0.1f)
    v=0.1f;
  p.setBrush(*loadBox);
  p.drawRect(x,y,w,h, 0,0,loadBox->w(),loadBox->h());

  int paddL = int((float(w)*75.f)/float(loadBox->w()));
  int paddT = int((float(h)*10.f)/float(loadBox->h()));

  if(Gothic::inst().version().game==1) {
    paddL = int((float(w)*30.f)/float(loadBox->w()));
    paddT = int((float(h)* 5.f)/float(loadBox->h()));
    }

  p.setBrush(*loadVal);
  p.drawRect(x+paddL,y+paddT,int(float(w-2*paddL)*v),h-2*paddT,
             0,0,loadVal->w(),loadVal->h());
  }

void MainWindow::drawLoading(Painter &p, int x, int y, int w, int h) {
  float v = float(Gothic::inst().loadingProgress())/100.f;
  drawProgress(p,x,y,w,h,v);
  }

void MainWindow::drawSaving(Painter &p) {
  if(auto back = Gothic::inst().loadingBanner(); back!=nullptr && !back->isEmpty()) {
    p.setBrush(Brush(*back,Painter::NoBlend));
    p.drawRect(0,0,this->w(),this->h(),
               0,0,back->w(),back->h());
    }

  if(saveback==nullptr)
    return;

  const float scale = Gothic::interfaceScale(this);
  int         szX   = Gothic::options().saveGameImageSize.w;
  int         szY   = Gothic::options().saveGameImageSize.h;

  if(szX<=460 || szY<=300) {
    // way too small otherwise
    szX = 460;
    szY = 300;
    }
  szX = int(float(szX)*scale);
  szY = int(float(szY)*scale);
  drawSaving(p,*saveback,szX,szY,scale);
  }

void MainWindow::drawSaving(Painter& p, const Tempest::Texture2d& back, int sw, int sh, float scale) {
  const int x = (w()-sw)/2, y = (h()-sh)/2;

  // SAVING.TGA is semi-transparent image with the idea to accomulate alpha over time
  // ... for loop for now
  p.setBrush(back);
  for(int i=0;i<10;++i) {
    p.drawRect(x,y,sw,sh, 0,0,back.w(),back.h());
    }

  float v = float(Gothic::inst().loadingProgress())/100.f;
  drawProgress(p, x+int(100.f*scale), y+sh-int(75.f*scale), sw-2*int(100.f*scale), int(40.f*scale), v);
  }

void MainWindow::isDialogClosed(bool& ret) {
  ret = !(dialogs.isActive() || document.isActive());
  }

#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
void MainWindow::logMemorySnapshot(const char* event) {
  const auto mem = MemoryInfo::snapshot();
  const bool ceilingValid = mem.footprintValid && mem.availableValid;
  const uint64_t ceiling = ceilingValid ? mem.footprintBytes+mem.availableBytes : 0;
  const int entitlementPresent = !mem.increasedMemoryLimitChecked ? -1 :
                                 (mem.increasedMemoryLimitPresent ? 1 : 0);
  string_frm<512> line("MEM v=1 event=",event!=nullptr ? event : "unknown",
                       " footprint_mb=",memoryMiB(mem.footprintBytes,mem.footprintValid),
                       " available_mb=",memoryMiB(mem.availableBytes,mem.availableValid),
                       " estimated_ceiling_mb=",memoryMiB(ceiling,ceilingValid),
                       " thermal=",MemoryInfo::thermalStateName(mem.thermal),
                       " entitlement_requested=",mem.increasedMemoryLimitRequested ? 1 : 0,
                       " entitlement_present=",entitlementPresent);
  Log::i(line.c_str());
  }

void MainWindow::processMemoryEvents() {
  const uint32_t events = MemoryInfo::consumeEvents();
  if(events==MemoryInfo::NoEvent)
    return;

  flushPerfWindow(perfNowUs(),true);
  if((events & MemoryInfo::MemoryWarning)!=0u)
    logMemorySnapshot("memory_warning");
  if((events & MemoryInfo::DidEnterBackground)!=0u)
    logMemorySnapshot("did_enter_background");
  if((events & MemoryInfo::WillEnterForeground)!=0u)
    logMemorySnapshot("will_enter_foreground");
  if((events & MemoryInfo::DidBecomeActive)!=0u)
    logMemorySnapshot("did_become_active");
  }

const char* MainWindow::perfScene() const {
  if(Gothic::inst().checkLoading()!=Gothic::LoadState::Idle)
    return "loading";
  if(video.isActive())
    return "video";
  if(Gothic::inst().world()!=nullptr)
    return "world";
  return "menu";
  }

void MainWindow::resetPerfWindow(uint64_t nowUs) {
  perfWindow.frameUs.clear();
  perfWindow.tickUs.clear();
  perfWindow.animationUs.clear();
  perfWindow.poseRefreshUs.clear();
  perfWindow.frameUs.reserve(2048);
  perfWindow.tickUs.reserve(2048);
  perfWindow.animationUs.reserve(2048);
  perfWindow.poseRefreshUs.reserve(2048);
  perfWindow.startedUs       = nowUs;
  perfWindow.lastSubmittedUs = 0;
  perfWindow.framesStarted   = 0;
  perfWindow.framesSubmitted = 0;
  perfWindow.fenceMisses     = 0;
  perfWindow.scene           = perfScene();
  }

void MainWindow::beginPerfFrame(uint64_t nowUs) {
  if(perfWindow.startedUs==0)
    resetPerfWindow(nowUs);
  perfWindow.framesStarted++;
  }

void MainWindow::submitPerfFrame(uint64_t nowUs) {
  if(perfWindow.lastSubmittedUs!=0 && nowUs>=perfWindow.lastSubmittedUs)
    perfWindow.frameUs.push_back(perfSample(nowUs-perfWindow.lastSubmittedUs));
  perfWindow.lastSubmittedUs = nowUs;
  perfWindow.framesSubmitted++;
  }

void MainWindow::flushPerfWindow(uint64_t nowUs, bool force) {
  static constexpr uint64_t WindowUs = 10u*1000u*1000u;
  if(perfWindow.startedUs==0 || nowUs<perfWindow.startedUs) {
    resetPerfWindow(nowUs);
    return;
    }

  const uint64_t elapsedUs = nowUs-perfWindow.startedUs;
  if(!force && elapsedUs<WindowUs)
    return;
  if(perfWindow.framesStarted==0 || elapsedUs==0) {
    resetPerfWindow(nowUs);
    return;
    }

  const auto mem = MemoryInfo::snapshot();
  const bool ceilingValid = mem.footprintValid && mem.availableValid;
  const uint64_t ceiling = ceilingValid ? mem.footprintBytes+mem.availableBytes : 0;
  const int entitlementPresent = !mem.increasedMemoryLimitChecked ? -1 :
                                 (mem.increasedMemoryLimitPresent ? 1 : 0);
  auto* const world = Gothic::inst().world();
  const size_t npcCount = world!=nullptr ? size_t(world->npcCount()) : 0u;
  const auto npcAnimation = world!=nullptr ? world->animationStats() : WorldObjects::AnimationStats{};
  const double measuredFps = double(perfWindow.framesSubmitted)*1000000.0/double(elapsedUs);
#if defined(OPENGOTHIC_IOS_THREE_FRAMES_IN_FLIGHT)
  constexpr const char* perfExperiment = "three_frames_in_flight";
#elif defined(OPENGOTHIC_NPC_DIALOG_CULLING)
  constexpr const char* perfExperiment = "npc_dialog_culling";
#else
  constexpr const char* perfExperiment = "control";
#endif
#if defined(OPENGOTHIC_GPU_EXPERIMENT_DYNAMIC_DRAW_DISTANCE)
  constexpr const char* gpuExperiment = "dynamic_draw_distance";
  const uint32_t worldFarPlane = Camera::configuredFarPlane();
  const uint32_t drawDistancePercent = worldFarPlane/1000u;
#elif defined(OPENGOTHIC_GPU_EXPERIMENT_WORLD_FAR_PLANE_60000)
  constexpr const char* gpuExperiment = "world_far_plane_60000";
  constexpr uint32_t worldFarPlane = 60000u;
  constexpr uint32_t drawDistancePercent = 60u;
#elif defined(OPENGOTHIC_GPU_EXPERIMENT_DIRECT_DRAWABLE_LAZY_SSAO)
  constexpr const char* gpuExperiment = "direct_drawable_v2_lazy_ssao";
  constexpr uint32_t worldFarPlane = 100000u;
  constexpr uint32_t drawDistancePercent = 100u;
#else
  constexpr const char* gpuExperiment = "control";
  constexpr uint32_t worldFarPlane = 100000u;
  constexpr uint32_t drawDistancePercent = 100u;
#endif
#if defined(OPENGOTHIC_GPU_EXPERIMENT_DIRECT_DRAWABLE_LAZY_SSAO)
  constexpr int directDrawable = 1;
#else
  constexpr int directDrawable = 0;
#endif
#if defined(__IOS__)
  constexpr const char* framePacer = "display_link";
#else
  constexpr const char* framePacer = "software_sleep";
#endif

  string_frm<1024> line("PERF v=1 scene=",perfWindow.scene,
                        " perf_exp=",perfExperiment,
                        " gpu_exp=",gpuExperiment,
                        " direct_drawable=",directDrawable,
                        " world_far_plane=",worldFarPlane,
                        " draw_distance_percent=",drawDistancePercent,
                        " fps_limit=",maxFpsTarget,
                        " frame_pacer=",framePacer,
                        " window_ms=",size_t(elapsedUs/1000u),
                        " fps=",measuredFps,
                        " frame_p50_ms=",percentileMs(perfWindow.frameUs,50u),
                        " frame_p95_ms=",percentileMs(perfWindow.frameUs,95u),
                        " frame_p99_ms=",percentileMs(perfWindow.frameUs,99u),
                        " cpu_tick_p95_ms=",percentileMs(perfWindow.tickUs,95u),
                        " cpu_anim_p95_ms=",percentileMs(perfWindow.animationUs,95u),
                        " cpu_pose_refresh_p95_ms=",percentileMs(perfWindow.poseRefreshUs,95u),
                        " frame_started=",perfWindow.framesStarted,
                        " frame_submitted=",perfWindow.framesSubmitted,
                        " fence_miss=",perfWindow.fenceMisses,
                        " frames_in_flight=",Resources::MaxFramesInFlight,
                        " ssao_buffers=",renderer.ssaoBuffersAllocated() ? 1 : 0,
                        " npc=",npcCount,
                        " npc_full_pose=",npcAnimation.fullPose,
                        " npc_events_only=",npcAnimation.eventsOnly,
                        " mem_footprint_mb=",memoryMiB(mem.footprintBytes,mem.footprintValid),
                        " mem_available_mb=",memoryMiB(mem.availableBytes,mem.availableValid),
                        " mem_ceiling_mb=",memoryMiB(ceiling,ceilingValid),
                        " thermal=",MemoryInfo::thermalStateName(mem.thermal),
                        " entitlement_requested=",mem.increasedMemoryLimitRequested ? 1 : 0,
                        " entitlement_present=",entitlementPresent);
  Log::i(line.c_str());
  resetPerfWindow(nowUs);
  }
#endif

template<Tempest::KeyEvent::KeyType k>
void MainWindow::onMarvinKey() {
  switch(k) {
    case Event::K_F2:
      if(Gothic::inst().isMarvinEnabled()) {
        console.resize(w(),h());
        console.setFocus(true);
        console.exec();
        }
      break;
    case Event::K_F3:
      setFullscreen(!SystemApi::isFullscreen(hwnd()));
      break;
    case Event::K_F4:
      if(Gothic::inst().isMarvinEnabled()) {
        auto camera = Gothic::inst().camera();
        auto pl = Gothic::inst().player();
        if(camera!=nullptr && pl!=nullptr) {
          camera->setMarvinMode(Camera::M_Normal);
          camera->reset(pl);
          }
        }
      break;
    case Event::K_F5: {
      const bool useQuickSaveKeys = Gothic::settingsGetI("GAME", "useQuickSaveKeys")!=0;
#ifdef NDEBUG
      const bool debug = false;
#else
      const bool debug = true;
#endif
      if(!debug && Gothic::inst().isMarvinEnabled() && !dialogs.isActive()) {
        if(auto camera = Gothic::inst().camera()) {
          camera->setMarvinMode(Camera::M_Freeze);
          }
        }
      else if(Gothic::inst().isInGameAndAlive() && !Gothic::inst().isPause() && useQuickSaveKeys) {
        Gothic::inst().quickSave();
        }
      break;
      }

    case Event::K_F6:
      if(Gothic::inst().isMarvinEnabled() && !dialogs.isActive()) {
        auto camera = Gothic::inst().camera();
        auto pl     = Gothic::inst().player();
        auto inter  = pl!=nullptr ? pl->interactive() : nullptr;
        if(camera!=nullptr && inter==nullptr)
          camera->setMarvinMode(Camera::M_Free);
        }
      break;
    case Event::K_F7:
      if(Gothic::inst().isMarvinEnabled() && !dialogs.isActive()) {
        if(auto camera = Gothic::inst().camera()) {
          camera->setMarvinMode(Camera::M_Pinned);
          }
        }
      break;
    case Event::K_F8:
      //player.marvinF8();
      break;

    case Event::K_F9: {
      const bool useQuickSaveKeys = Gothic::settingsGetI("GAME", "useQuickSaveKeys")!=0;
      if(Gothic::inst().isMarvinEnabled()) {
        if(runtimeMode==R_Normal)
          runtimeMode = R_Suspended; else
          runtimeMode = R_Normal;
        }
      else if(!Gothic::inst().isPause() && useQuickSaveKeys) {
        Gothic::inst().quickLoad();
        }
      break;
      }
    case Event::K_F10:
      if(runtimeMode==R_Suspended)
        runtimeMode = R_Step;
      break;
    case Event::K_P:
      if(Gothic::inst().isMarvinEnabled()) {
        if(auto p = Gothic::inst().player()) {
          auto pos = p->position();
          string_frm buf("Position: ", pos.x,'/',pos.y,'/',pos.z);
          Gothic::inst().onPrint(buf);
          }
        }
      break;
    }
  }

uint64_t MainWindow::tick() {
  auto time = Application::tickCount();
  auto dt   = time-lastTick;
  // NOTE: limit to ~200 FPS in game logic to avoid math issues
  if(dt<5)
    return 0;
  lastTick  = time;

#if defined(__MOBILE_PLATFORM__)
  mobileUi.tick();
  gamepad.tick(dt);
  if(const PadCtx pc = padContext(); pc!=lastPadCtx) {
    lastPadCtx   = pc;                       // flash the controls-help on context change
    padHintUntil = Application::tickCount() + 4000;
    }
  // Damage haptic: pulse when the player's HP drops.
  if(auto w = Gothic::inst().world(); w!=nullptr && w->player()!=nullptr) {
    const int hp = w->player()->attribute(ATR_HITPOINTS);
    if(lastPlayerHp>0 && hp<lastPlayerHp)
      Haptics::impact(Haptics::Heavy);
    lastPlayerHp = hp;
    } else {
    lastPlayerHp = -1;
    }
#endif

  auto st = Gothic::inst().checkLoading();
  if(st==Gothic::LoadState::Finalize || st==Gothic::LoadState::FailedLoad || st==Gothic::LoadState::FailedSave) {
    Gothic::inst().finishLoading();
    if(st==Gothic::LoadState::FailedLoad)
      rootMenu.setMainMenu();
    if(st==Gothic::LoadState::FailedSave)
      Gothic::inst().onPrint("unable to write savegame file");
    return 0;
    }
  else if(st!=Gothic::LoadState::Idle) {
    if(st==Gothic::LoadState::Loading)
      GameMusic::inst().setMusic(GameMusic::SysLoading); else
      rootMenu.processMusicTheme();
    return 0;
    }

#if defined(__IOS__)
  // The save request owns the next rendered frame while its thumbnail is
  // captured. Keep gameplay frozen, but continue rendering the immediate
  // saving feedback until the regular GPU fence permits a safe readback.
  if(pendingSave.active())
    return 0;
#endif

  video.tick();
  if(video.isActive())
    return 0;

  if(Gothic::inst().isPause()) {
    return 0;
    }

  if(dt>50)
    dt=50;

  if(runtimeMode==R_Step) {
    runtimeMode = R_Suspended;
    dt = 1000/60; //60 fps
    }
  else if(runtimeMode==R_Suspended) {
    auto camera = Gothic::inst().camera();
    if(camera!=nullptr && camera->isFree()) {
      tickMouse(dt);
      }
    update();
    return 0;
    }

  dialogs.tick(dt);
  inventory.tick(dt);
  Gothic::inst().tick(dt);
  player.tickFocus();

  if(dialogs.isActive())
    ;//clearInput();
  if(document.isActive())
    clearInput();
  tickMouse(dt);
  player.tickMove(dt);
  update();
  return dt;
  }

void MainWindow::updateAnimation(uint64_t dt) {
  Gothic::inst().updateAnimation(dt);
  }

void MainWindow::tickCamera(uint64_t dt) {
  auto pcamera = Gothic::inst().camera();
  auto pl      = Gothic::inst().player();
  if(pcamera==nullptr)
    return;

  auto&      camera       = *pcamera;
  const auto ws           = pl!=nullptr ? pl->weaponState() : WeaponState::NoWeapon;
  const bool meleeFocus   = (ws==WeaponState::Fist ||
                             ws==WeaponState::W1H  ||
                             ws==WeaponState::W2H);
  auto       pos          = pl!=nullptr ? pl->cameraBone(camera.isFirstPerson()) : Vec3();

  if(!camera.isCutscene() && !camera.isFree()) {
    const bool fs = SystemApi::isFullscreen(hwnd());
    if(!fs && mouseP[Event::ButtonLeft]) {
      camera.setSpin(camera.spin());
      camera.setTarget(pos);
      }
    else if(dialogs.isActive() && !dialogs.isMobsiDialog()) {
      dialogs.dialogCamera(camera);
      }
    else if(inventory.isActive()) {
      camera.setTarget(pos);
      }
    else if(player.focus().npc!=nullptr && meleeFocus && pl!=nullptr) {
      auto spin = camera.spin();
      spin.y = pl->rotation();
      camera.setSpin(spin);
      camera.setTarget(pos);
      }
    else if(pl!=nullptr && !camera.isFree()) {
      auto spin = camera.spin();
      if(pl->interactive()==nullptr && !pl->isDown())
        spin.y = pl->rotation();
      if(pl->isDive() && !camera.isMarvin())
        spin.x = -pl->rotationY();
      camera.setSpin(spin);
      camera.setTarget(pos);
      }
    }

  if(dt==0)
    return;
  if(camera.isToggleEnabled() && !camera.isCutscene())
    camera.setMode(solveCameraMode());
  camera.tick(dt);
  }

Camera::Mode MainWindow::solveCameraMode() const {
  const auto camera = Gothic::inst().camera();
  if(camera!=nullptr && camera->isFree())
    return Camera::Normal;

  if(dialogs.isActive())
    return Camera::Dialog;

  if(camera!=nullptr && camera->isFirstPerson())
    return Camera::FirstPerson;

  if(inventory.isOpen()==InventoryMenu::State::Equip ||
     inventory.isOpen()==InventoryMenu::State::Ransack)
    return Camera::Inventory;

  if(auto pl=Gothic::inst().player()) {
    if(pl->interactive()!=nullptr)
      return Camera::Mobsi;
    }

  if(auto pl = Gothic::inst().player()) {
    if(pl->isDead())
      return Camera::Death;
    if(pl->isDive())
      return Camera::Dive;
    if(pl->isSwim())
      return Camera::Swim;
    if(pl->isFallingDeep())
      return Camera::Fall;
    bool g2 = Gothic::inst().version().game==2;
    switch(pl->weaponState()){
      case WeaponState::Fist:
      case WeaponState::W1H:
      case WeaponState::W2H:
        return Camera::Melee;
      case WeaponState::Bow:
      case WeaponState::CBow:
        return g2 ? Camera::Ranged : Camera::Normal;
      case WeaponState::Mage:
        return g2 ? Camera::Ranged : Camera::Melee;
      case WeaponState::NoWeapon:
        return Camera::Normal;
      }
    }

  return Camera::Normal;
  }

void MainWindow::startGame(std::string_view slot) {
  // gothic.emitGlobalSound(gothic.loadSoundFx("NEWGAME"));

#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
  logMemorySnapshot("new_game_requested");
#endif

  if(Gothic::inst().checkLoading()==Gothic::LoadState::Idle){
    setGameImpl(nullptr);
    }

  Gothic::inst().startLoad("LOADING.TGA",[slot=std::string(slot)](std::unique_ptr<GameSession>&& game){
    game = nullptr; // clear world-memory now
    std::unique_ptr<GameSession> w(new GameSession(slot));
    return w;
    });

  background = Texture2d();
  update();
  }

void MainWindow::loadGame(std::string_view slot) {
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
  logMemorySnapshot("load_game_requested");
#endif

  if(Gothic::inst().checkLoading()==Gothic::LoadState::Idle){
    setGameImpl(nullptr);
    }

  Gothic::inst().setBenchmarkMode(Benchmark::None);
  Gothic::inst().startLoad("LOADING.TGA",[slot=std::string(slot)](std::unique_ptr<GameSession>&& game){
    game = nullptr; // clear world-memory now
    Tempest::RFile file(slot);
    Serialize      s(file);
    std::unique_ptr<GameSession> w(new GameSession(s));
    return w;
    });

  background = Texture2d();
  update();
  }

void MainWindow::saveGame(std::string_view slot, std::string_view name) {
  if(dialogs.isActive())
    return;
  if(auto w = Gothic::inst().world(); w!=nullptr && w->currentCs()!=nullptr)
    return;

#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
  logMemorySnapshot("save_game_requested");
#endif

#if defined(__IOS__)
  if(pendingSave.active() || Gothic::inst().checkLoading()!=Gothic::LoadState::Idle)
    return;

  // A GPU readback from this input callback used to collide with Metal's
  // active encoder. Queue it for the normal render command instead. The local
  // pending state makes the saving banner visible on the very next frame,
  // before screenshot preparation or save serialization can do any work.
  pendingSave.slot        = std::string(slot);
  pendingSave.name        = std::string(name);
  pendingSave.stage       = PendingSave::Stage::CaptureRequested;
  Gothic::inst().setLoadingProgress(0);
  if(!renderer.failureReason().empty()) {
    if(!rendererFailureSettled) {
      rendererFailureSettled = renderer.waitIdle();
      if(!rendererFailureSettled) {
        update();
        return;
        }
      }
    startPendingSave(Pixmap(4,4,TextureFormat::RGBA8));
    try {
      Log::e("[save] RendererIOS is stopped; saving with a CPU placeholder");
      }
    catch(...) {
      }
    return;
    }
  update();
  return;
#endif

  auto screen = std::make_shared<Pixmap>(renderer.screenshot());
  Gothic::inst().startSave(Texture2d(),
    [slot=std::string(slot),name=std::string(name),screen=std::move(screen)](std::unique_ptr<GameSession>&& game){
    if(!game)
      return std::move(game);

    Tempest::WFile f(slot);
    Serialize      s(f);
    game->save(s,name,*screen);

    // no print yet, because threading
    // gothic.print("Game saved");
    return std::move(game);
    });

  update();
  }

#if defined(__IOS__)
void MainWindow::startPendingSave(Pixmap&& preview) {
  pendingSave.preview = std::move(preview);
  pendingSave.stage   = PendingSave::Stage::ReadyCpu;
  startPendingSave();
  }

void MainWindow::startPendingSave() {
  if(pendingSave.stage!=PendingSave::Stage::ReadyCpu)
    return;

  // Copying here gives the pending request a strong exception guarantee: if
  // allocating the callback state fails, the CPU preview and destination stay
  // available for the next frame's retry.
  auto screen = std::make_shared<Pixmap>(pendingSave.preview);
  auto slot   = pendingSave.slot;
  auto name   = pendingSave.name;

  Gothic::inst().startSave(Texture2d(),
    [slot=std::move(slot),name=std::move(name),screen=std::move(screen)](std::unique_ptr<GameSession>&& game){
      if(!game)
        return std::move(game);
      Tempest::WFile f(slot);
      Serialize      s(f);
      game->save(s,name,*screen);
      return std::move(game);
      });
  // Keep the request intact until startSave has accepted the callback. If an
  // allocation/thread-start failure escapes, the next frame can retry the same
  // slot instead of silently losing it or saving to an empty path.
  pendingSave.preview = Pixmap();
  pendingSave.slot.clear();
  pendingSave.name.clear();
  pendingSave.stage = PendingSave::Stage::None;
  update();
  }

void MainWindow::processPendingSave() {
  if(pendingSave.stage==PendingSave::Stage::ReadyCpu) {
    if(!renderer.failureReason().empty() && !rendererFailureSettled)
      return;
    startPendingSave();
    return;
    }
  if(pendingSave.stage!=PendingSave::Stage::AwaitingGpu)
    return;

  Pixmap preview;
  try {
    if(!renderer.savePreviewReady())
      return;
    // savePreviewReady() can be the call that discovers a terminal Metal
    // error. Defer callbacks that may release world/UI owners until the common
    // fatal path has settled every older slot.
    if(!renderer.failureReason().empty() && !rendererFailureSettled)
      return;
    preview = renderer.takeSavePreview();
    }
  catch(const std::exception& e) {
    if(!renderer.failureReason().empty() && !rendererFailureSettled)
      return;
    preview = Pixmap(4,4,TextureFormat::RGBA8);
    try {
      Log::e("[save] preview readback failed; using placeholder: ",e.what());
      }
    catch(...) {
      }
    }
  catch(...) {
    if(!renderer.failureReason().empty() && !rendererFailureSettled)
      return;
    preview = Pixmap(4,4,TextureFormat::RGBA8);
    try {
      Log::e("[save] preview readback failed; using placeholder: ",
             ExceptionDump::describe(std::current_exception()));
      }
    catch(...) {
      }
    }
  startPendingSave(std::move(preview));
  }
#endif

void MainWindow::onVideo(std::string_view fname) {
  if(Gothic::inst().isBenchmarkMode())
    return;
  video.pushVideo(fname);
  }

void MainWindow::onStartLoading() {
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
  flushPerfWindow(perfNowUs(),true);
  logMemorySnapshot("loadsave_begin");
#endif
  player   .clearInput();
#if defined(__MOBILE_PLATFORM__)
  // A ring can own a display-only Item tied to the outgoing World. Destroy it
  // synchronously before the loader thread takes the GameSession away.
  gamepad.ringCancel();
#endif
  inventory.onWorldChanged();
  dialogs  .onWorldChanged();
  }

void MainWindow::onWorldLoaded() {
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
  flushPerfWindow(perfNowUs(),true);
#endif
  dMouse = Point();

  if(Gothic::inst().isBenchmarkMode()) {
    if(auto world = Gothic::inst().world()) {
      const TriggerEvent evt("TIMEDEMO","",world->tickCount(),TriggerEvent::T_Trigger);
      world->execTriggerEvent(evt);
      }
    benchmark.clear();
    }

  player   .clearInput();
  inventory.onWorldChanged();
  dialogs  .onWorldChanged();

  if(auto c = Gothic::inst().camera()) {
    const auto size = renderer.drawableSize();
    c->setViewport(uint32_t(size.w),uint32_t(size.h));
    }

  renderer.onWorldChanged();

  if(auto pl = Gothic::inst().player())
    rootMenu.setPlayer(*pl);

  if(auto pl = Gothic::inst().player())
    pl->multSpeed(1.f);
  lastTick = Application::tickCount();
  player.clearFocus();
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
  logMemorySnapshot("loadsave_complete");
#endif
  }

void MainWindow::onSessionExit() {
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
  flushPerfWindow(perfNowUs(),true);
  logMemorySnapshot("session_exit");
#endif
  rootMenu.setMainMenu();
  }

void MainWindow::onBenchmarkFinished() {
  if(benchmark.numFrames==0)
    return;

  double fps  = benchmark.fpsSum/double(benchmark.numFrames);
  double low1 = 0;
  size_t num1 = 0;
  for(size_t i=0; i<benchmark.low1procent.size(); ++i) {
    auto v = benchmark.low1procent[i];
    if(v<=0)
      continue;
    low1 += 1000.0/double(v);
    num1 += 1;
    }
  low1 = num1>0 ? low1/double(num1) : 0.0;
  benchmark.clear();

  string_frm str("Benchmark: low 1% = ", low1, " fps = ", fps);
  Log::i(str.c_str());
  console.printLine(str);

  if(Gothic::inst().isBenchmarkModeCi()) {
    Log::i("Exiting benchmark");
    Tempest::SystemApi::exit();
    return;
    }

  console.setFocus(true);
  console.exec();
  }

void MainWindow::setGameImpl(std::unique_ptr<GameSession> &&w) {
  Gothic::inst().setGame(std::move(w));
  }

void MainWindow::clearInput() {
  player.clearInput();
  std::memset(mouseP,0,sizeof(mouseP));
  }

void MainWindow::setFullscreen(bool fs) {
  SystemApi::setAsFullscreen(hwnd(),fs);
  }

bool MainWindow::rendererOperational() {
  const auto reason = renderer.failureReason();
  if(reason.empty())
    return true;
  if(!rendererFailureReported) {
    rendererFailureReported = true;
    std::array<char,513> message = {};
    const size_t length = std::min(reason.size(),message.size()-1u);
    std::memcpy(message.data(),reason.data(),length);
    try {
      SystemMsg::fatal("GPU rendering stopped",message.data());
      }
    catch(...) {
      }
    try {
      Log::e("RendererIOS stopped the frame loop: ",message.data());
      }
    catch(...) {
      }
    }
  if(!rendererFailureSettled) {
    // A fatal fence is terminal, but older slots may still reference world,
    // inventory, or video resources. Settle them before save/load callbacks
    // can release those owners.
    rendererFailureSettled = renderer.waitIdle();
    }

  if(!rendererFailureSettled)
    return false;

#if defined(__IOS__)
  if(pendingSave.stage==PendingSave::Stage::CaptureRequested) {
    startPendingSave(Pixmap(4,4,TextureFormat::RGBA8));
    try {
      Log::e("[save] RendererIOS stopped before capture; using a CPU placeholder");
      }
    catch(...) {
      }
    }
  else if(pendingSave.stage==PendingSave::Stage::AwaitingGpu ||
          pendingSave.stage==PendingSave::Stage::ReadyCpu) {
    processPendingSave();
    }
#endif
  pumpLoadingAfterRendererFailure();
  return false;
  }

void MainWindow::pumpLoadingAfterRendererFailure() {
  const auto state = Gothic::inst().checkLoading();
  if(state!=Gothic::LoadState::Finalize &&
     state!=Gothic::LoadState::FailedLoad &&
     state!=Gothic::LoadState::FailedSave)
    return;

  Gothic::inst().finishLoading();
  if(state==Gothic::LoadState::FailedLoad)
    rootMenu.setMainMenu();
  if(state==Gothic::LoadState::FailedSave)
    Gothic::inst().onPrint("unable to write savegame file");
  }

void MainWindow::render(){
  try {
    static uint64_t time=Application::tickCount();

#if defined(__IOS__)
    // No render encoder exists at this point. A preview submitted by an older
    // regular frame may therefore be read back safely once its fence signals.
    processPendingSave();
#endif
    if(!rendererOperational())
      return;

#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
    processMemoryEvents();
    const uint64_t perfFrameStart = perfNowUs();
    beginPerfFrame(perfFrameStart);
#endif

    static bool once=true;
    if(once) {
      Gothic::inst().emitGlobalSoundWav("GAMESTART.WAV");
      once=false;
      }

    if(T_UNLIKELY(Gothic::inst().isBenchmarkModeCi())) {
      const auto st = Gothic::inst().checkLoading();
      if(st==Gothic::LoadState::Loading) {
        // skip loading frames in benchmark-ci mode, for sake of easier tooling
        return;
        }
      }

    /*
      Note: game update goes first
      once player position is updated, animation bones(cameraBone in particular) can be updated
      lastly - camera position
      */
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
    const uint64_t tickStart = perfNowUs();
#endif
    const uint64_t dt = tick();
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
    perfWindow.tickUs.push_back(perfSample(perfNowUs()-tickStart));

    const uint64_t animationStart = perfNowUs();
#endif
    updateAnimation(dt);
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
    perfWindow.animationUs.push_back(perfSample(perfNowUs()-animationStart));
#endif
    tickCamera(dt);

    auto frame = renderer.beginFrame();
    if(!frame.has_value()) {
      if(!rendererOperational())
        return;
      // GPU rendering is not done, pass to next frame
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
      perfWindow.fenceMisses++;
      flushPerfWindow(perfNowUs(),false);
#endif
      std::this_thread::yield();
      return;
      }
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
    const uint64_t poseRefreshStart = perfNowUs();
#endif
    if(auto* world = Gothic::inst().world())
      world->refreshAnimationPose();
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
    perfWindow.poseRefreshUs.push_back(perfSample(perfNowUs()-poseRefreshStart));
#endif

    if(video.isActive()) {
      renderer.prepareVideo(*frame,video);
      uiLayer.clear();
      PaintEvent p(uiLayer,atlas,this->w(),this->h());
      video.paintEvent(p);
      }
    else if(needToUpdate() || Gothic::inst().checkLoading()!=Gothic::LoadState::Idle) {
      dispatchPaintEvent(uiLayer,atlas);

      numOverlay.clear();
      PaintEvent p(numOverlay,atlas,this->w(),this->h());
#if defined(__MOBILE_PLATFORM__)
      if(!gamepad.ringOpen())
#endif
      inventory.paintNumOverlay(p);
      }

#if defined(__IOS__)
    const bool captureSavePreview = pendingSave.stage==PendingSave::Stage::CaptureRequested;
#else
    constexpr bool captureSavePreview = false;
#endif

    [[maybe_unused]] const auto result = renderer.submitFrame(
      std::move(*frame),
      RendererIOS::FrameInput{uiLayer,numOverlay,inventory,video.isActive(),captureSavePreview});
#if defined(__IOS__)
    if(captureSavePreview && result.savePreviewQueued)
      pendingSave.stage   = PendingSave::Stage::AwaitingGpu;
#endif
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
    submitPerfFrame(perfNowUs());
#endif

#if defined(__IOS__)
    // UIKit and the game fibers share the main thread. Sleeping here also blocks
    // CADisplayLink, so after a 33 ms sleep the game has to wait for a later
    // display callback and an intended 30 FPS becomes roughly 27 FPS. Let the
    // native display link schedule Off/30/60 directly instead. Preserve the old
    // default 60 FPS menu policy, while an explicit user cap wins everywhere.
    uint32_t displayFps = maxFpsTarget;
    if(displayFps==0 && !Gothic::inst().isInGame() && !video.isActive())
      displayFps = 60u;
    if(iosFrameRateTarget!=displayFps) {
      tempestIosSetPreferredFrameRate(int(displayFps));
      iosFrameRateTarget = displayFps;
      }

    auto t = Application::tickCount();
#else
    uint64_t targetPeriodMs = 0;
    if(!Gothic::inst().isInGame() && !video.isActive())
      targetPeriodMs = 16u;
    targetPeriodMs = std::max(targetPeriodMs,maxFpsInv);

    auto t = Application::tickCount();
    if(targetPeriodMs>0 && t-time<targetPeriodMs) {
      const uint32_t delay = uint32_t(targetPeriodMs-(t-time));
      Application::sleep(delay);
      t += delay;
      }
#endif

    fps.push(t-time);
    if(Gothic::inst().isBenchmarkMode() && Gothic::inst().world()!=nullptr && Gothic::inst().world()->currentCs()!=nullptr)
      benchmark.push(t-time);
    time = t;
#if defined(OPENGOTHIC_PERF_DIAGNOSTICS)
    flushPerfWindow(perfNowUs(),false);
#endif
    }
  catch(const Tempest::SwapchainSuboptimal&) {
    try {
      Log::e("swapchain is outdated - reset renderer");
      }
    catch(...) {
      }
    try {
      renderer.resize();
      if(auto* camera = Gothic::inst().camera()) {
        const auto size = renderer.drawableSize();
        camera->setViewport(uint32_t(size.w),uint32_t(size.h));
        }
      }
    catch(const std::exception& e) {
      try {
        Log::e("swapchain reset failed: ",e.what());
        }
      catch(...) {
        }
      try { renderer.waitIdle(); } catch(...) {}
      }
    catch(...) {
      try {
        Log::e("swapchain reset failed with a non-std/ObjC exception: ",
               ExceptionDump::describe(std::current_exception()));
        }
      catch(...) {
        }
      try { renderer.waitIdle(); } catch(...) {}
      }
    }
  catch(const std::exception& e) {
    // A stray exception in the frame loop (e.g. during save/load finalize)
    // must not abort the whole app via std::terminate. Log the cause and try
    // to recover the device instead of crashing to the home screen.
    try {
      Log::e("unhandled exception in render loop: ", e.what());
      }
    catch(...) {
      }
    if(renderer.failureReason().empty())
      renderer.waitIdle();
    }
  catch(...) {
    // Objective-C / Metal exceptions are NOT std::exception; on the Apple ABI
    // they still unwind through C++ and terminate the app if unhandled. Catch
    // them here too (e.g. a Metal validation NSException on the saving screen)
    // and log their identity, so the device log names the throw site.
    try {
      Log::e("unhandled non-std/ObjC exception in render loop: ",
             ExceptionDump::describe(std::current_exception()));
      }
    catch(...) {
      }
    if(renderer.failureReason().empty())
      renderer.waitIdle();
    }
  }

double MainWindow::Fps::get() const {
  uint64_t sum=0,num=0;
  for(auto& i:dt)
    if(i>0) {
      sum+=i;
      num++;
      }
  if(num==0 || sum==0)
    return 60;
  uint64_t fps = (1000*100*num)/sum;
  return double(fps)/100.0;
  }

void MainWindow::Fps::push(uint64_t t) {
  for(size_t i=9;i>0;--i)
    dt[i]=dt[i-1];
  dt[0]=t;
  }

void MainWindow::BenchmarkData::push(uint64_t t) {
  fpsSum += t>0 ? (1000.0/double((t))) : 60.0;
  numFrames++;
  auto at = std::lower_bound(low1procent.begin(), low1procent.end(), t, std::greater<uint64_t>());
  low1procent.insert(at, t);
  low1procent.resize(std::min(low1procent.size(), (numFrames+99)/100));
  }

void MainWindow::BenchmarkData::clear() {
  low1procent.reserve(128);
  low1procent.clear();
  numFrames = 0;
  fpsSum = 0;
  }
