#include <algorithm>
#include <array>
#include <deque>
#include <string>
#include <unordered_set>
#include <vector>
#define private public
#include "../src/ui/queue_view.h"
#include "../src/ui/play.h"
#undef private
#include <cassert>
#include <cstdio>
int main() {
  assert(QueueView::count_suffix(0).empty()&&QueueView::count_suffix(1).empty());
  assert(QueueView::count_suffix(2)==" · 2");
  std::vector<QueueEntry> entries;
  for(uint32_t id=1;id<=512;++id)entries.push_back({id,"episode.mkv",id==1});
  for(double hz:{60.,120.,144.}) {
    QueueView q;q.sync(entries,1);q.layout(720,704,2);q.opened();Canvas c;q.draw(c,false);
    q.sync({},2);q.layout(720,336,2);
    assert(q.items_.empty()&&q.ghosts_.size()>0&&q.ghosts_.size()<=12&&q.height()==704);
    assert(q.up(400,160).action==QueueView::Action::None);
    // Logical addition is usable during a clear; no completion callback may clear it later.
    q.sync({{900,"new.mkv",false}},3);assert(q.items_.size()==1&&q.items_[0].enter==0);
    for(int n=0;n<(int)hz;++n){q.tick(1/hz);q.draw(c,false);}
    assert(q.items_[0].entry.id==900&&q.ghosts_.empty()&&q.height()==336&&q.items_[0].enter==1);
    assert(!q.tick(1/hz));
    q.sync({},4);q.closed();assert(q.ghosts_.empty()&&!q.tick(1/hz));
  }
  QueueView q;q.sync(entries,1);q.layout(720,704,2);q.opened();q.key("end",false,false);for(int n=0;n<120;++n)q.tick(1./120);
  q.sync({},2);assert(q.ghosts_.size()<=12);reduce_motion()=true;q.tick(.01);
  assert(q.ghosts_.empty());reduce_motion()=false;
  q.closed();q.sync(entries,3);assert(q.count_age_==1&&q.old_count_.empty());
  PlayView panel;panel.in.dur=100;panel.resize(1280,720,2);panel.reset(1);panel.key("q",false,false,1);
  for(int i=0;i<30;++i)panel.tick(1+i/60.,1./60);
  panel.key("esc",false,false,2);panel.tick(2.03,.03);double alpha=panel.pvis_.v;
  panel.key("q",false,false,2.03);assert(panel.pvis_.v==alpha&&panel.pvis_.target==1);
  puts("Queue refinement: zero/single counts hidden, bounded visible clear snapshots, immediate edits, finite motion at 60/120/144 Hz and closure/reduced-motion cleanup passed");
}
