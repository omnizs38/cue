#include "../src/core/store.h"
#include "../src/core/file_io.h"
#include "../src/core/json.h"
#include <cassert>
#include <climits>
#include "../src/core/release.h"
#include <iostream>
int main() {
  assert(compare_versions("999999999999999999999999999999999.1", "1.1.1") > 0);
  ReleaseInfo release;
  assert(!parse_release(R"({"tag_name":"1.2","assets":[{"name":"Cue-Setup.exe","browser_download_url":"https://example.com/setup.exe","size":1e300}]})", release));
  JVal j;
  assert(json_parse("1e300",j)); assert(j.integer()==INT_MAX); assert(j.integer<int64_t>()==INT64_MAX);
  assert(json_parse("-1e300",j)); assert(j.integer()==INT_MIN); assert(j.integer<int64_t>()==INT64_MIN);
  assert(json_parse("9223372036854775807",j)); assert(j.integer<int64_t>()==INT64_MAX);
  assert(json_parse("12.75",j)); assert(j.integer()==12);
  std::string many="[0"; for(int i=0;i<70000;++i) many+=",0"; many+="]";
  assert(!json_parse(many,j)); // bounded tree, even for compact input
  auto dir=std::filesystem::temp_directory_path()/"cue-state-limits-test";
  std::filesystem::create_directories(dir); auto path=dir/"state.json";
  { std::ofstream f(path); f << R"({"schema":5,"prefs":{"style":{"size":1e300},"ui_scale":-1e300},"session":{"window":{"w":1e300,"h":1e300},"update":{"checked":1e300}},"media":[{"path":"clip.mp4","sid":1e300,"pos":1e300,"dur":1e300}]})"; }
  Store state; assert(state.load(path)); assert(!state.win.has);
  assert(state.default_style.size==80 && state.ui_scale==-1);
  assert(state.recents[0].sid==INT64_MAX && state.recents[0].pos==1e9);
  state.volume=73; assert(state.save(path)); assert(state.save(path));
  { std::ofstream f(path); f << std::string(kMaxStateBytes+1,' '); }
  std::string text; assert(!read_state_file(path,text) && text.empty());
  Store recovered; assert(recovered.load(path) && recovered.volume==73 && recovered.load_status=="recovered");
  // A main file that exists but cannot be read is not overwritten by the backup/defaults.
  {
    auto unreadable=dir/"unreadable.json"; std::filesystem::create_directories(unreadable);
    Store blocked; assert(!blocked.load(unreadable)); assert(blocked.load_status=="unreadable" && blocked.save_blocked);
    assert(!blocked.save(unreadable)); assert(std::filesystem::is_directory(unreadable));
    // (1.4.1) with a backup beside it, the backup's settings are shown (no welcome, no defaults),
    // and saving stays blocked
    auto bak=unreadable; bak+=".bak";
    { Store b; b.volume=41; b.onboarded=true; assert(b.save(bak)); }
    Store shown; assert(!shown.load(unreadable));
    assert(shown.load_status=="unreadable" && shown.save_blocked && shown.volume==41 && shown.onboarded);
    assert(!shown.save(unreadable)); assert(std::filesystem::is_directory(unreadable));
  }
  { // (1.4.1) the former default subtitle style (44, letter spacing 0) moves to the new one (36, +1);
    // a chosen size or letter spacing stays; 1.4.1's test builds wrote 52 with schema 6
    auto old=dir/"old.json";
    { std::ofstream f(old); f << R"({"schema":5,"prefs":{"style":{"size":44,"track":0}},"media":[{"path":"a.mkv","style":{"size":44,"track":0}},{"path":"b.mkv","style":{"size":60,"track":0}},{"path":"c.mkv"},{"path":"d.mkv","style":{"size":44,"track":3}}]})"; }
    Store st; assert(st.load(old)); assert(st.default_style.size==36 && st.default_style.tracking==1 && st.load_status=="migrated");
    assert(st.find("a.mkv")->style.size==36 && st.find("a.mkv")->style.tracking==1);
    assert(st.find("b.mkv")->style.size==60 && st.find("b.mkv")->style.tracking==0 && !st.find("c.mkv")->has_style);
    assert(st.find("d.mkv")->style.size==36 && st.find("d.mkv")->style.tracking==3);
    { std::ofstream f(old); f << R"({"schema":5,"prefs":{"style":{"size":60,"track":0}}})"; }
    Store chosen_before; assert(chosen_before.load(old) && chosen_before.default_style.size==60 && chosen_before.default_style.tracking==0);
    { std::ofstream f(old); f << R"({"schema":6,"prefs":{"style":{"size":52,"track":0}}})"; }
    Store test_build; assert(test_build.load(old) && test_build.default_style.size==36 && test_build.default_style.tracking==1);
    { std::ofstream f(old); f << R"({"schema":6,"prefs":{"style":{"size":32,"track":0}}})"; }
    Store author; assert(author.load(old) && author.default_style.size==32 && author.default_style.tracking==0);  // chosen in a test build: kept
    { std::ofstream f(old); f << R"({"schema":7,"prefs":{"style":{"size":44,"track":0}}})"; }
    Store chosen; assert(chosen.load(old) && chosen.default_style.size==44 && chosen.default_style.tracking==0);  // chosen after 1.4.1: kept
    assert(Store{}.default_style.size==36 && Store{}.default_style.tracking==1);
  }
  { // (1.4.1) a state.json held at start and readable later: this session's changes over the file
    auto held=dir/"held.json", bak=held; bak+=".bak";
    { Store b; b.volume=40; b.language="pt-BR"; b.onboarded=true; b.recents.push_back({}); b.recents[0].path="old.mkv"; b.recents[0].last=10; assert(b.save(bak)); }
    std::filesystem::create_directories(held);  // held: cannot be read
    Store s; assert(!s.load(held) && s.save_blocked && s.volume==40);
    assert(!s.unblock(held) && s.save_blocked);  // still held
    s.volume=77;                                  // changed in this session
    RecentItem played; played.path="new.mkv"; played.last=30; played.pos=12; s.recents.insert(s.recents.begin(),played);
    std::filesystem::remove(held);
    { // the real file, newer than the backup: another language, a film the backup never knew
      Store r; r.volume=40; r.language="en"; r.onboarded=true; r.save_blocked=false;
      r.recents.push_back({}); r.recents[0].path="other.mkv"; r.recents[0].last=20;
      r.recents.push_back({}); r.recents[1].path="old.mkv"; r.recents[1].last=10; r.recents[1].pos=5;
      std::string o=r.json(); std::ofstream f(held,std::ios::binary); f<<o;
    }
    assert(s.unblock(held) && !s.save_blocked && s.load_status=="unblocked");
    assert(s.volume==77 && s.language=="en");     // mine where I changed it, the file's elsewhere
    assert(s.recents.size()==3 && s.recents[0].path=="new.mkv" && s.recents[1].path=="other.mkv" && s.recents[2].path=="old.mkv" && s.recents[2].pos==5);
    assert(s.save(held));
    Store again; assert(again.load(held) && again.volume==77 && again.language=="en" && again.recents.size()==3);
  }
  std::filesystem::remove_all(dir);
  std::cout << "state bounds: extreme integers, geometry, JSON node budget, oversized-file backup recovery passed\n";
}
