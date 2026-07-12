#include "paddiagram.h"

#include <Tempest/Painter>
#include <Tempest/Brush>
#include <Tempest/Color>
#include <Tempest/Texture2d>

#include <algorithm>
#include <cstdio>
#include <iterator>

#include "game/constants.h"
#include "utils/gthfont.h"
#include "ui/padglyph.h"
#include "gothic.h"

using namespace Tempest;

namespace {

struct Loc {
  const char* title;
  const char* itemRing;
  const char* move;
  const char* walkRun;
  const char* melee;
  const char* ranged;
  const char* quickSlots;
  const char* parry;
  const char* magicRing;
  const char* weapon;
  const char* jump;
  const char* sneak;
  const char* action;
  const char* camera;
  const char* targetLock;
  const char* inventory;
  const char* gameMenu;
  const char* quickSave;
  const char* quickLoad;
  const char* unstuck;
  const char* hold;
  };

// GthFont maps one byte to one glyph in the game codepage, not UTF-8. PL uses
// CP1250, DE CP1252. Hex escapes are split ("\xB3" "a") wherever the next
// character is a hex digit, or it would be swallowed by the escape.
const Loc& loc() {
  static const Loc en = {
    "Controller layout",
    "Item wheel",
    "Move / Turn",
    "Walk / Run",
    "Melee weapon",
    "Ranged weapon",
    "Quick slot 1 / 2",
    "Parry",
    "Magic wheel",
    "Draw / sheathe weapon",
    "Jump / Climb",
    "Sneak",
    "Action / Attack",
    "Camera",
    "Target lock",
    "Inventory",
    "Game menu",
    "Quick save",
    "Quick load",
    "Teleport when stuck",
    "hold",
    };
  static const Loc de = {
    "Controller-Belegung",
    "Gegenstands-Rad",
    "Bewegen / Drehen",
    "Gehen / Laufen",
    "Nahkampfwaffe",
    "Fernkampfwaffe",
    "Schnellslot 1 / 2",
    "Parieren",
    "Magie-Rad",
    "Waffe ziehen / wegstecken",
    "Springen / Klettern",
    "Schleichen",
    "Aktion / Angriff",
    "Kamera",
    "Ziel fixieren",
    "Inventar",
    "Spielermen\xFC",
    "Schnellspeichern",
    "Schnellladen",
    "Teleport, wenn man feststeckt",
    "halten",
    };
  static const Loc pl = {
    "Uk\xB3" "ad kontrolera",
    "Ko\xB3o przedmiot\xF3w",
    "Ruch / skr\xEAt",
    "Ch\xF3" "d / bieg",
    "Bro\xF1 bia\xB3" "a",
    "Bro\xF1 dystansowa",
    "Szybki slot 1 / 2",
    "Parowanie",
    "Ko\xB3o magii",
    "Dob\xB9" "d\x9F / schowaj bro\xF1",
    "Skok / wspinaczka",
    "Skradanie",
    "Akcja / atak",
    "Kamera",
    "Blokada celu",
    "Ekwipunek",
    "Menu gry",
    "Szybki zapis",
    "Szybkie wczytanie",
    "Teleport, gdy posta\xE6 utknie",
    "przytrzymaj",
    };
  switch(ScriptLang(Gothic::settingsGetI("GAME","language"))) {
    case ScriptLang::DE: return de;
    case ScriptLang::PL: return pl;
    default:             return en;
    }
  }

// One labelled callout: up to two glyphs, a text and the leader-line anchor in
// normalized diagram coordinates. Anchors were measured on the Xelu line-art.
struct Row {
  PadGlyph::Btn g1;
  PadGlyph::Btn g2;
  uint8_t       ng;
  const char*   txt;
  float         ax, ay;
  };
}

bool PadDiagram::available() {
  return PadGlyph::diagram()!=nullptr;
  }

void PadDiagram::draw(Painter& p, const GthFont& fnt, int w, int h, float scale) {
  const Texture2d* img = PadGlyph::diagram();
  if(img==nullptr)
    return;

  const Loc& L = loc();

  // Dim the whole page: the parchment menu background is too busy behind the
  // thin white line-art.
  p.setBrush(Color(0.f,0.f,0.f,0.62f));
  p.drawRect(0,0,w,h);

  const int   th     = fnt.pixelSize();
  const int   s      = std::max(18, int(26.f*scale));       // glyph side
  const int   rowH   = std::max(s,th) + int(8.f*scale);
  const int   gap    = int(10.f*scale);
  const int   tokGap = int(6.f*scale);
  const int   margin = int(18.f*scale);
  const int   lineT  = std::max(1, int(2.f*scale));
  const Color ink    = Color(0.86f,0.78f,0.60f,0.65f);

  // Title.
  const auto ts = fnt.textSize(L.title);
  fnt.drawText(p, (w-ts.w)/2, margin+ts.h, L.title);

  // Vertical bands: title / View+Menu callouts / diagram / combo footer.
  const int topBandY = margin + ts.h + int(10.f*scale);
  const int imgTop   = topBandY + s + int(14.f*scale);
  const int imgBot   = h - margin - 2*rowH - int(12.f*scale);
  const int colW     = int(float(w)*0.27f);

  // Fit the line-art into the middle band, keeping aspect.
  const int   availW = w - 2*colW;
  const int   availH = std::max(1, imgBot-imgTop);
  const float ratio  = std::min(float(availW)/float(img->w()),
                                float(availH)/float(img->h()));
  const int   dw     = int(float(img->w())*ratio);
  const int   dh     = int(float(img->h())*ratio);
  const int   imgX   = colW   + (availW-dw)/2;
  const int   imgY   = imgTop + (availH-dh)/2;

  p.setBrush(Brush(*img, Color(1.f,1.f,1.f,0.92f)));
  p.drawRect(imgX,imgY,dw,dh, 0,0,int(img->w()),int(img->h()));

  auto hline = [&](int x0, int x1, int y) {
    if(x1<x0)
      std::swap(x0,x1);
    p.setBrush(ink);
    p.drawRect(x0,y-lineT/2, x1-x0, lineT);
    };
  auto vline = [&](int x, int y0, int y1) {
    if(y1<y0)
      std::swap(y0,y1);
    p.setBrush(ink);
    p.drawRect(x-lineT/2,y0, lineT, y1-y0);
    };
  auto dot = [&](int x, int y) {
    p.setBrush(ink);
    p.drawRect(x-lineT,y-lineT, 2*lineT, 2*lineT);
    };
  // The compact hint bar maps the D-pad to keyboard-arrow art; the diagram
  // wants the real pad glyphs, so try those first.
  auto glyph = [&](PadGlyph::Btn b, int x, int y) {
    if(const Texture2d* t = PadGlyph::dpadTexture(b)) {
      p.setBrush(Brush(*t, Color(1.f,1.f,1.f,1.f)));
      p.drawRect(x,y,s,s, 0,0,int(t->w()),int(t->h()));
      return;
      }
    PadGlyph::draw(p,fnt,b,x,y,s);
    };

  // Keep in sync with GamepadInput::tickWorld. Sorted by anchor height so the
  // evenly spaced callout rows roughly track their buttons.
  const Row left[] = {
    {PadGlyph::LT,       PadGlyph::LT,        1, L.itemRing,   0.251f,0.068f},
    {PadGlyph::LStick,   PadGlyph::LStick,    1, L.move,       0.260f,0.446f},
    {PadGlyph::L3,       PadGlyph::L3,        1, L.walkRun,    0.260f,0.490f},
    {PadGlyph::DPadUp,   PadGlyph::DPadUp,    1, L.melee,      0.378f,0.595f},
    {PadGlyph::DPadLeft, PadGlyph::DPadRight, 2, L.quickSlots, 0.345f,0.643f},
    {PadGlyph::DPadDown, PadGlyph::DPadDown,  1, L.ranged,     0.378f,0.690f},
    };
  const Row right[] = {
    {PadGlyph::RT,     PadGlyph::RT,     1, L.parry,      0.752f,0.068f},
    {PadGlyph::RB,     PadGlyph::RB,     1, L.magicRing,  0.732f,0.137f},
    {PadGlyph::Y,      PadGlyph::Y,      1, L.weapon,     0.751f,0.334f},
    {PadGlyph::B,      PadGlyph::B,      1, L.jump,       0.817f,0.420f},
    {PadGlyph::X,      PadGlyph::X,      1, L.sneak,      0.683f,0.425f},
    {PadGlyph::A,      PadGlyph::A,      1, L.action,     0.748f,0.514f},
    {PadGlyph::RStick, PadGlyph::RStick, 1, L.camera,     0.622f,0.648f},
    {PadGlyph::R3,     PadGlyph::R3,     1, L.targetLock, 0.622f,0.690f},
    };

  auto rowCy = [&](size_t i, size_t n) {
    return imgY + int(float(dh)*(float(i)+0.5f)/float(n));
    };

  for(size_t i=0; i<std::size(left); ++i) {
    const Row& r   = left[i];
    const int  cy  = rowCy(i,std::size(left));
    const int  axp = imgX + int(r.ax*float(dw));
    const int  ayp = imgY + int(r.ay*float(dh));
    int gx = imgX - gap - s;               // rightmost glyph next to the art
    glyph(r.ng==2 ? r.g2 : r.g1, gx, cy-s/2);
    if(r.ng==2) {
      gx -= s + tokGap/2;
      glyph(r.g1, gx, cy-s/2);
      }
    fnt.drawText(p, gx-gap-fnt.textSize(r.txt).w, cy+th/2, r.txt);
    hline(imgX-gap+2, axp, cy);
    vline(axp, cy, ayp);
    dot(axp,ayp);
    }

  for(size_t i=0; i<std::size(right); ++i) {
    const Row& r   = right[i];
    const int  cy  = rowCy(i,std::size(right));
    const int  axp = imgX + int(r.ax*float(dw));
    const int  ayp = imgY + int(r.ay*float(dh));
    const int  gx  = imgX + dw + gap;
    glyph(r.g1, gx, cy-s/2);
    fnt.drawText(p, gx+s+gap, cy+th/2, r.txt);
    hline(axp, imgX+dw+gap-2, cy);
    vline(axp, cy, ayp);
    dot(axp,ayp);
    }

  // View / Menu sit in the middle of the pad: label them from above with a
  // straight drop, Gothic-Classic style.
  auto topLbl = [&](PadGlyph::Btn b, const char* txt, bool textOnLeft,
                    float ax, float ay) {
    const int axp = imgX + int(ax*float(dw));
    const int ayp = imgY + int(ay*float(dh));
    const int gx  = axp - s/2;
    glyph(b, gx, topBandY);
    if(textOnLeft)
      fnt.drawText(p, gx-gap-fnt.textSize(txt).w, topBandY+(s+th)/2, txt);
    else
      fnt.drawText(p, gx+s+gap, topBandY+(s+th)/2, txt);
    vline(axp, topBandY+s+2, ayp);
    dot(axp,ayp);
    };
  topLbl(PadGlyph::View, L.inventory, true,  0.433f,0.438f);
  topLbl(PadGlyph::Menu, L.gameMenu,  false, 0.571f,0.433f);

  // Footer: button combos that have no single spot on the art.
  struct Tok {
    PadGlyph::Btn b;
    const char*   t;   // nullptr -> glyph token
    };
  auto seqW = [&](const Tok* tk, size_t n) {
    int r = 0;
    for(size_t i=0; i<n; ++i)
      r += (tk[i].t==nullptr ? s : fnt.textSize(tk[i].t).w) + (i+1<n ? tokGap : 0);
    return r;
    };
  auto seqDraw = [&](const Tok* tk, size_t n, int x, int cy) {
    for(size_t i=0; i<n; ++i) {
      if(tk[i].t==nullptr) {
        glyph(tk[i].b, x, cy-s/2);
        x += s + tokGap;
        }
      else {
        fnt.drawText(p, x, cy+th/2, tk[i].t);
        x += fnt.textSize(tk[i].t).w + tokGap;
        }
      }
    };

  const int cy1 = imgBot + int(12.f*scale) + rowH/2;
  const int cy2 = cy1 + rowH;

  const Tok save[] = {{PadGlyph::LB,nullptr},{PadGlyph::LB,"+"},{PadGlyph::Menu,nullptr},{PadGlyph::LB,L.quickSave}};
  const Tok load[] = {{PadGlyph::LB,nullptr},{PadGlyph::LB,"+"},{PadGlyph::View,nullptr},{PadGlyph::LB,L.quickLoad}};
  const int groupGap = int(42.f*scale);
  const int wSave    = seqW(save,std::size(save));
  const int wLoad    = seqW(load,std::size(load));
  int x = (w - (wSave+groupGap+wLoad))/2;
  seqDraw(save,std::size(save), x,          cy1);
  seqDraw(load,std::size(load), x+wSave+groupGap, cy1);

  char buf[192] = {};
  std::snprintf(buf,sizeof(buf),"(%s)  %s", L.hold, L.unstuck);
  const Tok stuck[] = {{PadGlyph::L3,nullptr},{PadGlyph::L3,"+"},{PadGlyph::R3,nullptr},{PadGlyph::L3,buf}};
  seqDraw(stuck,std::size(stuck), (w-seqW(stuck,std::size(stuck)))/2, cy2);
  }
