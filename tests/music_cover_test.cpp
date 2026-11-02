#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include "../src/core/audio_mode.h"
#include "../src/core/store.h"
#define private public
#include "../src/ui/play.h"
#include "../src/ui/queue_view.h"
#undef private
#include <filesystem>
static void settle(PlayView& p,double& t,int n=40){while(n--){t+=1./60;p.tick(t,1./60);}}
int main(){
  // the engine's cover track: the album art, never a film
  Track a; a.id=1; a.type="audio";
  Track art; art.id=2; art.type="video"; art.albumart=true;
  Track film=art; film.albumart=false;
  assert(audio_cover_track({a})==0 && audio_cover_track({a,art})==2 && audio_cover_track({a,film})==0);
  // the cover look feeds on [vidN] and still leaves the audible branch alone
  std::string g=audio_waveform_graph(1,2);
  assert(g.find("[aid1]asplit=2[ao][cue_wave];")==0 && g.find("[vid2]")!=std::string::npos && g.find("[vo]")!=std::string::npos);
  assert(audio_waveform_graph(1,0).find("[vid")==std::string::npos);  // no cover: the spectrum alone
  assert(audio_mix_graph({1,3},true,2).find("[vid2]")!=std::string::npos && audio_mix_graph({1,3},false,2).find("[vid")==std::string::npos);
  // the spectrum runs at the display's pace, and ends without leaving solid wings behind
  assert(g.find("r=60")!=std::string::npos && g.find("alphamerge")==std::string::npos && g.find("colorkey")!=std::string::npos);
  // one picture, framed by the window: all of it, tall (words under the cover), or the cover alone
  {
    const MusicScene S=MusicScene::wide();
    assert(S.left_x()>=0 && S.right_x()+S.wing_w<=S.W && S.wing_y()>=0 && S.wing_y()+S.wing_h<=S.H);
    assert(S.text_x>=S.right_x()+S.wing_w && S.text_x+S.text_w<=S.W);
    assert(audio_waveform_graph(1,2,S).find("scale=1920:1080")!=std::string::npos);
    assert(music_frame_for(1280,720,1)==MusicFrame::Scene && music_frame_for(2560,1440,2)==MusicFrame::Scene);
    assert(music_frame_for(720,1000,1)==MusicFrame::Tall && music_frame_for(600,450,1)==MusicFrame::Cover);
    // the engine's zoom and pan put the cover exactly where the view says
    for(MusicFrame m:{MusicFrame::Scene,MusicFrame::Tall,MusicFrame::Cover}){
      int w=m==MusicFrame::Tall?720:m==MusicFrame::Cover?600:1280,h=m==MusicFrame::Tall?1000:m==MusicFrame::Cover?450:720;
      MusicView v=music_view(S,m,w,h,h-90);MpvFraming f=mpv_framing(S,v,w,h);
      double f0=std::min(w/(double)S.W,h/(double)S.H),fz=f0*std::pow(2.0,f.zoom);
      assert(std::abs(fz-v.f)<1e-9);
      assert(std::abs((w-S.W*fz)/2+f.pan_x*S.W*fz-v.ox)<1e-6&&std::abs((h-S.H*fz)/2+f.pan_y*S.H*fz-v.oy)<1e-6);
      double cx=v.ox+(S.x+S.side/2.0)*v.f,cy=v.oy+(S.y+S.side/2.0)*v.f,side=S.side*v.f;
      if(m==MusicFrame::Scene) assert(std::abs(f.zoom)<1e-9&&std::abs(f.pan_x)<1e-9&&std::abs(f.pan_y)<1e-9);
      else assert(std::abs(cx-w/2.0)<1e-6&&cy+side/2<=h-90&&cy-side/2>=0);  // centred, above the bar
    }
  }
  // the choice is saved for all music
  auto file=std::filesystem::temp_directory_path()/"cue-music-look.json";
  Store st; st.music_look=1; assert(st.save(file)); Store back; assert(back.load(file)&&back.music_look==1);
  std::filesystem::remove(file); std::filesystem::remove(file.string()+".bak");
  for(double scale:{1.,2.,2.5})for(bool tall:{false,true}){
    PlayView p;p.in.dur=200;p.in.time=10;p.in.audio_only=true;p.in.has_cover=true;p.in.cover_scene=true;p.in.cover_art=true;p.in.music_title="Song";p.in.music_artist="Artist";
    int W=(int)((tall?720:1280)*scale),H=(int)((tall?1000:720)*scale);p.in.music_scene=MusicScene::wide();
    p.in.music_view=music_view(p.in.music_scene,music_frame_for(W,H,scale),W,H,H-(int)(80*scale));
    p.resize(W,H,scale);double t=1;p.reset(t);p.changed(t);settle(p,t);
    Rect c=p.cover_screen_rect();
    assert(c.w>0&&std::abs(c.w-c.h)<=1&&c.x>0&&c.r()<W);
    // under the pointer the cover leans away from it, and settles flat when the pointer leaves
    p.mouse_move(c.r()-4,c.cy(),t);settle(p,t);
    auto k=p.cover_tilt();assert(k.ry<-3&&std::abs(k.rx)<1&&k.k>0.9);
    p.mouse_move(c.cx(),c.y+4,t);settle(p,t);k=p.cover_tilt();assert(k.rx<-3);
    p.pointer_left();settle(p,t,90);k=p.cover_tilt();assert(std::abs(k.rx)<0.1&&std::abs(k.ry)<0.1&&k.k<0.01);
    // wide: the words to the right of the cover, level with it; tall: under it
    p.compose(t);auto& M=p.layers[PlayView::L_MUSIC];assert(M.visible&&M.x>=0&&M.x+M.c.w<=W);
    if(tall) assert(M.y>=c.b());
    else assert(M.x>=c.r()&&M.y>=c.y&&M.y+M.c.h<=c.b());
    // the right-click menu offers both looks, with icons; choosing one asks for it
    p.right_click(c.cx(),c.cy(),t);settle(p,t);assert(p.panel_open());
    int row=-1;for(int i=0;i<(int)p.rows_.size();++i)if(p.rows_[i].id==PlayView::R_MUSIC_LOOK)row=i;
    assert(row>=0&&p.rows_[row].segs.size()==2&&p.rows_[row].seg_icons.size()==2&&p.rows_[row].seg==0);
    p.take_actions();p.panel_click(row,1,0,t);
    bool asked=false;for(auto& a2:p.take_actions())asked|=a2.a==PA::MusicLook&&a2.v==1;assert(asked);
    p.close_panel();
    // no scene (the engine could not draw it): no choice to make
    p.in.has_cover=false;p.in.cover_scene=false;p.changed(t);settle(p,t);
    p.right_click(c.cx(),c.cy(),t);settle(p,t);
    for(auto& r:p.rows_)assert(r.id!=PlayView::R_MUSIC_LOOK);
    p.close_panel();settle(p,t,30);p.compose(t);assert(!p.layers[PlayView::L_MUSIC].visible);
  }
  {
    // a small window keeps only the cover: no words, the cover centred above the bar
    PlayView p;p.in.dur=200;p.in.time=10;p.in.audio_only=true;p.in.has_cover=true;p.in.cover_scene=true;p.in.music_title="Song";
    p.in.music_scene=MusicScene::wide();p.in.music_view=music_view(p.in.music_scene,MusicFrame::Cover,600,450,370);
    p.resize(600,450,1);double t=1;p.reset(t);p.changed(t);settle(p,t,60);p.compose(t);
    Rect c=p.cover_screen_rect();assert(!p.layers[PlayView::L_MUSIC].visible&&std::abs(c.cx()-300)<=1&&c.b()<=370&&c.w>200);
  }
  {
    // music's dock: previous and next track around play, no 15-second steps, no subtitles button
    // unless the song has lyrics
    PlayView p;p.in.dur=200;p.in.time=10;p.in.audio_only=true;p.in.has_next=true;p.in.has_prev=true;
    p.resize(1280,720,1);double t=1;p.reset(t);p.changed(t);settle(p,t);
    auto has=[&](int id){for(auto& h:p.hots_)if(h.id==id)return true;return false;};
    assert(has(PlayView::B_PREV)&&has(PlayView::B_NEXT)&&has(PlayView::B_PLAY)&&!has(PlayView::B_REW)&&!has(PlayView::B_FWD)&&!has(PlayView::B_CC));
    Track sub;sub.id=1;sub.type="sub";p.in.subs={sub};p.changed(t);settle(p,t);assert(has(PlayView::B_CC));
    // "up next" appears at the top only in the song's last seconds, and only when something follows
    p.in.next_title="02 Next song";settle(p,t);p.compose(t);assert(!p.layers[PlayView::L_NEXT].visible);
    p.in.time=192;p.changed(t);settle(p,t,30);p.compose(t);auto& N=p.layers[PlayView::L_NEXT];
    assert(N.visible&&N.y<720/4&&std::abs(N.x+N.c.w/2-640)<=2);
    p.in.next_title.clear();p.changed(t);settle(p,t,60);p.compose(t);assert(!p.layers[PlayView::L_NEXT].visible);
    // a film keeps its 15-second steps
    PlayView v;v.in.dur=200;v.resize(1280,720,1);v.reset(t);v.changed(t);settle(v,t);
    bool rew=false;for(auto& h:v.hots_)rew|=h.id==PlayView::B_REW;assert(rew);
  }
  {
    // an album's list: square covers are asked for music too, and a list of only music is an album
    QueueView q;q.sync({{1,"01 Intro.mp3",true,true},{2,"02 Song.flac",false,true}},1);q.opened();q.layout(320,240,1);
    assert(q.album()&&q.thumbnail_request()!=0);
    q.sync({{1,"01 Intro.mp3",true,true},{3,"Film.mkv",false,false}},2);assert(!q.album());
    std::vector<uint32_t> px(84*48,0xff202020u);q.thumbnail(1,px);Canvas c;c.resize(320,240);q.draw(c,false);
  }
  {
    // repeat: off → the list → this one → off with a list; on and off without one
    PlayView p;p.in.dur=200;p.in.audio_only=true;p.resize(1280,720,1);double t=1;p.reset(t);p.changed(t);settle(p,t);
    auto loop_act=[&]{double v=-1;for(auto& a:p.take_actions())if(a.a==PA::Loop)v=a.v;return v;};
    p.take_actions();p.toggle_loop(t);assert(loop_act()==2&&p.in.loop&&!p.in.loop_all);
    p.toggle_loop(t);assert(loop_act()==0&&!p.in.loop);
    p.in.queue_active=true;p.in.queue={{1,"a.mp3",true,true},{2,"b.mp3",false,true}};p.in.queue_revision=7;p.changed(t);settle(p,t);
    p.toggle_loop(t);assert(loop_act()==1&&p.in.loop_all&&!p.in.loop);
    p.toggle_loop(t);assert(loop_act()==2&&p.in.loop&&!p.in.loop_all);
    p.toggle_loop(t);assert(loop_act()==0&&!p.in.loop&&!p.in.loop_all);
  }
  {
    // music's bar is narrower than a film's, and eases between the two
    PlayView v;v.in.dur=200;v.resize(1920,1080,2);double t=1;v.reset(t);v.changed(t);settle(v,t);
    PlayView m;m.in.dur=200;m.in.audio_only=true;m.resize(1920,1080,2);m.reset(t);m.changed(t);settle(m,t);
    assert(m.dw_<v.dw_);
    int film=v.dw_,music=m.dw_;
    v.in.audio_only=true;v.changed(t);t+=1./60;v.tick(t,1./60);t+=1./60;v.tick(t,1./60);t+=1./60;v.tick(t,1./60);
    assert(v.dw_<film&&v.dw_>music);  // on its way
    settle(v,t,60);assert(v.dw_==music);
  }
  {
    // the album's list opens beside the cover from the line under the artist, and closes from it
    PlayView p;p.in.dur=200;p.in.time=10;p.in.audio_only=true;p.in.has_cover=true;p.in.cover_scene=true;
    p.in.music_title="Song";p.in.music_artist="Artist";p.in.music_scene=MusicScene::wide();
    p.in.queue_active=true;p.in.queue={{1,"01 a.mp3",false,true},{2,"02 b.mp3",true,true},{3,"03 c.mp3",false,true}};p.in.queue_revision=3;
    p.resize(1920,1080,2);double t=1;p.reset(t);p.changed(t);settle(p,t);p.compose(t);
    Rect c=p.cover_screen_rect();
    assert(p.music_link_.w>0&&p.music_link_.x>=c.r());
    int lx=p.music_link_.cx(),ly=p.music_link_.cy();
    p.mouse_move(lx,ly,t);assert(p.cursor_kind()==1);
    p.mouse_down(lx,ly,t);p.mouse_up(lx,ly,t);settle(p,t,60);
    assert(p.queue_open()&&p.scene_list());
    Rect r=p.panel_rect();assert(r.x>=c.r()&&r.y>c.y&&r.b()<=c.b()+2&&r.r()<=1920);
    p.compose(t);auto& M=p.layers[PlayView::L_MUSIC];assert(std::abs(M.y-c.y)<=2);  // the words rose to the cover's top
    p.compose(t);lx=p.music_link_.cx();ly=p.music_link_.cy();
    p.mouse_down(lx,ly,t);p.mouse_up(lx,ly,t);settle(p,t,60);assert(!p.queue_open());
    // the spectrum alone (no captions): the list is the usual popover
    p.in.cover_scene=false;p.changed(t);settle(p,t,40);p.open_panel(PlayView::Panel::Queue,PlayView::B_QUEUE,t);assert(!p.scene_list());
  }
  {
    // a song without a cover wears Cue's tile in the cover look; the spectrum look stands in the
    // cover's place, so the words and the list sit beside it the same way
    std::string ph=audio_waveform_graph(1,0,MusicScene::wide(),0),sp=audio_waveform_graph(1,0,MusicScene::wide(),1);
    assert(ph.find("[vid")==std::string::npos&&ph.find("drawbox")!=std::string::npos&&ph.find("scale=1920:1080")!=std::string::npos);
    assert(sp.find("pad=1920:1080:282:297")!=std::string::npos&&sp.find("[vid")==std::string::npos);
    assert(audio_waveform_graph(1,2,MusicScene::wide(),1).find("[vid")==std::string::npos);  // the spectrum never waits on a cover
    // the spectrum look: no tilt, but the words beside it
    PlayView p;p.in.dur=200;p.in.audio_only=true;p.in.cover_scene=true;p.in.cover_art=false;p.in.music_title="Song";
    p.resize(1280,720,1);double t=1;p.reset(t);p.changed(t);settle(p,t);
    Rect c=p.cover_screen_rect();p.mouse_move(c.r()-4,c.cy(),t);settle(p,t);assert(std::abs(p.cover_tilt().ry)<0.01);
    p.compose(t);assert(p.layers[PlayView::L_MUSIC].visible&&p.layers[PlayView::L_MUSIC].x>=c.r());
    p.right_click(c.cx(),c.cy(),t);settle(p,t);bool look=false;for(auto& r:p.rows_)look|=r.id==PlayView::R_MUSIC_LOOK;assert(look);
  }
  {
    // the list beside the cover ends with a line that adds songs; dragging files in opens the
    // list and that line invites the drop; leaving without dropping closes it again
    PlayView p;p.in.dur=200;p.in.audio_only=true;p.in.cover_scene=true;p.in.music_title="Song";p.in.music_scene=MusicScene::wide();
    p.in.queue_active=true;p.in.queue={{1,"01 a.mp3",true,true},{2,"02 b.mp3",false,true}};p.in.queue_revision=4;
    p.resize(1920,1080,2);double t=1;p.reset(t);p.changed(t);settle(p,t);p.compose(t);
    p.drag_files(true,t);settle(p,t,40);assert(p.queue_open()&&p.scene_list()&&p.queue_view_.dropping());
    p.drag_files(false,t);settle(p,t,40);assert(!p.queue_open());
    p.drag_files(true,t);settle(p,t,40);p.drag_dropped();p.drag_files(false,t);assert(!p.queue_view_.dropping());
    p.drag_files(true,t);p.drag_dropped();settle(p,t,40);assert(p.queue_open());  // dropped: stays to show what came in
    Rect r=p.panel_rect();Rect a=p.queue_view_.add_row();
    assert(a.w>0&&a.y>=0);
    int ax=r.x+a.cx(),ay=r.y+a.cy();
    p.take_actions();p.mouse_move(ax,ay,t);p.mouse_down(ax,ay,t);p.mouse_up(ax,ay,t);
    bool add=false;for(auto& x:p.take_actions())add|=x.a==PA::QueueAdd;assert(add);
    // films keep their own drop: nothing opens
    PlayView v;v.in.dur=200;v.resize(1280,720,1);v.reset(t);v.changed(t);settle(v,t);v.drag_files(true,t);assert(!v.queue_open());
  }
  puts("Music cover: cover track, graph, saved look, tilt toward the pointer and back, wide and tall captions, right-click choice, music dock, up-next pill, album list, repeat modes, narrow bar, the list beside the cover, Cue tile, spectrum scene and drop-to-add passed");
}
