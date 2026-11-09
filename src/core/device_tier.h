// What this PC can comfortably do: read once at start (DXGI adapter 0, the one the engine's D3D11
// context draws on, and the processor's logical threads) so a weak laptop or a PC without a GPU
// driver does less work per frame without a setting to find, while a PC with a dedicated card is
// untouched. Pure decisions here; the Windows query lives in win/video_enhance.cpp.
#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

enum class GpuTier {
  Full,      // dedicated card (>= 2 GB of its own video memory): the engine's full defaults
  Light,     // integrated graphics (Intel UHD/Iris, AMD APUs) or entry 1-2 GB cards: lighter scaling and clocks
  Software,  // Microsoft Basic Render Driver (WARP) or no DXGI at all: the CPU draws every pixel
};

inline const char* gpu_tier_name(GpuTier t) {
  switch (t) {
    case GpuTier::Full: return "dedicated";
    case GpuTier::Light: return "integrated";
    case GpuTier::Software: return "software";
  }
  return "unknown";
}

// Adapter 0's tier from its DXGI description: software flag / WARP ids first, then its own video
// memory (integrated GPUs report 0 to ~512 MB of shared "dedicated" memory; 1 GB and entry 2 GB
// cards that report < 2 GiB usable in DXGI also benefit from the lighter scaling path).
inline GpuTier gpu_tier_for(bool software_adapter, uint64_t dedicated_bytes) {
  if (software_adapter) return GpuTier::Software;
  if (dedicated_bytes < (2ull << 30)) return GpuTier::Light;
  return GpuTier::Full;
}

struct DeviceTier {
  GpuTier gpu = GpuTier::Full;
  int threads = 8;

  bool software() const { return gpu == GpuTier::Software; }
  bool integrated() const { return gpu == GpuTier::Light; }
  bool weak_cpu() const { return threads <= 8; }

  // Music's spectrum picture: 30 fps on a weak CPU or non-dedicated graphics halves the lavfi
  // graph's work while the spectrum's analysis window and decay scale with fps so its motion and
  // the room's 30 Hz light show stay the same.
  int music_fps() const { return weak_cpu() || gpu != GpuTier::Full ? 30 : 60; }

  // The UI overlays sent to the engine while a film plays: 30 Hz on software rendering (every
  // upload is a full-window BGRA copy through the CPU renderer); 60 Hz everywhere else.
  double ui_frame_interval() const { return software() ? 1.0 / 30.0 : 1.0 / 60.0; }

  // Scene colour sampling for the ambient light / accent: half as often on a weak CPU or
  // integrated/software graphics (each sample decodes a small frame in the helper).
  double colour_interval_scale() const { return weak_cpu() || gpu != GpuTier::Full ? 2.0 : 1.0; }

  // Smooth motion (temporal frame blending): never on software rendering, where blending two
  // full frames on the CPU at the monitor's rate makes the film drop frames instead.
  bool allow_smooth_motion() const { return !software(); }
};

// The film's clock (`video-sync`): `display-resample` draws at the screen's refresh rate so the UI
// over the film glides at 60 Hz+, and is what a dedicated card on power uses up to 4K. On
// software rendering every refresh is a full-window CPU copy, so any film uses `audio` (one draw
// per video frame); on integrated graphics or on battery, anything above 1080p (> 1920x1080,
// including 1440p and 4K, turned or not) uses `audio`; above 4K (8K) always uses `audio`.
inline std::string film_clock(int64_t w, int64_t h, bool battery, GpuTier gpu) {
  if (w <= 0 || h <= 0) return "display-resample";
  if (gpu == GpuTier::Software) return "audio";
  const bool above_4k = w > 4096 || h > 2160;
  const bool heavy = std::max(w, h) > 1920 || std::min(w, h) > 1080;
  if (above_4k || (heavy && (battery || gpu == GpuTier::Light))) return "audio";
  return "display-resample";
}

struct RenderOption {
  const char* name;
  const char* value;
};

// Engine scaling options set once at start (`vo=gpu-next`). Dedicated cards keep mpv's own
// defaults. Integrated and software graphics use single-pass hardware bilinear scaling without
// linear-light/sigmoid conversions, anti-ringing downscalers, or dithering.
inline std::vector<RenderOption> render_options(GpuTier gpu) {
  if (gpu == GpuTier::Full) return {};
  return {
      {"scale", "bilinear"},
      {"dscale", "bilinear"},
      {"cscale", "bilinear"},
      {"dither", "no"},
      {"correct-downscaling", "no"},
      {"linear-downscaling", "no"},
      {"sigmoid-upscaling", "no"},
  };
}

// Music's cover scene: the spectrum-alone look always uses `nearest` so its dots stay crisp; with
// the cover, non-dedicated graphics use `bilinear` instead of two multi-tap filters per frame.
inline const char* music_scale(GpuTier gpu, bool cover) {
  if (!cover) return "nearest";
  return gpu != GpuTier::Full ? "bilinear" : "spline36";
}
inline const char* music_dscale(GpuTier gpu, bool cover) {
  if (!cover) return "nearest";
  return gpu != GpuTier::Full ? "bilinear" : "mitchell";
}
