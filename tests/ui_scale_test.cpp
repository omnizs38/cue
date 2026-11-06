#include <fstream>
#include <sstream>
#include "../src/core/ui_scale.h"
#include "../src/core/store.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>
#define private public
#include "../src/ui/play.h"
#undef private
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>

int main() {
  int choice = -1;
  for (int expected : {2, 0, 3, 3}) { choice = next_ui_size(choice, 1); assert(choice == expected); }
  for (int expected : {0, 2, -1, -1}) { choice = next_ui_size(choice, -1); assert(choice == expected); }
  assert(ui_scale(96,1280,720,0) == 2);
  assert(ui_scale(96,1280,720,2) == 1.5);
  assert(ui_scale(96,1280,720,3) == 2.5 && ui_scale(96,1280,720,1) == 2.5);  // the old 1.5x larger reads as larger
  // every size visibly different from its neighbours, from a small laptop to 8K at any Windows scale
  for (unsigned dpi : {96u, 120u, 144u, 192u, 288u, 384u})
    for (auto wh : {std::pair{1366, 768}, {1920, 1080}, {2560, 1440}, {3840, 2160}, {5120, 2880}, {7680, 4320}}) {
      double prev = 0;
      for (int i = 0; i < kUiSizes; ++i) {
        double v = ui_scale(dpi, wh.first, wh.second, ui_size_choice(i));
        double fit = std::floor(std::min(wh.first / 320.0, wh.second / 180.0) * 2) / 2;
        assert(v >= prev + 0.5 || v == fit);
        prev = v;
      }
    }
  // 8K: at 300% it matches 1440p at 100% (same space in DIPs, three times the pixels); at 100%
  // (a television) it keeps growing past 4K instead of stopping there
  assert(ui_scale(288, 7680, 4320, 2) == 3 * ui_scale(96, 2560, 1440, 2));
  assert(ui_scale(96, 7680, 4320, 2) > ui_scale(96, 3840, 2160, 2));
  for (unsigned dpi : {96, 120, 144, 168, 192, 240, 288}) {
    for (int w : {640, 960, 1280, 1920, 3840}) {
      int h = w * 9 / 16;
      double small = ui_scale(dpi,w,h,-1), medium = ui_scale(dpi,w,h,2);
      double large = ui_scale(dpi,w,h,0), larger = ui_scale(dpi,w,h,1);
      assert(small <= medium && medium <= large && large <= larger);
      assert(std::fmod(medium * 2, 1.0) == 0);  // whole or half art px
    }
  }
  // full screen: bigger than the window, but less than in proportion to the picture
  for (int choice : {-1, 2, 0, 1}) {
    double win = ui_scale(96, 1766, 993, choice), full = ui_scale(96, 2560, 1440, choice);
    assert(full >= win && full <= win * 2560 / 1766);
    double win4k = ui_scale(144, 2649, 1490, choice), full4k = ui_scale(144, 3840, 2160, choice);
    assert(full4k >= win4k && full4k <= win4k * 3840 / 2649 + 0.25);
  }
  // in step with the window up to a roomy one: 1280 -> 1600 px grows like the window
  assert(ui_scale(96, 1600, 900, 0) == 2.5 && ui_scale(96, 1280, 720, 0) == 2);
  // width leads: a 2.39:1 window gets the controls of the 16:9 window of the same width
  for (int w : {1280, 1920, 2560}) assert(ui_scale(96, w, (int)(w / 2.39), 2) == ui_scale(96, w, w * 9 / 16, 2));
  // a small window still holds the size down; the reference size is the base
  assert(ui_scale(192, 700, 394, 1) <= 2.5);
  assert(ui_base_scale(96, 2) == 1.5 && ui_base_scale(96, 3) == 2.5 && ui_base_scale(96, 1) == 2.5);
  auto path = std::filesystem::temp_directory_path() / "cue-ui-scale-test.json";
  for (int stored : {-1,0,2,3,1}) {
    Store before; before.ui_scale = stored; assert(before.save(path));
    Store after; assert(after.load(path)); assert(after.ui_scale == (stored == 1 ? 3 : stored));  // old 1.5x: larger
  }
  std::filesystem::remove(path); std::filesystem::remove(path.string()+".bak");
  for (bool audio : {false,true}) for (double scale : {1.,1.5,2.,2.5,3.,3.5,4.5}) for (int chip=0;chip<kUiSizes;++chip) {
    PlayView p; p.in.audio_only=audio; p.in.dur=120; p.resize(2560,1440,scale); p.reset(1);
    const auto subtitle = p.in.style;
    p.open_panel_for_preview(11,1);  // Settings
    for(int i=1;i<=45;++i) p.tick(1+i/60.,1/60.);
    p.take_actions();
    const auto& panel=p.layers[PlayView::L_PANEL];
    assert(panel.visible && panel.x>=0 && panel.x+panel.c.w<=2560);
    auto row=std::find_if(p.rows_.begin(),p.rows_.end(),[](const auto& r){return r.id==PlayView::R_UISCALE;});
    if(row==p.rows_.end()){  // a menu too tall for the room above the bar: the sizes are one row deeper
      auto sub=std::find_if(p.rows_.begin(),p.rows_.end(),[](const auto& r){return r.id==PlayView::R_INTERFACE;});
      assert(sub!=p.rows_.end()&&p.settings_fold_);
      p.panel_click((int)(sub-p.rows_.begin()),-1,0,1);
      for(int i=1;i<=30;++i) p.tick(1+i/60.,1/60.);
      row=std::find_if(p.rows_.begin(),p.rows_.end(),[](const auto& r){return r.id==PlayView::R_UISCALE;});
    }
    assert(row!=p.rows_.end()&&row->parts.size()==kUiSizes);
    Rect target=row->parts[chip];
    int x=panel.x+target.cx(),y=panel.y+target.cy();
    p.mouse_move(x,y,2);p.mouse_down(x,y,2);p.mouse_up(x,y,2.01);
    bool found=false;
    for (auto& a:p.take_actions()) if(a.a==PA::UiScale) {assert((int)a.v==ui_size_choice(chip));found=true;}
    assert(found); assert(p.in.style==subtitle);
  }
  // the smallest windows are the medium size's at every size chosen (at "maior" a film's window
  // could not get under 1280 x 720 px at 150%); there, the larger sizes step down to fit the canvas
  for (unsigned dpi : {96u, 120u, 144u, 168u, 192u, 288u}) {
    const double bm = ui_base_scale(dpi, kDefaultUiSize);
    for (int choice : {-1, 2, 0, 3}) {
      const int cw = (int)std::lround(320 * bm), ch = (int)std::lround(180 * bm);
      const double sc = ui_scale(dpi, cw, ch, choice);
      assert(sc >= 1 && 320 * sc <= cw + 1e-9 && 180 * sc <= ch + 1e-9);
      if (choice == kDefaultUiSize) assert(sc == bm);
    }
  }
  {
    std::ifstream fa("src/win/app.cpp"); std::stringstream sa; sa << fa.rdbuf();
    assert(sa.str().find("double bs = ui_base_scale(dpi_, kDefaultUiSize);") != std::string::npos);
  }
  std::puts("UI scale: four even choices, DPI bounds, stable resize, saved preferences, all menu hitboxes at six scales in audio/video passed");
}
