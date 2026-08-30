#include "quickring.h"

#include <Tempest/Color>
#include <Tempest/Painter>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "game/constants.h"
#include "game/gamescript.h"
#include "game/gamesession.h"
#include "game/inventory.h"
#include "graphics/inventoryrenderer.h"
#include "resources.h"
#include "utils/gthfont.h"
#include "world/objects/item.h"
#include "world/objects/npc.h"
#include "world/world.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace Tempest;

namespace {

constexpr float Pi = float(M_PI);

int itemCount(const Item& item) {
  return int(std::min(item.count(),size_t(std::numeric_limits<int>::max())));
  }

void drawSector(Painter& p, float cx, float cy, float innerRadius,
                float outerRadius, float begin, float end,
                const Color& color) {
  // Painter has triangles but no path/arc primitive. Subdivide each annular
  // sector enough for a smooth silhouette without adding any texture assets.
  constexpr float MaxStep = Pi/36.f; // five degrees
  const int steps = std::max(2,int(std::ceil((end-begin)/MaxStep)));
  p.setBrush(color);
  for(int i=0;i<steps;++i) {
    const float a0 = begin + (end-begin)*float(i)/float(steps);
    const float a1 = begin + (end-begin)*float(i+1)/float(steps);
    const float s0 = std::sin(a0), c0 = std::cos(a0);
    const float s1 = std::sin(a1), c1 = std::cos(a1);
    const float ix0 = cx+s0*innerRadius, iy0 = cy-c0*innerRadius;
    const float ix1 = cx+s1*innerRadius, iy1 = cy-c1*innerRadius;
    const float ox0 = cx+s0*outerRadius, oy0 = cy-c0*outerRadius;
    const float ox1 = cx+s1*outerRadius, oy1 = cy-c1*outerRadius;
    p.drawTriangle(ix0,iy0,0.f,0.f, ox0,oy0,0.f,0.f,
                   ox1,oy1,0.f,0.f);
    p.drawTriangle(ix0,iy0,0.f,0.f, ox1,oy1,0.f,0.f,
                   ix1,iy1,0.f,0.f);
    }
  }

std::string fitLabel(const GthFont& font, std::string text, int maxWidth) {
  if(font.textSize(text).w<=maxWidth)
    return text;
  static constexpr std::string_view Ellipsis = "...";
  while(!text.empty() && font.textSize(text+std::string(Ellipsis)).w>maxWidth)
    text.pop_back();
  text += Ellipsis;
  return text;
  }

} // namespace

QuickRing::QuickRing(Kind k) : kind(k) {
  }

QuickRing::~QuickRing() = default;

bool QuickRing::isEditing() const {
  return mode==Mode::Edit;
  }

int QuickRing::innerCells() const {
  return kind==Items ? ITEM_INNER_CELLS : WEAPON_INNER_CELLS;
  }

int QuickRing::outerCells() const {
  return kind==Items ? ITEM_OUTER_CELLS : WEAPON_OUTER_CELLS;
  }

int QuickRing::cellsCount() const {
  return innerCells()+outerCells();
  }

void QuickRing::open(Npc& pl) {
  static_assert(WEAPON_OUTER_CELLS==
                int(KeyCodec::WeaponMage10)-int(KeyCodec::WeaponMage3)+1,
                "the weapons ring must expose every native spell slot");
  static_assert(MAX_CELLS==int(GameSession::QuickItemSlotCount),
                "the saved quick-item layout must match the item ring");

  cells = {};
  syntheticTorch.reset();
  editSlots.fill(GameSession::NoQuickItem);
  editCls = GameSession::NoQuickItem;
  editName.clear();
  sel    = -1;
  band   = Band::None;
  opened = true;
  mode   = Mode::Use;

  if(kind==Weapons) {
    auto setAction = [&](int slot, const Item* item, KeyCodec::Action action) {
      if(item==nullptr)
        return;
      Cell& c    = cells[size_t(slot)];
      c.cls      = item->clsId();
      c.count    = itemCount(*item);
      c.equipped = item->isEquipped();
      c.type     = CellType::Action;
      c.action   = action;
      c.name     = std::string(item->displayName());
      };

    // Inner row: the two weapons currently equipped in the inventory.
    setAction(0,pl.currentMeleeWeapon(), KeyCodec::WeaponMele);
    setAction(1,pl.currentRangedWeapon(),KeyCodec::WeaponBow);

    // OpenGothic assigns spells to keyboard slots 3..10 and stores them at
    // currentSpell indices 0..7. Include index zero: it is normally the first
    // rune equipped through the inventory.
    for(int i=0;i<WEAPON_OUTER_CELLS;++i) {
      const auto action = KeyCodec::Action(int(KeyCodec::WeaponMage3)+i);
      setAction(WEAPON_INNER_CELLS+i,
                pl.inventory().currentSpell(uint8_t(i)), action);
      }
    return;
    }

  auto& game = pl.world().gameSession();
  if(game.hasCustomQuickItems())
    fillAssignedItems(pl,game.quickItemLayout());
  else
    fillAutomaticItems(pl);
  }

void QuickRing::collectItems(Npc& pl, std::vector<Cell>& items,
                             bool automaticOnly) {
  items.clear();
  items.reserve(MAX_CELLS);
  syntheticTorch.reset();

  const size_t torchCls = pl.world().script().findSymbolIndex("ItLsTorch");
  const bool hasLitTorch = pl.isUsingTorch() && torchCls!=size_t(-1);
  if(hasLitTorch) {
    // The lit torch is in the hand rather than Inventory. A lightweight
    // inventory-form Item supplies its normal name and renderer mesh; commit
    // still targets the real class id and Inventory::use stows the hand torch.
    syntheticTorch = std::make_unique<Item>(pl.world(),torchCls,Item::T_Inventory);
    Cell c;
    c.cls      = torchCls;
    c.count    = 1;
    c.equipped = true;
    c.type     = CellType::Item;
    c.name     = std::string(syntheticTorch->displayName());
    items.push_back(std::move(c));
    }

  // Inventory::Iterator can expose an equipped non-multi stack twice. Merge
  // by class id so one weapon/item can never occupy two radial cells.
  for(auto it=pl.inventory().iterator(Inventory::T_Inventory); it.isValid(); ++it) {
    const Item& item = *it;
    const auto main  = item.mainFlag();
    const bool torch = (uint32_t(item.itemFlag())&ITM_TORCH)!=0;
    if(item.isGold())
      continue;
    if(automaticOnly &&
       (main&(ITM_CAT_POTION|ITM_CAT_FOOD))==0 && !torch)
      continue;

    const size_t cls = item.clsId();
    auto at = std::find_if(items.begin(),items.end(),[cls](const Cell& c) {
      return c.cls==cls;
      });
    int count = itemCount(item);
    if(hasLitTorch && cls==torchCls && count<std::numeric_limits<int>::max())
      ++count;
    if(at!=items.end()) {
      at->count    = std::max(at->count,count);
      at->equipped = at->equipped || it.isEquipped();
      if(at->name.empty())
        at->name = std::string(item.displayName());
      continue;
      }
    if(automaticOnly && int(items.size())>=MAX_CELLS)
      continue;

    Cell c;
    c.cls      = cls;
    c.count    = count;
    c.equipped = it.isEquipped();
    c.type     = CellType::Item;
    c.name     = std::string(item.displayName());
    items.push_back(std::move(c));
    }
  }

void QuickRing::fillAutomaticItems(Npc& pl) {
  std::vector<Cell> items;
  collectItems(pl,items,true);

  // Items use the large outer row first; the four inner cells are overflow.
  size_t src = 0;
  for(int i=0;i<ITEM_OUTER_CELLS && src<items.size();++i,++src)
    cells[size_t(ITEM_INNER_CELLS+i)] = std::move(items[src]);
  for(int i=0;i<ITEM_INNER_CELLS && src<items.size();++i,++src)
    cells[size_t(i)] = std::move(items[src]);
  }

void QuickRing::fillAssignedItems(
    Npc& pl, const std::array<uint32_t,MAX_CELLS>& layout) {
  std::vector<Cell> items;
  collectItems(pl,items,false);
  for(size_t slot=0;slot<layout.size();++slot) {
    const uint32_t cls = layout[slot];
    if(cls==GameSession::NoQuickItem)
      continue;
    auto at = std::find_if(items.begin(),items.end(),[cls](const Cell& cell) {
      return cell.cls==size_t(cls);
      });
    if(at!=items.end())
      cells[slot] = *at;
    }
  }

bool QuickRing::openEdit(Npc& pl, size_t itemCls) {
  if(kind!=Items || itemCls>=size_t(GameSession::NoQuickItem))
    return false;

  cells = {};
  syntheticTorch.reset();
  editSlots.fill(GameSession::NoQuickItem);
  editCls = uint32_t(itemCls);
  editName.clear();
  sel    = -1;
  band   = Band::None;
  opened = true;
  mode   = Mode::Edit;

  bool found = false;
  for(auto it=pl.inventory().iterator(Inventory::T_Inventory); it.isValid(); ++it) {
    if(it->clsId()==itemCls) {
      found = true;
      editName = std::string(it->displayName());
      break;
      }
    }
  if(!found || pl.inventory().itemCount(itemCls)==0) {
    close();
    return false;
    }

  auto& game = pl.world().gameSession();
  if(game.hasCustomQuickItems()) {
    editSlots = game.quickItemLayout();
    fillAssignedItems(pl,editSlots);
    }
  else {
    // The first edit freezes the currently visible automatic arrangement as
    // the working layout. Cancelling leaves automatic mode untouched.
    fillAutomaticItems(pl);
    for(size_t i=0;i<cells.size();++i)
      if(cells[i].valid())
        editSlots[i] = uint32_t(cells[i].cls);
    }
  return true;
  }

void QuickRing::updateSelection(float sx, float sy) {
  if(!opened)
    return;
  const float mag = std::sqrt(sx*sx + sy*sy);
  if(mag<SELECT_EPS) {
    sel  = -1;
    band = Band::None;
    return;
    }

  // Keep the current row through a wide neutral strip. This prevents noisy
  // right-stick magnitude from flickering between concentric rows.
  if(band==Band::Inner) {
    if(mag>BAND_TO_OUTER)
      band=Band::Outer;
    }
  else if(band==Band::Outer) {
    if(mag<BAND_TO_INNER)
      band=Band::Inner;
    }
  else {
    band = mag>=BAND_FIRST_OUTER ? Band::Outer : Band::Inner;
    }

  float ang = std::atan2(sx,sy); // zero is up, clockwise
  if(ang<0.f)
    ang += 2.f*Pi;

  const int count = band==Band::Inner ? innerCells() : outerCells();
  const float step = 2.f*Pi/float(count);
  const int local = int((ang+step*0.5f)/step)%count;
  const int slot  = band==Band::Inner ? local : innerCells()+local;
  // Empty fixed cells still receive cursor feedback, but commit leaves them
  // inactive because their CellType remains Empty.
  sel = slot;
  }

void QuickRing::close() {
  opened = false;
  mode   = Mode::Use;
  cells  = {};
  sel    = -1;
  band   = Band::None;
  editSlots.fill(GameSession::NoQuickItem);
  editCls = GameSession::NoQuickItem;
  editName.clear();
  syntheticTorch.reset();
  }

std::optional<KeyCodec::Action> QuickRing::commit(Npc& pl) {
  std::optional<KeyCodec::Action> result;
  if(mode!=Mode::Use)
    return result;
  if(sel>=0 && sel<cellsCount()) {
    const Cell& c = cells[size_t(sel)];
    if(c.type==CellType::Item) {
      // Match a normal inventory click for manually assigned equipment: an
      // equipped weapon, rune, armour or accessory is toggled off instead of
      // running an unnecessary unequip/equip hook cycle. A burning torch is
      // the exception because it no longer exists in Inventory; useItem()
      // owns its explicit stow-and-return path.
      const size_t torchCls =
          pl.world().script().findSymbolIndex("ItLsTorch");
      const bool burningTorch = pl.isUsingTorch() && c.cls==torchCls;
      if(c.equipped && !burningTorch)
        pl.unequipItem(c.cls);
      else
        pl.useItem(c.cls,Item::NSLOT,false);
      }
    else if(c.type==CellType::Action)
      result = c.action;
    }
  close();
  return result;
  }

bool QuickRing::assignSelection(Npc& pl) {
  if(mode!=Mode::Edit || sel<0 || sel>=cellsCount() ||
     editCls==GameSession::NoQuickItem ||
     pl.inventory().itemCount(editCls)==0)
    return false;

  // One item has one stable home. Reassigning it moves the existing binding
  // instead of creating duplicate sectors which would vanish together.
  for(uint32_t& cls:editSlots)
    if(cls==editCls)
      cls=GameSession::NoQuickItem;
  editSlots[size_t(sel)] = editCls;
  pl.world().gameSession().setQuickItemLayout(editSlots);
  close();
  return true;
  }

bool QuickRing::clearSelection(Npc& pl) {
  if(mode!=Mode::Edit || sel<0 || sel>=cellsCount() ||
     editSlots[size_t(sel)]==GameSession::NoQuickItem)
    return false;
  editSlots[size_t(sel)] = GameSession::NoQuickItem;
  pl.world().gameSession().setQuickItemLayout(editSlots);
  cells[size_t(sel)] = Cell{};
  return true;
  }

void QuickRing::paint(Painter& p, InventoryRenderer& ir, const Npc* pl,
                      int screenW, int screenH, float scale) const {
  if(!opened)
    return;

  // Rebuild this frame's mesh icon set, like InventoryMenu does.
  ir.reset();

  p.setBrush(Color(0.f,0.f,0.f,0.48f));
  p.drawRect(0,0,screenW,screenH);

  const float cx = float(screenW)/2.f;
  const float cy = float(screenH)/2.f;
  const float R  = float(std::min(screenW,screenH))/3.f;

  const float innerR0 = R*0.24f;
  const float innerR1 = R*0.63f;
  const float outerR0 = R*0.69f;
  const float outerR1 = R*1.10f;
  const float outline = std::max(1.5f,2.2f*scale);

  const Color border(0.96f,0.55f,0.14f,0.92f);
  const Color selectedBorder(1.f,0.86f,0.20f,1.f);
  const Color empty(0.015f,0.012f,0.010f,0.48f);
  const Color occupied(0.015f,0.012f,0.010f,0.76f);
  const Color selected(0.88f,0.48f,0.05f,0.78f);

  auto paintBand = [&](int first, int count, float r0, float r1) {
    const float step = 2.f*Pi/float(count);
    for(int i=0;i<count;++i) {
      const int slot = first+i;
      const float center = step*float(i);
      const float angularGap = std::min(0.045f,outline/((r0+r1)*0.5f));
      const float a0 = center-step*0.5f+angularGap;
      const float a1 = center+step*0.5f-angularGap;
      const bool chosen = slot==sel;
      const Cell& cell = cells[size_t(slot)];
      drawSector(p,cx,cy,r0,r1,a0,a1,
                 chosen ? selectedBorder : border);
      const float insetAng = outline/std::max(1.f,(r0+r1)*0.5f);
      drawSector(p,cx,cy,r0+outline,r1-outline,
                 a0+insetAng,a1-insetAng,
                 chosen ? selected : (cell.valid() ? occupied : empty));
      }
    };

  paintBand(0,innerCells(),innerR0,innerR1);
  paintBand(innerCells(),outerCells(),outerR0,outerR1);

  std::array<const Item*,MAX_CELLS> icons{};
  if(syntheticTorch!=nullptr) {
    for(int i=0;i<cellsCount();++i)
      if(cells[size_t(i)].valid() &&
         cells[size_t(i)].cls==syntheticTorch->clsId())
        icons[size_t(i)] = syntheticTorch.get();
    }
  if(pl!=nullptr) {
    for(auto it=pl->inventory().iterator(Inventory::T_Inventory); it.isValid(); ++it) {
      for(int i=0;i<cellsCount();++i) {
        if(cells[size_t(i)].valid() && cells[size_t(i)].cls==(*it).clsId())
          icons[size_t(i)] = &*it;
        }
      }
    }

  auto& fnt = Resources::font(scale);
  const int iconSize = std::max(20,int(R*0.20f));
  auto paintCells = [&](int first, int count, float radius) {
    const float step = 2.f*Pi/float(count);
    for(int i=0;i<count;++i) {
      const int slot = first+i;
      const Cell& cell = cells[size_t(slot)];
      if(!cell.valid())
        continue;
      const float a = step*float(i);
      const int px = int(cx+std::sin(a)*radius);
      const int py = int(cy-std::cos(a)*radius);
      if(icons[size_t(slot)]!=nullptr)
        ir.drawItem(px-iconSize/2,py-iconSize/2,
                    iconSize,iconSize,*icons[size_t(slot)]);

      if(cell.count>1) {
        const std::string countText = std::to_string(cell.count);
        const auto ts = fnt.textSize(countText);
        // InventoryRenderer is flushed after Painter, so keep the counter just
        // outside the icon viewport rather than drawing underneath the mesh.
        const int countBaseline = py>=int(cy)
                                ? py-iconSize/2
                                : py+iconSize/2+ts.h;
        fnt.drawText(p,px-ts.w/2,countBaseline,countText);
        }
      }
    };

  paintCells(0,innerCells(),(innerR0+innerR1)*0.5f);
  paintCells(innerCells(),outerCells(),(outerR0+outerR1)*0.5f);

  if(mode==Mode::Edit) {
    std::string label = editName;
    if(sel>=0 && sel<cellsCount()) {
      const Cell& target = cells[size_t(sel)];
      label += " -> ";
      label += (target.valid() ? target.name : "[ ]");
      }
    label = fitLabel(fnt,std::move(label),std::max(1,screenW-16));
    const std::string commands = "RT: +    LT: -    B: ESC";
    const auto ls = fnt.textSize(label);
    const auto cs = fnt.textSize(commands);
    const int requested = int(cy+outerR1)+ls.h+std::max(3,int(4*scale));
    const int firstBaseline = std::max(ls.h+2,
                              std::min(screenH-cs.h-6,requested));
    fnt.drawText(p,int(cx)-ls.w/2,firstBaseline,label);
    fnt.drawText(p,int(cx)-cs.w/2,
                 std::min(screenH-4,firstBaseline+cs.h+2),commands);
    }
  else if(sel>=0 && sel<cellsCount()) {
    const Cell& cell = cells[size_t(sel)];
    std::string label = cell.name;
    if(cell.count>1)
      label += " x"+std::to_string(cell.count);
    label = fitLabel(fnt,std::move(label),std::max(1,screenW-16));
    const auto ts = fnt.textSize(label);
    const int baseline = std::min(screenH-4,
                                  int(cy+outerR1)+ts.h+std::max(3,int(4*scale)));
    fnt.drawText(p,int(cx)-ts.w/2,baseline,label);
    }
  }
