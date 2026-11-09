// Portable PlayView benchmark; no decoder, compositor or Windows timing.
// Build with portable core/UI objects, as in tools/test.sh.
#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#define private public
#include "../src/ui/play.h"
#undef private
#include <cstdio>
int main(){
 PlayView p;p.in.dur=7200;p.in.preview_aspect=40./22;p.resize(1280,720,2);double t=1;p.reset(t);
 auto step=[&](){t+=1/60.;p.activity(t);p.tick(t,1/60.);};
 for(int i=0;i<60;++i)step();
 Rect d=p.dock_rect();p.mouse_move(d.x+p.tl_.cx(),d.y+p.tl_.y,t);for(int i=0;i<45;++i)step();
 std::vector<uint32_t> px(40*22,0xFF4699CCu);
 std::vector<double> times;
 for(int j=0;j<5;++j){auto a=std::chrono::steady_clock::now();
  for(int i=0;i<2000;++i){p.set_thumb(px,40,22,i%100);step();}
  times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-a).count());
 }
 std::sort(times.begin(),times.end());
 std::printf("2000_frame_updates_median_ms=%.3f retained_display_source_bytes=%zu card_canvas_bytes=%zu\n",times[2],p.thumb_.px.capacity()*4,p.layers[PlayView::L_TIP].c.px.capacity()*4);
}
