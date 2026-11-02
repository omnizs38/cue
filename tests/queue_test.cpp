#include "../src/core/queue.h"
#include "../src/core/remote_proto.h"
#include "../src/ui/play.h"
#include "../src/core/i18n.h"
#include <cassert>
#include <cstdio>

int main() {
  MediaQueue q;
  assert(q.append({"one.mkv","two.mp3","three.mkv"})==3);
  auto one=q.items()[0].id,two=q.items()[1].id,three=q.items()[2].id;
  assert(q.select(one)&&q.next()->id==two);
  assert(q.move(three,-1)&&q.next()->id==three);
  // A delayed remove still means the same file, after its position changes.
  assert(q.remove(two)&&!q.get(two)&&q.next()->id==three);
  assert(q.remove(one)&&q.current()==0&&q.next()->id==three);
  assert(q.select(three)&&!q.next());
  assert(q.remove(three)&&q.active()&&!q.next()); // explicit queue ends, no unrelated fallback
  q.clear();assert(!q.active()&&!q.select(one));
  q.append({"same.mkv","same.mkv","song.flac"});
  auto a=q.items()[0].id,b=q.items()[1].id;
  assert(a!=b&&q.select(a)&&q.next()->id==b);
  assert(q.select(b)&&q.select_path("same.mkv")&&q.current()==b);
  std::vector<std::string> many(2000,"a.mp4");
  q.clear();assert(q.append(many)==MediaQueue::kLimit);
  for(int i=0;i<10000;++i) {
    auto id=q.items().back().id;
    q.move(id,-1);q.remove(id);q.append({"new.ogg"});
    assert(q.items().size()==MediaQueue::kLimit);
  }
  assert(q.append({""})==0);
  remote::Command c;
  for(auto cmd:{"queue-play","queue-remove","queue-up","queue-down"}) {
    assert(remote::parse_command(std::string("{\"c\":\"")+cmd+"\",\"v\":12}",c));
    for(auto bad:{"-1","0","1.2","1e100"})assert(!remote::parse_command(std::string("{\"c\":\"")+cmd+"\",\"v\":"+bad+"}",c));
  }
  assert(remote::parse_command(R"({"c":"queue-add","path":"C:\\video.mp4"})",c));
  assert(!remote::parse_command(R"({"c":"queue-add","path":"C:\\program.exe"})",c));
  // Q opens a transient panel, Esc dismisses it without pausing/closing media.
  for(int l=0;l<i18n::kLangCount;++l)for(auto size:{std::pair{540,960},std::pair{960,540},std::pair{1920,1080}}) {
    i18n::set_language((i18n::Lang)l);
    PlayView p;p.in.dur=100;p.in.queue_active=true;
    for(int i=0;i<12;++i)p.in.queue.push_back({(uint32_t)i+1,"A long episode name S01E"+std::to_string(i)+".mkv",i==0});
    p.resize(size.first,size.second,2);p.reset(1);p.key("q",false,false,1);
    for(int i=0;i<40;++i)p.tick(1+i/60.,1./60);
    const auto& panel=p.layers[PlayView::L_PANEL];
    assert(p.panel_open()&&panel.visible&&panel.x>=0&&panel.y>=0);
    assert(panel.x+panel.c.w<=size.first&&panel.y+panel.c.h<=size.second);
    p.key("esc",false,false,2); // queue closes, with no transport action
    assert(!p.panel_open()&&p.take_actions().empty());
  }
  // A folder that contains the playing file does not list it twice when the queue starts.
  assert(MediaQueue::same_path("C:\\Videos\\Show S01E02.mkv","c:/videos/show s01e02.MKV"));
  assert(!MediaQueue::same_path("C:\\Videos\\a.mkv","C:\\Videos\\b.mkv"));
  {
    auto batch=MediaQueue::without_first({"C:\\V\\a.mkv","C:\\V\\b.mkv","c:/v/B.mkv","C:\\V\\c.mkv"},"C:\\V\\B.MKV");
    assert((batch==std::vector<std::string>{"C:\\V\\a.mkv","c:/v/B.mkv","C:\\V\\c.mkv"}));
    MediaQueue seeded;seeded.append({"C:\\V\\b.mkv"});seeded.select_path("C:\\V\\b.mkv");seeded.append(batch);
    assert(seeded.items().size()==4&&seeded.items()[0].path=="C:\\V\\b.mkv"&&seeded.next()->path=="C:\\V\\a.mkv");
    assert(MediaQueue::without_first({"x.mkv"},"y.mkv").size()==1);
  }
  {
    MediaQueue q;q.append({"a","b","c"});assert(!q.previous());
    q.select(q.items()[0].id);assert(!q.previous()&&q.next()->path=="b");
    q.select(q.items()[2].id);assert(q.previous()->path=="b");
    q.remove(q.items()[2].id);assert(q.previous()->path=="b");  // the removed playing entry's predecessor
  }
  puts("Queue: stable IDs, audio/video order, duplicate paths, playing-file seed, current removal, end, cap, repeated mutation, strict remote commands and transient UI passed");
}
