#include "../src/core/seek_dispatch.h"
#include <cassert>
#include <cstdio>
#include <vector>
int main() {
  SeekDispatch dispatch;
  std::vector<double> sent;
  bool accept = false;
  auto send = [&](double t) { sent.push_back(t); return accept; };
  dispatch.request(1, send);  // rejected; no reply will ever arrive
  accept = true;
  dispatch.request(2, send);
  assert((sent == std::vector<double>{1,2}));
  for (int i=3;i<=1000;++i) dispatch.request(i,send);
  assert(sent.size()==2);
  dispatch.complete(send);
  assert(sent.back()==1000 && sent.size()==3);
  dispatch.request(1001,send);
  accept=false;
  dispatch.complete(send);  // rejection of a coalesced destination also frees it
  accept=true;
  dispatch.request(1002,send);
  assert(sent.back()==1002);
  dispatch.request(1003,send);
  dispatch.clear_pending();  // another file or an exact release supersedes it
  auto count=sent.size();
  dispatch.complete(send);
  assert(sent.size()==count);
  dispatch.request(7,send);
  assert(sent.back()==7 && sent.size()==count+1);
  puts("seek dispatch: rejected enqueue recovery, 1000-target sweep and stale destination clearing passed");
}
