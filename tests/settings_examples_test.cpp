// Settings' examples: on by default (each setting shown at work); the "i" in its header turns
// them off and on and says so to the app (saved); in a short window the picture stays on top
// while the rows can close up, then stands beside them where the window is wide, its column the
// same at half-step scales (a resize used to leave the rows over the picture); a change of the
// interface's size decides it all afresh (at large sizes the examples used to stay away).
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include "../src/core/store.h"
#include "../src/core/i18n.h"
#define private public
#include "../src/ui/play.h"

static void settle(PlayView& p, double& t) { p.activity(t); for (int i = 0; i < 30; ++i) { t += 1. / 60; p.tick(t, 1. / 60); } }
static int row_of(const PlayView& p, int id) {
  for (int i = 0; i < (int)p.rows_.size(); ++i) if (p.rows_[i].id == id) return i;
  return -1;
}

int main() {
  int sides = 0;
  for (bool music : {false, true})
    for (double scale : {1.5, 2.0, 2.5}) {
      PlayView p;
      p.in.dur = 300; p.in.audio_only = music; p.in.cover_scene = music; p.in.smooth_available = true;
      const int W = (int)(1920 * scale / 2 * 1.5), H = (int)(1080 * scale / 2 * 1.5);
      p.set_base_scale(scale); p.resize(W, H, scale);
      double t = 1; p.reset(t); settle(p, t);
      p.open_panel_for_preview(11, t); settle(p, t);
      assert(p.panel_ == PlayView::Panel::Settings && p.in.settings_examples);
      assert(row_of(p, PlayView::R_PREVIEW) >= 0);  // the picture on top
      const int head = row_of(p, PlayView::R_BACK_MORE);
      assert(head == 0 && !p.rows_[head].parts.empty());
      // its button: the examples off, then on again, told to the app
      p.take_actions();
      p.panel_click(head, 0, 0, t);
      bool told = false;
      for (auto& a : p.take_actions()) told |= a.a == PA::SettingsExamples && a.v == 0;
      assert(told && !p.in.settings_examples && row_of(p, PlayView::R_PREVIEW) < 0);
      p.panel_click(row_of(p, PlayView::R_BACK_MORE), 0, 0, t);
      told = false;
      for (auto& a : p.take_actions()) told |= a.a == PA::SettingsExamples && a.v == 1;
      assert(told && p.in.settings_examples && p.panel_ == PlayView::Panel::Settings);
      assert(row_of(p, PlayView::R_PREVIEW) >= 0);
      // the header itself still goes back
      p.panel_click(row_of(p, PlayView::R_BACK_MORE), -1, 0, t);
      assert(p.panel_ == PlayView::Panel::More);
      // a short, wide window: the picture on top as long as the rows can close up, then beside
      // them; after a resize too, the rows start where the picture's column ends, whatever the scale
      for (int h : {H * 6 / 10, H * 7 / 10, H * 6 / 10, H / 2, H * 45 / 100, H * 4 / 10, H * 35 / 100}) {
        p.resize(W, h, scale);
        p.open_panel_for_preview(11, t); settle(p, t);
        const double s = p.panel_s();
        if (!p.settings_compact_) assert(row_of(p, PlayView::R_PREVIEW) >= 0);
        if (!p.settings_side_) continue;
        ++sides;
        const int col = (int)std::lround(196 * s);
        for (auto& r : p.rows_)
          if (r.k != PlayView::RK::Header) assert(r.r.x >= col && r.r.x <= col + (int)std::lround(3 * s));
        assert(row_of(p, PlayView::R_PREVIEW) < 0 && p.pw_ <= W);
      }
      // off again
      p.open_panel_for_preview(11, t); settle(p, t);
      p.panel_click(row_of(p, PlayView::R_BACK_MORE), 0, 0, t);
      assert(!p.in.settings_examples && row_of(p, PlayView::R_PREVIEW) < 0);
    }
  assert(sides > 0);
  // the interface's size changed with Settings open (a large one): decided afresh, the picture on
  // top again where it fits (it used to stay away until Settings was opened again)
  for (bool music : {false, true})
    for (auto [W, H] : {std::pair{2560, 1440}, std::pair{1920, 1080}, std::pair{3840, 2160}}) {
      PlayView p;
      p.in.dur = 300; p.in.audio_only = music; p.in.cover_scene = music; p.in.smooth_available = true;
      p.set_base_scale(2); p.resize(W, H, 2);
      double t = 1; p.reset(t); settle(p, t);
      p.open_panel_for_preview(11, t); settle(p, t);
      for (double big : {3.0, 3.5, 4.5, 2.0, 4.5}) {
        p.resize(W, H, big); settle(p, t);
        const bool shown = row_of(p, PlayView::R_PREVIEW) >= 0 || p.settings_side_;
        // the examples go only where neither on top nor beside could fit
        if (!shown) assert(p.settings_stage_ >= 6);
        if (big <= 3.0 && H >= 1440) assert(shown);
        const auto& L = p.layers[PlayView::L_PANEL];
        assert(L.x >= 0 && L.x + L.c.w <= W && L.y >= 0 && L.y + L.c.h <= H);
      }
    }
  // the interface's page (Settings folded it there at a large size): a smaller size picked in it
  // leaves room for all of Settings, which comes back with the sizes in place (it stayed on the
  // page); still short of room, the page stays
  int folds = 0;
  for (bool music : {false, true}) {
    PlayView p;
    p.in.dur = 300; p.in.audio_only = music; p.in.cover_scene = music; p.in.smooth_available = true;
    const int W = 1920, H = 1000;
    p.set_base_scale(5); p.resize(W, H, 5);
    double t = 1; p.reset(t); settle(p, t);
    p.open_panel_for_preview(11, t); settle(p, t);
    assert(p.settings_fold_);
    ++folds;
    const int sub = row_of(p, PlayView::R_INTERFACE);
    assert(sub >= 0);
    p.panel_click(sub, -1, 0, t); settle(p, t);
    assert(p.panel_ == PlayView::Panel::Interface);
    p.resize(W, H, 4.5); settle(p, t);   // still large: the page stays (or Settings, if it fits)
    if (p.panel_ == PlayView::Panel::Settings) assert(!p.settings_fold_ && row_of(p, PlayView::R_UISCALE) >= 0);
    p.resize(W, H, 2); settle(p, t);     // medium: all of Settings, the sizes in it

    assert(p.panel_ == PlayView::Panel::Settings && !p.settings_fold_ && row_of(p, PlayView::R_UISCALE) >= 0);
  }
  assert(folds == 2);
  // The portrait recording: toggling the real header button must not change the
  // panel's horizontal placement or emit a native window-drag action.
  i18n::set_language(i18n::Lang::PtBR);
  for (double scale : {2., 2.5, 3.}) {
    PlayView p; p.in.dur=300; p.in.controls_free=true;
    p.resize(828,1374,scale); double t=1; p.reset(t); settle(p,t);
    p.open_panel_for_preview(11,t); settle(p,t);
    const Rect initial=p.panel_rect();
    auto click_info = [&] {
      const Rect panel=p.panel_rect(), button=p.rows_[0].parts[0];
      p.take_actions();
      p.mouse_move(panel.x+button.cx(),panel.y+button.cy(),t);
      p.mouse_down(panel.x+button.cx(),panel.y+button.cy(),t);
      p.mouse_up(panel.x+button.cx(),panel.y+button.cy(),t+.001);
      const auto actions=p.take_actions();
      assert(actions.size()==1 && actions[0].a==PA::SettingsExamples);
      assert(p.panel_rect().x==initial.x && p.panel_rect().w==initial.w);
      assert(p.in.w==828 && p.in.h==1374);
    };
    click_info(); assert(!p.in.settings_examples && !p.hints_entering_);
    click_info(); assert(p.in.settings_examples && p.hints_entering_);
    auto chroma = [&] {
      Canvas c; c.resize(400,188); c.clear({20,20,20});
      p.render_preview(c,{0,0,400,188},2);
      uint64_t value=0;
      for (uint32_t px:c.px) {
        int r=(px>>16)&255,g=(px>>8)&255,b=px&255;
        value+=std::max({r,g,b})-std::min({r,g,b});
      }
      return value;
    };
    const auto first=chroma();
    for (int i=0;i<6;++i) {t+=1/120.;p.tick(t,1/120.);}
    assert(p.hints_entering_);
    const auto middle=chroma();
    for (int i=0;i<13;++i) {t+=1/120.;p.tick(t,1/120.);}
    assert(!p.hints_entering_ && chroma()>middle && middle>first);
    // A quick reversal cancels the finite reveal; reduced motion presents it at once.
    click_info(); click_info(); assert(p.hints_entering_);
    click_info(); assert(!p.hints_entering_);
    reduce_motion()=true;
    click_info(); assert(p.in.settings_examples && !p.hints_entering_);
    reduce_motion()=false;
    p.close_panel(); assert(!p.hints_entering_);
  }
  // saved: on by default, and the choice kept (under its own name, so an older "off" from when
  // the examples were off by default does not carry over)
  {
    auto file = std::filesystem::temp_directory_path() / "cue-settings-examples.json";
    std::filesystem::remove(file);
    Store a; assert(a.settings_examples);
    a.settings_examples = false; a.save(file.string());
    Store b; b.load(file.string()); assert(!b.settings_examples);
    b.settings_examples = true; b.save(file.string());
    Store c; c.load(file.string()); assert(c.settings_examples);
    std::filesystem::remove(file); std::filesystem::remove(file.string() + ".bak");
  }
  // The horizontal layout's glow/labels never paint into the settings column,
  // including mid-reveal and at maximum ambient strength.
  for (bool tall : {false,true}) for (int demo=PlayView::D_AMBIENT;demo<=PlayView::D_ABOUT;++demo)
    for (double phase : {0.,.05,.14}) {
      PlayView p;p.resize(1280,720,2);p.demo_=demo;p.in.ambient_strength=1;
      p.hints_t0_=1;p.demo_t0_=.8;p.last_now_=1+phase;
      Canvas c;c.resize(700,450);c.clear({47,47,47});
      Rect box{20,20,tall?210:400,tall?400:188};
      p.render_preview(c,box,2,tall);
      for(int y=0;y<c.h;++y)for(int x=0;x<c.w;++x)
        if(!box.contains(x,y))assert(c.px[(size_t)y*c.w+x]==0xFF2F2F2Fu);
      p.close_panel();assert(p.hints_canvas_.px.empty());
    }
  std::puts("Settings examples: on by default, the header's button, saved, on top or beside the rows at every scale, a new interface size decided afresh, the interface's page back to Settings when it fits passed");
}
