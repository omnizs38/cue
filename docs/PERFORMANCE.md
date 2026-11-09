# Performance

Cue keeps playback independent from interface work. Hidden/idle UI avoids redraws, minimized windows suspend UI animation, and large temporary buffers are released after engine acknowledgement. Previews run in a low-priority helper; the main decoder and codec support are unchanged.

Current review, measurements and native checks: [PERFORMANCE-REVIEW.md](PERFORMANCE-REVIEW.md).

## Current approach

- Paint the window before loading the engine DLL; overlap engine startup with the home exit animation.
- Cache compiled shaders; register fonts and file associations after launch; enable verbose mpv logging only with `--log`.
- Coalesce engine events and save immutable state snapshots on a worker.
- Redraw only changed home regions and avoid formatting/layout work when visible state is unchanged.
- Pace animation through compositor vblank where available, with timer fallback. Playback UI targets approximately 60 Hz independently of video cadence.
- Delay background color sampling by 1.5 seconds. Warm the preview helper after the first frame/reveal and 250 ms; explicit timeline previews start once the first frame and opening transition settle. During window dragging, reuse lighting images and defer preview work.
- Defer video-folder enumeration for next-episode discovery until 1.5 seconds after the first frame and after the opening transition. The decoder opens the requested file before this optional disk read. Audio album discovery keeps its existing flow.
- Use fast keyframe navigation samples in stable one-second intervals. Dispatch the first hover immediately when navigation is idle. Let active navigation finish, coalesce later destinations to at most 30 Hz and preempt colour work; repeated pointer updates cannot starve decoding. Main-player clicks and colour probes remain precise. Keep the returned frame time with its image, reuse the bounded RAM cache and preserve card geometry and the current picture during an active hover, clearing old pixels on re-entry only. See [thumbnail policy and timing diagnostic](THUMBNAILS-2026-11-08.md).
- Reduce hover-thumbnail transfer/cache pixel count without reducing scene-color sampling resolution. Retain the small source in the view; render directly into a bounded tooltip canvas using repeated rows/runs, fitting both normal and mini windows without a retained enlarged image.
- Use two separable Gaussian passes for the frameless control cloud, with half-resolution intermediate textures; disable them when hidden. Visible blur still has a GPU cost.
- Treat a window covered by another app's full-screen window on the same monitor (or cloaked on another virtual desktop) like a minimised one: no UI frames, no shader-parameter updates, the cover pass off, and a film presented per frame (`video-sync=audio`, file-local) instead of per display refresh. Checked twice a second; activation wakes it at once.
- Music's light show: two passes at a quarter of the output size (the dots' mask, 4 to 6 taps; their glow, 16 taps, only where the way back can meet a lit dot: about a third of the window with the cover, under a third for the spectrum alone, the rest returning at once) and one at full size (most pixels return at the first tests), parameters sent at 30 Hz only while the show moves and the window can be seen; the meter is `astats` on the song's own audio (a few sums per sample). The halo windows are not used for music.
- Music's cover scene: spectrum at 60 fps on tiny pictures (24x20 cells, 32x18 light); the wings' dot grid is a single frame multiplied into their alpha. Measured with FFmpeg on 30 s of audio, single-threaded: 6.9 s → 5.1 s for the whole scene.

## Weaker PCs

Read once at start (`read_device_tier`, `core/device_tier.h`, logged as "graphics ..., N threads"):
the first graphics adapter (the one the engine draws on; Windows lists the preferred one first)
is dedicated (2 GB of its own or more), integrated (less: Intel UHD/Iris, AMD APUs, entry 1–2 GB
cards) or software (no driver, Microsoft's Basic Render Driver), and the processor's logical
threads.

- Integrated graphics: single-pass bilinear scaling (`scale`/`dscale`/`cscale=bilinear`), no
  linear-light/sigmoid conversions, no "correct" downscaler, no dithering, music's covers scaled
  bilinear; films above 1080p (1440p, 4K) timed by the sound (1080p stays timed by the display).
  Dedicated cards keep the engine's defaults.
- Software rendering: bilinear everywhere, no dithering, every film timed by the sound, no
  smoothing, the interface's pictures to the engine at most about 30 a second, music's covers
  scaled bilinear.
- Weak processor (8 threads or fewer), integrated graphics, or software rendering: music's picture
  at 30 a second (the spectrum's analysis step and decay follow, so it moves the same), the film's
  colours for the light sampled half as often.
- Control cloud and music light shaders: 14 paired bilinear Gaussian taps per pass (was 22) and
  16 jittered radial light-ray taps (was 32, matching `0.837` decay).
- Engine memory budgets: forward demuxer packet cache capped at 32 MiB and back-seek cache at
  12 MiB (down from 150 MiB / 50 MiB), and D3D11va extra hardware surfaces at 4 (down from 6).
- The preview helper retains its decoder-thread budget (half the logical threads, 1 to 4). Warmup/colours and idle use below-normal CPU priority; interactive navigation temporarily uses normal priority. GPU policy is unchanged.

## Up next panel

Scrolling a 512-entry queue with 32 loaded previews (Linux, GCC 13, `-O2`, panel
300×520 art px): 3.5 → 1.2 ms per frame at 2× scale and 7.9 → 2.4 ms at 3× after
caching row-sized previews and the panel background. Previews scaled for rows that
leave the viewport are released; at most one copy per visible row exists.

## Reproduce and validate

```bash
g++ -std=c++20 -O2 tools/benchmark_render.cpp src/ui/*.cpp src/core/*.cpp -o render-benchmark
./render-benchmark
bash tools/test.sh
```

Use the same machine, compiler, benchmark, and media to compare revisions. Additional logic benchmarks are in `tools/benchmark_ui.cpp`; buffer/invalidation checks are in `tests/performance_test.cpp`.

`cue.log` timestamps first paint, DLL mapping, engine initialization, open request, file load, first video frame, and iris start. Measure native Windows launch, pause/resume, fullscreen, resize, DPI changes, and playback under representative GPU/media load. Portable tests, Wine runs, and timing simulations cannot establish actual 60 fps or end-user startup time.
