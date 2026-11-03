// Windows' own media controls (System Media Transport Controls): what plays shows on the lock
// screen and in the flyout over the volume keys, with its title, artist, cover, timeline and
// previous / play-pause / next. Reached through the window's interop interface and the runtime
// functions loaded from combase.dll, so nothing changes for systems without them.
#pragma once
#include <windows.h>

#include <memory>
#include <string>

class MediaControls {
 public:
  // Buttons pressed there arrive as `msg` posted to `hwnd`, wParam one of these.
  enum Button : WPARAM { kPlay = 0, kPause = 1, kNext = 6, kPrevious = 7 };
  MediaControls();
  ~MediaControls();
  bool attach(HWND hwnd, UINT msg);
  void detach();
  // nothing open: the controls leave the lock screen
  void set_active(bool active);
  void set_playing(bool playing);
  void set_buttons(bool has_prev, bool has_next);
  // a new file: its words and (for a song) its cover, encoded (JPEG, PNG…), possibly empty
  void set_media(bool music, const std::string& title, const std::string& artist, const std::string& cover);
  void set_timeline(double pos, double dur);

 private:
  struct Impl;
  std::unique_ptr<Impl> d_;
};
