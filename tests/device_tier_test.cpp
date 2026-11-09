// Weaker PCs (core/device_tier.h): which graphics count as integrated or software, the film's
// clock by resolution and adapter, the lighter scaling passes, and the music/UI rates.
#include <cassert>
#include "../src/core/device_tier.h"

static bool has_opt(const std::vector<RenderOption>& v, std::string name, std::string value) {
  for (const auto& o : v) if (o.name == name && o.value == value) return true;
  return false;
}

int main() {
  constexpr uint64_t MB = 1ull << 20;
  constexpr uint64_t GB = 1ull << 30;

  // Adapter classification: software always wins; < 2 GB of dedicated VRAM is integrated/light.
  assert(gpu_tier_for(true, 0) == GpuTier::Software);
  assert(gpu_tier_for(true, 8 * GB) == GpuTier::Software);
  assert(gpu_tier_for(false, 0) == GpuTier::Light);
  assert(gpu_tier_for(false, 128 * MB) == GpuTier::Light);   // Intel UHD shared carve-out
  assert(gpu_tier_for(false, 512 * MB) == GpuTier::Light);   // APU carve-out
  assert(gpu_tier_for(false, 1 * GB) == GpuTier::Light);     // 1 GB entry card
  assert(gpu_tier_for(false, 2000 * MB) == GpuTier::Light);  // 2 GB card reporting < 2 GiB usable
  assert(gpu_tier_for(false, 2 * GB) == GpuTier::Full);
  assert(gpu_tier_for(false, 8 * GB) == GpuTier::Full);

  // Film clock: 1080p stays at the display rate on dedicated and integrated, audio on software;
  // above 1080p (1440p, 4K) uses audio on integrated or battery, display-resample on a dedicated card on power.
  for (bool battery : {false, true}) {
    assert(film_clock(1920, 1080, battery, GpuTier::Full) == "display-resample");
    assert(film_clock(1920, 1080, battery, GpuTier::Light) == "display-resample");
    assert(film_clock(1080, 1920, battery, GpuTier::Light) == "display-resample");  // vertical 1080p
    assert(film_clock(1920, 1080, battery, GpuTier::Software) == "audio");
    assert(film_clock(1080, 1920, battery, GpuTier::Software) == "audio");          // a phone video too
    assert(film_clock(2560, 1440, battery, GpuTier::Light) == "audio");             // >1080p on integrated
  }
  assert(film_clock(2560, 1440, false, GpuTier::Full) == "display-resample");
  assert(film_clock(2560, 1440, true, GpuTier::Full) == "audio");
  assert(film_clock(3840, 2160, false, GpuTier::Full) == "display-resample");
  assert(film_clock(3840, 2160, true, GpuTier::Full) == "audio");
  assert(film_clock(3840, 2160, false, GpuTier::Light) == "audio");
  assert(film_clock(2160, 3840, false, GpuTier::Light) == "audio");     // turned: the same
  assert(film_clock(7680, 4320, false, GpuTier::Full) == "audio");      // 8K

  // Render options: dedicated cards touch nothing; integrated and software use bilinear without dithering.
  assert(render_options(GpuTier::Full).empty());
  auto L = render_options(GpuTier::Light), S = render_options(GpuTier::Software);
  assert(has_opt(L, "scale", "bilinear") && has_opt(L, "dscale", "bilinear") && has_opt(L, "cscale", "bilinear"));
  assert(has_opt(L, "dither", "no") && has_opt(L, "linear-downscaling", "no") && has_opt(L, "correct-downscaling", "no"));
  assert(has_opt(S, "scale", "bilinear") && has_opt(S, "cscale", "bilinear") && has_opt(S, "dither", "no"));

  // Music and UI budgets.
  DeviceTier strong{GpuTier::Full, 12};
  DeviceTier mid_dedicated{GpuTier::Full, 8};
  DeviceTier office{GpuTier::Light, 8};
  DeviceTier laptop{GpuTier::Light, 4};
  DeviceTier none{GpuTier::Software, 8};

  assert(strong.music_fps() == 60);
  assert(mid_dedicated.music_fps() == 30 && office.music_fps() == 30 && laptop.music_fps() == 30 && none.music_fps() == 30);
  assert(strong.allow_smooth_motion() && office.allow_smooth_motion() && !none.allow_smooth_motion());
  assert(strong.ui_frame_interval() < 1.0 / 50.0 && none.ui_frame_interval() > 1.0 / 35.0);
  assert(strong.colour_interval_scale() == 1 && office.colour_interval_scale() == 2 && laptop.colour_interval_scale() == 2);
  assert(std::string(music_scale(GpuTier::Full, true)) == "spline36");
  assert(std::string(music_scale(GpuTier::Light, true)) == "bilinear");
  assert(std::string(music_scale(GpuTier::Software, true)) == "bilinear");
  assert(std::string(music_scale(GpuTier::Software, false)) == "nearest");
  assert(std::string(music_dscale(GpuTier::Full, true)) == "mitchell");
  assert(std::string(music_dscale(GpuTier::Light, true)) == "bilinear");
  assert(std::string(music_dscale(GpuTier::Software, true)) == "bilinear");
  return 0;
}
