#!/usr/bin/env python3
"""Execute production transport methods with deterministic timers and a fake engine.
This isolates control flow from Win32/rendering; it is not a native integration test.
"""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(os.environ.get('CUE_SOURCE_ROOT', Path(__file__).resolve().parent.parent))
source = (root / 'src/win/app.cpp').read_text()
remote_source = (root / 'src/win/app_remote.cpp').read_text()
def method(signature, text=source):
    start = text.index(signature)
    return text[start:text.index('\n}\n', start) + 3]

preamble = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include "seek_fade.h"
#include "preview_dispatch.h"
#include "playback_warmup.h"
#include <climits>
#include <cstdint>
using UINT_PTR = unsigned;
using UINT = unsigned;
static std::map<unsigned, unsigned> timers;
static unsigned SetTimer(int, unsigned id, unsigned delay, void*) { timers[id] = delay; return id; }
static void KillTimer(int, unsigned id) { timers.erase(id); }
static bool reduce_motion() { return false; }
static double clamp01(double x) { return std::clamp(x, 0., 1.); }
static double ease_out(double x) { return 1 - std::pow(1 - x, 3); }
static double smoothstep(double a, double b, double v) { double k=clamp01((v-a)/(b-a)); return k*k*(3-2*k); }
static double snap_px(double k, int) { return k; }
constexpr double kSwitchOut=.16, kSwitchIn=.32, kSwitchDim=.32, kSnapOut=.11, kSnapIn=.20, kSnapHold=.045;
using Commands=std::vector<std::vector<std::string>>;
static Commands audio_switch_commands(const std::string& graph, long long track) { return {{graph,std::to_string(track)}}; }
struct Engine {
  std::vector<std::vector<std::string>> sent;
  std::vector<Commands> batches;
  double position=15;
  bool cmd_batch(const Commands& c) { batches.push_back(c); return true; }
  void cmd_vec(const std::vector<std::string>& c) { sent.push_back(c); }
  void cmd(const std::vector<std::string>& c) { sent.push_back(c); }
  double get_double(const char*, double) { return position; }
  void set_flag_async(const char*, bool) {}
};
struct App {
  enum {T_SEEK_FADE=19,T_PREVIEW=8,WM_APP_THUMB=1028}; int hwnd_=1, ch_=1080;
  enum class Mode {Home,Play}; Mode mode_=Mode::Play;
  bool loaded_=true,first_frame_=true,audio_only_=false,fullscreen_=false;
  double first_frame_t_=-1,loaded_at_=0,open_started_=0,pending_ar_=0;
  struct {bool on=false;}geom_;
  bool window_settled()const{return !geom_.on;}
  void log(const std::string&){}
  void try_start_iris();
  bool in_sizemove_=false,local_ready=false,engine_log_=false;
  struct {bool loaded=true,busy=false,failed=false;uint64_t request=0,generation=1;} preview_;
  PreviewDispatch preview_dispatch_;
  double thumb_want_=-1,remote_peek_want_=-1,accent_want_=-1,thumb_interaction_at_=-1,thumb_seek_t_=0,thumb_dispatched_at_=0,thumb_request_at_=0;
  uint64_t thumb_want_epoch_=1;
  int thumb_metrics_key_=INT_MIN,thumb_seek_key_=0,remote_peek_sent_=INT_MIN;
  bool thumb_for_accent_=false;
  std::string thumb_path_="test.flac";
  struct Frame {std::vector<uint32_t> px{0};int w=1,h=1;double sample_time=0;};
  struct Cache:std::map<int,Frame>{void touch(int){}} thumb_cache_;
  struct {bool active=true;int count=1,peeks=0;bool running()const{return active;}int clients()const{return count;}void set_peek(double,const std::vector<uint32_t>&,int,int){++peeks;}}remote_;
  struct {bool up=false;int opens=0;std::vector<double> shots;bool running(){return up;}bool start(int,int){up=true;return true;}void open(const std::string&,int,uint64_t){++opens;}void shot(uint64_t,double t,int,bool){shots.push_back(t);}}thumbs_;
  bool background_ready(double)const{return local_ready;}
  bool atmosphere_ready()const{return local_ready;}
  int thumb_key(double t)const{return (int)t;}
  double thumb_step()const{return 1;}
  bool thumb_show(int,uint64_t){return false;}
  void trace(const std::string&){}
  void thumb_request(double,bool local=true);
  void thumb_next();
  bool remote_preview_pending()const;
  void remote_peek(double);
  void remote_peek_send();
  bool mpv_ok_=true, hidden=true, eof_=false, music_reopening_=false;
  bool home_closing_=false, close_stopped_=false;
  int paints=0, stops=0, cancellations=0;
  double time=1, switch_gain_=1;
  std::string path_="test.flac";
  SeekFade seek_fade_;
  Engine mpv_;
  struct { struct { bool paused=false, sub_visible=true; int sid=1; double dur=100; } in;
    bool waiting=true;int reveals=0;
    bool iris_waiting()const{return waiting;}
    void start_iris(double){waiting=false;++reveals;}
    bool preview_hovering()const{return true;}bool preview_active()const{return true;}
    uint64_t preview_epoch()const{return 1;}int thumb_decode_width()const{return 80;}int thumb_hover_width()const{return 40;}
  } play_;
  struct { int stage=0,retries=0; double at=0,position=0; bool paused=false,dirty=false,opening_mix=false,reopen=false; std::string graph="audio"; long long track=1; } audio_switch_;
  double now() const { return time; }
  bool hidden_now() const { return hidden; }
  void set_switch_gain(double g) { switch_gain_=g; }
  void set_snap(double,double,double) { ++paints; }
  void end_snap() {}
  void snap_subtitles(bool) {}
  void release_video_window() {}
  void ensure_anim() { ++paints; }
  void stop_to_home() { ++stops; close_stopped_=true; }
  void cancel_audio_switch() { ++cancellations; audio_switch_.stage=0; switch_gain_=1; }
  void seek_soft(std::vector<std::string>);
  bool step_seek_fade(double);
  void cancel_seek_fade();
  bool step_audio_switch(double);
  void step_hidden_playback(double);
};
'''
checks = r'''
int main() {
  {
    App a;timers.clear();a.seek_soft({"seek","15","relative"});
    assert(a.seek_fade_.stage==1 && timers.at(App::T_SEEK_FADE)==8 && a.mpv_.sent.empty() && a.paints==0);
    a.time=1.01;a.step_seek_fade(a.time);
    assert(a.mpv_.sent.size()==1 && a.seek_fade_.stage==2 && a.switch_gain_==0 && timers.at(App::T_SEEK_FADE)==50);
    a.time=1.02;a.seek_fade_.stage=3;a.seek_fade_.t0=a.time;a.step_seek_fade(a.time);
    a.time=1.06;a.step_seek_fade(a.time);
    assert(a.switch_gain_==1 && a.seek_fade_.stage==0 && timers.empty() && a.paints==0);
  }
  {
    App a;timers.clear();a.seek_soft({"seek","10"});a.time=1.004;a.seek_soft({"seek","30"});
    a.time=1.02;a.step_seek_fade(a.time);assert(a.mpv_.sent.size()==1 && a.mpv_.sent[0][1]=="30");
    a.time=3;a.step_seek_fade(a.time);a.time=3.04;a.step_seek_fade(a.time);
    assert(a.switch_gain_==1 && timers.empty());
  }
  {
    App a;timers.clear();a.seek_soft({"seek","10"});a.cancel_seek_fade();a.step_seek_fade(5);
    assert(a.mpv_.sent.empty() && timers.empty() && a.switch_gain_==1 && a.seek_fade_.cmd.empty());
  }
  for (const bool reopen : {false,true}) {
    App a;a.audio_switch_.stage=1;a.audio_switch_.at=a.time;a.audio_switch_.reopen=reopen;
    a.step_hidden_playback(a.time);
    assert(a.audio_switch_.stage==3 && a.paints==0);
    if(reopen)assert(a.mpv_.sent.size()==1 && a.music_reopening_);
    else assert(a.mpv_.batches.size()==1);
    a.step_hidden_playback(a.time+9);
    assert(a.audio_switch_.stage==0 && a.cancellations==1 && a.switch_gain_==1 && a.paints==0);
  }
  {
    App a;a.audio_switch_.stage=5;a.audio_switch_.at=a.time;a.switch_gain_=0;
    a.step_hidden_playback(a.time);assert(a.audio_switch_.stage==0 && a.switch_gain_==1 && a.paints==0);
    a.home_closing_=true;a.step_hidden_playback(a.time);a.step_hidden_playback(a.time);assert(a.stops==1);
  }
  {
    App a;a.hidden=false;a.audio_switch_.stage=1;a.step_hidden_playback(20);
    assert(a.audio_switch_.stage==1 && a.mpv_.batches.empty());
  }
  {
    App a;a.preview_.loaded=false;a.thumb_path_.clear();a.remote_peek(23);
    assert(a.thumbs_.up && a.thumbs_.opens==1 && a.preview_.busy && a.thumbs_.shots.empty());
    a.preview_.busy=false;a.preview_.loaded=true;a.thumb_path_=a.path_;a.thumb_want_=9;a.accent_want_=70;
    a.thumb_next();assert(a.thumbs_.shots.size()==1 && a.thumbs_.shots[0]==23 && a.thumb_want_<0);
    for(int i=30;i<40;++i){a.remote_peek_want_=i;a.thumb_next();}
    assert(a.thumbs_.shots.size()==1); // one active decode, latest waiting destination
    a.preview_.busy=false;a.time+=.1;a.thumb_next();assert(a.thumbs_.shots.size()==2 && a.thumbs_.shots.back()==39);
  }
  {
    App a;a.remote_.count=0;a.remote_peek_want_=20;a.thumb_next();assert(a.thumbs_.shots.empty());
    a.remote_.count=1;a.thumb_cache_[20].sample_time=20;a.remote_peek(20);a.remote_peek(20);
    assert(a.remote_.peeks==2 && a.remote_peek_want_<0 && a.thumbs_.shots.empty());
    a.audio_only_=true;a.remote_peek_want_=30;assert(!a.remote_preview_pending());
  }
  {
    App a;a.time=100;a.try_start_iris();assert(a.play_.reveals==0 && a.first_frame_t_<0);
    a.hidden=false;a.pending_ar_=9./16.;a.try_start_iris();assert(a.play_.reveals==0);
    a.pending_ar_=0;a.geom_.on=true;a.try_start_iris();assert(a.play_.reveals==0);
    a.geom_.on=false;a.try_start_iris();assert(a.play_.reveals==1);
  }
  std::cout<<"Hidden reveal: no offscreen iris, pending ratio and active restore geometry remain covered, reveal after settling passed\n";
  std::cout<<"Hidden previews: explicit phone warmup, latest-only decode, no local/color work, no phone/no decode and repeated cached gestures passed\n";
  std::cout<<"Production transport: hidden seek dispatch/gain recovery, latest target, cancellation, audio graph/reopen/watchdog and close completion without render frames passed\n";
}
'''
methods = '\n'.join(method(x) for x in [
    'void App::seek_soft(', 'bool App::step_seek_fade(', 'void App::cancel_seek_fade(',
    'bool App::step_audio_switch(', 'void App::step_hidden_playback(',
    'void App::thumb_request(', 'void App::thumb_next(', 'void App::try_start_iris('])
methods += '\n' + '\n'.join(method(x, remote_source) for x in [
    'bool App::remote_preview_pending(', 'void App::remote_peek(', 'void App::remote_peek_send('])
with tempfile.TemporaryDirectory(prefix='cue-hidden-playback-') as temp:
    cpp=Path(temp)/'test.cpp';exe=Path(temp)/'test'
    cpp.write_text(preamble+methods+checks)
    subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-I',str(root/'src/core'),str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
