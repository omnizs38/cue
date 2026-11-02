// Previous, play/pause and next under the window's preview in the taskbar, the way music and
// video players offer them. The buttons follow what the dock can do: hidden on the home,
// dimmed where there is no previous or next file.
#pragma once
#include <windows.h>
#include <string>

struct ITaskbarList3;

class TaskbarButtons {
 public:
  enum Command : UINT { kPrevious = 0x7c10, kPlayPause, kNext };
  ~TaskbarButtons();
  // Windows says the taskbar button exists (also after Explorer restarts): (re)attach to it.
  void created(HWND hwnd);
  struct State {
    bool shown = false, playing = false, has_prev = false, has_next = false;
    std::wstring previous, play, pause, next;  // tooltips, in the interface language
    bool operator==(const State&) const = default;
  };
  void sync(const State& s);
  static bool is_click(WPARAM w) { return HIWORD(w) == 0x1800 /* THBN_CLICKED */ && LOWORD(w) >= kPrevious && LOWORD(w) <= kNext; }

 private:
  HWND hwnd_ = nullptr;
  ITaskbarList3* list_ = nullptr;
  bool added_ = false, com_ = false;
  int icon_size_ = 0;
  HICON icons_[4]{};  // previous, play, pause, next
  State state_;
  void make_icons();
  void free_icons();
  void apply(bool add);
};
