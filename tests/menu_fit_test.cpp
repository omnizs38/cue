// Every menu at every interface size, on the windows people use (100% to 200%, a small window
// at 150%): whole in the window, clear of the window's buttons and the back arrow, clear of the
// control bar (music's own list beside the cover aside). The subtitles' style reached under the
// window's buttons and over the bar at "grande" and "maior"; the subtitle list and up next opened
// over the bar in a small window; the picture's menu could open over the back arrow.
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
#include "../src/core/ui_scale.h"
#define private public
#include "../src/ui/play.h"
static bool ov(const Rect& a, const Rect& b){return a.w>0&&b.w>0&&a.x<b.r()&&b.x<a.r()&&a.y<b.b()&&b.y<a.b();}
int main(){
  struct Win{int w,h; unsigned dpi;};
  Win wins[]={{1280,720,96},{1366,768,96},{1920,1080,144},{2560,1440,144},{3840,2160,192},{1600,900,120},{800,450,144},{1200,675,144},{480*3/2*2,270*3/2*2,144},{2000,1100,144},{1280,720,144},{960,540,96}};
  const char* names[]={"CC","Style","Audio","?","Speed","About","FileInfo","Language","Context","AudioMix","Queue","Settings","More"};
  int bad=0,total=0; std::map<std::string,int> by;
  for(bool music:{false,true}) for(auto W:wins) for(int choice:{-1,2,0,3}) for(int which:{0,1,2,4,5,6,7,8,10,11,3}) {
    if(music&&(which==0||which==1)) continue;
    double s=ui_scale(W.dpi,W.w,W.h,choice);
    PlayView p; p.in.dur=3000; p.in.time=100; p.in.audio_only=music; p.in.cover_scene=music; p.in.has_cover=music; p.in.cover_art=music;
    if(music){p.in.music_title="Dream Sequence";p.in.music_artist="Jane Remover";p.in.music_scene=MusicScene::wide();}
    else { Track a{1,"sub","por","","subrip"}; p.in.subs={a,Track{2,"sub","eng","English (SDH)","subrip"}}; p.in.sid=1; p.in.kind=SubKind::Text; p.in.sub_text={"porque é tudo só encenação."}; p.in.sub_lines=1;}
    p.in.audios={Track{1,"audio","por","",""},Track{2,"audio","eng","Commentary",""}};p.in.aid=1;
    p.in.queue_active=true; for(uint32_t i=1;i<=6;++i)p.in.queue.push_back({i,"item "+std::to_string(i),i==1,music});
    p.in.smooth_available=true;
    p.set_base_scale(s); p.resize(W.w,W.h,s); double t=1; p.reset(t);
    if(music) p.in.music_view=music_view(p.in.music_scene,music_frame_for(W.w,W.h,s),W.w,W.h,W.h*8/10);
    p.changed(t);
    for(int i=0;i<30;++i){p.activity(t);t+=1/60.;p.tick(t,1/60.);}
    p.open_panel_for_preview(which==3?12:which,t);
    for(int i=0;i<40;++i){p.activity(t);t+=1/60.;p.tick(t,1/60.);p.take_actions();}
    if(p.panel_==PlayView::Panel::None) continue;
    const Layer& L=p.layers[PlayView::L_PANEL]; if(!L.visible) continue;
    Rect r{L.x,L.y,L.c.w,L.c.h}; Rect d=p.dock_rect(); Rect ch=p.chrome_rect(); Rect bk=p.back_rect();
    ++total;
    std::string what;
    if(r.x<0||r.y<0||r.r()>W.w||r.b()>W.h) what+=" out";
    if(ov(r,ch)) what+=" chrome";
    if(ov(r,bk)) what+=" back";
    bool sl = p.panel_==PlayView::Panel::Queue && p.scene_list();
    if(!sl && ov(r,d)) what+=" dock";
    if(!what.empty()){++bad; by[std::string(names[which])+what]++;
      if(bad<=40) std::printf("%s %s %dx%d@%u c%d s=%.1f ps=%.2f panel=%d,%d %dx%d dock=%d,%d %dx%d:%s\n",music?"music":"film",names[which],W.w,W.h,W.dpi,choice,s,p.panel_draw_s(),r.x,r.y,r.w,r.h,d.x,d.y,d.w,d.h,what.c_str());}
  }
  // the mini players (a film's at 480 to 800 px at 100%, a song's card at 440 to 640) at every
  // size and DPI: their menus whole in the small window, clear of its buttons, at the bar's size;
  // the smallest of each at the medium size keeps the medium size's pixels
  for(unsigned dpi:{96u,120u,144u,192u}) for(int choice:{-1,2,0,3}) for(bool music:{false,true}) for(int lng:{440,480,640,800}) {
    if(music&&lng>640) continue;
    const double k=dpi/96.0; const int W=(int)(lng*k), H=(int)(lng*k/(music?1.9:16/9.0));
    const double s=mini_scale(dpi,W,H,choice,music);  // (as the app: one scale for the mini player)
    if(choice==kDefaultUiSize&&lng==(music?440:480)) assert(s==ui_base_scale(dpi,choice));
    assert(s<=ui_base_scale(dpi,choice));
    for(int which:{0,12}) {
      PlayView p; p.in.mini_player=true; p.in.dur=3000; p.in.audio_only=music; p.in.cover_scene=music; p.in.cover_art=music;
      if(music){p.in.music_title="Dream Sequence";p.in.music_artist="Jane Remover";p.in.music_scene=MusicScene::wide();}
      else { Track a{1,"sub","por","","subrip"}; p.in.subs={a,Track{2,"sub","eng","English (SDH)","subrip"}}; p.in.sid=1; p.in.kind=SubKind::Text; }
      if(music&&which==0) continue;
      p.set_base_scale(s); p.resize(W,H,s); double t=1; p.reset(t); p.changed(t);
      for(int i=0;i<30;++i){p.activity(t);t+=1/60.;p.tick(t,1/60.);}
      p.open_panel_for_preview(which,t);
      for(int i=0;i<40;++i){p.activity(t);t+=1/60.;p.tick(t,1/60.);p.take_actions();}
      const Layer& L=p.layers[PlayView::L_PANEL]; if(!L.visible) continue;
      ++total;
      const Rect r{L.x,L.y,L.c.w,L.c.h};
      std::string what;
      if(r.x<0||r.y<0||r.r()>W||r.b()>H) what+=" out";
      if(ov(r,p.chrome_rect())) what+=" chrome";
      if(p.panel_draw_s()!=s) what+=" scale";  // (its menus at the bar's own size: they fit as they are)
      if(!what.empty()){++bad;by[std::string(music?"card ":"mini ")+(which?"more":"subs")+what]++;
        if(bad<=20) std::printf("mini %s %dx%d@%u c%d s=%.1f panel %d,%d %dx%d:%s\n",music?"song":"film",W,H,dpi,choice,s,r.x,r.y,r.w,r.h,what.c_str());}
    }
  }
  for(auto&[k,v]:by) std::printf("  %s: %d\n",k.c_str(),v);
  assert(bad==0&&total>600);
  // a menu closed and faded keeps no pixels (about 10 MB at 4K in two copies); it opens again whole
  {
    PlayView p;p.in.dur=6000;p.resize(3840,2160,4.5);double t=1;p.reset(t);p.changed(t);
    for(int which:{11,1,12}){p.open_panel_for_preview(which,t);for(int i=0;i<40;++i){p.activity(t);t+=1/60.;p.tick(t,1/60.);p.take_actions();}p.close_panel();}
    for(int i=0;i<200;++i){t+=1/60.;p.tick(t,1/60.);p.take_actions();}
    assert(p.layers[PlayView::L_PANEL].c.px.capacity()==0&&p.panel_base_.px.capacity()==0);
    p.open_panel_for_preview(11,t);for(int i=0;i<40;++i){p.activity(t);t+=1/60.;p.tick(t,1/60.);p.take_actions();}
    const Layer& L=p.layers[PlayView::L_PANEL];
    assert(L.visible&&L.c.w>0&&L.c.px.size()==(size_t)L.c.w*L.c.h);
    size_t lit=0;for(uint32_t v:L.c.px)lit+=(v>>24)!=0;
    assert(lit>L.c.px.size()/2);
  }
  std::printf("Menu fit: %d menus at four sizes on twelve windows, whole, clear of the window's buttons, the back arrow and the bar, a closed menu keeps no pixels passed\n",total);
}
