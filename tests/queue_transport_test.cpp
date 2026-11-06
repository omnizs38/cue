#include <algorithm>
#include <array>
#include <deque>
#include <string>
#include <unordered_set>
#include <vector>
#define private public
#include "../src/ui/play.h"
#undef private
#include "../src/core/remote_proto.h"
#include "../src/core/json.h"
#include <cassert>
#include <cstdio>
int main(){
  for(double scale:{1.,1.5,2.,3.})for(auto size:{std::pair{640,900},std::pair{1280,720},std::pair{1920,1080}})for(int tracks:{1,3})for(bool frameless:{false,true})for(double speed:{1.,1.75,2.75}){
    PlayView p;p.in.dur=100;p.in.queue_active=true;p.in.has_next=true;p.in.controls_free=frameless;p.in.speed=speed;p.in.audios.resize(tracks);p.resize(size.first,size.second,scale);p.reset(1);
    auto rect=[&](int id){for(auto& h:p.hots_)if(h.id==id)return h.r;return Rect{};};
    auto play=rect(PlayView::B_PLAY),next=rect(PlayView::B_NEXT),fwd=rect(PlayView::B_FWD),back=rect(PlayView::B_REW);
    if(p.fold_steps_){  // a bar too narrow for both: previous · play · next, the 15-second steps on their keys
      auto prev=rect(PlayView::B_PREV);assert(back.w==0&&fwd.w==0&&prev.w>0&&next.w>0&&prev.r()<=play.x&&play.r()<=next.x);
      assert(std::abs((play.x-prev.x)-(next.x-play.x))<=(int)scale);
      p.activate(PlayView::B_NEXT,2);auto a=p.take_actions();assert(a.size()==1&&a[0].a==PA::NextEpisode);
      p.key("right",false,false,2);a=p.take_actions();assert(!a.empty()&&a[0].a==PA::SeekRel);
      continue;
    }
    assert(next.w>0&&back.r()<play.x&&play.r()<=fwd.x&&fwd.r()<=next.x);  // previous · -15 · play · +15 · next
    for(const auto& h:p.hots_)if(h.id!=PlayView::B_TIME&&h.id!=PlayView::B_TIMELINE&&h.id!=PlayView::B_NEXT&&h.id!=PlayView::B_DOCKBG)
      assert(h.r.b()<=next.y||h.r.y>=next.b()||h.r.r()<=next.x||h.r.x>=next.r());
    p.activate(PlayView::B_NEXT,2);auto a=p.take_actions();assert(a.size()==1&&a[0].a==PA::NextEpisode);
    p.activate(PlayView::B_FWD,2);a=p.take_actions();assert(a.size()==1&&a[0].a==PA::SeekRel&&a[0].v==15);
    // previous and next sit outside the 15-second steps; at the ends they stay, dimmed and inert
    auto prev=rect(PlayView::B_PREV);assert(prev.w>0&&prev.r()<=back.x&&back.r()<=play.x);
    assert(std::abs((play.x-prev.x)-(next.x-play.x))<=(int)scale);  // play is centred
    p.activate(PlayView::B_PREV,2);assert(p.take_actions().empty());   // nothing before the first
    p.in.has_prev=true;p.changed(2);p.activate(PlayView::B_PREV,2);a=p.take_actions();assert(a.size()==1&&a[0].a==PA::PreviousInQueue);
    p.in.has_next=false;p.changed(3);assert(rect(PlayView::B_NEXT).w>0&&rect(PlayView::B_PLAY).x==play.x);
    p.activate(PlayView::B_NEXT,3);assert(p.take_actions().empty());
  }
  // Up next keeps a permanent dock slot next to repeat, with or without a queue, in wide and narrow docks.
  for(double scale:{1.,2.,3.})for(int width:{560,640,700,720,760,800,1280,1920})for(int tracks:{1,3})for(bool active:{false,true})for(bool next:{false,true})for(double speed:{1.,2.75}){
    if(next&&!active)continue;
    PlayView p;p.in.dur=100;p.in.queue_active=active;p.in.has_next=next;p.in.speed=speed;p.in.audios.resize(tracks);
    p.resize((int)(width*scale/2),(int)(720*scale/2),scale);p.reset(1);
    auto rect=[&](int id){for(auto& h:p.hots_)if(h.id==id)return h.r;return Rect{};};
    // up next and repeat: on the bar, or (a narrow bar) one click away in "..."'s row of buttons
    for(int id:{PlayView::B_QUEUE,PlayView::B_LOOP})assert(rect(id).w>0||p.overflowed(id));
    if(rect(PlayView::B_QUEUE).w==0){
      p.activate(PlayView::B_MORE,2);assert(p.panel_==PlayView::Panel::More);
      int row=-1;for(int i=0;i<(int)p.rows_.size();++i)if(p.rows_[i].k==PlayView::RK::Tools)row=i;
      assert(row>=0);int part=-1;for(int k=0;k<(int)p.tool_ids_.size();++k)if(p.tool_ids_[k]==PlayView::B_QUEUE)part=k;
      assert(part>=0);p.panel_click(row,part,0,2);assert(p.queue_open());
      continue;
    }
    for(const auto& a:p.hots_)for(const auto& b:p.hots_) {
      auto ignored=[](int id){return id==PlayView::B_TIME||id==PlayView::B_TIMELINE||id==PlayView::B_DOCKBG||id==PlayView::B_VOLBAR;};
      if(&a==&b||ignored(a.id)||ignored(b.id))continue;
      assert(a.r.r()<=b.r.x||b.r.r()<=a.r.x||a.r.b()<=b.r.y||b.r.b()<=a.r.y);
    }
    p.activate(PlayView::B_QUEUE,2);assert(p.queue_open());
  }
  // Short, wide windows (ultrawide video): menus and About never cover the control bar.
  for(auto size:{std::pair{1920,540},std::pair{2560,720},std::pair{3440,900}})for(double scale:{2.,3.})for(int which:{0,1}) {
    PlayView p;p.in.dur=100;p.in.queue_active=true;p.resize(size.first,size.second,scale);double t=1;p.reset(t);
    p.open_panel_for_preview(3,t);
    if(which){for(int i=0;i<20;++i)p.key("down",false,false,t);p.key("right",false,false,t);}
    for(int i=0;i<30;++i){t+=1/60.;p.tick(t,1/60.);}
    Rect r=p.panel_rect(),d=p.dock_rect();
    if(r.h+d.h+16*scale>size.second)continue;  // no arrangement can fit both
    assert(r.x>=0&&r.y>=0&&r.r()<=size.first&&r.b()<=size.second);
    if(!(r.r()<=d.x||d.r()<=r.x||r.b()<=d.y||d.b()<=r.y))std::fprintf(stderr,"%dx%d s%.0f which%d panel %d %d %d %d dock %d %d %d %d\n",size.first,size.second,scale,which,r.x,r.y,r.w,r.h,d.x,d.y,d.w,d.h);
    assert(r.r()<=d.x||d.r()<=r.x||r.b()<=d.y||d.b()<=r.y);
  }
  remote::Command c;
  assert(remote::parse_command(R"({"c":"queue-move","v":42,"to":511})",c)&&c.to==511&&c.v==42);
  for(const char* to:{"-1","512","1.5","true","\"1\"","1e100"})assert(!remote::parse_command(std::string("{\"c\":\"queue-move\",\"v\":42,\"to\":")+to+"}",c));
  assert(!remote::parse_command(R"({"c":"queue-move","v":0,"to":1})",c));
  remote::State state;state.queue_active=true;state.queue_busy=true;state.queue_notice_id=7;state.queue_result=1;state.queue_notice="added";
  JVal value;assert(json_parse(remote::state_json(state),value));
  assert(value["queueBusy"].boolean()&&value["queueActive"].boolean()&&value["queueNoticeId"].num()==7&&value["queueResult"].num()==1);
  puts("Queue transport: separate next/15-second actions, stable play position, 288 layouts, previous/next either side of a centred play, inert at the ends, up next on the bar or in the more menu without overlaps in 288 docks, previous/play/next when the bar is too narrow for the steps, strict absolute reorder, menus clear of the bar in short wide windows and authoritative folder status passed");
}
