#include "touchinput.h"

#include <Tempest/Painter>
#include <Tempest/Event>

#include "game/playercontrol.h"
#include "utils/gamepad.h"
#include "mainwindow.h"
#include "gothic.h"

using namespace Tempest;
using A = KeyCodec::Action;
using M = KeyCodec::Mapping;

TouchInput::TouchInput(MainWindow& owner, PlayerControl& ctrl)
  : owner(owner), ctrl(ctrl) {
  }

std::array<TouchInput::Btn,6> TouchInput::layout() const {
  const int W = w(), H = h();
  const int s  = H/8;             // button size
  const int m  = H/40;            // margin
  const int bx = W - s - m;       // right column x
  const int by = H - s - m;       // bottom row y
  return {{
    { bx,         by,         s, A::ActionGeneric, 0.90f, 0.30f, 0.30f }, // interact/attack
    { bx,         by-(s+m),   s, A::Jump,          0.30f, 0.60f, 0.90f }, // jump
    { bx-(s+m),   by,         s, A::Weapon,        0.90f, 0.80f, 0.30f }, // draw weapon
    { bx-(s+m),   by-(s+m),   s, A::Sneak,         0.50f, 0.90f, 0.40f }, // sneak/crouch
    { W-s-m,      m,          s, A::Inventory,     0.85f, 0.85f, 0.85f }, // inventory
    { W-2*(s+m),  m,          s, A::Escape,        0.85f, 0.85f, 0.85f }, // menu/escape
  }};
  }

std::array<TouchInput::MBtn,6> TouchInput::menuLayout() const {
  const int W = w(), H = h();
  const int s  = H/9;
  const int m  = H/40;
  using E = Tempest::Event;
  const int cx = m + s;           // d-pad centre column (bottom-left)
  const int by = H - m;           // bottom edge
  return {{
    { cx,          by-3*s-2*m,  s, E::K_Up,     0.85f, 0.85f, 0.85f }, // up
    { cx,          by-s,        s, E::K_Down,   0.85f, 0.85f, 0.85f }, // down
    { cx-(s+m),    by-2*s-m,    s, E::K_Left,   0.80f, 0.80f, 0.90f }, // decrease value
    { cx+(s+m),    by-2*s-m,    s, E::K_Right,  0.80f, 0.80f, 0.90f }, // increase value
    { W-2*(s+m),   by-s,        s, E::K_Return, 0.40f, 0.90f, 0.40f }, // OK
    { W-(s+m),     by-s,        s, E::K_ESCAPE, 0.90f, 0.40f, 0.40f }, // back
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
    { cx,          by-3*s-2*m,  s, E::K_Up,     0.85f, 0.85f, 0.85f }, // previous choice
    { cx,          by-s,        s, E::K_Down,   0.85f, 0.85f, 0.85f }, // next choice
    { W-2*(s+m),   by-s,        s, E::K_Return, 0.40f, 0.90f, 0.40f }, // select choice
    { W-(s+m),     by-s,        s, E::K_ESCAPE, 0.90f, 0.40f, 0.40f }, // skip spoken line
  }};
  }

void TouchInput::paintEvent(PaintEvent& e) {
  if(Gamepad::poll().connected)
    return;                          // a gamepad drives the UI -> hide the touch overlay
  Painter p(e);
  switch(owner.padContext()) {
    case PadCtx::World: {
      const int H  = h();
      const int ms = H/3, mm = H/20;
      p.setBrush(Color(1.f, 1.f, 1.f, 0.10f));           // movement pad (bottom-left)
      p.drawRect(mm, H-ms-mm, ms, ms);
      for(auto& b:layout()) {
        p.setBrush(Color(b.r, b.g, b.b, 0.22f));
        p.drawRect(b.x, b.y, b.s, b.s);
        }
      break;
      }
    case PadCtx::Dialog:
      for(auto& b:dialogLayout()) {
        p.setBrush(Color(b.r, b.g, b.b, 0.30f));
        p.drawRect(b.x, b.y, b.s, b.s);
        }
      break;
    case PadCtx::Menu:
    case PadCtx::Inventory:
      for(auto& b:menuLayout()) {
        p.setBrush(Color(b.r, b.g, b.b, 0.30f));
        p.drawRect(b.x, b.y, b.s, b.s);
        }
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

  if(ctx==PadCtx::World) {
    for(auto& b:layout())
      if(pos.x>=b.x && pos.x<b.x+b.s && pos.y>=b.y && pos.y<b.y+b.s) {
        // Escape/Inventory are window-level actions, not PlayerControl ones (B1).
        if(b.act==A::Escape || b.act==A::Inventory) {
          owner.uiAction(b.act);
          return;
          }
        ctrl.onKeyPressed(b.act, Event::K_NoKey, M::Primary);
        btnDown[id] = b.act;
        return;
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

  // UI contexts: a tap fires one synthetic key, routed to the active widget
  // (menu / dialogue / inventory) by MainWindow::dispatchKey.
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
  e.ignore();                     // loading etc. -> not our screen
  }

void TouchInput::mouseDragEvent(MouseEvent& e) {
  if(owner.padContext()!=PadCtx::World) { e.ignore(); return; }

  const Point pos = e.pos();
  const int   id  = e.mouseID;

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
    set(2, dx < -dz, A::Left);
    set(3, dx >  dz, A::Right);
    return;
    }
  e.ignore();
  }

void TouchInput::mouseUpEvent(MouseEvent& e) {
  const int id = e.mouseID;

  auto it = btnDown.find(id);
  if(it!=btnDown.end()) {
    ctrl.onKeyReleased(it->second, M::Primary);
    btnDown.erase(it);
    return;
    }

  if(id==moveId) {
    if(mv[0]) ctrl.onKeyReleased(A::Forward, M::Primary);
    if(mv[1]) ctrl.onKeyReleased(A::Back,    M::Primary);
    if(mv[2]) ctrl.onKeyReleased(A::Left,    M::Primary);
    if(mv[3]) ctrl.onKeyReleased(A::Right,   M::Primary);
    mv[0]=mv[1]=mv[2]=mv[3]=false;
    moveId = -1;
    return;
    }

  if(id==lookId) { lookId = -1; return; }

  e.ignore();
  }
