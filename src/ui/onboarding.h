// First run: four short screens before the home, on one grid (the picture, the heading, its
// line, the control and the footer stand at the same height on every screen).
//   0  "oi, sou o Cue"         the eyes, lit from both sides, standing on a line with their reflection
//   1  dynamic lighting     a little scene; with the switch on, the window itself glows in its
//                           colours (the real halo, App::step_welcome_light; drawn around the scene
//                           instead when the window is maximized and the halo has nowhere to go)
//   2  controls background  the same scene with the player's controls over it, with their
//                           background or without (1.4.1)
//   3  interface size       the same scene with the player's controls over it, at each size, in
//                           the background chosen
// Drawn by the same pixel renderer as the home, into the same canvas.
#pragma once
#include <string>
#include <vector>

#include "anim.h"
#include "chrome.h"
#include "face.h"
#include "gfx.h"

enum class ObAct { None, Ambient, ControlsBg, UiScale, Finish, WinMin, WinMax, WinClose, WinPin };
struct ObAction { ObAct a = ObAct::None; int v = 0; };

struct OnboardingView {
  static constexpr int kPages = 4;
  static constexpr int kPageLight = 1, kPageBg = 2, kPageSize = 3;
  static constexpr int kZones = 26;  // FrameColors::kZones: the halo's stretches, clockwise from the top-left
  int w = 0, h = 0;
  double s = 2;        // the welcome's own pixel scale (fixed by the window, not by the size chosen)
  double ui_s = 2;     // the interface scale in use (the window's buttons)
  // the interface scale each of the four sizes gets in this window (pequena .. maior); 0: derived
  // from ui_s and the choice (previews and tests). The App fills it before set_size.
  double size_s[4] = {0, 0, 0, 0};
  bool ambient = true;  // what the toggle shows (the App owns the preference)
  int ui_scale = 2;     // persisted id: -1 pequena, 2 média (default), 0 grande, 1 maior
  bool controls_free = false;  // the bar without its background (the App owns the preference)
  bool light_inside = false;  // the window is maximized: the scene's light is drawn around it, inside
  Face face;
  WindowChrome chrome;

  void set_size(int w_, int h_, double s_);
  void start(double now);
  bool tick(double now, double dt);
  void render(Canvas& c, double now);
  double next_event(double now) const;

  ObAction mouse_move(int x, int y, double now);
  ObAction mouse_down(int x, int y, double now);
  void mouse_leave(double now);
  ObAction key(const std::string& k, bool shift, double now);
  bool track(int x, int y, double now);
  bool is_background(int x, int y) const;

  // the end. From the first screen the eyes fly to where the home keeps them; from the others the
  // welcome goes dark and the home opens through the iris (App, HomeView::enter_iris)
  void finish(double now, double face_x, double face_y, double face_scale);
  bool finishing() const { return finish_t0_ > 0; }
  bool finished(double now) const { return finish_t0_ > 0 && now - finish_t0_ >= (face_handoff_ ? 0.5 : kDarkDur); }
  int page() const { return page_; }
  bool page_settled() const { return pos_.settled(); }  // not sliding between screens
  Rect next_rect() const { return L_[page_].next; }      // the way on (for previews and tests)
  Rect back_rect() const { return L_[page_].back; }
  bool face_handoff() const { return face_handoff_; }  // the home keeps the eyes where they landed
  void go(int p, double now);
  void set_ambient(bool on, double now);     // the App confirms (or a preview sets it)
  void set_scale_choice(int id, double now);
  void set_controls_free(bool free, double now);

  // The real halo around the window (second screen): how much of it (0 = none) and its colours,
  // the scene's own. The App sends them to Ambient; nothing is asked of it while this is 0.
  double window_light() const;
  void scene_zones(double now, Rgb zone[kZones], double light[kZones]) const;
  // the halo's strength for the demonstration only: the light's own default (Store::ambient_strength),
  // so the welcome shows what the person will get, not a brighter promise
  static constexpr double kDemoStrength = 0.15;

 private:
  static constexpr double kDarkDur = 0.22;
  enum Hit { kNone = -1, kNext = 1, kBack, kSkip, kToggle, kSeg0, kSeg1, kSeg2, kSeg3, kSeg4, kDot0, kDot1, kDot2, kDot3, kBg0, kBg1 };
  int page_ = 0;
  double t0_ = -1;
  Spring pos_ = Spring::rd(0.42, 1.0, 0.002);         // page position (0..2): the words slide a little, no overshoot
  Spring knob_ = Spring::rd(0.30, 0.62, 0.004);       // the toggle's knob, a little bounce
  Spring glow_ = Spring::rd(0.55, 1.0, 0.004);        // the scene's light (inside only)
  Spring seg_x_ = Spring::rd(0.36, 0.82, 0.05), seg_w_ = Spring::rd(0.36, 0.82, 0.05);  // size selector
  Spring dot_ = Spring::rd(0.38, 0.9, 0.004);         // page indicator
  Spring bg_ = Spring::rd(0.36, 0.82, 0.004);         // the background chosen: 0 with, 1 without (its highlight slides, the bars crossfade)
  Spring fx_ = Spring::rd(0.5, 0.82, 0.3), fy_ = Spring::rd(0.5, 0.82, 0.3), fs_ = Spring::rd(0.5, 0.9, 0.003);
  bool face_placed_ = false;
  bool face_handoff_ = false;
  double finish_t0_ = -1, finish_fx_ = 0, finish_fy_ = 0, finish_fs_ = 1;
  double page_s_w_ = 0, page_s_h_ = 0;  // the window the page scale was made for
  double page_s_base_ = 0;  // and the interface scale it stood on (1.4.1: a new DPI is a new page)

  int hover_ = kNone, focus_ = kNone;
  bool kb_ = false;
  int mx_ = -1, my_ = -1;
  int tx_ = -100000, ty_ = -100000;
  double track_moved_ = -10;
  std::vector<double> hov_;  // hover fade per hit id
  double hov(int id) const { return id >= 0 && id < (int)hov_.size() ? ease_out(hov_[id]) : 0.0; }
  double happy_until_ = 0, page_enter_ = 0;
  double toggle_t_ = -10;  // last flip (the face reacts)
  double scale_t_ = -10;   // last size change
  int prev_scale_idx_ = -1;  // the size before it (the controls crossfade from it)
  uint64_t scene_key_ = 0;  // what the scene looked like at the last tick (redraw only when it changes)

  // layout (physical px), recomputed by layout(): one grid for the three screens
  bool overflow_ = false;  // layout(): the grid does not fit even with the picture at its smallest
  Rect box_;               // the picture: the eyes, then the scene
  int title_y_ = 0, line_y_ = 0, ctrl_y_ = 0, line_rows_ = 1;
  double eff_s_[4] = {1, 1.5, 2, 2.5};  // size_s, or derived (sizes())
  double ctrl_s_[4] = {1, 1, 1, 1};  // the controls' pixel at each size (their real one when it fits)
  struct Lay {
    Rect next, back, toggle, heading;
    std::vector<Rect> segs, dots;
    int text_y = 0;
  } L_[kPages];
  double face_x_ = 0, face_y_ = 0, face_scale_ = 1;
  int line_at_ = 0;        // the line the eyes stand on (y, physical px)
  Canvas eye_light_;       // the light behind the eyes (made once per layout)
  uint64_t eye_light_key_ = 0;
  Canvas eye_light_base_;  // the same on the welcome's ground: copied in place while fully lit
  Rect eye_light_r_;
  Canvas scene_light_;     // the scene's light when it is drawn inside (made when its colours change)
  Rect scene_light_r_;
  uint64_t scene_light_key_ = 0;
  mutable Canvas reflect_;  // the eyes, drawn small to be mirrored under their line
  void layout();
  void sizes();
  int hit(int x, int y) const;
  std::vector<int> focus_order() const;
  ObAction activate(int id, double now);
  void place_face(double now, bool snap);
  int scale_index() const;
  ObAction pick_scale(int index, double now);

  struct Mood { Rgb top, bot, sun, hill_far, hill_near, glow_top, glow_bot; };
  Mood mood(double now) const;
  double scene_time(double now) const;
  uint64_t scene_signature(double now) const;
  void draw_scene(Canvas& c, const Rect& r, double a, double now, bool progress) const;
  void draw_eyes(Canvas& c, double a, double now);
  void draw_toggle(Canvas& c, const Rect& r, double a, Rgb on) const;
  Rgb scene_ink(double now) const;
  void draw_controls(Canvas& c, const Rect& scene, double a, int size_index, bool free, double menu_a) const;
  void soften_under(Canvas& c, const Rect& scene, const Rect& bar, double a) const;
  void draw_button(Canvas& c, const Rect& r, const std::string& label, bool primary, double hv, double a, bool chevron,
                   bool label_only = false) const;
  void draw_words(Canvas& c, int pg, double ox, double a, double now);
  void draw_footer(Canvas& c, double a, double now);
};

// Word wrap for the pixel font (spaces only), for previews and the onboarding text.
std::vector<std::string> wrap_text(const std::string& t, int max_w, double s);
