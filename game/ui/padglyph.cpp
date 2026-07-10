#include "padglyph.h"

#include <Tempest/Painter>
#include <Tempest/Color>
#include <Tempest/Brush>
#include <Tempest/Texture2d>
#include <cmath>

#include "utils/gthfont.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace Tempest;

static void fillDisc(Painter& p, float cx, float cy, float r, const Color& c) {
  p.setBrush(c);
  const int seg = 22;
  for(int i=0;i<seg;++i) {
    const float a0 = 2.f*float(M_PI)*float(i)/float(seg);
    const float a1 = 2.f*float(M_PI)*float(i+1)/float(seg);
    p.drawTriangle(cx, cy, 0.f, 0.f,
                   cx+std::cos(a0)*r, cy+std::sin(a0)*r, 0.f, 0.f,
                   cx+std::cos(a1)*r, cy+std::sin(a1)*r, 0.f, 0.f);
    }
  }

static void fillTri(Painter& p, float x0,float y0, float x1,float y1, float x2,float y2, const Color& c) {
  p.setBrush(c);
  p.drawTriangle(x0,y0,0.f,0.f, x1,y1,0.f,0.f, x2,y2,0.f,0.f);
  }

static void centerLabel(Painter& p, const GthFont& fnt, float cx, float cy, std::string_view t) {
  const auto ts = fnt.textSize(t);
  fnt.drawText(p, int(cx)-ts.w/2, int(cy)+ts.h/2, t);
  }

void PadGlyph::draw(Painter& p, const GthFont& fnt, Btn b, int x, int y, int s, float a) {
  if(const Tempest::Texture2d* t = PadGlyph::texture(b)) {   // real Xelu art if bundled
    p.setBrush(Brush(*t, Color(1.f,1.f,1.f,a)));
    p.drawRect(x, y, s, s, 0, 0, int(t->w()), int(t->h()));
    return;
    }

  const float cx = float(x) + float(s)*0.5f;
  const float cy = float(y) + float(s)*0.5f;
  const float r  = float(s)*0.42f;

  auto disc = [&](float rr, float gg, float bb){
    fillDisc(p, cx, cy, r, Color(rr,gg,bb,0.95f*a));
    };
  auto pill = [&](float h){   // rounded-ish shoulder/trigger bar (approximated by a rect)
    p.setBrush(Color(0.82f,0.82f,0.86f,0.9f*a));
    p.drawRect(int(float(x)+float(s)*0.08f), int(cy-float(s)*h*0.5f),
               int(float(s)*0.84f),          int(float(s)*h));
    };
  const Color ink(0.90f,0.90f,0.90f,0.9f*a);

  switch(b) {
    case A: disc(0.30f,0.78f,0.35f); centerLabel(p,fnt,cx,cy,"A"); break;
    case B: disc(0.90f,0.28f,0.26f); centerLabel(p,fnt,cx,cy,"B"); break;
    case X: disc(0.24f,0.52f,0.94f); centerLabel(p,fnt,cx,cy,"X"); break;
    case Y: disc(0.95f,0.78f,0.20f); centerLabel(p,fnt,cx,cy,"Y"); break;

    case LB: pill(0.5f); centerLabel(p,fnt,cx,cy,"LB"); break;
    case RB: pill(0.5f); centerLabel(p,fnt,cx,cy,"RB"); break;
    case LT: pill(0.6f); centerLabel(p,fnt,cx,cy,"LT"); break;
    case RT: pill(0.6f); centerLabel(p,fnt,cx,cy,"RT"); break;

    case L3: fillDisc(p,cx,cy,r,Color(0.55f,0.55f,0.60f,0.9f*a)); centerLabel(p,fnt,cx,cy,"L3"); break;
    case R3: fillDisc(p,cx,cy,r,Color(0.55f,0.55f,0.60f,0.9f*a)); centerLabel(p,fnt,cx,cy,"R3"); break;

    case LStick: fillDisc(p,cx,cy,r,Color(0.40f,0.40f,0.46f,0.55f*a)); centerLabel(p,fnt,cx,cy,"L"); break;
    case RStick: fillDisc(p,cx,cy,r,Color(0.40f,0.40f,0.46f,0.55f*a)); centerLabel(p,fnt,cx,cy,"R"); break;

    case DPadUp:    fillTri(p, cx, cy-r, cx-r*0.8f, cy+r*0.4f, cx+r*0.8f, cy+r*0.4f, ink); break;
    case DPadDown:  fillTri(p, cx, cy+r, cx-r*0.8f, cy-r*0.4f, cx+r*0.8f, cy-r*0.4f, ink); break;
    case DPadLeft:  fillTri(p, cx-r, cy, cx+r*0.4f, cy-r*0.8f, cx+r*0.4f, cy+r*0.8f, ink); break;
    case DPadRight: fillTri(p, cx+r, cy, cx-r*0.4f, cy-r*0.8f, cx-r*0.4f, cy+r*0.8f, ink); break;

    case Menu: {   // three stacked lines
      p.setBrush(ink);
      const int lw = int(float(s)*0.5f), lh = std::max(2,int(float(s)*0.07f));
      for(int i=0;i<3;++i)
        p.drawRect(int(float(x)+float(s)*0.25f), int(float(y)+float(s)*(0.34f+0.16f*float(i))), lw, lh);
      break;
      }
    case View: {   // two panes
      p.setBrush(ink);
      p.drawRect(int(float(x)+float(s)*0.22f), int(float(y)+float(s)*0.34f), int(float(s)*0.24f), int(float(s)*0.32f));
      p.drawRect(int(float(x)+float(s)*0.54f), int(float(y)+float(s)*0.34f), int(float(s)*0.24f), int(float(s)*0.32f));
      break;
      }
    }
  }

int PadGlyph::drawLabelled(Painter& p, const GthFont& fnt, Btn b,
                           int x, int y, int s, std::string_view label, float a) {
  draw(p, fnt, b, x, y, s, a);
  const int gap = std::max(2, s/6);
  const int tx  = x + s + gap;
  fnt.drawText(p, tx, y + s - (s - fnt.textSize(label).h)/2, label);
  return s + gap + fnt.textSize(label).w + gap*2;
  }
