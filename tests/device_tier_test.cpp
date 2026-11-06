// Weaker PCs (core/device_tier.h): which graphics count as integrated or software, the film's
// clock per graphics and size, the engine's lighter picture options, and music at 30 pictures a
// second with the spectrum moving the same as at 60.
#include "../src/core/device_tier.h"
#include "../src/core/audio_mode.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

static bool has(const std::vector<RenderOption>& o, const char* k, const char* v) {
  for (auto& r : o) if (std::string(r.name) == k) return std::string(r.value) == v;
  return false;
}

int main() {
  const uint64_t MB = 1ull << 20;
  assert(gpu_tier_for(true, 8192 * MB) == GpuTier::Software);
  assert(gpu_tier_for(false, 128 * MB) == GpuTier::Light);   // Intel UHD / Iris
  assert(gpu_tier_for(false, 512 * MB) == GpuTier::Light);   // an APU's carve-out
  assert(gpu_tier_for(false, 0) == GpuTier::Light);
  assert(gpu_tier_for(false, 2048 * MB) == GpuTier::Full);
  assert(gpu_tier_for(false, 1024 * MB) == GpuTier::Full);

  // the clock: 1080p always by the display, except with no graphics
  for (bool battery : {false, true}) {
    assert(film_clock(1920, 1080, battery, GpuTier::Full) == "display-resample");
    assert(film_clock(1920, 1080, battery, GpuTier::Light) == "display-resample");
    assert(film_clock(1920, 1080, battery, GpuTier::Software) == "audio");
    assert(film_clock(1080, 1920, battery, GpuTier::Software) == "audio");  // a phone video too
    assert(film_clock(2560, 1440, battery, GpuTier::Light) == "display-resample");
  }
  assert(film_clock(3840, 2160, false, GpuTier::Full) == "display-resample");
  assert(film_clock(3840, 2160, true, GpuTier::Full) == "audio");
  assert(film_clock(3840, 2160, false, GpuTier::Light) == "audio");
  assert(film_clock(2160, 3840, false, GpuTier::Light) == "audio");     // turned: the same
  assert(film_clock(7680, 4320, false, GpuTier::Full) == "audio");      // 8K

  // picture options: a dedicated card keeps the engine's defaults
  assert(render_options(GpuTier::Full).empty());
  auto L = render_options(GpuTier::Light), S = render_options(GpuTier::Software);
  for (auto* o : {&L, &S}) {
    assert(has(*o, "correct-downscaling", "no") && has(*o, "linear-downscaling", "no") &&
           has(*o, "sigmoid-upscaling", "no") && has(*o, "cscale", "bilinear"));
  }
  assert(has(L, "scale", "spline16") && has(L, "dscale", "hermite") && !has(L, "dither", "no"));
  assert(has(S, "scale", "bilinear") && has(S, "dscale", "bilinear") && has(S, "dither", "no"));
  assert(std::string(music_scale(GpuTier::Full, true)) == "spline36" && std::string(music_scale(GpuTier::Software, true)) == "bilinear");
  assert(std::string(music_scale(GpuTier::Software, false)) == "nearest" && std::string(music_dscale(GpuTier::Light, true)) == "mitchell");

  // the device's own choices
  DeviceTier strong{GpuTier::Full, 16}, laptop{GpuTier::Light, 4}, office{GpuTier::Light, 8}, none{GpuTier::Software, 8};
  assert(strong.music_fps() == 60 && office.music_fps() == 60);
  assert(laptop.music_fps() == 30 && none.music_fps() == 30);
  assert(strong.overlay_rate_cap() == 90 && none.overlay_rate_cap() == 45);
  assert(strong.colour_interval_scale() == 1 && laptop.colour_interval_scale() == 2);
  assert(strong.smoothing_ok() && laptop.smoothing_ok() && !none.smoothing_ok());
  // overlay cadence: 60 Hz -> every frame (or every second frame without graphics), 144 Hz -> 72 (or 48)
  auto every = [](double hz, double cap) { return std::max(1, (int)std::ceil(hz / cap - 0.05)); };
  assert(every(60, strong.overlay_rate_cap()) == 1 && every(60, none.overlay_rate_cap()) == 2);
  assert(every(144, strong.overlay_rate_cap()) == 2 && every(144, none.overlay_rate_cap()) == 4);

  // music: at 60 the graph is exactly as before; at 30 the rate, the analysis step and the
  // decay follow (two steps of 0.965 per picture)
  MusicScene sc;
  for (int look : {0, 1, 2}) {
    std::string g60 = audio_waveform_graph(1, look == 1 ? 0 : 2, sc, look), g30 = audio_waveform_graph(1, look == 1 ? 0 : 2, sc, look, 30);
    assert(g60 == audio_waveform_graph(1, look == 1 ? 0 : 2, sc, look, 60));
    assert(g60.find(":r=30") == std::string::npos && g30.find(":r=60") == std::string::npos && g30.find(":r=30") != std::string::npos);
    if (look != 2) {
      assert(g60.find("overlap=0.92") != std::string::npos && g60.find("lagfun=decay=0.965") != std::string::npos);
      assert(g30.find("overlap=0.84") != std::string::npos && g30.find("lagfun=decay=0.931") != std::string::npos);
    }
  }
  assert(audio_mix_graph({1, 2}, true, 3, sc, 0, 30).find("showfreqs=s=24x20:r=30") != std::string::npos);
  // the analysis keeps up with the picture at both rates: 24 kHz / hop >= fps
  assert(24000.0 / (4096 * (1 - 0.92)) >= 60 && 24000.0 / (4096 * (1 - 0.84)) >= 30);
  puts("Device tiers: integrated/software graphics, film clock per graphics, lighter picture options, music at 30 fps with the same decay passed");
}
