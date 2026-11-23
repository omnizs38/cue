// Home: the only place the character lives. Asymmetric horizontal composition,
// character on the left (~42%), Recentes on the right.
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "anim.h"
#include "chrome.h"
#include "face.h"
#include "gfx.h"
#include "light.h"

// An entry's picture: a film's frame (where it was left) or a song's cover, as opaque
// 0xFFRRGGBB pixels, small (the app keeps frames at most 256 px wide). Shared and never changed
// once made: the home scales it for the window once and keeps that copy.
struct HomeThumb {
  std::vector<uint32_t> px;
  int w = 0, h = 0;
  bool cover = false;  // a cover (music, a book): shown whole and square, on a tint of its own colours
};

// What an entry is (see RecentKind): it picks the picture drawn while there is none of its own.
enum class HomeKind { Video, Episode, Music, Listen, Book };

struct HomeRow {
  std::string name;      // the title: a show's name, or the file name without its extension
  std::string right;     // "24 min left", or the length when not started
  double progress = -1;  // 0..1 when in progress
  std::string sub;       // the quiet second line: "S18E15 · yesterday", "artist · today"
  bool audio = false;
  HomeKind kind = HomeKind::Video;  // (an audio row left as Video is drawn as music)
  std::string seed;      // what its stand-in picture is drawn from (the path): always the same one
  std::shared_ptr<const HomeThumb> thumb;  // null: the stand-in
  double thumb_at = -100;  // when the picture arrived (it fades in over the stand-in)
};

enum class HomeAct { None, OpenDialog, OpenFolder, OpenRecent, ClearRecents, Covers, EggSeen, WinMin, WinMax, WinClose, WinPin, Update, SkipUpdate,
                     Language, Remote, News, NewsClose, Survey, SurveyClose, Settings };  // Language: index -1 automatic, else i18n::Lang
struct HomeAction { HomeAct a = HomeAct::None; int index = -1; };

struct HomeView {
  int w = 0, h = 0; double s = 2;
  std::vector<HomeRow> rows;
  std::string engine_problem;  // shown instead of the micro-instruction
  std::string notice;          // short-lived ("could not open")
  int language = -1;           // the saved interface language (-1: follow Windows), checked in the menu
  double notice_until = 0;
  int eggs_seen = 0;
  // a newer version on GitHub: a quiet line in the bottom-left corner
  std::string update_version;   // "" = nothing to offer
  double update_progress = -1;  // 0..1 while downloading
  bool update_busy = false;     // checking / downloading / starting the installer
  bool news = false;            // "what's new" at the bottom until it is closed (once per version)
  bool remote_available = true;
  // the recent files with their pictures and the one to continue apart (true), or a plain list of
  // words, newest first, as many as fit (the ··· menu's "mostrar imagens"; the App keeps it)
  bool covers = true;  // the platform has the phone remote (the ports do not yet)
  // the survey's invitation: a small card in the bottom-right corner (the app decides when; the
  // card shows only where it covers nothing, and goes for good once answered or declined)
  bool survey = false;
  bool survey_shown() const { return survey_vis_.v > 0.01; }
  Rect survey_rect() const { return r_survey_; }        // (for previews and tests)
  Rect survey_answer_rect() const { return r_survey_go_; }
  Rect survey_close_rect() const { return r_survey_no_; }

  Face face;
  WindowChrome chrome;

  void set_size(int w_, int h_, double s_);
  void set_rows(std::vector<HomeRow> r);
  // a picture found later (read on a worker): it fades in over the stand-in. `i` counts the rows
  // given to set_rows (shown or not)
  void set_thumb(int i, std::shared_ptr<const HomeThumb> t, double now);
  // Cue starting: the list waits (at most `secs`) for the pictures already on disk, so they come
  // in with it instead of fading in one by one over their stand-ins. show_list: they are here,
  // or the wait is over; the list fades in (pictures that arrive later still fade in alone)
  void hold_list(double now, double secs);
  void show_list(double now);
  bool list_held(double now) const { return list_hold_until_ > 0 && now < list_hold_until_; }
  // "clear recent": call just before the empty list is set. The entries leave one after another
  // (top first, each fading as it drifts a few pixels right), the eyes follow them down and blink
  // once it is done, then "Recent" glides to its new place and the empty line fades in.
  // scratch: a full-window canvas to draw the list's last look in (the app's back buffer: no
  // 33 MB allocation at 4K at the moment of the click); null: one of its own
  void clear_animated(double now, Canvas* scratch = nullptr);
  bool clearing() const { return !ghosts_.empty(); }
  // "mostrar imagens" switched: call just before the new look is set (covers, set_rows). The
  // list as it is now is kept to fade out while the new one comes in. scratch: as clear_animated
  void swap_animated(double now, Canvas* scratch = nullptr);
  // the pointer (or the keyboard) is on an entry: the person may be about to open it
  bool list_hot() const { return active() >= 0 && active() < (int)rows.size(); }
  // the pictures' size in px of art (0 x 0: the list goes without them, a narrow window)
  int thumb_w() const { return thumb_w_; }
  int thumb_h() const { return thumb_h_; }
  Rect row_rect(int i) const { return i >= 0 && i < (int)r_rows_.size() ? r_rows_[i] : Rect{}; }
  // the first entry is one to go back to (started, not finished, not a song): it stands apart
  // under "continuar", its picture twice the list's, lit by its own colours; the list below
  // ("recentes") goes on from the second entry. Row indices stay those given to set_rows.
  bool continuing() const { return cont_; }
  Rect picture_rect(int i) const { return thumb_rect(i); }  // an entry's picture (for previews and tests)
  void render(Canvas& c, double now);
  // Only the eyes moved: the app may draw again just their corner of the window. face_reach() is
  // every pixel the face can draw in this layout (eyes looking anywhere they may, a blink, a hop,
  // a thought rising beside them, the mouth); eyes_only_key() names everything else on the home.
  // Two frames with the same key differ only inside face_reach(), so render() with the canvas
  // clipped to it gives the same window as a whole new frame (tests/home_partial_test.cpp).
  // 0: something else is moving or depends on the time: draw it all.
  Rect face_reach() const;
  uint64_t eyes_only_key(double now) const;
  bool tick(double now, double dt);
  double next_event(double now) const;

  HomeAction mouse_move(int x, int y, double now);
  HomeAction mouse_down(int x, int y, double now);
  void mouse_leave(double now);
  HomeAction key(const std::string& k, bool shift, double now);
  // the cursor anywhere on screen (client coords, may be outside the window): the eyes follow it
  bool track(int x, int y, double now);  // true when the eyes need to move
  void drag_over(int x, int y, double now);
  void drag_leave(double now);
  bool dragging() const { return drag_; }
  Rect dots_rect() const { return r_dots_; }  // the ··· beside Recentes (for previews)
  Rect settings_rect() const { return r_settings_; }  // the line itself (its click)
  // where the Settings panel rises from: the line, as wide as the two lines together (beside them
  // in a short window, it clears "open a file" too)
  Rect settings_anchor() const { Rect r = r_settings_; const int x1 = std::max(r.r(), r_open_.r()); r.x = std::min(r.x, r_open_.x); r.w = x1 - r.x; return r; }
  bool settings_lit = false;  // the panel it opened is showing: the line stays lit
  // (1.4.1) the Settings panel over the list (a short or narrow window: beside the line, or wide
  // with its picture beside it): the list steps out while it is there, instead of showing in
  // pieces round the panel (a title cut under it, the end of a name beside it), and comes back
  // when the panel leaves it or closes
  void cover_list(bool on);
  Rect list_area() const { return list_reach(); }  // the list's titles and entries (the panel's test)
  // opening a video: the home bows out (swallow = a dropped file gets eaten first)
  void leave(double now, bool swallow);
  // already gone: Cue started with a file, so the window's first picture is the film's black,
  // never the home (it used to show for a moment, then fade)
  // (a flag, not a time in the past: at the start the clock is still near zero)
  void left(double now) { leave(now, false); gone_ = true; }
  bool leaving() const { return leave_t0_ > 0; }
  bool leave_done(double now) const { return leave_t0_ > 0 && (gone_ || now - leave_t0_ >= leave_dur()); }
  void cancel_leave(double now);   // the file did not open: come back
  void enter(double now, bool keep_face = false);  // back from a video (or from the welcome, eyes already there)
  // the welcome ended on a screen without the eyes: the home opens through the same pixel iris
  // that opens a film (black outside a growing ellipse of the window's proportions)
  void enter_iris(double now);
  bool iris_opening(double now) const { return iris_t0_ > 0 && !reduce_motion() && now - iris_t0_ < kIrisOpen; }
  void show_notice(const std::string& n, double now) { notice = n; notice_until = now + 3.0; }
  // for previews/tests
  void force_egg(int variant, double now) { start_egg(now, variant); }
  void set_hover_row(int i, double now);
  // empty background: the window can be dragged from here (and snapped by Windows)
  bool is_background(int x, int y) const;
  void relayout() { layout(); }  // the words changed (another interface language)

 private:
  Fade list_cover_{0, 0, 0.12, 0.16};
  Rect list_reach() const;  // the list's titles and entries, as the list's fade-in covers them
  // the ··· menu next to Recentes: "idioma ›", "controle remoto" and "limpar recentes"; language opens automatic plus the supported catalogs
  enum : int { kOpen = 100, kDots = 101, kMenuClear = 102, kUpdate = 103, kUpdateSkip = 104, kMenuLang = 105,
               kLangOpt = 106 /* ..114: automatic, then eight languages */, kMenuPad = 115, kMenuRemote = 116, kNews = 117,
               kNewsClose = 118, kMenuFolder=119, kSurveyGo = 120, kSurveyNo = 121, kSurveyPad = 122, kMenuCovers = 123, kSettings = 124, kHitIds = 25 };
  int hover_ = -1, focus_ = -1;
  bool kb_ = false, menu_ = false, menu_lang_ = false, drag_ = false;
  std::vector<int> menu_ids_;
  std::vector<Rect> r_menu_items_;
  void layout_menu();
  std::string menu_label(int id) const;
  void open_menu(bool lang);
  int mx_ = -1, my_ = -1;
  int tx_ = -100000, ty_ = -100000;
  double track_moved_ = -10;
  std::vector<double> clicks_;
  int egg_ = 0;
  double egg_t0_ = 0;
  Rng rng_{42};
  // the Christmas surprises: snowflakes (and the ones that land on the eyes), a falling hat,
  // a string of lights
  struct Flake { double x, y0, at, v, sway, ph; int size; int eye; double land; };
  std::vector<Flake> flakes_;
  double shake_t_ = -1;

  Rect r_settings_;  // "configurações" under "abra um arquivo": the player's Settings panel, over the home
  Rect r_open_, r_title_, r_dots_, r_menu_, r_list_, r_update_, r_update_skip_, r_news_, r_news_close_;
  Rect r_survey_, r_survey_go_, r_survey_no_;
  bool survey_fits_ = false;
  Fade survey_vis_{0, 0, 0.35, 0.18};
  double survey_t0_ = -1;    // the card's little scene: when it started writing (it writes twice, then rests)
  bool survey_hot_ = false;  // the pointer on the card: it looks up and writes again
  int survey_drawn_step_ = -2;
  void layout_survey();
  void draw_survey(Canvas& c, double now, double a) const;
  int survey_step(double now) const;  // the scene's frame (8 a second while it writes; -1 resting)
  void layout_update();
  std::vector<Rect> r_rows_;
  bool cont_ = false;      // rows[0] is the entry to continue (see continuing())
  bool recent_title_ = true;  // "recentes" has entries under it (else the dots go beside "continuar")
  Rect r_cont_label_;
  int cont_tw_ = 0, cont_th_ = 0;  // its picture, art px (0: a narrow window, none)
  // its light: the picture's own colours around it, made once per picture and size
  mutable Canvas cont_light_;
  mutable const void* cont_light_src_ = nullptr;
  mutable int cont_light_w_ = 0, cont_light_h_ = 0, cont_reach_ = 0;
  const Canvas* cont_light(Rect& at) const;
  void draw_cont(Canvas& c, double ha, double now) const;
  void draw_cont_back(Canvas& c, double now) const;
  std::vector<HomeRow> all_rows_;  // what was given; `rows` is the part that fits the window
  // the list's pictures: their size (art px; 0 without), and each row's picture made for this
  // window once (scaled, its corners cut), redrawn only when the size or the picture changes
  int thumb_w_ = 0, thumb_h_ = 0;
  struct Tile { const void* src = nullptr; std::string seed; HomeKind kind{}; int w = 0, h = 0; Canvas img; };
  mutable std::vector<Tile> tiles_, stand_ins_;
  struct Disc { const void* src = nullptr; int side = 0, turn = -1, light = -1; uint32_t label = 0; Canvas img; };
  mutable std::vector<Disc> discs_;
  const Canvas& disc_for(int i, double turn, double light) const;
  Rect thumb_rect(int i) const;
  const Canvas& tile_for(int i) const;      // the row's picture, or its stand-in
  const Canvas& stand_in_for(int i) const;  // the stand-in alone (under a picture that fades in)
  void draw_thumb(Canvas& c, int i, double ha, double now) const;
  bool thumbs_fading(double now) const;

  void layout();
  int hit(int x, int y) const;
  int active() const { return kb_ ? focus_ : hover_; }
  void start_egg(double now, int forced = 0);
  void update_face_target(double now);
  void draw_snow(Canvas& c, double t) const;
  void draw_hat(Canvas& c, double t) const;
  void draw_lights(Canvas& c, double t) const;
  int lights_n() const;
  double lights_y(double x, double t) const;
  int egg_variant_ = 0;
  double leave_t0_ = -1, enter_t0_ = -10;
  double iris_t0_ = -1;
  double list_hold_until_ = -1, list_in_t0_ = -10;  // the list waiting for its pictures at the start
  double list_in(double now) const;                 // how far the list has come in, 0..1
  bool list_moving(double now) const { return list_held(now) || (list_in_t0_ > 0 && now - list_in_t0_ < 0.25) || swapping(now); }
  // the list changing look (pictures on or off): the old one fades out where it was, the new one
  // comes in under it a moment later, rising a few pixels
  Canvas swap_old_;
  Rect swap_r_;
  double swap_t0_ = -10;
  static constexpr double kSwapOut = 0.16, kSwapDelay = 0.08, kSwapIn = 0.26;
  bool swapping(double now) const { return swap_t0_ > 0 && now - swap_t0_ < kSwapDelay + kSwapIn + 0.02; }
  Rect list_band() const;
  void draw_swap(Canvas& c, double now) const;
  bool gone_ = false;  // left before it was ever shown (see left)
  bool keep_face_ = false;
  bool swallow_ = false;
  double leave_dur() const { return swallow_ ? 0.36 : 0.2; }
  void fades(double now, double& content, double& face) const;  // 1 = fully shown
  Fade menu_fade_{0, 0, 0.12, 0.14};  // the ··· menu unfolding and folding
  std::vector<double> hov_;  // hover fade per hit id (rows, open, dots, menu)
  double hov(int id) const;
  // "clear recent" (see clear_animated): the entries as they were drawn, leaving
  struct Ghost { Canvas img; Rect r; };
  std::vector<Ghost> ghosts_;
  double clear_t0_ = -10;
  int title_from_y_ = 0;
  bool blinked_ = false;
  double clear_end() const;
  void clear_fx(double now, double& title_k, double& empty_k) const;
};
