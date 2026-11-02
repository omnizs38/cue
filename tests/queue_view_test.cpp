#include <algorithm>
#include <deque>
#include <string>
#include <unordered_set>
#include <vector>
#define private public
#include "../src/ui/queue_view.h"
#include "../src/ui/play.h"
#undef private
#include "../src/core/queue.h"
#include "../src/core/i18n.h"
#include <cassert>
#include <cstdio>

static void settle(QueueView& q) {for(int n=0;n<180;++n)q.tick(1./120);}
int main() {
  std::vector<QueueEntry> entries;
  for(uint32_t i=1;i<=512;++i)entries.push_back({i,"Episode with a long name S01E"+std::to_string(i)+".mkv",i==1,i==3});
  QueueView q;q.sync(entries,1);q.layout(720,704,2);q.opened();
  assert(q.items_.size()==512&&q.viewport_.h>0&&q.thumbnail_request()==1);
  // Pointer threshold, stable drag identity, animated gap, one commit on release.
  int y=q.viewport_.y+q.row_h()/2;
  q.down(16,y);q.move(18,y+2);assert(!q.dragging());
  q.move(16,y+3*q.row_h());assert(q.drag_id_==1&&q.drop_==3);
  auto event=q.up(16,y+3*q.row_h());assert(event.action==QueueView::Action::Move&&event.id==1&&event.index==3);
  MediaQueue model;std::vector<std::string> paths(512,"same.mkv");model.append(paths);model.select(1);
  assert(model.move_to(event.id,event.index)&&model.current()==1&&model.items()[3].id==1&&model.next()->id==5);
  assert(!model.move_to(1,-1)&&!model.move_to(1,512)&&!model.move_to(999,2));
  auto removed=model.items()[2].id;model.select(removed);model.remove(removed);
  auto next=model.next()->id;model.move_to(model.items().back().id,0);assert(model.next()->id==next);
  // External queue revision during a drag cancels instead of playing/removing a different item.
  q.down(16,y);q.move(16,y+2*q.row_h());entries.erase(entries.begin());q.sync(entries,2);
  assert(!q.held()&&q.up(16,y).action==QueueView::Action::None);
  q.wheel(-1);assert(q.scroll_.target>0);settle(q);
  q.key("end",false,false);settle(q);assert(q.focus_==512&&q.scroll_.x==q.max_scroll());
  q.key("home",false,false);settle(q);assert(q.focus_==2&&q.scroll_.x==0);
  auto bottom=q.viewport_.b()-2;q.down(16,y);q.move(16,bottom);double before=q.scroll_.x;
  for(int n=0;n<60;++n)q.tick(1./60);
  assert(q.scroll_.x>before);q.key("esc",false,false);assert(!q.held());
  q.down(16,y);q.move(16,y+2*q.row_h());q.layout(710,680,2);
  assert(!q.held()&&q.up(16,y).action==QueueView::Action::None);
  // Scrollbar owns its gesture; release never starts playback.
  q.down(q.bar_.x,q.bar_.cy());q.move(q.bar_.x,q.bar_.b());
  assert(q.up(q.bar_.x,q.bar_.b()).action==QueueView::Action::None);
  // Only visible videos request images; cache, failure set and pixels stay bounded.
  q.key("home",false,false);settle(q);std::vector<uint32_t> pixels(84*48,0xff445566);
  for(uint32_t id=2;id<=512;++id)q.thumbnail(id,pixels);
  assert(q.pictures_.size()==32);
  q.thumbnail(2,{});q.thumbnail(3,{});assert(q.failed_.size()==2);
  q.sync({},3);assert(q.pictures_.empty()&&q.failed_.empty());
  // Keyboard toolbar is reachable, and reduced motion lands at its endpoint.
  q.sync(entries,4);q.opened();q.key("tab",false,false);assert(q.control_==0);  // the list first
  q.key("tab",false,false);
  assert(q.key("enter",false,false).action==QueueView::Action::Add);
  q.key("tab",false,false);assert(q.key("enter",false,false).action==QueueView::Action::Folder);
  q.key("down",false,false);q.key("end",false,false);reduce_motion()=true;q.tick(1./60);
  assert(q.scroll_.settled());reduce_motion()=false;
  auto shortened=entries;shortened.pop_back();q.sync(shortened,5);
  assert(q.focus_==511&&q.scroll_.target<=q.max_scroll());
  // Clipping / adaptive geometry across locales, UI scales and short/portrait windows.
  for(int lang=0;lang<i18n::kLangCount;++lang)for(double scale:{1.,1.5,2.,3.})for(auto size:{std::pair{320,240},std::pair{540,960},std::pair{1920,800}}) {
    i18n::set_language((i18n::Lang)lang);PlayView p;p.in.dur=100;p.in.queue_active=true;p.in.queue=entries;
    p.resize(size.first,size.second,scale);p.reset(1);p.key("q",false,false,1);
    for(int n=0;n<80;++n)p.tick(1+n/60.,1./60);
    Rect r=p.panel_rect();assert(r.x>=0&&r.y>=0&&r.r()<=size.first&&r.b()<=size.second);
    assert(p.queue_view_.viewport_.h>=p.queue_view_.row_h());
    p.key("end",false,false,3);p.key("enter",false,false,3);
    auto actions=p.take_actions();assert(!actions.empty()&&actions.back().a==PA::QueuePlay&&actions.back().id==512);
  }
  PlayView playing;playing.in.dur=100;playing.in.queue_active=true;playing.in.queue={{1,"current.mkv",true},{2,"next.mkv",false}};
  playing.resize(1280,720,2);playing.reset(1);playing.key("q",false,false,1);
  playing.key("enter",false,false,2);auto transport=playing.take_actions();
  assert(transport.size()==1&&transport[0].a==PA::TogglePause&&playing.queue_open());
  // Wheel inside the sheet cannot accidentally change volume or pause playback.
  Rect sheet=playing.panel_rect();playing.mouse_move(sheet.cx(),sheet.cy(),2);playing.wheel(-1,2);
  assert(playing.take_actions().empty());
  // The panel comes out of its button: its list icon sits over the bar's up-next icon
  // (unless the window edge stops it), it stays inside the window and off the controls.
  for(bool free:{false,true})for(auto size:{std::pair{1920,1080},std::pair{2560,1080},std::pair{3840,2160}})for(double scale:{1.,2.}) {
    PlayView p;p.in.dur=100;p.in.controls_free=free;p.in.queue_active=true;p.in.queue=entries;
    p.resize(size.first,size.second,scale);p.reset(1);p.key("q",false,false,1);
    for(int n=0;n<40;++n)p.tick(1+n/60.,1./60);
    Rect r=p.panel_rect();assert(r.x>=0&&r.r()<=size.first&&r.y>=0&&r.b()<=size.second);
    Rect b;for(auto& h:p.hots_)if(h.id==PlayView::B_QUEUE)b=h.r;
    int icon=r.x+(int)std::lround(16*p.queue_s_),button=p.dock_rect().x+b.cx();
    bool clamped=r.x<=(int)std::lround(8*p.queue_s_)||r.r()>=size.first-(int)std::lround(8*p.queue_s_);
    assert(clamped||std::abs(icon-button)<=1);
    for(auto& h:p.hots_) {Rect b=h.r;b.x+=p.dock_rect().x;b.y+=p.dock_rect().y;assert(b.r()<=r.x||b.x>=r.r()||b.b()<=r.y||b.y>=r.b());}
  }
  puts("Queue sheet: complete 512-item list, drag/drop, cancellation, scroll, keyboard, image bounds, 96 adaptive layouts and panels anchored to their button and clear of the controls passed");
}
