#include "music_light.h"

#include <algorithm>
#include <cmath>

namespace {
double approach(double v, double target, double dt, double tau_up, double tau_down) {
  double tau = target > v ? tau_up : tau_down;
  return tau <= 0 ? target : v + (target - v) * (1 - std::exp(-dt / tau));
}
double smooth(double k) { k = std::clamp(k, 0.0, 1.0); return k * k * (3 - 2 * k); }
}  // namespace

void MusicLight::reset() { *this = MusicLight{}; }

void MusicLight::feed(double now, double rms_db, double zcr, double latency) {
  double level = std::isfinite(rms_db) ? std::pow(10.0, std::clamp(rms_db, -120.0, 0.0) / 20) : 0.0;
  if (!std::isfinite(zcr)) zcr = 0;
  if (!pending_.empty() && pending_.back().at > now + latency) pending_.clear();  // a seek, a new song
  pending_.push_back({now + std::max(0.0, latency), level, std::clamp(zcr, 0.0, 1.0)});
  while (pending_.size() > 256) pending_.pop_front();
}

void MusicLight::step(double now, double dt, bool playing, bool reduced) {
  if (started_ < 0) started_ = now;
  dt = std::clamp(dt, 0.0, 0.25);
  while (!pending_.empty() && pending_.front().at <= now) {
    level_ = pending_.front().level;
    zcr_ = pending_.front().zcr;
    have_ = true;
    pending_.pop_front();
  }
  const double before = f_.energy, before_s = f_.sparkle, before_turn = f_.turn, before_b = f_.breath;
  double target = 0.3, spark = 0;
  if (playing && have_) {
    // the song's own scale: its loud parts reach the top, quiet songs still breathe
    peak_ = std::max({level_, peak_ * std::exp(-dt / 6.0), 0.003});
    double e = std::clamp(level_ / peak_, 0.0, 1.0);
    target = std::pow(e, 1.4);
    fast_ = approach(fast_, e, dt, 0.015, 0.11);
    slow_ = approach(slow_, e, dt, 0.45, 0.45);
    // verse or chorus is read on the true level (the gain above would hide it)
    if (long_ <= 0) mid_ = long_ = level_;
    mid_ = approach(mid_, level_, dt, 6.0, 6.0);
    long_ = approach(long_, level_, dt, 30.0, 30.0);
    zavg_ = approach(zavg_, std::max(zcr_, 0.001), dt, 4.0, 4.0);
    // a beat: the quick envelope jumps over the slower one; never more than three a second
    if (!reduced && fast_ > slow_ * 1.28 + 0.04 && now - last_ring_ >= 1.0 / kMaxRingsPerSecond) {
      double a = std::clamp(0.22 + (fast_ / std::max(slow_, 0.05) - 1) * 0.35, 0.18, kMaxWave);
      f_.waves.push_back({0, a * (0.75 + 0.25 * f_.section)});
      last_ring_ = now;
      ++rings_;
    }
    // a chorus: the last few seconds are clearly louder than the half minute before
    bool was = chorus_;
    if (!chorus_ && mid_ > long_ * 1.12 && mid_ > 0.01) chorus_ = true;
    else if (chorus_ && mid_ < long_ * 1.02) chorus_ = false;
    if (chorus_ && !was && !reduced) { turn_from_ = turn_target_; turn_target_ += 0.5; turn_t0_ = now; }
    spark = std::clamp(0.12 + (zcr_ / zavg_ - 1) * 1.4, 0.0, 1.0) * target;
  }
  if (reduced) {
    // only a slow breath
    f_.energy = approach(f_.energy, 0.3 + 0.4 * target, dt, 0.6, 1.4);
    f_.waves.clear();
    spark = 0;
  } else {
    // a slow swell, not a flash: the light follows the music's body, not each hit
    f_.energy = approach(f_.energy, target, dt, 0.4, 1.1);
  }
  // breathing: one slow cycle, 6.5 s in quiet passages down to 4 s when the music is full, and
  // deeper as it fills; it fades out over a couple of seconds when the music stops, and only
  // keeps a long shallow breath for reduced motion
  {
    const double period = reduced ? 8.0 : 6.5 - 2.5 * f_.energy;
    const double deep = !playing ? 0.0 : reduced ? 0.35 : 0.45 + 0.55 * f_.energy;
    depth_ = approach(depth_, deep, dt, 1.2, 1.6);
    if (!playing && depth_ < 0.01) depth_ = 0;  // at rest: still
    if (depth_ > 1e-3) phase_ = std::fmod(phase_ + dt * 6.283185307179586 / period, 6.283185307179586);
    f_.breath = depth_ < 1e-3 ? 0.0 : depth_ * std::sin(phase_);
  }
  f_.section = approach(f_.section, chorus_ && playing ? 1.0 : 0.0, dt, 1.2, 2.0);
  f_.sparkle = approach(f_.sparkle, spark, dt, 0.08, 0.3);
  // the colours drift round the window, a little faster when the music is full
  if (playing && !reduced) turn_from_ += dt * (0.006 + 0.012 * f_.energy), turn_target_ += dt * (0.006 + 0.012 * f_.energy);
  double k = smooth((now - turn_t0_) / 1.4);
  f_.turn = turn_from_ + (turn_target_ - turn_from_) * k;
  if (k >= 1) turn_from_ = turn_target_;
  for (auto& w : f_.waves) { w.r += dt / kRingLife; w.a *= std::exp(-dt / 0.55); }
  f_.waves.erase(std::remove_if(f_.waves.begin(), f_.waves.end(), [](const MusicLightFrame::Wave& w) { return w.r > 1.15 || w.a < 0.01; }),
                 f_.waves.end());
  f_.time = now - started_;
  moving_ = std::fabs(f_.energy - before) > 1e-3 || std::fabs(f_.breath - before_b) > 1e-3 || std::fabs(f_.sparkle - before_s) > 1e-3 || std::fabs(f_.turn - before_turn) > 1e-5 ||
            !f_.waves.empty() || f_.sparkle > 0.01;
}
