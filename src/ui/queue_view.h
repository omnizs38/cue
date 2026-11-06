// A temporary, scrollable queue. Stable IDs survive edits and async thumbnails.
// Layout: a header (title, count, close), the list, and a footer whose actions add to
// the end of the list or clear it. Dropping files on the window while it is open adds them.
#pragma once
#include "anim.h"
#include "gfx.h"
#include <deque>
#include <unordered_set>

struct QueueEntry { uint32_t id; std::string name; bool current; bool audio=false; };
class QueueView {
 public:
  enum class Action { None, Close, Add, Folder, Clear, Play, Remove, Move };
  struct Event { Action action=Action::None; uint32_t id=0; int index=0; };
  static constexpr int kHeader=32, kFooter=28, kRow=46;  // art px
  static constexpr int kRowCompact=24;  // the mini player's card: one line per song (art px)
  // The panel height (art px) that shows `count` rows, before the window clamps it.
  static int natural_height(size_t count,size_t max_rows=6) { return kHeader+kFooter+2+(count?(int)std::max<size_t>(2,std::min(max_rows,count+1))*kRow:64); }
  void sync(const std::vector<QueueEntry>& items,uint64_t revision);
  void layout(int width,int height,double scale);
  void opened();
  void closed();
  int height() const { return h_; }
  static std::string count_suffix(size_t count) { return count>1?" · "+std::to_string(count):""; }
  static std::string display_name(const std::string& name);
  void cancel();
  bool tick(double dt);
  void draw(Canvas& c,bool paused);
  void move(int x,int y);
  void down(int x,int y);
  Event up(int x,int y);
  Event key(const std::string& key,bool shift,bool ctrl);
  void wheel(double direction);
  bool held() const { return pressed_!=-1; }
  bool dragging() const { return drag_id_!=0; }
  bool over_grip() const { return mx_<int(grip_w()*s_)&&hit(mx_,my_)>=0; }
  uint32_t thumbnail_request() const;
  void thumbnail(uint32_t id,const std::vector<uint32_t>& pixels);
  bool album() const { return album_; }
  // Bare: only the rows, on no sheet, with no title or footer (music's list beside the cover,
  // where the song's own title stands above it)
  void set_bare(bool bare) { if(bare!=bare_) {bare_=bare;frame_s_=0;reshape();} }
  bool bare() const { return bare_; }
  // Compact (bare only): the mini player's card. One line per song at the card's own type size: a
  // small cover, the title, the artist dimmed beside it ("Artist - Title" file names are read as
  // such), so the list reads like the card's words instead of a smaller copy of the full list
  void set_compact(bool compact);
  bool compact() const { return compact_; }
  // (one more row: the list's last line adds songs, by a click or by dropping files on it)
  // (an empty list: its line that adds songs alone, over the footer)
  static int bare_height(size_t count,size_t max_rows,bool compact=false) { return (int)(count==0?1:std::max<size_t>(2,std::min(max_rows,count+1)))*(compact?kRowCompact:kRow)+4+(compact?0:kFooter); }
  // "Artist - Title" (a file name or a tag line): the title and who, when the name says; else the
  // whole name is the title and the artist is empty
  static void split_artist_title(const std::string& name,std::string& title,std::string& artist);
  // a title without the uploads' labels at its end ("(audio)", "[Official Video]", "(320 Kbps)"):
  // the card's narrow rows give the room to the song's own name (the file keeps its name)
  static std::string plain_title(std::string title);
  // Files are being dragged over the window: the add row says "drop to add" and comes into view.
  void set_dropping(bool on) { if(on!=dropping_) {dropping_=on;if(on)scroll_.target=max_scroll();} }
  bool dropping() const { return dropping_; }
 private:
  // y: list position. enter: arrival fade. fresh: a short highlight on rows that were just added.
  struct Item { QueueEntry entry; Spring y{38,1,.05}; double hover=0, enter=1, delay=0, fresh=0; };
  // `image` is the 84x48 source; `scaled` is it at the row's size, kept only while the row is
  // on screen, so scrolling copies whole rows instead of resampling every pixel each frame.
  struct Picture { uint32_t id; Canvas image, scaled; double fade=0; uint64_t used=0; bool drawn=false; };
  // A removed row's last pose. It slides toward its remove button while it fades.
  struct Ghost { Canvas image; int y=0; double age=0, duration=.16; };
  std::vector<Item> items_;
  std::deque<Ghost> ghosts_;
  Canvas row_image_;
  PolishedMotion height_motion_;
  int desired_height_=0;
  bool open_=false,last_paused_=false;
  bool album_=false;  // every row is music: square covers and a tighter text column
  bool bare_=false;
  bool compact_=false;
  bool dropping_=false;
  double drop_k_=0,drop_t_=0;  // the drop invitation's fade and its slow breath
  Rect add_row() const;        // the row after the songs, in panel coordinates
  bool add_row_on() const;
  void draw_add_row(Canvas& list);
  void draw_footer(Canvas& c,bool rules);
  double count_age_=1,empty_age_=1;
  std::string old_count_;
  void reshape();
  std::deque<Picture> pictures_;
  std::unordered_set<uint32_t> failed_;
  uint64_t revision_=~uint64_t(0),clock_=0;
  int w_=0,h_=0; double s_=1;
  Rect viewport_,bar_,thumb_;
  Canvas list_;
  Canvas frame_;  // the panel's background at its current size; scrolling only copies it
  double frame_s_=0;
  Spring scroll_{40,1,.05};
  // While dragging: the lifted row's pose (answers at once, then settles) and the empty
  // slot it will land in, which glides between positions instead of jumping.
  double lift_=0;
  double control_hover_[4]={0,0,0,0};  // add, folder, clear, close
  Spring slot_{44,.9,.05};
  int mx_=-1,my_=-1,pressed_=-1,down_x_=0,down_y_=0,drop_=0;
  uint32_t drag_id_=0,focus_=0,pressed_id_=0;
  double grab_=0,scroll_grab_=0;
  bool keyboard_=false;
  int control_=0;
  int row_h() const { return std::max(1,(int)std::lround((compact_?kRowCompact:kRow)*s_)); }
  int grip_w() const { return compact_?8:22; }  // art px from the row's left where a press only drags
  double max_scroll() const;
  int hit(int x,int y) const;
  int index(uint32_t id) const;
  int current_index() const;
  void ensure_visible();
  void reveal(int first,int last);
  void targets();
  void scrollbar();
  Rect button(int n) const;
  void draw_item(Canvas& c,Item& item,int y,bool floating,bool paused,int current);
  void draw_compact(Canvas& c,Item& item,int y,bool floating,bool paused,int current);
  Rect cover_box(const QueueEntry& entry,int y) const;  // where a row's cover or preview goes
  void draw_picture(Canvas& c,const QueueEntry& entry,const Rect& image,bool played);
};
