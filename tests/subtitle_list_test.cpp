#include <algorithm>
#include <array>
#include <deque>
#include <string>
#include <unordered_set>
#include <vector>
#define private public
#include "../src/ui/play.h"
#undef private
#include "../src/core/i18n.h"
#include "../src/core/util.h"
#include <cassert>
#include <cstdio>
static void settle(PlayView& p,double& t) {for(int i=0;i<50;++i){t+=1./60;p.tick(t,1./60);}p.take_actions();}
int main() {
  std::vector<Track> tracks;
  for(int64_t id=1;id<=500;++id) {
    Track track;track.id=id;track.type="sub";track.lang=id%2?"en-US":"pt-BR";
    track.title="A complete real subtitle title with region and commentary · 日本語 · "+std::to_string(id);
    track.forced=id%5==0;track.hearing_impaired=id%7==0;tracks.push_back(track);
  }
  auto labels=track_labels(tracks);
  assert(labels.size()==500&&labels[0].find("US")!=std::string::npos&&labels[0].find(tracks[0].title)!=std::string::npos);
  assert(labels[6].find("SDH")!=std::string::npos&&labels[4].find(tr(str::track_forced))!=std::string::npos);
  Track unknown;unknown.id=1001;unknown.type="sub";unknown.title="Bulgarian (Bulgaria) complete subtitle";
  assert(track_label(unknown,{unknown})==unknown.title);
  Track caption;caption.type="sub";caption.lang="eng";caption.title="SDH";
  assert(track_label(caption,{caption}).find("SDH")!=std::string::npos);
  for(int lang=0;lang<i18n::kLangCount;++lang)for(double scale:{1.,1.5,2.,3.})for(auto size:{std::pair{320,240},std::pair{540,960},std::pair{1920,800}}) {
    i18n::set_language((i18n::Lang)lang);PlayView p;p.in.dur=100;p.in.subs=tracks;p.in.sid=500;p.in.kind=SubKind::Text;
    p.resize(size.first,size.second,scale);p.reset(1);double t=1;p.key("c",false,false,t);settle(p,t);
    Rect r=p.panel_rect();assert(r.x>=0&&r.y>=0&&r.r()<=size.first&&r.b()<=size.second);
    assert(p.cc_count_==500&&p.cc_viewport_.h>=p.cc_row_h_&&p.cc_scroll_max()>0);
    auto& selected=p.rows_[p.cc_first_+499];assert(selected.checked);
    assert(selected.r.y>=p.cc_scroll_.x-1&&selected.r.b()<=p.cc_scroll_.x+p.cc_viewport_.h+1);
    assert(selected.label.find(tracks.back().title)!=std::string::npos);
    assert(text_w(selected.display_label,p.cc_scale_)<=selected.r.w-29*p.cc_scale_);
    for(auto& row:p.rows_)if(row.id!=PlayView::R_SUB_TRACK)assert(row.r.y>=0&&row.r.b()<=p.ph_);
    auto rows=p.rows_.data();auto count=p.panel_gen_;
    for(int i=0;i<100;++i){p.in.time=i;p.changed(t);}
    assert(p.rows_.data()==rows);settle(p,t);assert(p.panel_gen_<=count+1);
    p.mouse_move(r.x+p.cc_viewport_.cx(),r.y+p.cc_viewport_.cy(),t);
    double before=p.cc_scroll_.target;p.wheel(.25,t);assert(p.cc_scroll_.target<before&&p.take_actions().empty());
    settle(p,t);p.key("home",false,false,t);settle(p,t);assert(p.pfocus_==p.cc_first_&&p.cc_scroll_.x==0);
    p.key("enter",false,false,t);auto acts=p.take_actions();assert(acts.size()==1&&acts[0].a==PA::SelectSub&&acts[0].id==1);
    p.key("end",false,false,t);settle(p,t);assert(p.pfocus_==p.cc_first_+499);
    p.mouse_down(r.x+p.cc_thumb_.cx(),r.y+p.cc_thumb_.cy(),t);assert(p.cc_drag_);
    p.mouse_move(r.x+p.cc_thumb_.cx(),r.y+p.cc_bar_.y,t+.01);p.mouse_up(r.x,r.y,t+.02);
    assert(!p.cc_drag_&&p.cc_scroll_.x==0&&p.take_actions().empty());
    Rect toggle=p.rows_[0].r;assert(p.hit_panel(r.x+toggle.cx(),r.y+toggle.cy(),nullptr)==0);
    p.key("esc",false,false,t);assert(!p.panel_open()&&!p.cc_drag_&&p.cc_scroll_.settled());
  }
  // Simple lists retain all tracks and have no scrollbar; metadata revisions refresh labels.
  for(int n:{0,1,2}) {
    PlayView p;p.in.dur=100;p.in.subs.assign(tracks.begin(),tracks.begin()+n);p.in.sid=n;
    p.resize(1280,720,2);p.reset(1);p.key("c",false,false,1);assert(p.cc_count_==n&&p.cc_scroll_max()==0);
    if(n) {p.in.subs[0].title="Updated metadata";++p.in.tracks_revision;p.changed(2);assert(p.rows_[1].label.find("Updated metadata")!=std::string::npos);}
  }
  // the menu opens over the CC button, as audio, speed and "..." do, not in the bar's middle
  for(auto size:{std::pair{1330,652},std::pair{2413,633},std::pair{1920,1080}}) {
    PlayView p;p.in.dur=3900;p.in.subs={tracks[0]};p.in.sid=1;p.in.kind=SubKind::Text;
    p.resize(size.first,size.second,2);p.reset(1);double t=1;p.key("c",false,false,t);settle(p,t);
    Rect d=p.dock_rect(),r=p.panel_rect();int bx=-1;for(auto& hb:p.hots_)if(hb.id==PlayView::B_CC)bx=d.x+hb.r.cx();
    assert(bx>0);
    bool over=r.x<=bx&&r.r()>=bx;             // the button stays under the menu
    bool snug=std::abs(r.cx()-bx)<=2*2||r.r()==d.r()||r.r()>=d.r()-2*2;  // centred on it, or at the bar's edge
    assert(over&&snug&&std::abs(r.cx()-d.cx())>20);
  }
  // up next too: over its own button (the sizes of the screenshots it was found in)
  for(auto size:{std::pair{1062,638},std::pair{1330,652},std::pair{1920,1080}}) {
    PlayView p;p.in.dur=3900;p.in.queue_active=true;
    p.resize(size.first,size.second,2);p.reset(1);double t=1;p.key("q",false,false,t);settle(p,t);
    Rect d=p.dock_rect(),r=p.panel_rect();int bx=-1;for(auto& hb:p.hots_)if(hb.id==PlayView::B_QUEUE)bx=d.x+hb.r.cx();
    assert(bx>0&&r.x<=bx&&r.r()>=bx);
    assert(std::abs(r.cx()-bx)<=4||r.r()>=d.r()-4||r.w>=d.w);
  }
  puts("Subtitle list: 0/1/2/500 tracks, 96 bounded layouts, pinned controls, menu over its button, long labels/metadata, selected visibility, fractional wheel, keyboard, no steady rebuild and close cleanup passed");
}
