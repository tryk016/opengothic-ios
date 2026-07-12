#include "quickring.h"

#include <Tempest/Painter>
#include <Tempest/Color>
#include <cmath>
#include <algorithm>
#include <string>

#include "world/objects/npc.h"
#include "world/objects/item.h"
#include "game/inventory.h"
#include "game/constants.h"
#include "graphics/inventoryrenderer.h"
#include "resources.h"
#include "utils/gthfont.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace Tempest;

void QuickRing::open(Npc& pl) {
  cells.clear();
  sel = -1;

  const Inventory& inv = pl.inventory();
  for(auto it=inv.iterator(Inventory::T_Inventory); it.isValid(); ++it) {
    const Item& itm = *it;
    const auto  flg = itm.mainFlag();
    bool match = false;
    if(kind==Magic)
      // runes and scrolls share ITM_CAT_RUNE; selecting one equips it
      match = (flg & ITM_CAT_RUNE)!=0;
    else
      match = (flg & (ITM_CAT_POTION|ITM_CAT_FOOD))!=0;
    if(!match)
      continue;

    Cell c;
    c.cls      = itm.clsId();
    c.count    = int(it.count());
    c.equipped = it.isEquipped();
    c.name     = std::string(itm.displayName());
    cells.push_back(std::move(c));
    if(int(cells.size())>=MAX_CELLS)
      break;
    }

  opened = true;
  }

void QuickRing::updateSelection(float sx, float sy) {
  if(cells.empty()) { sel=-1; return; }
  const float mag = std::sqrt(sx*sx + sy*sy);
  if(mag < SELECT_EPS) { sel=-1; return; }
  float ang = std::atan2(sx, sy);                 // 0 == up, clockwise
  if(ang<0.f)
    ang += 2.f*float(M_PI);
  sel = int(ang/(2.f*float(M_PI))*float(cells.size())) % int(cells.size());
  }

void QuickRing::close() {
  opened = false;
  cells.clear();
  sel = -1;
  }

void QuickRing::commit(Npc& pl) {
  if(sel>=0 && sel<int(cells.size()))
    pl.useItem(cells[size_t(sel)].cls, Item::NSLOT, false);  // equips weapons, consumes items
  close();
  }

void QuickRing::paint(Painter& p, InventoryRenderer& ir, const Npc* pl,
                      int screenW, int screenH, float scale) const {
  if(!opened)
    return;

  // rebuild this frame's icon set from scratch, like the inventory menu does
  ir.reset();

  p.setBrush(Color(0.f,0.f,0.f,0.45f));           // dim the world behind the ring
  p.drawRect(0,0,screenW,screenH);

  auto&     fnt = Resources::font(scale);
  const int cx  = screenW/2;
  const int cy  = screenH/2;

  if(cells.empty()) {
    std::string_view msg = (kind==Magic) ? "no runes or scrolls" : "no items";
    const auto ts = fnt.textSize(msg);
    fnt.drawText(p, cx-ts.w/2, cy, msg);
    return;
    }

  const int n   = int(cells.size());
  const int R   = std::min(screenW,screenH)/3;
  const int hs  = std::max(18,int(30*scale));
  const int bar = std::max(3,int(4*scale));

  // resolve live items in one inventory pass: cells store only the class id,
  // because the world keeps running while the ring is open and an item may be
  // gone by now (then its tile simply stays empty)
  std::vector<const Item*> icon(cells.size(),nullptr);
  if(pl!=nullptr) {
    for(auto it=pl->inventory().iterator(Inventory::T_Inventory); it.isValid(); ++it) {
      for(size_t c=0; c<cells.size(); ++c)
        if(icon[c]==nullptr && cells[c].cls==(*it).clsId()) {
          icon[c] = &*it;
          break;
          }
      }
    }

  for(int i=0;i<n;++i) {
    const float a  = 2.f*float(M_PI)*float(i)/float(n);   // 0 at top, clockwise
    const int   px = cx + int(std::sin(a)*float(R));
    const int   py = cy - int(std::cos(a)*float(R));
    const bool  s  = (i==sel);

    p.setBrush(Color(0.f,0.f,0.f, s ? 0.75f : 0.4f));
    p.drawRect(px-hs, py-hs, hs*2, hs*2);
    if(s) {
      p.setBrush(Color(1.f,0.43f,0.43f,0.65f));          // highlight (255,110,110)
      p.drawRect(px-hs, py-hs,       hs*2, bar);
      p.drawRect(px-hs, py+hs-bar,   hs*2, bar);
      }
    if(cells[size_t(i)].equipped) {
      p.setBrush(Color(0.4f,0.9f,0.4f,0.55f));           // equipped marker
      p.drawRect(px-hs, py-hs, bar, hs*2);
      }

    if(icon[size_t(i)]!=nullptr)
      ir.drawItem(px-hs, py-hs, hs*2, hs*2, *icon[size_t(i)]);

    std::string label = cells[size_t(i)].name;
    if(cells[size_t(i)].count>1)
      label += " x" + std::to_string(cells[size_t(i)].count);
    const auto ts = fnt.textSize(label);
    fnt.drawText(p, px-ts.w/2, py+hs+ts.h, label);
    }

  std::string_view title = (kind==Magic) ? "Magic" : "Items";
  const auto tts = fnt.textSize(title);
  fnt.drawText(p, cx-tts.w/2, cy+tts.h/2, title);
  }
