// The clock of a short window glide (a film's shape, full screen). A glide starts while the file
// opens, and the window's thread can wait a few frames on the engine there: on a wall clock the
// window then jumped most of the way in one frame. A gap longer than a couple of frames moves the
// start forward by the time lost (minus the one frame that really passed), so a stall is a pause
// and the glide goes on from where it was.
#pragma once

struct GlideClock {
  double t0 = 0, last = -1;
  void start(double t) { t0 = t; last = -1; }
  // the time since the start, as the glide should see it, at t (frame: the display's interval)
  double elapsed(double t, double frame) {
    if (last >= 0 && t - last > 2.5 * frame) t0 += (t - last) - frame;
    last = t;
    return t - t0;
  }
};
