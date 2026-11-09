#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>
#define private public
#include "../src/ui/play.h"
#undef private
#include "../src/core/ui_scale.h"
#include <cassert>
#include <cstdio>

static void settle(PlayView& p, double& t) {
  for (int i=0; i<45; ++i) { t+=1/60.; p.activity(t); p.tick(t,1/60.); }
}

int main() {
  int cases = 0;
  for (auto window : {std::pair{288,162}, std::pair{480,270}, std::pair{640,360},
                      std::pair{270,480}, std::pair{1280,720}, std::pair{1920,1080}})
    for (double dpi : {1., 1.5, 2.})
      for (double aspect : {16./9, 9./16, 1., 32./9})
        for (bool free : {false, true}) {
          int W = window.first*dpi, H = window.second*dpi;
          PlayView p;
          p.in.dur = 7200; p.in.preview_aspect = aspect; p.in.controls_free = free;
          p.in.mini_player = window.first <= 640;
          double scale = p.in.mini_player ? mini_scale(96*dpi,W,H,2,false) : ui_scale(96*dpi,W,H,2);
          p.resize(W,H,scale);
          double t = 1; p.reset(t); settle(p,t);
          auto hover = [&](double fraction) {
            Rect d = p.dock_rect();
            p.mouse_move(d.x+p.tl_.x+p.tl_.w*fraction, d.y+p.tl_.y, t);
            settle(p,t);
          };
          auto bounds = [&] {
            const auto& tip = p.layers[PlayView::L_TIP];
            assert(tip.visible && tip.x >= 0 && tip.y >= 0);
            assert(tip.x+tip.c.w <= W && tip.y+tip.c.h <= H);
            assert(tip.y >= (int)(PlayView::kStrip*scale)); // window buttons stay clear
            Rect d = p.dock_rect();
            assert(tip.y+tip.c.h <= d.y || tip.y >= d.b());
          };
          hover(.1); assert(p.preview_active()); bounds();
          const int card_w = p.layers[PlayView::L_TIP].c.w, card_h = p.layers[PlayView::L_TIP].c.h;
          // Simulate a cached image captured at a larger UI scale; geometry uses this window.
          const int sw = 80, sh = std::max(1,(int)(sw/aspect));
          std::vector<uint32_t> pixels(sw*sh);
          for (int y=0; y<sh; ++y) for (int x=0; x<sw; ++x)
            pixels[y*sw+x] = 0xFF000000u | (uint32_t)((x*3)<<16) | (uint32_t)((y%64)<<8) | 0xCC;
          assert(p.show_thumb(pixels,sw,sh,123,p.preview_epoch()));
          settle(p,t); bounds();
          assert(p.thumb_.px.size() == (size_t)sw*sh); // no display-sized retained enlargement
          assert(p.layers[PlayView::L_TIP].c.w == card_w && p.layers[PlayView::L_TIP].c.h == card_h);
          int fw,fh; p.tip_frame_size(fw,fh);
          const auto& picture = p.layers[PlayView::L_TIP].c;
          const int ix=2*scale,iy=2*scale, image_w=picture.w-2*ix;
          int cw=sw,ch=sh;
          if(image_w>fw) {
            double factor=std::max((double)image_w/sw,(double)fh/sh);
            cw=std::clamp((int)std::lround(image_w/factor),1,sw);
            ch=std::clamp((int)std::lround(fh/factor),1,sh);
          }
          for (int y=0; y<fh; ++y) for (int x=0; x<image_w; ++x)
            assert(picture.px[(iy+y)*picture.w+ix+x] == pixels[((sh-ch)/2+y*ch/fh)*sw+(sw-cw)/2+x*cw/image_w]);
          hover(.9); bounds();
          p.pointer_left(); hover(.9); bounds();
          assert(p.thumb_.px.empty() && p.layers[PlayView::L_TIP].c.h == card_h);
          // Moving the bar to the top still leaves the card entirely inside the window.
          p.py_.snap(0); hover(.5); bounds();
          // A music mini player shows time only and emits no frame requests.
          p.in.audio_only = true; p.take_actions(); hover(.6);
          p.tip_frame_size(fw,fh); assert(!fw && !fh);
          for (auto action : p.take_actions()) assert(action.a != PA::Preview);
          ++cases;
        }
  // Portrait images are about 5% smaller in each dimension; landscape is unchanged.
  PlayView p; p.in.dur=300; p.resize(1280,720,2); double t=1; p.reset(t); settle(p,t);
  int fw,fh;
  p.in.preview_aspect=9./16; p.tip_frame_size(fw,fh);
  assert(fw==152 && fh==270); // formerly 160 x 284 at this scale
  p.in.preview_aspect=16./9; p.tip_frame_size(fw,fh);
  assert(fw==160 && fh==90);
  std::printf("Preview layout: %d window/DPI/aspect/style cases, mini/full cache reuse, 5%% smaller portraits, first-card geometry, clean re-entry, top dock and music passed\n",cases);
}
