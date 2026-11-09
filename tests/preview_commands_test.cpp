#include "../src/core/preview_commands.h"
#include <cassert>
#include <future>
#include <iostream>
int main() {
  PreviewCommands q;
  for (size_t i=0;i<q.kMaxItems;++i) assert(q.push("x"));
  assert(!q.push("x")); // stalled consumer cannot accumulate unbounded control work
  assert(q.push("o 2 100 C:\\new.mp4", true)); // new file replaces stale backlog
  std::string s;
  assert(q.take(s) && s == "o 2 100 C:\\new.mp4");
  assert(!q.push(std::string(q.kMaxLine+1, 'a')));
  assert(!q.push("x\nq")); assert(!q.push(std::string("x\0q",3)));
  for(int i=0;i<4;++i) assert(q.push(std::string(q.kMaxLine,'a')));
  assert(!q.push("x")); // byte cap independent of count
  assert(q.push("x",true)); assert(q.take(s) && s=="x");
  auto reader = std::async(std::launch::async,[&] { std::string out; return q.take(out); });
  q.stop();
  assert(reader.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
  assert(!reader.get()); assert(!q.push("x",true));
  PreviewCommands stress;
  auto consumer = std::async(std::launch::async,[&] { std::string out; while(stress.take(out)) assert(out=="x"); });
  for (int i=0;i<100000;++i) assert(stress.push("x",true));
  stress.stop(); consumer.get();

  // A slow helper retains the load and ONLY the most recent destination.
  PreviewCommands hover;
  PreviewCommands::Ticket load, old, latest;
  assert(hover.push("o 1 100 movie.mp4", true));
  for (int i=0;i<100000;++i) assert(hover.push("s " + std::to_string(i) + " 10 100"));
  assert(hover.take(s, &load) && s == "o 1 100 movie.mp4");
  assert(!hover.superseded(load, true));  // incoming shots cannot cancel their own file load
  assert(hover.take(s, &old) && s == "s 99999 10 100");
  assert(!hover.superseded(old));
  assert(hover.push("s 100000 20 100"));
  assert(hover.superseded(old));  // an active exact seek can be interrupted
  assert(hover.take(s, &latest) && s == "s 100000 20 100");
  assert(!hover.superseded(latest));
  assert(hover.push("o 2 100 replacement.mp4", true));
  assert(hover.superseded(load, true) && hover.superseded(latest));
  assert(hover.take(s, &latest) && s == "o 2 100 replacement.mp4");
  hover.stop();
  assert(hover.superseded(latest, true));  // shutdown interrupts loading too
  std::cout << "preview command bounds, cancellation and 100000 replacements passed\n";
}
