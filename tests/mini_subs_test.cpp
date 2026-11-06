// The mini player is a quick look: no menu on the picture, no up next in a film's, no notice
// of what comes next. The film's mini player: its subtitles panel is short (on, which track by its arrows, how big,
// and a line that sends the rest to the full player), never the full player's long list; the
// size is one of three presets, told to the app and saved; the full player keeps its own panel.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include "../src/core/store.h"
#define private public
#include "../src/ui/play.h"

static void settle(PlayView& p, double& t) { p.activity(t); for (int i = 0; i < 30; ++i) { t += 1. / 60; p.tick(t, 1. / 60); } }
static int row_of(const PlayView& p, int id) {
  for (int i = 0; i < (int)p.rows_.size(); ++i) if (p.rows_[i].id == id) return i;
  return -1;
}

int main() {
  for (double scale : {1.5, 2.0, 2.5}) {
    for (bool mini : {true, false}) {
      PlayView p;
      p.in.mini_player = mini; p.in.dur = 3883; p.in.time = 1472;
      p.in.subs = {Track{1, "sub", "por", "", "subrip"}, Track{2, "sub", "eng", "English (SDH)", "subrip"}, Track{3, "sub", "spa", "", "ass"}};
      p.in.sid = 1; p.in.kind = SubKind::Text;
      const int W = mini ? (int)std::lround(360 * scale) : (int)std::lround(960 * scale);
      const int H = mini ? W * 9 / 16 : (int)std::lround(540 * scale);
      p.in.w = W; p.in.h = H;
      p.set_base_scale(scale); p.resize(W, H, scale);
      double t = 1; p.reset(t); p.changed(t); settle(p, t);
      p.open_panel_for_preview(0, t); settle(p, t);
      if (!mini) {  // the full player: the full panel, its list of tracks
        assert(p.panel_ == PlayView::Panel::CC && row_of(p, PlayView::R_SUB_PICK) < 0);
        continue;
      }
      assert(p.panel_ == PlayView::Panel::SubMini);
      assert(row_of(p, PlayView::R_SUB_TRACK) < 0 && row_of(p, PlayView::R_STYLE) < 0);
      const int pick = row_of(p, PlayView::R_SUB_PICK), size = row_of(p, PlayView::R_SUB_SIZE);
      assert(pick >= 0 && size > pick && row_of(p, PlayView::R_SUB_TOGGLE) == 0);
      // it fits the small window
      assert(p.ph_ <= H && p.pw_ <= W);
      // the track's arrows: the same distance from each end of its track, the size control as wide
      const auto& pr = p.rows_[pick];
      assert(pr.parts.size() == 2);
      assert(std::abs((pr.parts[0].x - pr.r.x) - (pr.r.r() - pr.parts[1].r())) <= 1);
      const auto& sr = p.rows_[size];
      assert(sr.parts.size() == 3 && sr.parts.front().x == pr.parts[0].x && sr.parts.back().r() == pr.parts[1].r());
      // the next track, round the list both ways
      p.take_actions();
      p.panel_click(pick, 1, 0, t);
      // the app is asked for the next track while its own sid is still the old one (it compares
      // the two: a pick that had already changed in.sid was taken for the current track and lost)
      bool sel = false;
      for (auto& a : p.take_actions()) sel |= a.a == PA::SelectSub && a.id == 2 && a.id != p.in.sid;
      assert(sel && p.in.sid == 1);
      assert(p.rows_[row_of(p, PlayView::R_SUB_PICK)].label.find("English") != std::string::npos);  // its name at once
      p.in.sid = 2; p.in.sub_selection.reset(); p.changed(t);  // the engine confirms
      // twice back before the engine answers: round the list from the track just picked
      p.panel_click(row_of(p, PlayView::R_SUB_PICK), 0, 0, t);
      p.panel_click(row_of(p, PlayView::R_SUB_PICK), 0, 0, t);
      int64_t last = -1;
      for (auto& a : p.take_actions()) if (a.a == PA::SelectSub) last = a.id;
      assert(last == 3 && p.in.sid == 2);
      p.in.sid = 3; p.in.sub_selection.reset(); p.changed(t);
      assert(p.in.sub_selection.shown(p.in.sid) == 3);
      // three sizes: told to the app, the panel stays open
      p.panel_click(row_of(p, PlayView::R_SUB_SIZE), 2, 0, t);
      bool told = false;
      for (auto& a : p.take_actions()) told |= a.a == PA::MiniSubSize && a.v == 2;
      assert(told && p.in.mini_sub_size == 2 && p.panel_ == PlayView::Panel::SubMini);
      assert(p.rows_[row_of(p, PlayView::R_SUB_SIZE)].seg == 2);
      // a quick look, nothing to manage: no menu on the picture, no up next, no notice of it
      p.close_panel(); settle(p, t);
      p.right_click(W / 2, H / 3, t); settle(p, t);
      assert(p.panel_ == PlayView::Panel::None);
      p.open_panel_for_preview(10, t); settle(p, t);
      assert(p.panel_ != PlayView::Panel::Queue);
      p.open_panel_for_preview(3, t); settle(p, t);
      assert(p.panel_ == PlayView::Panel::More);
      for (int id : p.tool_ids_) assert(id != PlayView::B_QUEUE);
      p.next_notice("a seguir 5s", t);
      assert(!p.next_toast_);
      p.close_panel(); settle(p, t);
      p.open_panel_for_preview(0, t); settle(p, t);
      // leaving the mini player closes it
      p.in.mini_player = false; p.resize(W * 3, H * 3, scale); p.changed(t); settle(p, t);
      assert(p.panel_ != PlayView::Panel::SubMini);
    }
  }
  // a song's card: no "up next" pill over it either
  {
    PlayView p;
    p.in.mini_player = true; p.in.audio_only = true; p.in.cover_scene = true; p.in.dur = 200; p.in.time = 195;
    p.in.next_title = "Music Baby";
    p.set_base_scale(1.5); p.resize(640, 337, 1.5);
    assert(!p.next_wanted());
    p.in.mini_player = false;
    assert(p.next_wanted());
  }
  // saved: as the full player's by default, the choice kept
  {
    auto file = std::filesystem::temp_directory_path() / "cue-mini-subs.json";
    std::filesystem::remove(file);
    Store a; assert(a.mini_sub_size == 0);
    a.mini_sub_size = 2; a.save(file.string());
    Store b; b.load(file.string()); assert(b.mini_sub_size == 2);
    std::filesystem::remove(file); std::filesystem::remove(file.string() + ".bak");
  }
  std::puts("Mini player subtitles: a short panel, arrows round the tracks, three sizes saved, the full player's panel untouched passed");
}
