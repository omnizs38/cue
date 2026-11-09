// The mini player's card: its list reads at the card's own type size (one line per song), the
// names it shows ("Artist - Title", the uploads' labels left out), and the menu and list keep the
// card's scale. Portable: the PlayView and QueueView the app draws, without the engine.
#include <algorithm>
#include <cmath>
#include <deque>
#include <string>
#include <unordered_set>
#include <vector>
#define private public
#include "../src/ui/queue_view.h"
#include "../src/ui/play.h"
#undef private
#include "../src/core/i18n.h"
#include "../src/core/ui_scale.h"
#include <cassert>
#include <cstdio>

static void run(PlayView& p, double& now, double secs) {
  for (double t = 0; t < secs; t += 1 / 60.0) { now += 1 / 60.0; p.tick(now, 1 / 60.0); p.take_actions(); }
}

int main() {
  i18n::set_language(i18n::Lang::PtBR);
  // names
  std::string t, a;
  QueueView::split_artist_title("Jane Remover - Dream Sequence (audio)", t, a);
  assert(t == "Dream Sequence (audio)" && a == "Jane Remover");
  QueueView::split_artist_title("Ice Palace Adolescence", t, a);
  assert(t == "Ice Palace Adolescence" && a.empty());
  QueueView::split_artist_title("Sigur R\xc3\xb3s \xe2\x80\x93 Hopp\xc3\xadpolla", t, a);  // an en dash
  assert(t == "Hopp\xc3\xadpolla" && a == "Sigur R\xc3\xb3s");
  QueueView::split_artist_title(" - lone", t, a);  // nothing before the dash: the whole name
  assert(t == " - lone" && a.empty());
  QueueView::split_artist_title("AC-DC - T.N.T. - Live", t, a);  // the first spaced dash, the title keeps its own
  assert(a == "AC-DC" && t == "T.N.T. - Live");
  assert(QueueView::plain_title("Dream Sequence (audio)") == "Dream Sequence");
  assert(QueueView::plain_title("Beauty Sleep (320 Kbps)") == "Beauty Sleep");
  assert(QueueView::plain_title("Song [Official Video] (Lyrics)") == "Song");
  assert(QueueView::plain_title("Song (Live at Wembley)") == "Song (Live at Wembley)");  // a real part of the name
  assert(QueueView::plain_title("(audio)") == "(audio)");                              // only a label: kept
  assert(QueueView::plain_title("Remix (feat. X)") == "Remix (feat. X)");

  // compact rows: one line, about half the full list's row; the grip zone is narrow
  {
    std::vector<QueueEntry> e;
    for (uint32_t i = 1; i <= 10; ++i) e.push_back({i, "Artist - Song " + std::to_string(i) + ".mp3", i == 3, true});
    QueueView q; q.set_bare(true); q.sync(e, 1); q.layout(360, 200, 2.5); q.opened();
    const int full = q.row_h();
    q.set_compact(true);
    assert(q.row_h() == (int)std::lround(QueueView::kRowCompact * 2.5) && q.row_h() * 1.8 < full);
    for (int i = 0; i < 10; ++i) assert(std::fabs(q.items_[i].y.x - i * q.row_h()) < 1e-6);  // rows re-stacked at once
    assert(QueueView::bare_height(10, 12, true) < QueueView::bare_height(10, 12, false));
    // a press on the cover plays (the old 22 px grip would have swallowed it), on the grip only drags
    for (int n = 0; n < 200; ++n) q.tick(1. / 120);
    int i = q.current_index();
    int y = q.viewport_.y + (int)std::lround(q.items_[i].y.x - q.scroll_.x) + q.row_h() / 2;
    q.down((int)(12 * 2.5), y);
    auto ev = q.up((int)(12 * 2.5), y);
    assert(ev.action == QueueView::Action::Play && ev.id == 3);
    q.down((int)(4 * 2.5), y);
    ev = q.up((int)(4 * 2.5), y);
    assert(ev.action == QueueView::Action::None);
    Canvas c; q.draw(c, false);  // draws (covers, the add row) without touching outside the list
    assert(c.w == 360);
  }

  // the card at its sizes on 100%, 150% and 200% screens: the list keeps the card's scale (the
  // artist line's size), and so does the menu, also after a move (the scale the window gets then)
  int cards = 0;
  for (unsigned dpi : {96u, 144u, 192u})
    for (int lng : {440, 480, 560, 640}) {
      const int cw = (int)std::lround(lng * dpi / 96.0), ch = (int)std::lround(lng * dpi / 96.0 / 1.9);
      const double base = ui_base_scale(dpi, kDefaultUiSize);
      const double s = std::max(ui_scale(dpi, cw, ch, kDefaultUiSize), base);  // App::scale_for in the mini player
      assert(s >= base);
      PlayView p;
      p.in.audio_only = p.in.cover_scene = p.in.cover_art = p.in.has_cover = p.in.mini_player = true;
      p.in.dur = 250; p.in.time = 20; p.in.music_title = "Dream Sequence"; p.in.music_artist = "Jane Remover";
      for (uint32_t i = 1; i <= 10; ++i) p.in.queue.push_back({i, "Jane Remover - Song " + std::to_string(i) + ".mp3", i == 2, true});
      p.in.queue_active = true; p.in.queue_revision = 1;
      p.set_base_scale(s);
      p.resize(cw, ch, std::max(s, music_compact_scale(s, cw, ch, kDefaultUiSize, base)));
      double now = 1;
      p.reset(now); p.changed(now);
      assert(p.card_);
      run(p, now, 0.5);  // (the words are up: the list opens in their place)
      p.open_panel_for_preview(10, now);
      run(p, now, 0.6);
      assert(p.scene_list() && p.queue_view_.compact());
      assert(p.scene_list_scale() == p.s_);  // the card's own size, not a size or two down
      Rect r = p.scene_list_rect();
      assert(r.x >= p.card_cover().r() && r.r() <= cw);  // beside the cover, inside the window
      p.key("esc", false, false, now);
      run(p, now, 0.5);
      p.open_panel_for_preview(3, now);
      run(p, now, 0.6);
      assert(p.panel_s() == p.s_);
      ++cards;
    }
  assert(cards == 12);
  // a song changing: its words step out, the next ones come in
  {
    PlayView p;
    p.in.audio_only = p.in.cover_scene = p.in.cover_art = p.in.mini_player = true;
    p.in.dur = 250; p.in.music_title = "One"; p.in.music_artist = "A";
    p.set_base_scale(2.5); p.resize(1200, 632, 2.5);
    double now = 1;
    p.reset(now); p.changed(now); run(p, now, 1);
    assert(p.music_vis_.v > 0.99);
    p.in.music_leaving = true; p.changed(now); run(p, now, 0.4);
    assert(p.music_vis_.v < 0.01);
    p.in.music_leaving = false; p.in.music_title = "Two"; p.changed(now); run(p, now, 1);
    assert(p.music_vis_.v > 0.99);
  }
  // Shuffle in the compact menu must keep a visible state even with a pointer on it.
  for (double scale : {1.,1.5,2.}) {
    PlayView p;p.in.dur=180;p.in.audio_only=true;p.in.mini_player=true;p.in.cover_scene=true;
    p.resize(480*scale,270*scale,scale);double now=1;p.reset(now);run(p,now,.6);
    p.open_panel_for_preview(3,now);run(p,now,.5);
    auto where=std::find(p.tool_ids_.begin(),p.tool_ids_.end(),PlayView::B_SHUFFLE);
    assert(where!=p.tool_ids_.end());int part=where-p.tool_ids_.begin();
    auto row=std::find_if(p.rows_.begin(),p.rows_.end(),[](const auto& r){return r.id==PlayView::R_TOOLS;});
    assert(row!=p.rows_.end());const int row_index=row-p.rows_.begin();Rect cell=row->parts[part];
    auto click=[&] {
      Rect panel=p.panel_rect();p.take_actions();
      p.mouse_move(panel.x+cell.cx(),panel.y+cell.cy(),now);
      p.mouse_down(panel.x+cell.cx(),panel.y+cell.cy(),now);
      p.mouse_up(panel.x+cell.cx(),panel.y+cell.cy(),now);
      auto actions=p.take_actions();assert(actions.size()==1&&actions[0].a==PA::Shuffle);
      assert((actions[0].v>.5)==p.in.shuffle);run(p,now,.2);
    };
    click();assert(p.in.shuffle && p.panel_==PlayView::Panel::More);
    const auto& on=p.layers[PlayView::L_PANEL].c;
    const int mark_x=cell.cx(),mark_y=std::lround(cell.b()-2*scale);
    uint32_t mark=on.px[(size_t)mark_y*on.w+mark_x];
    assert((mark>>24)>240 && ((mark>>16)&255)>200);
    assert(p.rows_[row_index].segs[part]==tr(str::tip_shuffle_on));
    click();assert(!p.in.shuffle);
    const auto& off=p.layers[PlayView::L_PANEL].c;
    assert(off.px[(size_t)mark_y*off.w+mark_x]!=mark);
  }
  std::puts("Mini card: compact rows, labels, cover/grip, menu scale, title motion and persistent shuffle state passed");
}
