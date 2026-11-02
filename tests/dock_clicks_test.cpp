#include <cassert>
#include <string>
#include <vector>
#define private public
#include "../src/ui/play.h"
#undef private
#include <cstdio>
static void settle(PlayView& p,double& t){p.activity(t);for(int i=0;i<40;++i){t+=1./60;p.tick(t,1./60);}}
int main(){
  for(double scale:{1.,2.}){
    PlayView p;p.in.dur=200;p.in.time=20;p.resize(1280*scale,720*scale,scale);double t=1;p.reset(t);
    p.mouse_move(640*scale,600*scale,t);settle(p,t);p.take_actions();
    auto rect=[&](int id){Rect d=p.dock_rect();for(auto& h:p.hots_)if(h.id==id)return Rect{h.r.x+d.x,h.r.y+d.y,h.r.w,h.r.h};return Rect{};};  // on screen
    // a right click on the bar opens nothing; on the picture it opens the menu
    Rect d=p.dock_rect();assert(d.w>0);
    p.right_click(d.x+2*(int)scale,d.cy(),t);settle(p,t);assert(!p.panel_open());
    p.right_click(100*scale,100*scale,t);settle(p,t);assert(p.panel_open());
    p.right_click(100*scale,100*scale,t);settle(p,t);assert(!p.panel_open());  // a second one only dismisses
    // with a menu open, another button of the bar acts at once
    p.activate(PlayView::B_MORE,t);settle(p,t);assert(p.panel_open());p.take_actions();
    Rect fs=rect(PlayView::B_FS);p.mouse_down(fs.cx(),fs.cy(),t);p.mouse_up(fs.cx(),fs.cy(),t);
    bool full=false;for(auto& a:p.take_actions())full|=a.a==PA::Fullscreen;
    assert(full&&!p.panel_open());
    // its own button just closes it
    p.activate(PlayView::B_MORE,t);settle(p,t);assert(p.panel_open());p.take_actions();
    Rect more=rect(PlayView::B_MORE);p.mouse_down(more.cx(),more.cy(),t);p.mouse_up(more.cx(),more.cy(),t);settle(p,t);
    assert(!p.panel_open());
    // the transition to full screen takes an open menu along on the same frame
    p.activate(PlayView::B_MORE,t);settle(p,t);assert(p.layers[PlayView::L_PANEL].visible);
    p.begin_viewport_transition(t,true);assert(!p.layers[PlayView::L_PANEL].visible&&!p.panel_open());
  }
  puts("Dock clicks: right click on the bar does nothing, other buttons act through an open menu, full screen takes the menu along passed");
}
