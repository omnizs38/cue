// Shuffle (music): every song of the list once a round, never the one playing next; the way back
// is the songs played, newest first; songs added go into the round, songs removed leave it;
// repeat-all deals a new round, without it the round ends.
#include <cassert>
#include <cstdio>
#include <set>
#include "../src/core/queue.h"

int main() {
  for (uint32_t seed : {1u, 7u, 42u, 1234u}) {
    MediaQueue q;
    q.append({"a.mp3", "b.mp3", "c.mp3", "d.mp3", "e.mp3", "f.mp3"});
    q.select(q.items()[0].id);
    ShuffleDeck d(seed);
    // a round: every other song once, then nothing (repeat off)
    std::set<uint32_t> seen{q.current()};
    std::vector<uint32_t> order{q.current()};
    for (int i = 0; i < 5; ++i) {
      d.sync(q, false);
      const auto* n = d.next(q);
      assert(n && n->id != q.current() && !seen.count(n->id));
      seen.insert(n->id);
      q.select(n->id);
      order.push_back(n->id);
    }
    d.sync(q, false);
    assert(seen.size() == 6 && !d.next(q));
    // back: the songs played, newest first
    for (int i = 4; i >= 0; --i) {
      d.sync(q, false);
      const auto* p = d.previous(q);
      assert(p && p->id == order[i]);
      q.select(p->id);
    }
    d.sync(q, false);
    assert(!d.previous(q));
    // repeat-all: a new round once this one is over
    MediaQueue r;
    r.append({"1.mp3", "2.mp3", "3.mp3"});
    r.select(r.items()[0].id);
    ShuffleDeck e(seed);
    for (int i = 0; i < 9; ++i) {
      e.sync(r, true);
      const auto* n = e.next(r);
      assert(n && n->id != r.current());
      r.select(n->id);
    }
    // songs added go into the round; a song removed leaves it
    MediaQueue m;
    m.append({"x.mp3", "y.mp3", "z.mp3"});
    m.select(m.items()[0].id);
    ShuffleDeck f(seed);
    f.sync(m, false);
    m.append({"w.mp3"});
    f.sync(m, false);
    assert(f.deck().size() == 3);
    const uint32_t gone = f.deck().front();
    m.remove(gone);
    f.sync(m, false);
    assert(f.deck().size() == 2 && f.next(m) && f.next(m)->id != gone);
  }
  std::puts("Shuffle: each song once a round, the way back, repeat-all's new round, songs added and removed passed");
}
