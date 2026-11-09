#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>
#define private public
#include "../src/ui/play.h"
#undef private
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

int main() {
  for (double duration : {12., 1799., 1800., 7200.}) {
    PlayView p; p.in.dur=duration; p.resize(1280,720,2); p.reset(1);
    p.take_actions();
    for (int i=0;i<10;++i) {
      p.key(std::to_string(i),false,false,2);
      auto actions=p.take_actions();
      assert(actions.size()==1 && actions[0].a==PA::SeekAbsExact);
      assert(std::abs(actions[0].v-duration*i/10)<1e-9);
    }
    p.key("zero",false,false,2);
    auto actions=p.take_actions();assert(actions.size()==1 && actions[0].v==0);
    for (auto mods : {std::pair{true,false}, std::pair{false,true}, std::pair{true,true}}) {
      p.key("5",mods.first,mods.second,2); assert(p.take_actions().empty());
    }
    p.in.mini_player=true;
    for (int i=0;i<10;++i) p.key(std::to_string(i),false,false,2);
    assert(p.take_actions().empty());
    p.in.mini_player=false;p.in.audio_only=true;
    p.key("5",false,false,2);assert(p.take_actions().empty());p.in.audio_only=false;
    for (auto panel : {PlayView::Panel::Settings, PlayView::Panel::More, PlayView::Panel::Queue,
                       PlayView::Panel::Keys, PlayView::Panel::CC, PlayView::Panel::FileInfo}) {
      p.panel_=panel;p.key("5",false,false,2);assert(p.take_actions().empty());
    }
    p.panel_=PlayView::Panel::None;
    for (bool* blocked : {&p.viewport_transition_, &p.scrubbing_, &p.vol_drag_, &p.dragging_dock_,
                          &p.drag_pending_, &p.sub_drag_, &p.win_drag_}) {
      *blocked=true;p.key("5",false,false,2);assert(p.take_actions().empty());*blocked=false;
    }
    for (double invalid : {0., -1., std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
      p.in.dur=invalid;p.key("5",false,false,2);assert(p.take_actions().empty());
    }
  }
  puts("Digit seeks: 0-90%, short/long video, exact single seek, modifiers, mini/audio/menu/drag/transition guards and invalid durations passed");
}
