#include <cassert>
#include <cstdio>
#include "../src/core/subtitle_selection.h"
#define private public
#include "../src/ui/play.h"
#undef private

static int row_of(const PlayView& p, int id, int64_t track = -1) {
  for (int i = 0; i < (int)p.rows_.size(); ++i)
    if (p.rows_[i].id == id && (track < 0 || p.rows_[i].track == track)) return i;
  return -1;
}

int main() {
  for (bool mini : {false, true}) {
    PlayView p;
    p.in.mini_player = mini; p.in.dur = 100; p.in.sid = 1;
    p.in.subs = {Track{1, "sub", "por", "", "subrip"}, Track{2, "sub", "eng", "", "ass"}, Track{3, "sub", "spa", "", "subrip"}};
    p.resize(mini ? 640 : 1280, mini ? 360 : 720, 2);
    p.reset(1); p.key("c", false, false, 1); p.take_actions();
    auto pick = [&](int64_t id) {
      if (mini) {
        int64_t cur = p.in.sub_selection.shown(p.in.sid);
        const int dir = id == (cur % 3 + 1) ? 1 : -1;
        p.panel_adjust(row_of(p, PlayView::R_SUB_PICK), dir, 2);
      } else p.panel_click(row_of(p, PlayView::R_SUB_TRACK, id), 0, 0, 2);
      const auto actions = p.take_actions();
      assert(actions.size() == 1 && actions[0].a == PA::SelectSub && actions[0].id == id);
      return p.in.sub_selection.begin(actions[0].id);
    };
    const auto b = pick(2);
    const auto a = pick(1);  // the old sid is still 1: the last click must still be sent
    assert(p.in.sid == 1 && p.in.sub_selection.shown(p.in.sid) == 1);
    p.changed(2);  // a clock/menu refresh must not mistake old sid=1 for confirmation
    assert(p.in.sub_selection.pending());
    assert(!p.in.sub_selection.finish(b));
    // A delayed observer event for B must not change the menu or the next arrow's starting track.
    p.in.sid = 2; p.changed(2.1);
    assert(p.in.sub_selection.shown(p.in.sid) == 1);
    if (mini) assert(p.rows_[row_of(p, PlayView::R_SUB_PICK)].track == 1);
    else assert(p.rows_[row_of(p, PlayView::R_SUB_TRACK, 1)].checked);
    const auto c = pick(3);
    assert(!p.in.sub_selection.finish(a));
    assert(p.in.sub_selection.finish(c));
    p.in.sid = 3; p.changed(2.2);
    assert(p.in.sub_selection.shown(p.in.sid) == 3);
    // A rejected request is also finished; the engine's actual track wins.
    const auto failed = pick(2);
    assert(p.in.sub_selection.finish(failed));
    p.changed(2.3);
    assert(p.in.sub_selection.shown(p.in.sid) == 3);
    // Closing and reopening the panel while the command is pending keeps the latest choice.
    const auto next = pick(1);
    p.close_panel(); p.key("c", false, false, 3); p.changed(3);
    assert(p.in.sub_selection.shown(p.in.sid) == 1);
    // Changing files invalidates the old reply, even when the new file has the same track ids.
    p.in.sub_selection.reset(); p.reset(4); p.in.sid = 2;
    assert(!p.in.sub_selection.finish(next));
    assert(p.in.sub_selection.shown(p.in.sid) == 2);
    const auto fresh = p.in.sub_selection.begin(3);
    assert(fresh != next && SubtitleSelection::is_reply(fresh));
    assert(!p.in.sub_selection.finish(next) && p.in.sub_selection.pending());
    assert(p.in.sub_selection.finish(fresh));
  }
  assert(!SubtitleSelection::is_reply(0) && !SubtitleSelection::is_reply(1000));
  puts("Subtitle selection: full/mini rapid A-B-A, delayed replies/observers, no observer change, failure, panel reopen and file replacement passed");
}
