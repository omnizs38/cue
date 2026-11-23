// Ambient light, in the spirit of an Ambilight TV: a soft halo just outside the window, each
// stretch lit by the colour of the picture's border next to it. Built to cost almost nothing:
// the halo's shape is computed once per window size; an update only recolours a prepared list
// of pixels through a cached colour ramp, at up to 30 Hz, and moving the window moves it as
// is. A separate click-through layered window kept right below the player in z-order.
#pragma once
#include <windows.h>

#include <cstdint>
#include <vector>

#include "../ui/ambient_reveal.h"
#include "../ui/atmosphere.h"
#include "../ui/gfx.h"

class Ambient {
 public:
  // the window is held by the person (moved): the halo moves with it, and any rebuild waits
  void hold(bool on) { held_ = on; }
  static constexpr int kZones = FrameColors::kZones;
  bool create(HINSTANCE inst);
  void destroy();
  void reset();  // hide immediately between videos, reusing allocated surfaces
  // zones clockwise from the top-left corner (see FrameColors); light 0..1 per zone
  // strength 0..1 scales the whole halo (no repaint: only the strips' opacity changes)
  bool update(HWND owner, const Rgb* zones, const double* light, bool visible, double strength, double dt, bool defer_geometry = false);
  void follow(HWND owner, bool defer_geometry = false);
  // put out with the colours it has (the Cue closing: nobody steps the colours any more)
  bool fade(HWND owner, double dt) { return update(owner, zone_, light_, false, strength_, dt); }
  // the windows and the shape made ahead, hidden (nothing shows until update() lights it)
  void prepare(HWND owner, HINSTANCE inst);
  bool prepared(HWND owner) const;  // made for this window's size already (lighting it is cheap)
  void set_topmost(bool on);  // the glow stays just under its window, on top or not  // move cached pixels during interaction
  bool active() const { return reveal_.phase != AmbientReveal::Off || want_; }
  // going out (gathering back into the window): the poll keeps its full pace until it is gone, even
  // minimized (at 4 a second the last of it stayed where the window had been)
  bool leaving() const { return reveal_.phase == AmbientReveal::Out; }
  // coming or going (the poll asks Windows for a fine clock meanwhile: 30 steps a second, not ~21)
  bool revealing() const { return reveal_.phase == AmbientReveal::In || reveal_.phase == AmbientReveal::Out; }
  // put out in this very instant, keeping its shape (minimizing: see Ambient::follow)
  void vanish();
  // the next exit's length, when it is not the usual one (the picture going dark into full screen)
  void exit_in(double s) { next_out_ = s; }
  // reduce motion: the light comes and goes by its opacity alone
  void set_motion(bool on) { motion_ = on; }

 private:
  // four thin strips around the player (top and bottom include the corners): only pixels that
  // can glow are ever stored or sent, never the big transparent middle. The shape is the whole
  // strip's, made once per size; the surface sent to Windows is only the part on a screen.
  // n pixels of one colour from `off` (row-major in the whole strip), going right, or down when `down`
  struct Run { uint32_t off; uint16_t n; uint16_t lut; uint8_t a; uint8_t down; };
  struct Strip {
    HWND wnd = nullptr;
    HDC mem = nullptr;
    HBITMAP bmp = nullptr;
    uint32_t* bits = nullptr;
    int fx = 0, fy = 0, fw = 0, fh = 0;  // the whole strip, relative to the halo's top-left
    int x = 0, y = 0, w = 0, h = 0;      // its surface (the part on a screen), same coordinates
    std::vector<Run> runs;  // the strip's shape
    std::vector<uint32_t> held;  // the colour each run holds on the surface now (see paint)
  };
  Strip st_[4];
  bool want_ = false, failed_ = false;
  AmbientReveal reveal_;  // in and out (see ambient_reveal.h)
  bool motion_ = true;
  double next_out_ = -1;
  uint8_t remap_[256]{}, painted_remap_[256]{};  // falloff level -> level shown (only while it comes or goes)
  bool remap_on_ = false, painted_remap_on_ = false;
  // What a paint of this halo costs here (ms, smoothed; the pixel loop and UpdateLayeredWindow). The
  // spreading repaints the halo on each step, so it adapts to the PC: from kHalfRate on, its steps go at
  // 15 a second; from kOpacityOnly on (a weak PC with a big window, software drawing), the light comes
  // and goes by its opacity alone, which paints nothing more than before.
  static constexpr double kHalfRateMs = 6.0, kOpacityOnlyMs = 14.0;
  double paint_ms_ = 0;
  void begin_out(HWND owner);
  double strength_ = 0.15;
  BYTE blend() const;
  int last_a_ = -1;
  double since_paint_ = 1;
  bool colors_dirty_ = true;
  Rgb zone_[kZones]{};
  double light_[kZones]{};
  // what the last paint showed. A small change waits and goes with the next (17/11): one 8-bit step at
  // most (invisible) up to 0.1 s, three at most up to 1/15 s; anything larger at the usual pace. The
  // light flows all the time now, mostly in small steps: painted on every tick it would repaint the
  // whole halo twice as often as before (measured), for steps no one can tell apart.
  Rgb painted_zone_[kZones]{};
  double painted_light_[kZones]{};
  int ticks_for_change() const;
  RECT rect_{};          // the player's visible edge the shape was built for
  int M_ = 0;
  // How far the halo reaches under each corner: Windows 11 rounds windows by 8 px at 96 dpi,
  // which left a dark wedge between the square halo and the round corner.
  int C_ = 0;
  static int corner_tuck(int dpi) { return MulDiv(8, dpi, 96) + 1; }
  bool built_ = false;
  static constexpr int kLut = 512;
  uint8_t lr_[kLut]{}, lg_[kLut]{}, lb_[kLut]{}, la_[kLut]{};
  std::vector<uint32_t> tint_;
  void build_shape(const RECT& r, int dpi);
  // the strips' surfaces are cut to the screens (all of them: a halo reaching onto the monitor
  // beside is seen there); light off every screen is never sent. The shape itself is never cut,
  // so a window coming back from past a screen's edge finds its whole halo (it used to come back
  // without the side that had been off screen, until the drag ended)
  void layout_cut(const RECT& r, RECT out[4]) const;
  bool covers(const RECT cut[4]) const;   // the surfaces already hold all of that
  bool cut_surfaces(const RECT cut[4]);   // new surfaces for those parts (the shape is kept)
  void free_bitmaps();
  bool rebuild_pending_ = false;          // the surfaces are bigger than the screens need: cut again when it settles
  double since_build_ = 1;
  int dpi_ = 96;
  void build_lut();
  void paint(HWND owner);
  void place(HWND owner, bool show);
  void free_surfaces();
  void hide();
  bool held_ = false;
};
