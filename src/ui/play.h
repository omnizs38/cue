// Playback UI: everything drawn over the video. The video is the interface;
// this layer appears only when the mouse moves and leaves after 2 s.
#pragma once
#include <array>
#include <memory>
#include <string>
#include <vector>

#include "../core/audio_mode.h"
#include "../core/cover_turn.h"
#include "../core/model.h"
#include "../core/media_info.h"
#include "../core/subtitle_selection.h"
#include "anim.h"
#include "queue_view.h"
#include "chrome.h"
#include "gfx.h"
#include "scene_contrast.h"
#include "blur_cloud.h"

struct PlayInput {
  int w = 0, h = 0; double s = 2;
  bool fullscreen = false;
  bool paused = false;
  bool audio_only = false;
  double time = 0, dur = 0, volume = 100;
  bool muted = false;
  std::vector<Track> subs, audios;
  uint64_t tracks_revision=0;  // metadata/track list changes, not playback ticks
  int64_t sid = 0, aid = 0;  // 0 = none
  SubtitleSelection sub_selection;  // the latest requested track until its command finishes
  bool amix = false;          // several audio tracks at once (aid is then the last single one)
  std::vector<int64_t> amix_ids;  // the tracks playing together (2 or more while amix)
  bool sub_visible = true;
  double sub_delay = 0;
  SubStyle style, default_style;
  SubKind kind = SubKind::None;  // kind of the selected subtitle track
  int sub_lines = 1, sub_chars = 28;
  bool sub_shown = false;  // a subtitle line is on screen right now
  // the lines on screen right now (from the engine); laid out like libass to know where they are
  std::vector<std::string> sub_text;
  double speed = 1.0;
  int ui_scale = 2;         // -1 small, 2 medium (default), 0 large, 1 larger
  bool controls_free = false;
  SceneGrid scene;
  uint64_t scene_revision = 0;
  bool ambient = true;
  double ambient_strength = 0.15;  // 0.05..1, starts soft
  Rgb accent{255, 255, 255};  // atmosphere of the film (already smoothed); white when unknown
  double accent_k = 0;        // 0 = neutral, 1 = full accent
  InfoFields file_info;
  // updates: "" when there is nothing to say; otherwise what the row should read
  std::string update_label;    // "check for updates", "update to 1.2", "downloading 42%"
  bool update_ready = false;   // a newer version is known: the More menu shows it too
  bool loop = false;           // repeat the file forever (music videos, loops)
  bool loop_all = false;       // repeat the queue: after its last item, the first again
  bool shuffle = false;        // music: the list plays in a shuffled order (each song once a round)
  int language = -1;           // interface language as saved: -1 automatic (follow Windows), else i18n::Lang
  using QueueItem = QueueEntry;
  std::vector<QueueItem> queue;
  uint64_t queue_revision=0;
  bool queue_active=false;
  bool has_next = false;
  bool has_prev = false;  // an explicit queue has an entry before the playing one
  bool ended = false;          // stopped on the last frame (the controls stay in view)
  // what this platform offers (the macOS and Linux ports have no ambient light or phone remote yet)
  bool ambient_available = true;
  bool remote_available = true;
  std::string open_shortcut = "ctrl+o";  // as the platform writes it ("cmd+o" on macOS)
  uint64_t file_info_revision = 0;
  // music: the file has a cover (the right-click menu offers both looks); the cover look is on
  // screen (the cover can be tilted); which look the person chose; the captions under the cover
  bool has_cover = false, cover_scene = false;
  int music_look = 0;  // 0 cover and spectrum, 1 spectrum only, 2 cover only
  bool music_mono = false;  // music in black and white
  bool cover_tilt = true;   // the cover leans under the pointer (a choice; on by default)
  bool settings_examples = true;   // Settings shows each setting at work (the "i" in its header)
  int mini_sub_size = 0;    // the film's mini player's subtitles: 0 as the full player's (kept readable), 1 larger, 2 largest
  bool mini_player = false;      // the window is the mini player (small, in a corner, on top)
  bool smooth_available = false; // the film's frame rate does not match the screen's: smoothing helps
  bool smooth_motion = false;    // blend frames at their changes (no judder on pans), a choice (off by default)
  std::string music_title, music_artist;
  // the card's cover drawn by Cue (the spectrum-alone look has no cover in the engine's picture):
  // read ahead at the card's size; shown while the card is up and no turn is drawing it
  std::shared_ptr<const CoverArt> card_art;
  bool card_art_show = false;
  MusicScene music_scene;  // the cover look's composition (the engine's picture)
  bool cover_art = false;  // the scene shows a cover (the file's or Cue's tile): it tilts under the pointer
  MusicView music_view;    // how the window frames it (all of it, tall, the cover alone); f = 0: fit
  std::string next_title;  // music: what plays next, as it will be named (shown as the song ends)
  bool music_leaving = false;  // music: the song is being changed (its words step out before the next)
  // the engine draws the opening iris on the picture (its video shader): the interface keeps the
  // iris's timing but draws no full-window bitmap for it
  bool iris_engine = false;
};

enum class PA {
  TogglePause, SeekRel, SeekAbs, SeekAbsExact, Volume, ToggleMute, Fullscreen, SelectSub, SelectAudio, AudioMix,
  AddSubFile, OpenFile, CloseVideo, Style, StyleLive, StyleDefault, SubDelay, SubVisible, Placement, SubLift,
  WinMin, WinMax, WinClose, WinPin, WinDragBegin, WinDragEnd, Preview, Speed, SaveFrame, UiScale, Ambient, AmbientStrength,
  OpenFrameFolder, OpenLinkedIn, OpenGitHub, OpenEmail, OpenLicenses, FileInfo, ControlsFree, Update, Loop, SubEdit,
  Language,  // v: -1 automatic, else i18n::Lang
  NextEpisode, PreviousInQueue, CancelNext, QueueAdd, QueueFolder, QueuePlay, QueueRemove, QueueMove, QueueReorder, QueueClear,
  Remote,    // the phone remote's pairing scene
  MusicLook,  // v: 0 cover and spectrum, 1 spectrum only, 2 cover only
  MusicMono,  // v: 0 colour, 1 black and white
  CoverTilt,  // v: 0 still, 1 leans under the pointer
  MiniPlayer,    // v: 1 into the mini player, 0 back
  Shuffle,       // v: 1 on, 0 off (music)
  SmoothMotion,  // v: 0 off, 1 on
  SettingsExamples,  // v: 0 the settings alone, 1 each one shown at work
  MiniSubSize        // v: 0, 1, 2 (the film's mini player's subtitles)
};
struct PlayAction { PA a; double v = 0; int64_t id = 0; };

struct Layer {
  Canvas c;
  int x = 0, y = 0;
  bool visible = false;
  bool dirty = true;  // contents or position changed since the last upload
  // what was last composed, so unchanged frames are never re-uploaded
  int lx = -1, ly = -1, la = -1;
  const void* lsrc = nullptr;
  uint64_t lgen = 0;
};

class PlayView {
 public:
  enum { L_ART, L_MUSIC, L_NEXT, L_DOCK, L_PANEL, L_TOAST, L_TIP, L_G0, L_G1, L_G2, L_HINT, L_WIN, L_FLASH, L_BACK, L_IRIS, L_COUNT };
  PlayInput in;
  DockPlace place;
  std::array<Layer, L_COUNT> layers;
  WindowChrome chrome;

  void resize(int w, int h, double s);
  // the window's own scale, before a portrait or square window raises the bar's: menus, subtitles
  // and up next stay at a size that suits the window's width instead of following the bar
  void set_base_scale(double b) { base_s_ = b; }
  double panel_s() const {
    if (base_s_ <= 0 || base_s_ >= s_) return s_;
    // half way between the window's own scale and the raised bar's, in half steps: a menu comes
    // out about as wide as the bar it opens from (one silhouette), and never wider than about
    // 72% of the window (a menu is ~230 art px)
    double mid = std::floor((base_s_ + s_) / 2 * 2) / 2;
    double by_width = std::floor(w_ * 0.72 / 230.0 * 2) / 2;
    return std::max(1.0, std::min(s_, std::max(base_s_, std::min(mid, by_width))));
  }
  void changed(double now);   // App updated `in`
  void reset(double now);     // new file
  void begin_viewport_transition(double now, bool instant = false);
  void end_viewport_transition(double now);
  bool viewport_hidden() const { return viewport_transition_ && viewport_vis_.v <= 0; }
  void glide_next_resize() { glide_ = true; }  // the next resize animates the dock instead of jumping
  bool tick(double now, double dt);
  std::vector<PlayAction> take_actions() { auto a = std::move(acts_); acts_.clear(); return a; }

  void mouse_move(int x, int y, double now);
  void pointer_left();  // the pointer left the window: a tilted cover settles back
  // music: the cover's turn (degrees) and how much of its light shows, for the shader
  struct CoverTilt { double rx = 0, ry = 0, k = 0; };
  CoverTilt cover_tilt() const { return {cov_rx_.x, cov_ry_.x, cov_k_}; }
  // where the music scene's cover is on screen (the scene fills the window like a 16:9 picture)
  Rect cover_screen_rect() const;
  // set by the window just before mouse_down: the click that activated an inactive window
  // only wakes it (controls appear), the picture never pauses on it
  bool activation_click = false;
  void mouse_down(int x, int y, double now);
  void mouse_up(int x, int y, double now);
  void right_click(int x, int y, double now);
  void wheel(double dir, double now);
  void key(const std::string& k, bool shift, bool ctrl, double now);
  bool set_shift(bool on);  // Shift held or let go (a dragged bar: centred while it is held); true: it moved
  bool dock_dragging() const { return dragging_dock_; }
  void toast(const std::string& text, const char* icon, double now, bool icon_after = false);
  // "frame saved ✓ | folder": stays a little longer, and its folder button opens where the frame went
  void set_next_details(const std::string& name);
  void set_next_picture(const std::vector<uint32_t>& pixels,int w,int h);
  void next_notice(const std::string& label, double now);
  void toast_folder(const std::string& text, double now);
  // the sound changed language: its name turns over letter by letter, like a departures board
  void announce_audio(double now);
  void activity(double now);
  double last_activity() const { return last_activity_; }
  uint64_t dock_signature() const { return dock_revision_; }
  int cursor_kind() const;  // 0 arrow, 1 link, 2 open hand (subtitle), 3 closed hand (dragging it)
  void flash(const std::string& text, double now);  // tiny centred feedback ("+15", "1.25×")
  // the phone remote drives the same actions as the dock (and shows the same feedback)
  void remote_toggle_pause(double now) { activate(B_PLAY, now); }
  void remote_act(PA a, double v = 0, int64_t id = 0) { act(a, v, id); }
  void flash_pause(bool paused, double now);
  // timeline preview: a small frame near the hovered time (BGRA, opaque)
  void set_thumb(const std::vector<uint32_t>& px, int w, int h, double t);
  int thumb_width() const { return 80 * s_; }
  // previews are decoded at half size and shown doubled, pixel for pixel (lighter, and in the
  // same pixel language as the rest of the UI)
  // Hover images use one quarter of their display width (versus half for colours).
  int thumb_hover_width() const { return std::max(16, (int)std::lround(20 * s_)); }
  int thumb_decode_width() const { return std::max(32, (int)(40 * s_ / 2) * 2); }

  // opening a video: black until the first frame is up, then it opens in a pixel ellipse
  void begin_cover();
  void start_iris(double now);
  bool iris_waiting() const { return iris_ == 1; }
  bool iris_active() const { return iris_ != 0; }
  // how far the iris is open: -1 none, 0 covered (waiting for the picture), up to 1 (eased)
  double iris_open(double now) const;
  void prepare(double now) { compose(now); }  // compose the layers right away (before the first tick)

  bool pinned() const;
  // the stacked composition (music framed close): what the app reserves under and over the cover
  bool music_stacked() const { return stack_; }
  bool music_card() const { return card_; }
  // music's list open over the cover (the close compositions): how far the picture steps back (0..1)
  double list_veil() const;
  // Music's list where the words' column cannot hold it (the close compositions, small windows):
  // one column, centred, using the window's height. A header (the cover, shrunk to a thumbnail on
  // the column's left, the title and artist beside it), the list under it, the bar under the list;
  // the three centred as a group when the list is short. The cover's thumbnail is where the app
  // frames the picture (it glides there).
  struct ListLayout { bool on = false; Rect thumb, words, list; double bar_y = 0; };
  ListLayout list_layout() const;
  Rect list_cover() const { ListLayout L = list_layout(); return L.on ? L.thumb : Rect{}; }
  double list_header_mix() const { return list_header_ ? ease_out(std::clamp(list_k_.x, 0.0, 1.0)) : 0.0; }
  bool list_compact() const;
  double music_words_w() const;  // the words beside the cover, as the composition centres them
  Rect card_cover() const { return card_cover_; }
  double music_stack_below() const;
  double music_stack_above() const;
  double music_centre_below() const;  // Centre: the words under the cover, with the air above them
  bool music_list_beside() const;     // Centre: the album's list open in the right half
  double centre_words_gap() const;
  Rect subtitle_rect() const { return sub_rect(); }  // where the subtitle sits on screen (client px)
  void toggle_loop(double now);
  void begin_sub_edit(double now);  // a subtitle drag starts: pause + grey, controls step aside
  bool dragging() const;  // a button is held on a control
  bool win_dragging() const { return win_drag_; }  // UI must stay (scrubbing, panel, dragging...)
  void handoff_window_drag(double now); // clear gesture before Windows takes over its move loop
  void set_double_click_time(double seconds) { double_click_time_ = std::clamp(seconds, 0.1, 5.0); }
  void cancel_queue_gesture() { cc_drag_=false;if(queue_open())queue_view_.cancel();panel_dirty_=true; }
  bool queue_open() const { return panel_ == Panel::Queue; }
  // Files dragged over the window while music plays: the list opens and invites the drop (and
  // closes again if they leave without being dropped). Films keep their own drop behaviour.
  void drag_files(bool over, double now);
  void drag_dropped() { drop_opened_ = false; queue_view_.set_dropping(false); panel_dirty_ = true; }
  // An action from outside the window (the taskbar's buttons), handled like the dock's own.
  void request(PA a, double v = 0) { act(a, v); }
  uint32_t queue_thumbnail_request() const { return queue_open()?queue_view_.thumbnail_request():0; }
  void queue_thumbnail(uint32_t id,const std::vector<uint32_t>& pixels) { queue_view_.thumbnail(id,pixels);panel_dirty_=true; }
  bool panel_open() const { return panel_ != Panel::None; }
  bool dock_visible() const { return vis_.v > 0; }
  BlurCloud blur_cloud() const { return {dock_rect(), (mini_row_ ? 1.0 : style_mix_) * ease_out(vis_.v) * ease_out(viewport_vis_.v) * (1 - ease_out(edit_.x))}; }
  // something of the UI is on screen or about to change (otherwise playback updates draw nothing)
  bool needs_frame() const {
    if (!cov_rx_.settled() || !cov_ry_.settled() || (cov_k_ > 0 && !cov_hot_) || music_vis_.v != music_vis_.target) return true;
    if (!list_k_.settled() || !music_w_motion_.settled()) return true;
    if (next_vis_.v != (next_wanted() ? 1.0 : 0.0) || !next_drop_.settled()) return true;
    if (vis_.v > 0 || vis_.target > 0 || pvis_.v > 0 || edit_.x > 0 || edit_.target > 0 || iris_ != 0 || sub_settling_ || dragging() || pending_click_ != P_NONE) return true;
    for (auto& L : layers) if (L.dirty) return true;
    return !toast_text_.empty() || last_now_ - flash_t0_ < 0.9;
  }
  double lifted_pos() const;  // sub-pos to apply now (rises above the dock)
  // for previews
  void open_panel_for_preview(int which, double now);
  void set_hover_for_preview(int x, int y, double now) { mouse_move(x, y, now); }
  void begin_sub_drag_for_preview(double now);

 private:
  enum Btn { B_NONE = 0, B_VOL, B_VOLBAR, B_REW, B_PLAY, B_FWD, B_CC, B_AUDIO, B_FS, B_MORE, B_TIMELINE, B_DOCKBG, B_SPEED, B_TIME, B_LOOP, B_QUEUE, B_NEXT, B_PREV, B_EXPAND, B_MINI, B_SHUFFLE, B_COUNT };
  enum class Panel { None, CC, Style, Audio, AudioMix, More, Speed, Context, About, FileInfo, Language, Queue, Keys, Volume, Interface, Settings,
                     SubMini };  // SubMini: the film's mini player's subtitles (a few rows: on, which, how big)
  enum class RK { Header, Option, Action, Submenu, Slider, Fonts, Stepper, Segmented, Hint, Separator, Links, Toggle,
                  SpeedBar, Chips, Brand, Credit, Profile, Info, CloseHeader, Contacts, Keys, Tools, VSlider, Preview, Picker };
  enum RowId {
    R_NONE, R_SUB_TOGGLE, R_SUB_TRACK, R_ADD_SUB, R_STYLE, R_DELAY, R_BACK, R_FONTS, R_SIZE, R_TEXT, R_BG, R_OUTLINE, R_LINEGAP, R_TRACK,
    R_ASS, R_LINKS, R_AUDIO_TRACK, R_AUDIO_MIX, R_AUDIO_MIX_TRACK, R_AUDIO_MIX_ALL, R_BACK_AUDIO, R_STYLE_RESET, R_OPEN, R_CLOSE, R_SPEED, R_SPEED_OPT, R_BACK_MORE, R_UISCALE, R_AMBIENT,
    R_FRAME, R_FRAME_CLEAN, R_FS_ROW, R_AUDIO_ROW, R_AMBIENT_STR, R_SPEED_VALUE, R_SPEED_BAR, R_SPEED_CHIPS,
    R_ABOUT, R_LINKEDIN, R_GITHUB, R_EMAIL, R_CONTACTS, R_LICENSES, R_FILE_INFO, R_DISMISS, R_CONTROLS_FREE, R_UPDATE,
    R_LANGUAGE, R_LANG_OPT, R_REMOTE, R_QUEUE, R_MUSIC_LOOK, R_MUSIC_COLOR, R_KEYS, R_COVER_TILT,
    R_VOLUME, R_MUTE, R_OVF_LOOP, R_OVF_CC, R_OVF_AUDIO, R_MINI, R_SMOOTH, R_TOOLS, R_INTERFACE,
    R_SETTINGS, R_BACK_SETTINGS, R_PREVIEW, R_SUB_PICK, R_SUB_SIZE
  };
  struct Row {
    RK k;
    RowId id = R_NONE;
    std::string label, right;
    bool checked = false;
    bool enabled = true;
    int64_t track = 0;
    double value = 0, minv = 0, maxv = 1;
    double mark = -1e9;  // a slider's usual value: a notch under its track (the handle sticks to it)
    int seg = 0;
    double opt = 0;  // value carried by an option (speed)
    std::vector<std::string> segs;
    std::vector<const char*> seg_icons;  // a small icon before each segment's words (optional)
    bool icons_only = false;  // a compact control at the row's end; the row's words name the choice
    const char* icon = nullptr;
    // layout (panel-local physical px)
    Rect r;
    std::string display_label;
    std::vector<Rect> parts;  // slider bar, stepper -/+, segments, links
    // the shortcut sheet: one or two columns, each a section's name or an entry and its keys
    struct KeyHalf { std::string label; std::vector<std::string> caps; bool section = false; } kh;
    int kcol = 0;  // its column
  };
  int keys_cols_ = 2, keys_col_w_ = 0;  // the shortcut sheet: columns, and each one's width (art px)
  bool keys_compact_ = false;
  double keys_s_ = 2;  // its own scale: a small window gets the whole sheet, a little smaller
  void build_keys();
  static int key_cap_w(const std::string& cap);  // art px

  QueueView queue_view_;
  void queue_event(QueueView::Event event,double now);
  bool layout_queue_=false,layout_next_=false,layout_music_=false,layout_cc_=true;
  int layout_ends_=0;  // has_prev | has_next << 1, as last drawn
  size_t queue_count_seen_=0;
  Fade queue_mark_{0,0,.12,.14};
  int w_ = 0, h_ = 0; double s_ = 2, base_s_ = 0;
  std::vector<PlayAction> acts_;
  void act(PA a, double v = 0, int64_t id = 0) { acts_.push_back({a, v, id}); }

  // dock
  int dw_ = 0, dh_ = 0;  // physical
  Spring px_{17, 0.86, 0.3}, py_{17, 0.86, 0.3};
  Fade vis_{0, 0, tok::dock_in, tok::dock_out};
  double last_activity_ = -10;
  int hover_ = B_NONE, focus_ = B_NONE;
  bool kb_ = false;
  bool dock_dirty_ = true;
  Canvas dock_base_;
  uint64_t dock_gen_ = 0, panel_gen_ = 0, toast_gen_ = 0;
  std::array<int64_t, 17> dock_state_{};
  int layout_speed_w_ = -1;  // width of the speed label the dock was laid out for
  double style_mix_ = 0;
  PolishedMotion style_motion_;
  bool style_animating_ = false;
  bool layout_free_ = false;
  bool dock_state_valid_ = false, layout_hours_ = false;
  size_t layout_audio_count_ = 0;
  uint64_t dock_revision_ = 0;
  std::string panel_sig_;
  int info_label_w_ = 68;    // "about file": the label column fits the longest label (art px)
  bool about_compact_ = false, about_retry_ = false;  // "about Cue" in a window shorter than the card
  Spring morph_{26, 1.0, 0.01};  // 0 = play, 1 = pause
  // moving the subtitle: the video pauses and turns grey, the controls step aside (0..1)
  Spring edit_ = Spring::rd(0.26, 1.0, 0.004);
  bool edit_resume_dock_ = false;
  Spring rise_{34, 1.0, 0.01};  // dock entrance, art px (soft overshoot)
  double last_vis_target_ = 0;
  std::array<double, B_COUNT> hov_{};  // hover fade per dock button
  // magnetic hover: each icon leans (physical px) toward the cursor inside its hitbox
  std::array<Spring, B_COUNT> magx_{}, magy_{};
  bool remaining_ = false;
  bool compact_ = false;           // the bar is narrower than all its buttons need
  // The bar by priority (see layout_dock): as the window narrows it first shortens the volume
  // line, then folds the volume into its icon (a click opens it above) and hands its least
  // needed buttons to "..." one by one. Play stays centred, "..." stays at the right end.
  int dock_step_ = 0;              // how far it has folded (0: everything shown)
  int fold_left_ = 0, fold_right_ = 0;  // each side's fold (see layout_dock)
  bool fold_steps_ = false;        // the 15-second steps left the bar (a playlist keeps previous/next)
  bool more_collapsed_ = false;    // a short window: "..." folds its interface section into a submenu
  std::vector<int> tool_ids_;      // "..."'s first row: the buttons the bar handed over, in its order
  void tool_click(int part, double now);
  uint32_t overflow_ = 0;          // buttons (1 << Btn) living in "..." right now
  bool vol_folded_ = false;        // the volume line is folded into its icon
  bool dur_shown_ = true;          // the duration beside the timeline (a narrow bar drops it)
  bool mini_ = false;              // mini player: play, volume, the timeline, back to full size and "..."
  bool mini_row_ = false;           // the film's mini player: two rows on its bottom edge, no bar behind them
  bool mini_times_ = false;         // the film's mini player shows the times beside its timeline
  double mini_t0_ = 0, mini_t1_ = 0;  // where they go (art px)
  bool card_ = false;
  bool expand_on_release_ = false;  // the mini player's corner arrow: back to full size on release              // a song's mini player: cover left, words and bar beside it
  Rect card_cover_{}, card_col_{};
  bool stack_ = false;             // music framed close, and a song's mini player: cover, words, bar as one
  bool music_full_ = false;        // music in a roomy window: the bar along the window's foot, the timeline over the buttons
  int list_room_h_ = -1;  // the room music's list was laid out in (its height)
  bool layout_scene_ = false;  // the cover's scene the bar was laid out for (music's full bar needs it)
  int layout_badge_ = 0;       // the list button's count: how many digits it was laid out for (0: none)
  int settings_w_ = 0;         // Settings' rows' width (art px)
  int settings_sheet_w_ = 0;   // all of Settings (its picture's column beside included), for its About page
  Spring list_top_ = Spring::rd(0.22, 1.0, 0.5);  // music's list: its middle, gliding to where it is given
  bool list_top_on_ = false, list_mid_ = false;
  const CoverArt* art_drawn_ = nullptr;  // the card's drawn cover: the art and place it was drawn for
  Rect art_rect_{};
  int layout_frame_ = -1;          // the music composition the bar was laid out for
  void music_full_place(double& x, double& y) const;
  // the card's controls (art px): where they start, the rows' middles, the time's place
  double card_dock_x_ = 0, card_dock_y_ = 0, card_rows_[3] = {0, 0, 0}, card_time_x_ = -1;
  int card_words_y_ = 0, card_words_h_ = 0;  // the card's words: the room they are centred in (px)
  int card_list_bottom_ = 0;       // the card's list ends here (px): above its timeline
  double stack_words_h() const;
  void stack_place(double& x, double& y) const;
  bool music_compact_ = false;     // music in a reduced window: previous, play, next and the timeline
  bool slots_ready_ = false;
  // each button glides to its new place and fades in or out when the bar re-folds; offsets are
  // kept from the button's anchor (left edge, centre, right edge), so a window being dragged
  // wider or narrower moves nothing on a spring, only a change of fold does
  struct Slot { int anchor = 0; double y = 0, w = 0, h = 0; Spring rel{38, 1.0, 0.02}, wid{38, 1.0, 0.02}, ys{38, 1.0, 0.02}; Fade vis{0, 0, 0.16, 0.12}; bool shown = false; };
  std::array<Slot, B_COUNT> slots_{};
  bool step_slots(double dt);
  Rect slot_rect(int id) const;    // where a button is drawn now (dock-local px), empty when gone
  double slot_alpha(int id) const;
  bool in_bar(int id) const { return id >= 0 && id < B_COUNT && slots_[id].shown; }
  bool overflowed(int id) const { return (overflow_ >> id) & 1u; }
  bool glide_ = false;
  bool viewport_transition_ = false;
  Fade viewport_vis_{1,1,.22,.11};  // in: with the rise spring; out: quick, before the zoom gets going
  double tl_near_ = 0;             // 0..1: the cursor approaches the timeline           // narrow window (portrait video): the dock recomposes
  int still_x_ = -1, still_y_ = -1;  // where the pointer rested when the dock left (jitter guard)
  bool swallow_up_ = false;        // the press closed a panel: its release does nothing
  int ctx_x_ = 0, ctx_y_ = 0;      // context menu anchor
  Rect closing_rect_;              // a closing panel fades where it was
  int contact_k_ = 0;              // About's contact icons: the one the keyboard is on
  double queue_s_ = 2;  // the up-next panel's own scale (compacted in small windows)
  std::string flash_text_;
  int flash_icon_ = -1;            // -1 text, 0 play, 1 pause
  double flash_t0_ = -10;
  double last_now_ = 0;
  uint64_t flash_gen_ = 0;
  Canvas flash_base_;
  std::vector<double> phov_;      // hover fade per panel row
  double hover_since_ = 0, ack_until_ = 0, content_t0_ = -10;
  bool ack_now_ = false;
  std::string chrome_sig_;
  enum Press { P_NONE, P_VIDEO, P_SUB, P_STRIP, P_GRIP };  // P_GRIP: the mini player's bar background (moves the window, a click does nothing)
  Press press_ = P_NONE;
  int press_x_ = 0, press_y_ = 0;
  bool win_drag_ = false;
  bool close_on_release_ = false;
  static constexpr int kStrip = 26;  // top strip (art px) that drags the window
  bool sub_draggable() const;
  Canvas thumb_, thumb_prev_;
  double thumb_xfade_ = 1;  // 0..1: the new frame dissolves over the previous one
  double thumb_t_ = -1, last_preview_req_ = -100;
  uint64_t thumb_gen_ = 0;
  struct Hot { int id; Rect r; };
  std::vector<Hot> hots_;
  Rect tl_;  // timeline track, dock-local
  std::string shown_time_;
  int shown_tl_px_ = -1;
  bool drag_pending_ = false, dragging_dock_ = false;
  bool free_dock_grab_ = false;  // a press on the empty places of the bar without background
  bool shift_ = false;           // Shift held: a dragged bar keeps its centre on the window's middle
  void dock_drag_to(int x, int y);
  double side_room() const;
  // the subtitle's way back: to the person's default (or Cue's), and once more to undo it
  SubStyle undo_style_, undo_target_;
  bool undo_ok_ = false, resetting_ = false;
  bool style_customized() const { return in.style != in.default_style || in.style != SubStyle{}; }
  void reset_style(double now);
  static constexpr double kSnap = 28;  // art px: a dock place pulls, and catches it when dropped
  int ghost_ = -1, ghost_drawn_ = -2;  // the place the dock would land in (-1: none)
  void update_ghost();
  int grab_x_ = 0, grab_y_ = 0, down_x_ = 0, down_y_ = 0;
  bool scrubbing_ = false, vol_drag_ = false;
  double scrub_t_ = 0;
  int mx_ = -1, my_ = -1;
  double last_click_t_ = -10;
  int last_click_x_ = 0, last_click_y_ = 0;
  double double_click_time_ = 0.35;
  Press pending_click_ = P_NONE;
  bool activation_press_ = false;
  bool click_toggled_ = false;     // the first click of a possible double click paused or played  // this press only brought the window forward
  bool second_click_ = false;
  bool click_was_paused_ = false;
  void commit_click();

  // panel
  Panel panel_ = Panel::None;
  int panel_btn_ = B_NONE;
  std::vector<Row> rows_;
  int pw_ = 0, ph_ = 0;
  int phover_ = -1, ppart_ = -1, pfocus_ = -1;
  int slider_row_ = -1;
  Fade pvis_{0, 0, 0.12, 0.10};
  Spring prise_ = Spring::rd(0.36, 0.8, 0.03);  // panel entrance, art px
  // the music cover under the pointer: its turn in degrees (a soft spring, a touch of overshoot)
  // and its light; the captions under it (title, artist, what comes next) fade with the look
  Spring cov_rx_ = Spring::rd(0.42, 0.74, 0.02), cov_ry_ = Spring::rd(0.42, 0.74, 0.02);
  double cov_k_ = 0;
  bool cov_hot_ = false;
  void cover_hover(int x, int y);
  bool next_wanted() const {
    return in.audio_only && !in.next_title.empty() && in.dur > 20 && in.dur - in.time <= 12 && !in.ended && !viewport_transition_ &&
           !(in.mini_player && !in.fullscreen);  // (the mini player: no notice, the next song just comes)
  }
  // the film's mini player (not a small window of the full player, not a song's card)
  bool film_mini() const { return in.mini_player && !in.audio_only && !in.fullscreen;
  }
  Fade music_vis_{0, 0, 0.22, 0.18};
  double music_dock_ = 0;    // the captions step back while the bar covers them
  // music: "up next" at the top as the song ends: it drops in on a spring and fades
  Fade next_vis_{0, 0, 0.20, 0.24};
  Spring next_drop_ = Spring::rd(0.42, 0.78, 0.05);
  Canvas next_base_;
  uint64_t next_gen_ = 0;
  std::string next_key_;
  Canvas music_base_;
  uint64_t music_gen_ = 0;
  std::string music_key_;
  uint32_t lang_rev_ = 0;  // the interface language the cached words were drawn in
  std::string music_song_;  // title and artist last drawn: a new song's words come in afresh
  void render_music();
  // music: "12 tracks" under the artist opens the album's list beside the cover; the captions
  // rise to the cover's top to make room (list_k_: 0 centred on the cover, 1 at its top)
  Rect music_link_, music_link_local_;
  bool link_hot_ = false;
  Spring list_k_ = Spring::rd(0.34, 0.9, 0.004);
  bool music_list_ok() const;
  MusicView view() const;  // the framing, or the plain fit when none was given
  Rect scene_list_rect() const;
  Rect beside_list_rect() const;
  Rect beside_list_rect_for(const MusicView& V) const;
  double beside_scale_for(const MusicView& V) const;
  MusicView target_view() const;
  Rect over_list_rect() const;
  Rect centre_list_rect() const;
  Rect centre_list_area() const;
  double beside_scale() const;
  double over_scale() const;
  bool list_over() const;  // music's list goes over the cover (the words' column cannot hold it)
  mutable ListLayout list_lay_;
  bool list_header_ = false;  // the list's header composition is up (or the list closing from it)  // the last one: what the words and the bar glide back from as the list closes
  double list_mix() const { return ease_out(std::clamp(list_k_.x, 0.0, 1.0)); }
  bool scene_list() const;
  double scene_list_scale() const;
  // music's bar is narrower than a film's; the width eases between the two
  PolishedMotion music_w_motion_;
  double music_w_ = 0;
  bool music_w_init_ = false;
  bool drop_opened_ = false;  // the list was opened by files being dragged in
  bool panel_dirty_ = true;
  Canvas panel_base_;

  // Only track rows scroll; their original labels and stable IDs are retained.
  double cc_scale_=2;
  int cc_first_=1,cc_count_=0,cc_row_h_=36;
  Rect cc_viewport_,cc_bar_,cc_thumb_;
  Canvas cc_list_;
  Spring cc_scroll_{40,1,.05};
  bool cc_drag_=false;
  double cc_grab_=0;
  void layout_sub_list();
  void render_sub_list();
  void sub_scrollbar();
  double cc_scroll_max() const;
  void sub_ensure_visible(int row,bool instant=false);

  // subtitle drag
  bool sub_drag_ = false, sub_hover_ = false;
  double sub_start_pos_ = 0;
  int sub_start_y_ = 0;
  Spring sub_spring_{20, 0.9, 0.02};
  bool sub_settling_ = false;

  // toast
  std::string toast_text_;
  const char* toast_icon_ = nullptr;
  bool toast_icon_after_ = false;
  double toast_t0_ = -10;
  double flap_t0_ = -10;  // the toast's letters are turning over (announce_audio)
  bool toast_dirty_ = false;
  Canvas toast_base_;
  double toast_hold_ = 1.1;      // seconds at full opacity
  std::string next_name_;
  Canvas next_picture_;
  bool next_toast_ = false;
  bool toast_btn_ = false;       // has the folder button
  bool toast_hot_ = false;       // the cursor is on that button
  double toast_alpha_ = 0;
  Rect toast_rect_, toast_btn_rect_;  // window coords / toast-local
  bool toast_hit(int x, int y, bool* on_btn) const;

  // back to the home: a small arrow in the top-left corner, with the rest of the UI
  Fade back_vis_{0, 0, 0.14, 0.30};
  double back_hov_ = 0;
  bool back_hot_ = false;
  std::string back_sig_;
  Rect back_rect() const;
  bool back_hit(int x, int y) const;

  // tooltip on the timeline
  bool tip_on_ = false;
  std::string tip_text_;
  double tip_t_ = 0;
  Spring tipx_{55, 1.0, 0.3};    // the preview glides after the cursor, ~35 ms behind, no wobble
  Fade tipvis_{0, 0, 0.08, 0.09};
  std::string tip_ts_;           // last time shown, kept while it fades out
  bool tip_want() const;
  void tip_size(int& W, int& H) const;

  // "legenda" switch in the CC panel
  Spring sw_ = Spring::rd(0.3, 0.66, 0.01);

  // layout + helpers
  void layout_dock();
  Rect dock_rect() const;  // physical, current animated position
  void anchor_pos(DockAnchor a, double& x, double& y) const;
  void place_target(double& x, double& y) const;
  int hit_dock(int x, int y) const;  // window coords
  int dock_btn_at(int lx, int ly) const;
  void render_dock();
  void render_panel();
  void compose(double now);
  double time_at(int x) const;
  void seek_scrub(int x, bool final);
  void set_volume_from(int x);
  void activate(int btn, double now);
  std::vector<int> focus_order() const;

  void open_panel(Panel p, int btn, double now);
  void close_panel();
  void build_rows();
  // Settings: what Cue keeps between files (the light, smoothing, the interface's size and
  // style, the language), each shown at work in a small picture at the top of the panel as the
  // pointer (or the keyboard) rests on it, with a line that says what it does
  enum Demo { D_AMBIENT, D_SMOOTH, D_SIZE, D_BG, D_LANGUAGE, D_LOOK, D_COLOR, D_TILT, D_ABOUT };
  int demo_ = D_AMBIENT;       // what the picture shows (the last setting pointed at)
  int demo_seg_ = -1;          // a segment under the pointer (size, background): shown before it is taken
  double demo_t0_ = 0;         // when the picture changed (it fades in)
  int demo_frame_ = -1;        // the moving pictures are drawn at 30 frames a second
  bool settings_compact_ = false;  // a short window: no picture, only the settings
  bool settings_side_ = false;     // a short, wide window: the picture beside the rows, on the left
  bool settings_fold_ = false;
  bool settings_tight_ = false;    // shorter still: the groups closer together (settings_fold_: the interface one row deeper)
  // the layouts Settings tries, in order, until one fits (see kSettingsStages): with the examples,
  // the picture on top, beside the rows, then the rows closing up, and only then no picture
  int settings_stage_ = 0;
  double panel_fit_ = 0;             // > 0: a menu's scale stepped down so it fits whole (see layout_rows)
  bool panel_fit_wanted() const;
  double panel_draw_s() const;       // the open menu's scale: panel_s(), or smaller to fit
  int panel_room_h() const;
  void settings_stage(int stage);  // sets the four flags for a stage
  void settings_restart();         // decided afresh: the first stage for the examples' choice
  bool lang_from_settings_ = false;  // the language list was opened from Settings (back goes there)
  void settings_demo_track(double now);
  void render_preview(Canvas& c, const Rect& box, double s, bool tall = false);  // tall: beside the rows, the whole column
  std::array<Spring, 96> tsw_{};  // each switch in a menu glides on its own
  bool switch_on() const { return panel_ == Panel::More || panel_ == Panel::Settings ? in.ambient : panel_ == Panel::Context ? in.cover_tilt : (in.sid != 0 && in.sub_visible); }
  void layout_rows();
  int slider_label_w() const;
  int slider_value_w() const;
  int seg_inset() const;
  bool seg_inset_on_ = false;
  bool icon_grid() const;
  Rect panel_rect() const;
  int chrome_room() const;   // below the window's own buttons
  Rect chrome_rect() const;
  int hit_panel(int x, int y, int* part) const;
  void panel_click(int row, int part, int x, double now);
  void panel_adjust(int row, int dir, double now);
  void slider_set(int row, int x, bool live);
  bool selectable(const Row& r) const;
  void set_speed(double v);  // rounded to 0.05, 0.25..3; tells the engine and redraws
  void style_changed(bool live);

  Rect sub_rect() const;              // where the subtitle is drawn right now
  Rect sub_rect_at(double pos) const;
  Rect sub_band() const;
  bool sub_hit(int x, int y) const;  // over the subtitle's actual lines (where the hand appears)
  bool sub_line_rects(double pos, std::vector<Rect>& out) const;  // false when the layout is not known
  mutable int style_y_ = -1, style_x_ = 0;
  void update_sub_layers();
  double snap_sub(double pos) const;
  double last_lift_ = -1;
  EaseOutMotion subtitle_lift_;  // moves with the dock: starts on the same tick, lands with it
  bool sub_was_shown_ = false;
  double wanted_subtitle_lift() const;
  mutable bool lift_cache_valid_=false;
  mutable SubStyle lift_style_;
  mutable Rect lift_dock_;
  mutable int lift_w_=0,lift_h_=0,lift_lines_=0,lift_chars_=0;
  mutable double lift_scale_=0,lift_value_=0;
  mutable std::vector<std::string> lift_text_;

  int iris_ = 0;  // 0 none, 1 covered (waiting for the picture), 2 opening
  double iris_t0_ = 0, iris_drawn_ = -1;
  int iris_frame_ = -1;
  void render_iris(double k);
};
