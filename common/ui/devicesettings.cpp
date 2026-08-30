#include "devicesettings.h"

#if defined(__IOS__)

#include <Tempest/Color>
#include <Tempest/Event>
#include <Tempest/Painter>

#include <algorithm>
#include <iterator>

#include "game/constants.h"
#include "gothic.h"
#include "mainwindow.h"
#include "resources.h"
#include "ui/iosuilocalization.h"
#include "ui/paddiagram.h"
#include "utils/gthfont.h"
#include "utils/string_frm.h"

using namespace Tempest;

namespace {
constexpr int rates[] = {0,30,60};

bool contains(int x, int y, int rx, int ry, int rw, int rh) {
  return x>=rx && x<rx+rw && y>=ry && y<ry+rh;
  }
}

DeviceSettings::DeviceSettings(MainWindow& owner) : owner(owner) {
  setCursorShape(CursorShape::Hidden);
  }

bool DeviceSettings::isOpen() const {
  return active;
  }

void DeviceSettings::open() {
  if(active)
    return;
  active = true;
  setFocus(true);
  update();
  }

void DeviceSettings::close() {
  if(!active)
    return;
  active = false;
  owner.setFocus(true);
  update();
  }

bool DeviceSettings::isFrameRateLocked() const {
  return Gothic::options().fpsLimit>0;
  }

int DeviceSettings::frameRate() const {
  if(isFrameRateLocked())
    return Gothic::options().fpsLimit;
  const int fps = Gothic::settingsGetI("ENGINE","zMaxFps");
  for(const int value:rates)
    if(fps==value)
      return fps;
  return 30;
  }

void DeviceSettings::setFrameRate(int value) {
  if(isFrameRateLocked())
    return;
  if(value!=0 && value!=30 && value!=60)
    value = 30;
  Gothic::settingsSetI("ENGINE","zMaxFps",value);
  Gothic::flushSettings();
  update();
  }

void DeviceSettings::cycleFrameRate(int direction) {
  if(isFrameRateLocked() || direction==0)
    return;
  int index = 1;
  const int current = frameRate();
  for(size_t i=0; i<std::size(rates); ++i)
    if(rates[i]==current) {
      index = int(i);
      break;
      }
  index = (index+direction+int(std::size(rates))) % int(std::size(rates));
  setFrameRate(rates[index]);
  }

void DeviceSettings::keyDownEvent(KeyEvent& event) {
  if(!active) {
    event.ignore();
    return;
    }
  switch(event.key) {
    case Event::K_ESCAPE:
    case Event::K_B:
      close();
      break;
    case Event::K_Left:
    case Event::K_A:
      cycleFrameRate(-1);
      break;
    case Event::K_Right:
    case Event::K_D:
    case Event::K_Return:
      cycleFrameRate(1);
      break;
    default:
      break;
    }
  event.accept();
  }

void DeviceSettings::keyUpEvent(KeyEvent& event) {
  // Consume the matching key-up even after Escape closed the overlay; otherwise
  // MainWindow would forward that release to the menu below it.
  event.accept();
  }

void DeviceSettings::mouseDownEvent(MouseEvent& event) {
  if(!active) {
    event.ignore();
    return;
    }
  const int panelW = std::min(w()-24,std::max(280,int(float(w())*0.58f)));
  const int panelH = std::max(128,int(float(h())*0.22f));
  const int panelX = (w()-panelW)/2;
  const int panelY = h()-panelH-16;
  const int gap    = 8;
  const int btnW   = (panelW-4*gap)/3;
  const int btnY   = panelY+int(float(panelH)*0.47f);
  const int btnH   = std::max(32,int(float(panelH)*0.27f));
  const Point pos = event.pos();
  if(!isFrameRateLocked()) {
    for(size_t i=0; i<std::size(rates); ++i)
      if(contains(pos.x,pos.y,panelX+gap+int(i)*(btnW+gap),btnY,btnW,btnH)) {
        setFrameRate(rates[i]);
        event.accept();
        return;
        }
    }
  if(contains(pos.x,pos.y,panelX,panelY,panelW,panelH)) {
    event.accept();
    return;
    }
  close();
  event.accept();
  }

void DeviceSettings::paintEvent(PaintEvent& event) {
  if(!active)
    return;

  Painter p(event);
  const float scale = Gothic::interfaceScale(this);
  auto& fnt = Resources::font(scale);
  const ScriptLang language = IosUiLocalization::currentLanguage();
  const auto& txt = IosUiLocalization::deviceSettings(language);

  // Keep the complete mapping in its own native layer, never in Controls.
  PadDiagram::draw(p,fnt,w(),h(),scale,language,false);

  const int panelW = std::min(w()-24,std::max(280,int(float(w())*0.58f)));
  const int panelH = std::max(128,int(float(h())*0.22f));
  const int panelX = (w()-panelW)/2;
  const int panelY = h()-panelH-16;
  const int gap    = 8;
  const int btnW   = (panelW-4*gap)/3;
  const int btnY   = panelY+int(float(panelH)*0.47f);
  const int btnH   = std::max(32,int(float(panelH)*0.27f));

  p.setBrush(Color(0.06f,0.06f,0.08f,0.93f));
  p.drawRect(panelX,panelY,panelW,panelH);
  fnt.drawText(p,panelX+gap,panelY+fnt.pixelSize()+gap,txt.title);
  fnt.drawText(p,panelX+gap,panelY+int(float(panelH)*0.36f),txt.frameRate);

  const bool locked = isFrameRateLocked();
  const int  current = frameRate();
  for(size_t i=0; i<std::size(rates); ++i) {
    const int x = panelX+gap+int(i)*(btnW+gap);
    const bool selected = current==rates[i];
    const Color color = locked ? Color(0.20f,0.20f,0.22f,0.85f) :
                        selected ? Color(0.64f,0.48f,0.18f,0.95f) :
                                   Color(0.20f,0.20f,0.24f,0.95f);
    p.setBrush(color);
    p.drawRect(x,btnY,btnW,btnH);
    const char* label = rates[i]==0 ? txt.off : (rates[i]==30 ? "30" : "60");
    const auto sz = fnt.textSize(label);
    fnt.drawText(p,x+(btnW-sz.w)/2,btnY+(btnH+sz.h)/2,label);
    }
  if(locked) {
    string_frm message(txt.controlled," (",current," FPS)");
    fnt.drawText(p,panelX+gap,panelY+panelH-gap,message);
    }
  else {
    const auto sz = fnt.textSize(txt.back);
    fnt.drawText(p,panelX+panelW-gap-sz.w,panelY+panelH-gap,txt.back);
    }
  }

#endif
