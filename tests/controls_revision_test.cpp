#define private public
#include "../src/ui/play.h"
#undef private
#include "../src/core/store.h"
#include "../src/core/window_policy.h"
#include <cassert>
#include <cstdio>
static void settle(PlayView& p,double& t){for(int i=0;i<45;++i){t+=1./60;p.tick(t,1./60);}}
static std::vector<uint32_t> clock(const Layer& l,int scale,int y){std::vector<uint32_t> b;for(int yy=(y-5)*scale;yy<(y+5)*scale;++yy)for(int x=8*scale;x<60*scale;++x)b.push_back(l.c.px[yy*l.c.w+x]);return b;}
int main(){
 assert(Store{}.ambient);
 assert(!restore_saved_window(false, 540, 960));
 assert(restore_saved_window(false, 960, 540));
 assert(!restore_saved_window(true, 540, 960));
 assert(!restore_saved_window(false, 0, 0));
 for(int width:{240,640})for(int scale:{1,2,3}){
  PlayView p;p.in.dur=300;p.in.time=30;double t=1;p.resize(width*scale,540*scale,scale);p.reset(t);p.mouse_move(100,100,t);settle(p,t);p.take_actions();
  p.key("c",false,false,t);settle(p,t);assert(p.panel_open()&&p.take_actions().empty());p.key("esc",false,false,t);settle(p,t);
  auto& l=p.layers[PlayView::L_DOCK];assert(l.visible&&l.c.w<=394*scale);  // the bar's rhythm (22/32 art px) sets its minimumassert(l.y+l.c.h<540*scale-40*scale);
  int y=28;assert(l.c.h==38*scale);  // one row of controls and the timeline, at every width
  auto before=clock(l,scale,y);
  for(int i=0;i<2;++i){int x=l.x+20*scale,yy=l.y+y*scale;p.mouse_move(x,yy,t);p.mouse_down(x,yy,t);p.mouse_up(x,yy,t);p.mouse_move(width*scale/2,100,t);settle(p,t);assert(p.take_actions().empty());assert(!p.in.paused&&p.in.time==30);assert((clock(l,scale,y)==before)==(i==1));}
  p.key("space",false,false,t);t+=.1;p.tick(t,.1);assert(p.in.paused&&p.layers[PlayView::L_FLASH].visible);auto actions=p.take_actions();assert(actions.size()==1&&actions[0].a==PA::TogglePause);
  p.key("space",false,false,t);t+=.1;p.tick(t,.1);assert(!p.in.paused&&p.layers[PlayView::L_FLASH].visible);
  if(width==240){
    // the folded bar: play centred, -15 and +15 beside it, full screen and "..." on the right
    p.take_actions();
    auto hot=[&](int id){for(auto& h:p.hots_)if(h.id==id)return h.r;return Rect{};};
    auto click_hot=[&](int id){Rect r=hot(id);assert(r.w>0);int x=l.x+r.cx(),yy=l.y+r.cy();p.mouse_move(x,yy,t);p.mouse_down(x,yy,t);p.mouse_up(x,yy,t);};
    assert(std::abs(hot(PlayView::B_PLAY).cx()-l.c.w/2)<=scale);
    for(int dir:{-1,1}){click_hot(dir<0?PlayView::B_REW:PlayView::B_FWD);auto a=p.take_actions();assert(a.size()==1&&a[0].a==PA::SeekRel&&a[0].v==15*dir);}
    click_hot(PlayView::B_PLAY);auto a=p.take_actions();assert(a.size()==1&&a[0].a==PA::TogglePause);settle(p,t);p.take_actions();
    if(p.in_bar(PlayView::B_VOLBAR)){
      Rect vb=hot(PlayView::B_VOLBAR);int px=l.x+vb.r()-2*scale,py=l.y+vb.cy();
      p.mouse_move(px,py,t);p.mouse_down(px,py,t);p.mouse_up(px,py,t);
      auto volume=p.take_actions();assert(!volume.empty());for(auto action:volume)assert(action.a==PA::Volume);
    }else{
      click_hot(PlayView::B_VOL);assert(p.panel_==PlayView::Panel::Volume&&p.take_actions().empty());p.key("esc",false,false,t);settle(p,t);
    }
    click_hot(PlayView::B_FS);a=p.take_actions();assert(a.size()==1&&a[0].a==PA::Fullscreen);
    click_hot(PlayView::B_MORE);assert(p.panel_open()&&p.take_actions().empty());p.key("esc",false,false,t);
  }
  assert(bmp("cue:skip-back")&&bmp("cue:skip-forward"));
 }
 puts("Controls: one-row folded bar, compact width, raised position, C opens CC, clock toggles without pause/seek, central feedback and default light passed");
}
