#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>
#define private public
#include "../src/ui/play.h"
#undef private
#include <cassert>
#include <cstdio>

int main() {
  for(double fps:{60.,120.,144.})for(double fraction:{.1,.5,.9}) {
    PlayView p;p.in.sid=1;p.in.kind=SubKind::Text;p.in.style.pos=100;
    p.in.sub_lines=2;p.in.sub_text={"first line","second line"};
    p.resize(1280,720,2);double t=1,dt=1/fps;p.reset(t);
    for(int i=0;i<(int)fps;++i){p.activity(t);t+=dt;p.tick(t,dt);p.take_actions();}
    double raised=p.lifted_pos();assert(raised<99);
    p.last_activity_=-100;
    for(int i=0;i<std::max(1,(int)(fps*.18*fraction));++i){t+=dt;p.tick(t,dt);}
    double before=p.rise_.x,velocity=p.rise_.v,subtitle=p.lifted_pos();
    p.activity(t);
    assert(p.rise_.x==before&&p.rise_.v==velocity&&p.lifted_pos()==subtitle);
    p.tick(t,0);
    assert(p.rise_.x==before&&p.lifted_pos()==subtitle);
    t+=dt;p.tick(t,dt);

    assert(std::abs(p.rise_.x-before)<1.5&&std::abs(p.lifted_pos()-subtitle)<3);
    for(int i=0;i<(int)fps;++i){p.activity(t);t+=dt;p.tick(t,dt);}
    assert(std::abs(p.rise_.x)<.001&&std::abs(p.lifted_pos()-raised)<.01);
    reduce_motion()=true;p.last_activity_=-100;t+=dt;p.tick(t,dt);
    assert(p.subtitle_lift_.settled()&&p.rise_.settled());reduce_motion()=false;
  }
  puts("Dock/subtitle motion: reversals at 10/50/90 percent of the exit interval across 60/120/144 Hz, continuous trajectory and reduced motion passed");
}
