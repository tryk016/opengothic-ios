#include "touchinput.h"

#include <Tempest/Platform>

// The on-screen pad exists only on mobile: MainWindow instantiates it (and
// defines the pad* bridges it calls) under __MOBILE_PLATFORM__ only, so
// desktop builds compile this TU empty.
#if defined(__MOBILE_PLATFORM__)

#include <Tempest/Painter>
#include <Tempest/Event>
#include <algorithm>

#include "game/playercontrol.h"
#include "utils/gamepad.h"
#include "utils/gthfont.h"
#include "ui/padglyph.h"
#include "resources.h"
#include "mainwindow.h"
#include "gothic.h"

using namespace Tempest;
using A = KeyCodec::Action;
using M = KeyCodec::Mapping;

static PadGlyph::Btn glyphOfKey(Tempest::Event::KeyType k) {
  switch(k) {
    case Tempest::Event::K_Up:     return PadGlyph::DPadUp;
    case Tempest::Event::K_Down:   return PadGlyph::DPadDown;
    case Tempest::Event::K_Left:   return PadGlyph::DPadLeft;
    case Tempest::Event::K_Right:  return PadGlyph::DPadRight;
    case Tempest::Event::K_Return: return PadGlyph::A;
    case Tempest::Event::K_ESCAPE: return PadGlyph::B;
    default:                       return PadGlyph::A;
    }
  }

TouchInput::TouchInput(MainWindow& owner, PlayerControl& ctrl)
  : owner(owner), ctrl(ctrl) {
  }

std::array<TouchInput::Btn,16> TouchInput::layout() const {
  const int W = w(), H = h();
  const int s  = H/11;
  const int m  = H/40;
  const int bx = W-s-m, by = H-s-m;                 // face cluster anchor (bottom-right)
  const int tR = W-s-m, tL = m;                     // shoulder columns
  const int row0 = m, row1 = m+s+m;
  const int dcx = int(float(W)*0.44f), dcy = H-int(float(s)*1.7f)-m;   // d-pad centre
  namespace G = PadGlyph;
  using     K = TAct;
  return {{
    // face
    { bx,        by,        s, G::A, K::Key, A::ActionGeneric },
    { bx,        by-(s+m),  s, G::B, K::Key, A::Jump          },
    { bx-(s+m),  by,        s, G::X, K::Key, A::Sneak         },
    { bx-(s+m),  by-(s+m),  s, G::Y, K::Key, A::Weapon        },
    // shoulders / triggers
    { tR,        row0,      s, G::RB, K::MagicRing, A::ActionGeneric },
    { tR,        row1,      s, G::RT, K::Key,       A::Parade        },
    { tL,        row0,      s, G::LB, K::QSave,     A::ActionGeneric },
    { tL,        row1,      s, G::LT, K::ItemRing,  A::ActionGeneric },
    // stick clicks
    { m,         H-H/3-m-s-m, s, G::L3, K::Key,  A::Walk          },
    { bx,        by-2*(s+m),  s, G::R3, K::Lock, A::ActionGeneric },
    // d-pad, Gothic-Remake style: melee / ranged / two assignable quick slots
    { dcx,       dcy-s, s, G::DPadUp,    K::Key,   A::WeaponMele    },
    { dcx,       dcy+s, s, G::DPadDown,  K::Key,   A::WeaponBow     },
    { dcx-s,     dcy,   s, G::DPadLeft,  K::SlotL, A::ActionGeneric },
    { dcx+s,     dcy,   s, G::DPadRight, K::SlotR, A::ActionGeneric },
    // system
    { W/2-(s+m), m, s, G::View, K::Key, A::Inventory },
    { W/2+m,     m, s, G::Menu, K::Key, A::Escape    },
  }};
  }

std::array<TouchInput::MBtn,6> TouchInput::menuLayout() const {
  const int W = w(), H = h();
  const int s  = H/9;
  const int m  = H/40;
  using E = Tempest::Event;
  const int cx = m + s;
  const int by = H - m;
  return {{
    { cx,          by-3*s-2*m,  s, E::K_Up     },
    { cx,          by-s,        s, E::K_Down   },
    { cx-(s+m),    by-2*s-m,    s, E::K_Left   },
    { cx+(s+m),    by-2*s-m,    s, E::K_Right  },
    { W-2*(s+m),   by-s,        s, E::K_Return },
    { W-(s+m),     by-s,        s, E::K_ESCAPE },
  }};
  }

std::array<TouchInput::MBtn,4> TouchInput::dialogLayout() const {
  const int W = w(), H = h();
  const int s  = H/9;
  const int m  = H/40;
  using E = Tempest::Event;
  const int cx = m + s;
  const int by = H - m;
  return {{
    { cx,          by-3*s-2*m,  s, E::K_Up     },
    { cx,          by-s,        s, E::K_Down   },
    { W-2*(s+m),   by-s,        s, E::K_Return },
    { W-(s+m),     by-s,        s, E::K_ESCAPE },
  }};
  }

void TouchInput::aimRing(const Point& pos) {
  const float R  = float(std::min(w(),h()))/3.f;
  const float nx = float(pos.x - w()/2)/R;
  const float ny = float(h()/2 - pos.y)/R;      // up positive, matching the stick
  owner.padRingAim(nx, ny);
  }

void TouchInput::releaseWorldTouches() {
  if(mv[0]) ctrl.onKeyReleased(A::Forward, M::Primary);
  if(mv[1]) ctrl.onKeyReleased(A::Back,    M::Primary);
  if(mv[2]) ctrl.onKeyReleased(A::RotateL, M::Primary);
  if(mv[3]) ctrl.onKeyReleased(A::RotateR, M::Primary);
  mv[0]=mv[1]=mv[2]=mv[3]=false;
  moveId = -1;
  lookId = -1;

  for(auto& held : btnDown)
    ctrl.onKeyReleased(held.second, M::Primary);
  btnDown.clear();
  }

void TouchInput::paintEvent(PaintEvent& e) {
  if(Gamepad::poll().connected)
    return;                          // a gamepad drives the UI -> hide the touch overlay

  Painter p(e);
  auto&   fnt = Resources::font(Gothic::interfaceScale(this));

  if(owner.padVideoActive()) {
    // A bink is playing: don't cover it with menu buttons, just hint that a tap skips it.
    const char* hint = "Tap to skip";
    const auto  ts   = fnt.textSize(hint);
    fnt.drawText(p, w()-ts.w-h()/20, h()-h()/20, hint);
    return;
    }

  switch(owner.padContext()) {
    case PadCtx::World: {
      const int H  = h();
      const int ms = H/3, mm = H/20;
      PadGlyph::draw(p, fnt, PadGlyph::LStick, mm, H-ms-mm, ms, 0.7f);   // movement pad
      for(auto& b:layout())
        PadGlyph::draw(p, fnt, b.glyph, b.x, b.y, b.s);
      break;
      }
    case PadCtx::Dialog:
      for(auto& b:dialogLayout())
        PadGlyph::draw(p, fnt, glyphOfKey(b.key), b.x, b.y, b.s);
      break;
    case PadCtx::Menu:
    case PadCtx::Inventory:
      for(auto& b:menuLayout())
        PadGlyph::draw(p, fnt, glyphOfKey(b.key), b.x, b.y, b.s);
      break;
    case PadCtx::Loading:
      break;
    }
  }

void TouchInput::mouseDownEvent(MouseEvent& e) {
  if(Gamepad::poll().connected) { e.ignore(); return; }   // gamepad active -> ignore taps

  const Point  pos = e.pos();
  const int    id  = e.mouseID;
  const PadCtx ctx = owner.padContext();

  if(owner.padVideoActive()) { owner.padSkipVideo(); return; }   // any tap skips the intro/cutscene

  if(ctx==PadCtx::World) {
    // A radial ring is open -> this touch aims it, release commits.
    if(owner.padRingOpen()) { ringId = id; aimRing(pos); return; }

    for(auto& b:layout())
      if(pos.x>=b.x && pos.x<b.x+b.s && pos.y>=b.y && pos.y<b.y+b.s) {
        switch(b.kind) {
          case TAct::Key:
            if(b.act==A::Escape || b.act==A::Inventory) { owner.uiAction(b.act); return; }
            ctrl.onKeyPressed(b.act, Event::K_NoKey, M::Primary);
            btnDown[id] = b.act;
            return;
          case TAct::MagicRing:
            releaseWorldTouches(); owner.padOpenMagicRing(); ringId = id; return;
          case TAct::ItemRing:
            releaseWorldTouches(); owner.padOpenItemRing(); ringId = id; return;
          case TAct::Lock:       ctrl.toggleTargetLock();   return;
          case TAct::SlotL:      owner.padUseQuickSlot(0);  return;
          case TAct::SlotR:      owner.padUseQuickSlot(1);  return;
          case TAct::QSave:      owner.padQuickSave();       return;
          }
        }

    const int H = h(), ms = H/3, mm = H/20;
    if(pos.x>=mm && pos.x<mm+ms && pos.y>=H-ms-mm && pos.y<H-mm) {
      moveId = id; moveOrigin = pos;
      return;
      }
    if(pos.x > w()/2) {
      lookId = id; lookLast = pos;
      return;
      }
    e.ignore();
    return;
    }

  // UI contexts: a tap fires one synthetic key, routed to the active widget.
  auto tap = [&](const auto& arr)->bool{
    for(auto& b:arr)
      if(pos.x>=b.x && pos.x<b.x+b.s && pos.y>=b.y && pos.y<b.y+b.s) {
        KeyEvent ev(b.key);
        owner.dispatchKey(ev);
        return true;
        }
    return false;
    };

  if(ctx==PadCtx::Dialog) {
    tap(dialogLayout());
    return;
    }
  if(ctx==PadCtx::Menu || ctx==PadCtx::Inventory) {
    tap(menuLayout());
    return;
    }
  e.ignore();
  }

void TouchInput::mouseDragEvent(MouseEvent& e) {
  if(Gamepad::poll().connected)         { e.ignore(); return; }
  if(owner.padContext()!=PadCtx::World) { e.ignore(); return; }

  const Point pos = e.pos();
  const int   id  = e.mouseID;

  if(id==ringId && owner.padRingOpen()) {
    aimRing(pos);
    return;
    }

  if(id==lookId) {
    const Point d = pos - lookLast;
    lookLast = pos;
    ctrl.onRotateMouse(float(-d.x)*4.f, float(-d.y)*2.f);
    return;
    }

  if(id==moveId) {
    const int H  = h();
    const int dz = H/16;          // dead-zone
    const int dx = pos.x - moveOrigin.x;
    const int dy = pos.y - moveOrigin.y;
    auto set = [&](int idx, bool on, A a){
      if(on && !mv[idx])      { ctrl.onKeyPressed(a, Event::K_NoKey, M::Primary); mv[idx]=true;  }
      else if(!on && mv[idx]) { ctrl.onKeyReleased(a, M::Primary);                mv[idx]=false; }
      };
    set(0, dy < -dz, A::Forward);
    set(1, dy >  dz, A::Back);
    // pad X turns the character (Gothic-classic rotate), it does not strafe
    set(2, dx < -dz, A::RotateL);
    set(3, dx >  dz, A::RotateR);
    return;
    }
  e.ignore();
  }

void TouchInput::mouseUpEvent(MouseEvent& e) {
  const int id = e.mouseID;

  if(id==ringId) {
    if(owner.padRingOpen())
      owner.padRingCommit();
    ringId = -1;
    return;
    }

  auto it = btnDown.find(id);
  if(it!=btnDown.end()) {
    ctrl.onKeyReleased(it->second, M::Primary);
    btnDown.erase(it);
    return;
    }

  if(id==moveId) {
    if(mv[0]) ctrl.onKeyReleased(A::Forward, M::Primary);
    if(mv[1]) ctrl.onKeyReleased(A::Back,    M::Primary);
    if(mv[2]) ctrl.onKeyReleased(A::RotateL, M::Primary);
    if(mv[3]) ctrl.onKeyReleased(A::RotateR, M::Primary);
    mv[0]=mv[1]=mv[2]=mv[3]=false;
    moveId = -1;
    return;
    }

  if(id==lookId) { lookId = -1; return; }

  e.ignore();
  }

#endif
