// Ambient light, in the spirit of an Ambilight TV: a soft halo just outside the window, each
// stretch lit by the colour of the picture's border next to it. Built to cost almost nothing:
// the halo's shape is computed once per window size; an update only recolours a prepared list
// of pixels through a cached colour ramp, at up to 30 Hz, and moving the window moves it as
// is. A separate click-through layered window kept right below the player in z-order.
#pragma once
#include <windows.h>

#include <cstdint>
#include <vector>

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
  void set_topmost(bool on);  // the glow stays just under its window, on top or not  // move cached pixels during interaction
  bool active() const { return alpha_ > 0.001 || want_; }

 private:
  // four thin strips around the player (top and bottom include the corners): only pixels that
  // can glow are ever stored or sent, never the big transparent middle. The shape is the whole
  // strip's, made once per size; the surface sent to Windows is only the part on a screen.
  struct Px { uint16_t x, y; uint16_t lut; uint8_t a; };  // x, y: in the whole strip
  struct Strip {
    HWND wnd = nullptr;
    HDC mem = nullptr;
    HBITMAP bmp = nullptr;
    uint32_t* bits = nullptr;
    int fx = 0, fy = 0, fw = 0, fh = 0;  // the whole strip, relative to the halo's top-left
    int x = 0, y = 0, w = 0, h = 0;      // its surface (the part on a screen), same coordinates
    std::vector<Px> shape;
  };
  Strip st_[4];
  bool want_ = false, failed_ = false;
  double alpha_ = 0;     // global fade
  double strength_ = 0.15;
  BYTE blend() const;
  int last_a_ = -1;
  double since_paint_ = 1;
  bool colors_dirty_ = true;
  Rgb zone_[kZones]{};
  double light_[kZones]{};
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
