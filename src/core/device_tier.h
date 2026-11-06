#pragma once
// What this PC can comfortably do, read once (win/video_enhance.cpp: the graphics adapter the
// engine draws on, and the processor's threads), and the choices that follow from it. Portable,
// so the choices are tested without Windows.
//
// Full: a graphics card with its own memory. Light: graphics inside the processor (Intel UHD /
// Iris, AMD Radeon in an APU: little or no memory of its own). Software: no graphics driver at
// all (Windows' "Basic Render Driver"): every picture is drawn by the processor.
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

enum class GpuTier { Full, Light, Software };

inline GpuTier gpu_tier_for(bool software_adapter, uint64_t dedicated_bytes) {
  if (software_adapter) return GpuTier::Software;
  return dedicated_bytes < (1ull << 30) ? GpuTier::Light : GpuTier::Full;  // under 1 GB: shared memory
}

inline const char* gpu_tier_name(GpuTier g) {
  return g == GpuTier::Software ? "software" : g == GpuTier::Light ? "integrated" : "dedicated";
}

struct DeviceTier {
  GpuTier gpu = GpuTier::Full;
  int threads = 8;  // logical processors
  bool software() const { return gpu == GpuTier::Software; }
  bool integrated() const { return gpu != GpuTier::Full; }
  // two cores with hyper-threading, or four plain ones: the laptops and office PCs where a film
  // decoded by the processor already takes most of it
  bool weak_cpu() const { return threads <= 4; }
  // music's picture is made by the processor (the engine's filters) at 60 a second; where that
  // or drawing it costs too much, 30 (the interface over it follows the same rate)
  int music_fps() const { return weak_cpu() || software() ? 30 : 60; }
  // the interface's pictures sent to the engine: a software renderer redraws the whole window for
  // each one, so it gets at most 30 a second (Cue's own frames are unchanged)
  double overlay_rate_cap() const { return software() ? 45.0 : 90.0; }
  // how often a film's colours are sampled for the light (a picture decoded by the processor in
  // the preview helper each time): less often where the processor is the bottleneck
  double colour_interval_scale() const { return weak_cpu() || software() ? 2.0 : 1.0; }
  // smoothing blends frames on the graphics side: never offered without real graphics
  bool smoothing_ok() const { return !software(); }
};

// A film's clock (App::decide_window). Timed by the display, the engine shows a picture at every
// refresh and everything Cue draws over the film (the bar, the timeline's preview, the opening
// iris) moves at the screen's rate; timed by the sound it shows only the film's own frames (24 a
// second), which is lighter. Full graphics: by the display up to 4K (not on battery). Integrated:
// by the display up to 1440p. Software: always by the sound. Beyond 4K: always by the sound.
inline std::string film_clock(int64_t w, int64_t h, bool battery, GpuTier gpu) {
  const bool heavy = std::max(w, h) > 2600 || std::min(w, h) > 1600;  // more than 1440p
  const bool huge = std::max(w, h) > 4600 || std::min(w, h) > 2600;   // more than 4K
  if (gpu == GpuTier::Software || huge) return "audio";
  if (heavy && (battery || gpu == GpuTier::Light)) return "audio";
  return "display-resample";
}

// The engine's picture options for this graphics (set once, before it starts). Full keeps the
// engine's own high-quality defaults. Integrated keeps a sharp scaler but drops the passes that
// cost the most for the least visible gain (linear-light and sigmoid scaling, the correct but
// heavy downscaler on 4K, a high-quality chroma scaler). Software takes the engine's "fast"
// choices: bilinear everywhere, no dithering.
struct RenderOption { const char* name; const char* value; };
inline std::vector<RenderOption> render_options(GpuTier gpu) {
  if (gpu == GpuTier::Full) return {};
  std::vector<RenderOption> o = {
      {"correct-downscaling", "no"}, {"linear-downscaling", "no"}, {"sigmoid-upscaling", "no"},
      {"cscale", "bilinear"}};
  if (gpu == GpuTier::Light) {
    o.push_back({"scale", "spline16"});
    o.push_back({"dscale", "hermite"});
  } else {
    o.push_back({"scale", "bilinear"});
    o.push_back({"dscale", "bilinear"});
    o.push_back({"dither", "no"});
  }
  return o;
}

// music's own scalers (file-local, per look): smooth for a cover, square pixels for the spectrum
inline const char* music_scale(GpuTier gpu, bool cover) { return !cover ? "nearest" : gpu == GpuTier::Software ? "bilinear" : "spline36"; }
inline const char* music_dscale(GpuTier gpu, bool cover) { return !cover ? "nearest" : gpu == GpuTier::Software ? "bilinear" : "mitchell"; }
