#pragma once

#include <Tempest/Widget>
#include <array>
#include <unordered_map>

#include "utils/keycodec.h"
#include "ui/padglyph.h"
#include "ui/padsystemgesture.h"

class PlayerControl;
class MainWindow;

// On-screen virtual gamepad for when no controller is connected. Context-aware
// via MainWindow::padContext():
//  * World     — full pad: left-bottom move pad, right side = camera, and glyph
//    buttons for A/B/X/Y, RB/RT/LB/LT, L3/R3, D-pad, View/Menu, each wired to its
//    action (rings open a radial that touch then aims + commits).
//  * Menu/Inv  — on-screen D-pad + OK/Back.
//  * Dialogue  — Up/Down pick a choice, Select confirms, Skip skips the line.
// Multitouch via MouseEvent::mouseID.
class TouchInput : public Tempest::Widget {
  public:
    TouchInput(MainWindow& owner, PlayerControl& ctrl);

    void tick();
    void paintEvent(Tempest::PaintEvent& e);
    void mouseDownEvent(Tempest::MouseEvent& e);
    void mouseDragEvent(Tempest::MouseEvent& e);
    void mouseUpEvent(Tempest::MouseEvent& e);

  private:
    // What a World button does when tapped.
    enum class TAct : uint8_t {
      Key, Interact, Special, Lt, Rt, Lb, Rb,
      WeaponsRing, ItemRing, Lock, JournalOrFocus, MapOrFocus,
      SystemView, SystemMenu
      };
    struct Btn  { int x, y, s; PadGlyph::Btn glyph; TAct kind; KeyCodec::Action act; };
    struct MBtn { int x, y, s; Tempest::Event::KeyType key; };
    struct PageBtn { int x, y, s; PadGlyph::Btn glyph; int direction; };

    std::array<Btn,16> layout()       const;   // full virtual pad (World)
    std::array<MBtn,6> menuLayout()   const;   // menu / inventory: dpad + ok/back
    std::array<MBtn,4> dialogLayout() const;   // dialogue: up/down/select/skip
    std::array<PageBtn,2> characterPageLayout() const;

    void aimRing(const Tempest::Point& pos);
    void releaseWorldTouches();
    bool dispatchSystemEffect(PadSystemGesture::Effect effect);

    MainWindow&    owner;
    PlayerControl& ctrl;

    int            moveId = -1;   // touch id driving movement
    int            lookId = -1;   // touch id driving camera
    int            ringId = -1;   // touch id aiming an open radial ring
    int            walkId = -1;   // LB temporarily forcing walk outside melee
    int            viewId = -1;   // touch id pressing View
    int            menuId = -1;   // touch id pressing Menu
    PadSystemGesture systemGesture;
    Tempest::Point moveOrigin;
    Tempest::Point lookLast;
    bool           mv[4] = {};    // forward, back, left, right currently pressed
    std::unordered_map<int,KeyCodec::Action> btnDown;
  };
