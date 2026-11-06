#pragma once
#include <windows.h>

#include <array>
#include <atomic>
#include <climits>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "../core/store.h"
#include "../core/playback_warmup.h"
#include "../core/preview_session.h"
#include "../core/overlay_buffers.h"
#include "../core/frame_cache.h"
#include "../core/episode_advance.h"
#include "../core/queue.h"
#include "../core/media_geometry.h"
#include "../ui/home.h"
#include "../ui/onboarding.h"
#include "../ui/play.h"
#include "../ui/atmosphere.h"
#include "ambient.h"
#include "flight_ghost.h"
#include "cover_turn_window.h"
#include "mpv.h"
#include "thumbhost.h"
#include "platform.h"
#include "vsync.h"
#include "updater.h"
#include "remote_server.h"
#include "file_thumbnail.h"
#include "cover_image.h"
#include "single_instance.h"
#include "media_controls.h"
#include "../ui/music_light.h"
#include "remote_scene.h"
#include "taskbar_buttons.h"
#include "overlay.h"
#include "../ui/news_view.h"

// cue.log: opened (and cleared when older than 5 days) at start; buffered lines reach the disk
// together. crashing: from the crash handler (never waits on the lock).
void cue_log_open(const std::filesystem::path& dir);
void cue_log_flush(bool crashing = false);

class App {
 public:
  void set_instance(SingleInstance* s) { instance_ = s; }  // released as soon as everything is saved, on leaving
  bool init(HINSTANCE inst, const std::vector<std::string>& args, int show);
  int run();

 private:
  enum class Mode { Home, Play };
  enum : UINT { WM_APP_MPV = WM_APP + 1, WM_APP_ENGINE = WM_APP + 2, WM_APP_RECENTS = WM_APP + 3, WM_APP_THUMB = WM_APP + 4, WM_APP_MOUSE = WM_APP + 5, WM_APP_OPEN_FILES = WM_APP + 6, WM_APP_LEFT_DOWN = WM_APP + 7, WM_APP_LEFT_UP = WM_APP + 8, WM_APP_RIGHT_UP = WM_APP + 9, WM_APP_VSYNC = WM_APP + 10, WM_APP_UPDATE = WM_APP + 11, WM_APP_REMOTE = WM_APP + 12, WM_APP_QUEUE_CANCEL = WM_APP + 13, WM_APP_WHEEL = WM_APP + 14, WM_APP_MEDIA_BUTTON = WM_APP + 15 };
  enum : UINT_PTR { T_ANIM = 1, T_POLL = 2, T_WAKE = 3, T_SAVE = 4, T_THUMB = 5, T_PERSIST = 6, T_GEOM = 7, T_PREFETCH = 8, T_FINE = 9, T_MAINTENANCE = 10, T_REMOTE = 11, T_REMOTE_START = 12, T_NEXT = 13, T_NEWS = 14, T_FOLDER = 15, T_QUEUE = 16 };

  HINSTANCE inst_ = nullptr;
  HWND hwnd_ = nullptr, video_ = nullptr;
  Mpv mpv_;
  bool mpv_ok_ = false;
  std::atomic<bool> mpv_wake_pending_{false};
  void wake_mpv();
  std::string engine_err_;
  Store store_;
  std::filesystem::path state_path_, data_dir_;
  HomeView home_;
  OnboardingView ob_;      // first run: shown in the home's place until finished
  bool onboarding_ = false;
  void on_onboarding(ObAction a);
  PlayView play_;
  Canvas home_canvas_;      // what is on screen (WM_PAINT copies from it)
  Canvas home_back_;        // the next frame is drawn here, then compared and swapped
  void present_home(double t);
  Mode mode_ = Mode::Home;
  double s_ = 2;
  UINT dpi_ = 96;
  int cw_ = 0, ch_ = 0;
  bool fullscreen_ = false;
  bool fullscreen_transition_ = false;
  int fullscreen_stage_ = 0, osd_w_ = 0, osd_h_ = 0;
  double fullscreen_changed_at_ = 0;
  void perform_fullscreen_switch();
  bool step_fullscreen_transition(double t);
  uint64_t pause_request_ = 0;
  bool pause_pending_ = false, pause_target_ = false;
  void request_pause(bool paused);
  DWORD corner_preference_ = DWORD(-1);
  WINDOWPLACEMENT wp_{sizeof(WINDOWPLACEMENT)};
  LONG_PTR saved_style_ = 0;
  DropTarget* drop_ = nullptr;

  // playback session
  std::string path_;
  bool loaded_ = false;
  double loaded_at_ = 0;
  double resume_toast_ = -1;
  std::vector<std::string> pending_subs_;
  std::string pending_open_;
  std::vector<std::string> launch_files_;
  int ext_subs_ = 0;
  double last_time_ = 0;
  bool seek_in_flight_ = false;
  double seek_pending_ = -1;
  // After a seek the engine can still report the old time for a while (music rebuilds its picture
  // around the cover first): the bar keeps showing where it was sent until the engine agrees.
  double seek_hold_ = -1, seek_hold_until_ = 0;
  int open_failures_ = 0;
  bool taskbar_seen_play_ = false;  // queued files in a row that would not open (skipping stops after three)
  void hold_seek(double target);
  bool cursor_autohide_ = true;
  POINT last_cursor_{-1, -1};
  bool anim_on_ = false;
  double last_tick_ = 0, home_paint_at_ = -1;
  FrameCadence animation_cadence_;
  VsyncPacer pacer_;              // display-locked ticks (SetTimer only as a fallback)
  // the mini player: a small window in a corner of the screen, above everything; the window's
  // place, size and pin before it are given back when it ends (and saved instead of the mini's)
  // the picture's helpers: super resolution (decided when a film opens) and smoothing
  double video_fps_ = 0;
  std::string smooth_logged_;  // the last smoothing line written to cue.log
  bool smooth_on_ = false;     // smoothing is on for this film (it needs display-resample timing)
  bool sync_decided_ = false;  // this film's timing has been chosen (decide_window)
  std::string sync_sent_;      // the film's timing as last sent (file-local, reset per file)
  void send_sync();
  bool vsr_on_ = false;
  void apply_smooth();
  bool mini_player_ = false;
  bool mini_was_pinned_ = false;
  bool wing_sent_ = false;
  double words_w_seen_ = -2;    // the words' width the wide composition was last centred on      // the card's wings-out pass was asked for
  bool resizing_ = false;       // an edge is held (a move alone keeps the light around the window)
  bool mini_drag_ = false;      // the mini window is being moved by the person (size held)
  SIZE mini_drag_size_{};
  POINT mini_grab_{};
  double mini_drops_ = 0;
  int engine_warns_ = 0;        // engine warnings written to cue.log for this file       // late or dropped frames when the mini player began (cue.log)
  RECT mini_keep_place(double ar) const;
  SIZE mini_client(bool smallest) const;
  void mini_settle();
  // the flight into the mini player and back, in three steps: 0 the picture breaks into pixels
  // where it is (and, out of the mini player, flies toward its old place), 1 out of sight while
  // the window takes its new size, 2 back in sight, landing (into the corner) as it comes sharp
  // ghost: the flight is a live picture of the window (FlightGhost) that shrinks or grows from
  // where the window was to where it lands, the window already there out of sight; from, to: the
  // picture's rects on screen. Without a ghost, the plain flight (out of sight while it changes
  // size, then a spring into the corner)
  struct MiniFx { bool on = false, cloaked = false, entering = false, ghost = false; int stage = 0; double t0 = 0, rise = 0.14, dur = 0.45, hold = 0.08; RECT to{}, from{}, gto{}; } mini_fx_;
  FlightGhost ghost_;
  StageCover stage_cover_;  // black over the screen while full screen changes the window's size
  RECT client_screen() const;  // the client area on screen
  bool cloaked_ = false;
  void set_cloak(bool on);
  struct SeekFade { int stage = 0; double t0 = 0; std::vector<std::string> cmd; } seek_fade_;  // 1 out, 2 jumped, 3 in
  void seek_soft(std::vector<std::string> cmd);
  bool step_seek_fade(double t);
  bool start_mini_fx(bool entering);
  void end_mini_fx(bool apply = true);  // apply: a switch still pending happens now (else it is dropped)
  void mini_switch(bool entering);  // the mini player on or off, the window at its new size  // false: no pixels (Windows' animations off, another effect running)
  bool step_mini_fx(double t);
  bool keep_music_window_ = false;  // a song after a song: the window keeps the person's size
  WINDOWPLACEMENT mini_restore_{sizeof(WINDOWPLACEMENT)};
  void enter_mini();
  void exit_mini(bool animate);
  RECT mini_rect(double ar, int corner) const;
  RECT mini_place(int cw, int ch, int corner) const;  // corner: 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right
  int mini_corner() const;                      // the screen corner the mini window is nearest
  double mini_aspect() const;
  void set_pin(bool on);
  bool was_iconic_ = false;  // the last WM_SIZE minimized the window
  double click_activated_t_ = 0;  // when a mouse click last activated the window (0: not by a click)
  double play_interval_ = 1.0 / 60;  // overlay uploads: a divisor of the refresh rate, at most ~90 Hz
  void refresh_display_rate();
  void arm_animation_timer();
  LARGE_INTEGER qpf_{}, qp0_{};

  // One upload in flight per layer. mpv copies the pixels when it executes overlay-add, which
  // can be a while after we queue it: the buffer it points at must stay untouched until its
  // reply arrives. Never reused, resized or freed before that.
  // Two buffers: the one the engine shows (it may keep reading it) and the one being sent.
  using Ov = OverlayBuffers;
  bool overlays_pending() const;
  void update_blur_cloud();
  std::string cloud_options_;
  FrameCadence cloud_cadence_;
  bool cloud_inflight_ = false;
  bool cloud_pending_ = false;
  std::array<Ov, PlayView::L_COUNT> ov_;

  static LRESULT CALLBACK wndproc(HWND, UINT, WPARAM, LPARAM);
  static LRESULT CALLBACK videoproc(HWND, UINT, WPARAM, LPARAM);
  LRESULT handle(UINT, WPARAM, LPARAM);

  double now() const;
  // The engine loads on its own thread after the shell's first paint (to avoid
  // loader-lock stalls during window creation). The UI thread touches mpv_ only after
  // WM_APP_ENGINE arrives (mpv_ok_), so nothing is shared while the worker writes.
  struct EngineCfg {
    int64_t wid = 0;
    std::string slang, alang;
    double volume = 100;
    bool muted = false, log = false;
  } engine_cfg_;
  HANDLE engine_thread_ = nullptr, engine_go_ = nullptr;
  bool engine_log_ = false, engine_failed_ = false;  // --log: routine steps too (see trace)
  bool maintenance_started_ = false;
  double open_started_ = 0;
  static DWORD WINAPI engine_main(void* self);
  bool start_engine();  // worker: load, create, set options, initialize
  void on_engine_ready(bool ok);
  void install_user_fonts();
  void start_maintenance();
  void write_input_conf(const std::wstring& path);
  void drain_mpv();
  void on_property(uint64_t id, const mpv_event_property* p);
  void on_client_message(const mpv_event_client_message* m);
  void on_file_loaded();
  void refresh_file_info();
  void configure_audio_mode();
  bool audio_only_ = false, waveform_active_ = false, waveform_fallback_ = false;
  // music with a cover: the engine's cover track, whether the cover look is on screen, whether
  // it failed once for this file (then the spectrum alone), and whether the shader still has it
  int64_t cover_vid_ = 0;
  MusicScene music_scene_;  // the cover look's composition for this song (wide or tall)
  void music_album_queue();
  void music_fit_window();
  const MediaQueue::Item* queue_following() const;
  // shuffle (music): the deck of songs still to come this round, and the way back
  bool shuffle_on() const;
  void shuffle_reset();
  const MediaQueue::Item* shuffle_previous() const;
  mutable ShuffleDeck shuffle_;
  bool hidden_ = false;   // covered by another app's full-screen window (or on another desktop)
  int hidden_poll_ = 0;
  bool covered() const;
  void set_hidden(bool h);
  bool hidden_now() const { return hidden_ || IsIconic(hwnd_); }
  void update_music_idle();  // a film under a full-screen window: frames timed by the sound
  bool poll_slow_ = false;    // minimized: the poll at 4 Hz
  bool film_idle_ = false;    // a film under a full-screen window: video-sync=audio
  // a song opened again (a new look): its sound fades out and back in through the engine's own
  // gain (volume-gain, dB), never the person's volume, so the reopen is a breath, not a cut
  double switch_gain_ = 1;
  void set_switch_gain(double g);
  void reopen_music();
  void reopen_song();           // the same song opened again at the same moment (behind pixels)
  bool cover_graph_live() const { return audio_only_ && waveform_active_ && cover_art_ && cover_vid_ > 0; }
  MusicFrame music_frame_ = MusicFrame::Scene;
  MusicView view_from_;
  double view_t0_ = -1;           // a glide between two framings, when it began
  MpvFraming framing_sent_{1e9, 0, 0};
  MusicView music_view_target() const;
  bool step_music_view(double t);          // a new look or composition for the playing song
  bool music_reopening_ = false;  // the load in progress is that same song, opened again
  bool music_intro_ = false;
  bool last_was_music_ = false;   // the file before this one was a song (a song that follows keeps the UI)      // the next playback start reveals a new song through the pixels
  TaskbarButtons taskbar_;
  UINT taskbar_created_ = RegisterWindowMessageW(L"TaskbarButtonCreated");
  void taskbar_sync();
  void taskbar_command(UINT id);
  // cover_scene_: the music scene is on (either look: the words, the list, the framing);
  // cover_art_: it shows a cover (the file's, or Cue's tile), which the shader lifts and tilts
  bool cover_scene_ = false, cover_art_ = false, cover_fallback_ = false, cov_sent_ = false;
  int music_look_on_ = 0;  // the look the playing song was built with
  int64_t visual_cover() const { return cover_art_ ? cover_vid_ : 0; }
  void set_music_look(int look);
  void on_end_file(const mpv_event_end_file* e);

  void open(std::string path, std::vector<std::string> subs = {});
  void handle_files(const std::vector<std::string>& files);
  MediaQueue queue_;
  void queue_refresh();
  void queue_clear();
  void queue_notice(const std::string& text,int result=0);
  std::string queue_notice_;
  uint64_t queue_notice_id_=0;
  int queue_result_=0;
  void queue_thumbnail_tick();
  std::shared_ptr<FileThumbnailJob> queue_thumb_job_;
  uint32_t queue_thumb_id_=0;
  bool queue_thumb_timer_=false;
  size_t queue_add(const std::vector<std::string>& paths, bool replace = false);
  void queue_play(uint32_t id);  // (in the card, through the cover's turn: flip_)
  struct FolderScan {
    std::atomic<bool> cancelled{false}, done{false};
    std::string path;
    bool replace=false, remote=false, limited=false, failed=false;
    std::vector<std::string> files;
  };
  std::shared_ptr<FolderScan> folder_scan_;
  static DWORD WINAPI scan_folder(void*);
  void queue_folder(const std::string& path, bool replace, bool remote = false);
  void finish_folder();
  void go_home();
  void refresh_recents(bool check = true);
  void resolve_props();
  // timeline previews
  ThumbClient thumbs_;  // previews + film colour, decoded in a background helper process
  std::string thumb_path_;
  PreviewSession preview_;
  double thumb_want_ = -1, thumb_seek_t_ = -1;
  int thumb_shown_ = INT_MIN;
  using ThumbImg = FrameCache::Image;
  FrameCache thumb_cache_;  // on-demand timeline frames, bounded by bytes and count
  double thumb_step() const;
  int thumb_key(double t) const;
  bool first_frame_ = false, reveal_pending_ = false, swallow_next_ = false;
  void reveal_play();
  void try_start_iris();
  void thumb_request(double t);  // t < 0: just keep filling in the background
  void thumb_next();
  void thumb_store(std::vector<uint32_t>&& px, int w, int h);
  void on_thumb(ThumbMsg* m);
  void thumb_stop();
  unsigned recents_gen_ = 0;
  std::set<std::wstring> missing_;
  std::vector<std::string> shown_paths_;
  std::string observed_path_;
  bool has_background_box_ = false;
  void save_state();   // debounced: sliders and drags do not write on every step
  void save_now(bool synchronous = false);
  HANDLE save_thread_ = nullptr;
  bool tracks_applied_ = false;
  void apply_media_tracks();
  void save_frame(bool with_subs);
  std::wstring last_frame_;  // the frame saved last, for the toast's folder button
  bool eof_ = false;         // paused on the last frame (play starts over)
  void set_ui_scale(int v);
  void apply_language(bool relayout);  // resolve and apply the interface language
  void set_language_pref(int lang);    // -1 automatic (follow Windows), else i18n::Lang
  // updates from GitHub Releases: checked every 3 days in the background, or when asked
  enum class Upd { Idle, Checking, Available, Downloading, Launching };
  Upd upd_ = Upd::Idle;
  ReleaseInfo upd_rel_;           // empty url: only the version is known (from a previous run)
  bool update_auto_attempted_=false, update_check_pending_=false;
  bool upd_manual_ = false, upd_install_after_check_ = false;
  int upd_pct_ = -1;
  void check_updates(bool manual);
  void start_update();
  void on_update(UpdateMsg* m);
  void show_update_state();
  void say(const std::string& text);  // a short message wherever the person is (home or player)
  // the film's atmosphere: sampled from the preview engine (never the playback one)
  Ambient ambient_;
  ColorSmoother accent_sm_, zone_sm_[FrameColors::kZones];
  Rgb accent_target_{255, 255, 255}, zone_target_[FrameColors::kZones]{};
  double accent_k_target_ = 0, zone_light_target_[FrameColors::kZones]{};
  double accent_want_ = -1, accent_req_t_ = -10;
  double accent_media_t_ = -1;
  bool thumb_for_accent_ = false;
  // music's light show: the engine's loudness for each audio frame (an astats filter on the
  // song's own filter chain), the cover's colours, and the motion they make
  MusicLight music_light_;
  std::shared_ptr<CoverJob> light_cover_job_;
  double bg_ready_t_ = -1;  // when the cover's colours arrived (the room's light fades in from then)
  bool bg_sent_ = false;
  std::shared_ptr<CoverJob> cover_job_;
  // The playing file's folder, listed on a worker (a network share or a sleeping disk can take
  // seconds): up next and a song's album wait for it, the window never does.
  struct FolderList { std::wstring dir; std::vector<std::wstring> names; std::atomic<bool> done{false}; };
  std::shared_ptr<FolderList> folder_list_;
  bool folder_wait_ = false;  // up next / the album asked before the folder was read
  void list_folder(const std::wstring& dir);
  bool folder_names(const std::wstring& dir, std::vector<std::string>& out);
  double mono_k_ = 0;         // music in black and white: 0 colour .. 1 grey, eased
  bool mono_sent_ = false;
  bool step_mono(double dt);
  void set_music_mono(bool on);  // the song's cover inside black bars (from open)
  bool light_meter_ = false;     // the song has the meter (set when it was opened)
  bool light_found_ = false;     // the song has a cover (its accent lights the spectrum-only look)
  Rgb light_accent_{196, 198, 214};
  double light_k_ = 0;           // the show's fade
  Rgb light_pal_[8]{{150, 152, 168}, {150, 152, 168}, {150, 152, 168}, {150, 152, 168},
                    {150, 152, 168}, {150, 152, 168}, {150, 152, 168}, {150, 152, 168}};  // the cover's colours round it
  double light_kick_[4]{};       // the corners' light, kicked in turn by the beats
  int light_rings_ = 0;
  bool lit_sent_ = false;
  // song to song: the room's light keeps the last colours and crosses to the new cover's when
  // they are read (it used to go black and fade in again from nothing)
  bool room_kept_ = false;
  Rgb light_pal_from_[8]{}, light_pal_to_[8]{};
  double pal_t0_ = -1;
  // A song opening after a song: from the request until the engine has read the new file, the
  // picture on screen is still the last song's. Its shader passes (the wings' mask, the room's
  // light, the cover) and its framing stay as they were sent, instead of falling back for that
  // moment to the bare scene: the spectrum's wings, a black room, the picture unframed.
  bool music_gap_ = false;
  double music_gap_t_ = 0;
  std::string music_opts_;          // the music passes' part of the shader options, as last sent
  void begin_music_gap();
  void end_music_gap();
  void frame_plain();               // no framing in the engine (a film, or a song with no picture)
  // the last song's masked wings stay masked until the next song's own picture is up (its first
  // frames come a moment after the engine has read it)
  bool wing_hold_ = false;
  double wing_hold_t_ = 0;
  // A song's change in the card: the cover turns away edge-on as the old song breathes out, the
  // next one opens while it is turned, then turns in from the other side as it starts playing.
  struct CardFlip { int stage = 0; double t0 = 0, from = 0; uint32_t id = 0; } flip_;
  double flip_ry_ = 0;               // the turn (radians), added to the cover's tilt
  bool card_flip_ok() const;
  bool card_drawn() const;
  bool survey_wanted() const;  // the survey's card on the home (see answer_survey)
  void answer_survey(bool yes);   // the card shows a cover Cue draws (the spectrum-alone look)
  bool sync_card_art();
  bool step_card_flip(double t);
  // The turn drawn by Cue itself (cover_turn_window.h): stages 1 away, 2 at the edge and turning
  // in, 3 flat and waiting for the engine's picture, 4 the engine's cover back, 5 fading out.
  // Falls back to the engine's own turn (flip_ry_ alone) when the window cannot be made.
  bool list_side_seen_ = false;      // Centre: the list was open beside the cover at the last frame
  double centre_below_seen_ = -1;    // Centre: the words' height under the cover at the last frame
  CoverTurnWindow turn_;
  bool turn_on_ = false;             // this flip is the window's
  double cov_hide_ = 0;              // the engine leaves the cover out (> 0) and this much of its shadow
  double turn_restart_t_ = -1, turn_t_ = 0;
  RECT turn_cover_{};                // the cover on screen when the turn began (a moved window ends it)
  std::shared_ptr<CoverArtJob> card_art_, next_art_;  // the cover in the card, and the next song's
  std::string card_art_path_;
  uint32_t next_art_id_ = 0;
  struct Ahead { uint32_t id = 0; std::shared_ptr<CoverArtJob> job; } ahead_next_, ahead_prev_;  // the songs either side, read ahead
  void card_art_prefetch();
  bool card_cover_screen(RECT& cover, RECT& clip) const;
  bool step_card_turn(double t);
  void end_card_flip(bool play_pending);  // at once; a song still waiting for the turn opens (or not)
  void queue_play_now(uint32_t id);
  SingleInstance* instance_ = nullptr;
  Rgb backdrop_sent_{0, 0, 0};  // the window's colour around the music's picture (the engine's background-color)
  bool backdrop_known_ = false;
  double veil_sent_ = 0;
  Rect list_cover_seen_{};
  bool edge_sent_ = false;  // the music picture's soft edges toward the window's black
  double light_latency_ = 0.25;  // how far the meter runs ahead of the speakers
  bool step_music_light(double dt);
  // the lock screen's and the volume flyout's media card (Windows' media controls)
  MediaControls media_controls_;
  bool media_attached_ = false;
  std::string media_key_;        // file whose card words were parsed
  bool media_words_audio_ = false;
  std::shared_ptr<CoverJob> media_cover_job_;
  std::shared_ptr<CardStillJob> media_still_job_;
  std::string media_title_, media_sub_;
  double media_still_due_ = -1;  // a film: when to take its still (in memory, see kReplyStill)
  int media_still_tries_ = 0;
  void media_sync();
  PlaybackWarmup background_warmup_;
  bool atmosphere_sampled_ = false;
  bool background_ready(double delay = PlaybackWarmup::kDelay) const;
  bool atmosphere_ready() const;
  void reset_atmosphere(bool keep_music_light = false);
  void sample_atmosphere(const std::vector<uint32_t>& px, int w, int h);
  bool step_atmosphere(double dt);
  double atmo_t_ = 0;
  // the window follows the content: portrait video -> portrait window, landscape -> landscape
  int64_t vid_w_ = 0, vid_h_ = 0, vid_rot_ = 0;
  MediaGeometry media_geometry_;
  bool geometry_query_pending_=false;
  bool geometry_query_dirty_=false;
  void request_video_geometry();
  void video_geometry_reply(const mpv_event* event);
  bool open_timed_ = false;     // the song's opening time logged
  VideoShape early_shape_{};  // the film's shape from its file, before its first picture (0: unknown)
  bool geom_done_ = true;          // this file's adaptation already happened (or is not wanted)
  bool user_sized_ = false;        // the person resized during this media: respect it
  bool auto_portrait_ = false;     // the current portrait shape came from us
  bool in_sizemove_ = false;
  SIZE sizemove_start_{0, 0};
  double pending_ar_ = 0;          // decided while fullscreen: applied on the way out
  SIZE landscape_size_{0, 0};      // last landscape window size, to come back to
  // kind: 0 a shape change, 1 growing into fullscreen, 2 coming back out of it
  struct GeomAnim { bool on = false; RECT from{}, to{}; double t0 = 0, dur = 0.26; int kind = 0; } geom_;
  double geom_end_t_ = -10;    // when the last window glide landed
  double first_frame_t_ = -1;  // when this file's first frame was up
  bool window_settled() const;  // the window has its shape for this file (the iris may open)
  void begin_fullscreen_motion();
  // fullscreen zoom, drawn by the compositor (see begin_fullscreen_motion)
  struct FullscreenZoom {
    bool entering = true, animate = true;
    double t0 = 0, ready_at = -1, video_ar = 16.0/9.0;
    RECT monitor{}, from{}, to{};  // screen coordinates
  } fz_;
  // fullscreen snap: uniforms of the video shader (see step_fullscreen_transition)
  struct Snap {
    bool on = false, was_on = false, subs_hidden = false, subs_were = true;
    double dark = 0, px = 1, gray = 0;
  } snap_;
  // moving the subtitle: the film pauses and goes grey under it (0..1), and plays on after
  // the grey spreads from the subtitle in a ring of pixel blocks; on release the colour comes back
  // the same way from where it was left. The subtitle itself is lifted a little (sub-scale).
  struct SubFx { bool on = false; double t_in = -1, t_out = -1, ax = .5, ay = .5, bx = .5, by = .5; } subfx_;
  Spring sub_lift_ = [] { Spring k = Spring::rd(0.30, 0.58, 0.0004); k.snap(1.0); return k; }();  // sub-scale
  std::string sub_scale_sent_ = "1.000";
  bool sub_edit_resume_ = false;
  bool step_sub_fx(double t, double dt);
  double scale_for(int cw, int ch, int choice) const;  // DPI + window + choice
  double play_scale(int choice) const;                 // the player's: portrait and music framed close raise it
  void rescale();   // a new interface scale: laid out again as for a new size
  void relayout();  // the window's size or scale changed: everything measured in them, music's framing too
  void refresh_ui_scale();
  std::pair<double, double> sub_center_out() const;
  void set_snap(double dark, double px, double gray);
  void snap_subtitles(bool hide);
  void end_snap();
  int logged_fs_stage_ = -1;
  double video_aspect() const;
  void settle_fullscreen_motion();
  void maybe_adapt_window();
  void decide_window();
  std::string video_sync_ = "display-resample", hwdec_;
  bool logged_hw_ = false;
  double hw_log_t_ = 0;
  void adapt_window(double ar);
  void animate_window_to(const RECT& r);
  void step_geom(double t);
  RECT window_rect() const;
  void save_position(bool finished = false);
  void apply_style(bool live = false);
  std::map<std::string, std::string> prop_name_, applied_;
  void run_actions();
  void upload_overlays();
  void remove_overlays();
  void ensure_anim();
  void frame();
  void kick();
  bool fine_timer_ = false;                      // input arrived: draw now, not on the next timer tick
  void on_play_mouse(int x, int y);
  int cursor_kind_ = 0;
  void make_cursors();
  void schedule_wake();
  void toggle_fullscreen();
  void toggle_maximize();
  void begin_window_drag();
  void update_cursor(double t);
  void release_video_window();
  void try_hide_video();
  void shutdown_engine(DWORD max_ms);
  void quit_now();
  bool closing_video_ = false, hide_video_pending_ = false, closing_app_ = false;
  double hide_video_since_ = 0;
  HWND mpv_win_ = nullptr;
  double last_move_t_ = 0;
  void set_title(const std::string& t);
  void set_edge_highlight(bool on);
  void apply_theme_frame();
  void apply_window_corners(bool force = false);
  void key(const std::string& k, bool shift, bool ctrl);
  void mouse_wheel(WPARAM keys,LPARAM screen_point);
  // the phone remote (src/win/remote_server, src/ui/remote_view)
  RemoteServer remote_;
  RemoteScene scene_;
  std::string remote_code_;
  double remote_pair_deadline_ = 0;
  int remote_timer_ = 0;               // 0 off, -1 the end of this episode, else minutes
  double remote_timer_end_ = 0;
  double what_until_ = -1;             // "what did they say?": subtitles shown until here
  int64_t what_sid_ = 0;               // ... then given back as they were
  bool what_vis_ = true;
  double what_restored_at_ = -10;      // its own echo is not the person's choice
  std::shared_ptr<FileThumbnailJob> next_thumb_job_;
  bool next_thumb_requested_ = false;
  bool next_auto_ = false, next_cancelled_ = false;
  int next_count_ = -1;
  bool media_idle_ = false;
  EpisodeAdvance next_advance_;
  bool queue_next_episode();
  std::string next_ep_;                // the next file of this series, or ""
  double remote_toast_at_ = -1000;
  int remote_ticks_ = 0;
  bool remote_start();
  void remote_stop(bool tell_phones);
  void remote_tick();
  void remote_publish();
  void remote_events();
  void remote_command(const remote::Command& c);
  uint64_t remote_queue_revision_=~uint64_t(0),remote_queue_epoch_=0;
  bool remote_path_ok(const std::string& path,bool directory=false) const;
  void remote_boot();
  void remote_roots();
  void remote_progress();
  void remote_default_folders();
  std::vector<RemoteChip> remote_chips() const;
  void open_remote_scene();
  // every audio track at once (a recording in parts: game, microphone, voice chat)
  bool amix_ = false;
  void set_audio_mix(std::vector<int64_t> ids, bool remember);
  std::vector<int64_t> all_audio_ids() const;
  void select_audio(int64_t id);
  bool change_audio_graph(const std::string& graph, int64_t single_track);
  struct AudioSwitch {
    int stage = 0, retries = 0;
    double at = 0, position = 0;
    bool paused = false, dirty = false;
    bool opening_mix = false;  // restore saved video audio with the fullscreen visual transition
    bool reopen = false;  // music: the song is opened again at the same moment, not rebuilt
    std::string graph;
    int64_t track = 0;
  } audio_switch_;
  bool step_audio_switch(double t);
  void audio_switch_restart();
  void cancel_audio_switch();
  int64_t sid_reselect_ = 0;  // a subtitle track taken again after an audio change
  void select_subtitle(int64_t id);
  void subtitle_selection_reply(uint64_t request, int error);
  void set_subtitle_visibility(bool visible);
  void refresh_subtitles();
  int64_t aid_reselect_ = 0;  // a track being chosen again after a mix (its passing "no" is ignored)
  void clear_audio_mix();
  void pick_subs_only(const RecentItem* r);
  int size_wheel_ = 0;  // ctrl + wheel, summed until a notch
  bool pinned_ = false;  // always on top (this session)
  void toggle_pin();
  void say_ui_size();
  // while the pairing scene is up the film waits, in black and white
  bool scene_paused_ = false;
  double gray_ = 0, gray_target_ = 0, gray_last_ = -1;
  int gray_sent_ = 0;
  void scene_gray(double t);
  std::vector<std::string> remote_devices_;  // phones connected now, by name
  double remote_peek_want_ = -1;              // a phone's wheel points here (its preview)
  int remote_peek_sent_ = INT_MIN;
  void remote_peek(double t);
  void remote_peek_send();
  // "saiu do forno": once, after an update, over the home
  Overlay news_host_;
  NewsView news_;
  bool news_due_ = false;
  void show_news(bool asked = false);
  void close_news_line();
  void remote_scene_action(RemoteAction a);
  void find_next_episode();
  void end_what(bool restore);
  void mouse_button(int which, bool down, int x, int y);  // coordinates at the event, in Play mode
  POINT cursor_client() const;
  void log(const std::string& msg);
  void trace(const std::string& msg);  // a routine step: written only with --log
};
