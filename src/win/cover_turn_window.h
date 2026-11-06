// The card's turning cover in a see-through window of its own, over the engine's picture.
//
// A song change in the mini player's card turns the cover away and the next one in. The engine's
// shader could only move it when the engine drew a picture: a song's pictures come on the music's
// clock, not the screen's, and around a song change the engine draws nothing for a moment (one
// song closes, the next opens, the engine holds its redraws a little after a start). The turn
// stepped, however its curves were tuned. Here a thread of its own draws the cover (core/
// cover_turn.h) once per screen refresh (DwmFlush), from the clock alone, into a layered window
// that clicks pass through; the UI thread opening the next song, the engine closing the old one,
// none of it can hold the turn back. The engine meanwhile leaves the cover and its shadow out of
// its picture (`cov_hide`) and keeps drawing the room; the shadow is the window's, so it turns with
// the cover even while the engine draws nothing. Where the two hand over (the start, the end), the
// shadows cross-fade over 0.12 s, so a frame's difference between them never shows.
//
// A turn: the old cover, flat, exactly where the engine shows it (shown before the engine is told
// to leave it out, so nothing blinks), away to the edge (ease-in), the next song's cover in from
// the other side (ease-out, back) as soon as it is read (it is read ahead: one movement through the
// edge), flat, held until the engine shows the same cover again, then a short fade.
#pragma once
#include <windows.h>

#include <atomic>
#include <memory>
#include <mutex>

#include "../core/cover_turn.h"

class CoverTurnWindow {
 public:
  ~CoverTurnWindow() { abort(); }
  // cover: the cover on screen (screen px; a square); clip: the window's client area on screen
  // (nothing is drawn past it); from: the cover as the engine shows it. Blocks until the first
  // picture is on screen (at most ~0.1 s); false when no window could be made.
  bool begin(HINSTANCE inst, HWND owner, const RECT& cover, const RECT& clip, std::shared_ptr<const CoverArt> from);
  void set_to(std::shared_ptr<const CoverArt> to);  // the next song's cover, when read
  void go();                                       // the next song has started: it may turn in
  void tilt(double rx, double ry, double k);       // the cover's lean under the pointer (radians)
  // the turn landed and the engine shows the next song: the window's shadow fades out (the engine's
  // fades in, from shadow()); then finish(): the cover fades and the window goes
  void release();
  void finish();                                   // the engine shows the cover again: fade and go
  void abort();                                    // gone at once
  bool on() const { return thread_ != nullptr; }
  bool edge() const;     // the old cover reached the edge: the next song can open
  bool landed() const;   // the next cover is flat
  bool done() const;     // faded out (or ended): abort() just tidies up
  double angle() const;  // the turn now (radians, about the upright axis)
  // how much of the shadow the window draws now (0..1): the engine draws the rest (cov_hide)
  double shadow() const;
  bool shadow_out() const;  // released, and its shadow gone: the engine can show the cover

 private:
  struct State;
  std::shared_ptr<State> st_;
  HANDLE thread_ = nullptr;
  static DWORD WINAPI main(void* p);
};
