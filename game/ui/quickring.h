#pragma once

#include <vector>
#include <string>
#include <cstddef>
#include <cstdint>

namespace Tempest { class Painter; }
class Npc;
class InventoryRenderer;

// A radial quick-bar driven by the right stick: hold a trigger to open it, aim
// the stick at a slice, release to activate (equip weapon / use item). Content
// is pulled live from the player's inventory. Draws its own vector segments with
// text labels — no bundled glyph assets (control spec 4). Item icons are 3D
// meshes collected into the shared InventoryRenderer (flushed by the engine).
class QuickRing {
  public:
    enum Kind : uint8_t { Magic, Items };
    explicit QuickRing(Kind k) : kind(k) {}

    bool isOpen() const { return opened; }
    void open (Npc& pl);                        // rebuild slices from inventory
    void updateSelection(float sx, float sy);   // right-stick aim
    void close();                               // dismiss without acting
    void commit(Npc& pl);                       // activate the selected slice
    void paint(Tempest::Painter& p, InventoryRenderer& ir, const Npc* pl,
               int screenW, int screenH, float scale) const;

  private:
    struct Cell {
      size_t      cls      = 0;
      int         count    = 0;
      bool        equipped = false;
      std::string name;
      };

    Kind              kind;
    bool              opened = false;
    std::vector<Cell> cells;
    int               sel = -1;

    static constexpr float SELECT_EPS = 0.35f;  // stick magnitude needed to select
    static constexpr int   MAX_CELLS  = 12;
  };
