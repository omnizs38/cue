// Exercise the same repeat cycle and rendered state in both bars and overflow menus.
#define private public
#include "../src/ui/play.h"
#undef private
#include "../src/core/i18n.h"
#include <cassert>
#include <cstdio>

static void settle(PlayView& p, double& t) {
  for (int i = 0; i < 40; ++i) { t += 1./60; p.activity(t); p.tick(t, 1./60); }
}

int main() {
  int checked = 0;
  for (bool music : {false, true}) for (bool overflow : {false, true})
    for (bool list : {false, true}) for (bool free : {false, true})
      for (double s : {1., 1.5, 2., 2.5}) {
        PlayView p; p.in.dur = 180; p.in.audio_only = music;
        p.in.mini_player = overflow; p.in.cover_scene = music;
        p.in.controls_free = free;
        p.in.queue_active = list;
        if (list) p.in.queue = {{1,"A",true,music},{2,"B",false,music}};
        p.resize((overflow ? 480 : 1300)*s, (overflow ? 270 : 730)*s, s);
        double t = 1; p.reset(t); settle(p,t);
        Rect cell; int part = -1, row_index = -1;
        if (overflow) {
          p.activate(PlayView::B_MORE,t); settle(p,t);
          for (int k=0;k<(int)p.tool_ids_.size();++k) if (p.tool_ids_[k]==PlayView::B_LOOP) part=k;
          for (int i=0;i<(int)p.rows_.size();++i) if (p.rows_[i].id==PlayView::R_TOOLS) row_index=i;
          assert(part>=0 && row_index>=0); cell=p.rows_[row_index].parts[part];
        } else { assert(p.in_bar(PlayView::B_LOOP)); cell=p.slot_rect(PlayView::B_LOOP); }
        auto frame=[&]() { return p.layers[overflow?PlayView::L_PANEL:PlayView::L_DOCK].c; };
        auto click=[&]() {
          p.take_actions();
          if (overflow) {
            Rect panel=p.panel_rect();
            p.mouse_move(panel.x+cell.cx(),panel.y+cell.cy(),t);
            p.mouse_down(panel.x+cell.cx(),panel.y+cell.cy(),t);
            p.mouse_up(panel.x+cell.cx(),panel.y+cell.cy(),t);
          } else p.activate(PlayView::B_LOOP,t);
          auto a=p.take_actions(); assert(a.size()==1 && a[0].a==PA::Loop);
          settle(p,t); return (int)a[0].v;
        };
        // Keep the pointer on the menu button: hover must not erase the active marker.
        if (overflow) { Rect panel=p.panel_rect();p.mouse_move(panel.x+cell.cx(),panel.y+cell.cy(),t);settle(p,t); }
        const Canvas off=frame();
        assert(click()==(list?1:2));
        const Canvas enabled=frame();
        const double grid=overflow?p.panel_s():s;
        const int mx=overflow?cell.cx():(int)((cell.x+cell.w/2)/s)*s;
        const int my=overflow?std::lround(cell.b()-2*grid):(int)((cell.y+(cell.h+7*s)/2)/s)*s+2*s;
        assert(mx>=0 && mx<enabled.w && my>=0 && my<enabled.h);
        assert(enabled.px[(size_t)my*enabled.w+mx]!=off.px[(size_t)my*off.w+mx]);
        if (list) {
          assert(p.in.loop_all && !p.in.loop);
          if (overflow) assert(p.rows_[row_index].segs[part]==tr(str::tip_loop_all));
          assert(click()==2 && p.in.loop && !p.in.loop_all);
          const Canvas single=frame(); int different=0;
          // Numeral at the shoulder: the underlying loop stays, the numeral appears.
          for(int y=std::max(0,cell.y);y<std::min(single.h,cell.b());++y)
            for(int x=std::max(0,cell.cx());x<std::min(single.w,cell.r());++x)
              different+=single.px[(size_t)y*single.w+x]!=enabled.px[(size_t)y*enabled.w+x];
          assert(different>0);
          if (overflow) assert(p.rows_[row_index].segs[part]==tr(str::tip_loop_one));
        } else assert(p.in.loop && !p.in.loop_all);
        assert(click()==0 && !p.in.loop && !p.in.loop_all);
        assert(frame().px[(size_t)my*enabled.w+mx]==off.px[(size_t)my*off.w+mx]);
        ++checked;
      }
  std::printf("Repeat: %d video/audio bar/menu cycles, list/single/off, markers and numerals passed\n",checked);
}
