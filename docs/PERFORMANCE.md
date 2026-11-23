# Performance

Cue keeps playback independent from interface work. Hidden/idle UI avoids redraws, minimized windows suspend UI animation, and large temporary buffers are released after engine acknowledgement. Previews run in a low-priority helper; the main decoder and codec support are unchanged.

Current review, measurements and native checks: [PERFORMANCE-REVIEW.md](PERFORMANCE-REVIEW.md).

## Current approach

- Paint the window before loading the engine DLL; overlap engine startup with the home exit animation. Opened with a film, the window is made in the film's shape from its file's header (`core/media_header.h`) and paints black: no home, no reshape (1.4).
- Cache compiled shaders; register fonts and file associations after launch; enable verbose mpv logging only with `--log`.
- Coalesce engine events and save immutable state snapshots on a worker.
- Redraw only changed home regions and avoid formatting/layout work when visible state is unchanged.
- Pace animation through compositor vblank where available, with timer fallback. Playback UI targets approximately 60 Hz independently of video cadence.
- Delay background color sampling by 1.5 seconds. Warm the preview helper after the first frame/reveal and 250 ms; explicit timeline previews start once the first frame and opening transition settle. During window dragging, reuse lighting images and defer preview work.
- Defer video-folder enumeration for next-episode discovery until 1.5 seconds after the first frame and after the opening transition. The decoder opens the requested file before this optional disk read. Audio album discovery keeps its existing flow.
- Use fast keyframe navigation samples in stable one-second intervals. Dispatch the first hover immediately when navigation is idle. Let active navigation finish, coalesce later destinations to at most 30 Hz and preempt colour work; repeated pointer updates cannot starve decoding. Main-player clicks and colour probes remain precise. Keep the returned frame time with its image, reuse the bounded RAM cache and preserve card geometry and the current picture during an active hover, clearing old pixels on re-entry only. See [thumbnail policy and timing diagnostic](THUMBNAILS-2026-11-08.md).
- Reduce hover-thumbnail transfer/cache pixel count without reducing scene-color sampling resolution. Retain the small source in the view; render directly into a bounded tooltip canvas using repeated rows/runs, fitting both normal and mini windows without a retained enlarged image.
- Blend translucent fills and fade whole canvases four pixels at a time (SSE2), with the scalar code's exact rounding: a full-window fade at 4K costs a few milliseconds instead of more than a frame (1.4).
- Use two separable Gaussian passes for the frameless control cloud, with half-resolution intermediate textures; disable them when hidden. Visible blur still has a GPU cost.
- Give a film its window's shape at load, from the demuxer (`demux_shape`): the picture opens with its first frame instead of after a glide that waited for it (1.4).
- Pace the window's poll (`core/poll_pace.h`): 30 a second while anything can change, 10 on a still home, 4 for a calm paused film or a home behind the window in front (whose eyes then stop following); any input, animation or engine event restores it at once (1.4).
- Redraw only the eyes' corner of the home when nothing else changed (`HomeView::eyes_only_key`, `face_reach`, `Canvas::clip`): about 0.4 ms instead of 8.5 to 11.5 ms a frame at 4K, the same pixels (1.4).
- Pace the film's colour samples by what they cost (`core/colour_pace.h`): the helper decodes at most a tenth of the time on films with far-apart keyframes (1.4).
- Timeline previews: the helper answers a second of the timeline that lands on a keyframe it already decoded with that picture, without decoding (`core/keyframe_index.h`): it knows from the engine's answers (a request at t that returned the keyframe a covers [a, t]) and from the file's own index (Matroska Cues, MP4 sync samples), trusted once two decoded answers agree with it and dropped for the file at the first disagreement. HEVC keyframes decode with slice threading instead of frame threading (the engine's `on_preloaded` hook, file-local). Measured with the real helper under Wine: the same pictures and times for every request; a 30 s sweep of a 4K HEVC film decoded 0 to 4 pictures instead of 30 (6.0 s of decoding to under 0.1 s), an HEVC keyframe 18% faster (1.4).
- End the preview helper after 60 s idle on the home; let the remote's server sleep with no phone and leave at once when turned off (1.4).
- Treat a window covered by another app's full-screen window on the same monitor (or cloaked on another virtual desktop) like a minimised one: no UI frames, no shader-parameter updates, the cover pass off, and a film presented per frame (`video-sync=audio`, file-local) instead of per display refresh. Checked twice a second; activation wakes it at once.
- Music's passes over the whole window (1.4.1): the room's light is drawn inside the light show's pass, and the cover's shadow and tilt, the edges, the list's veil and black and white share one pass: a centred song has two full-window passes instead of four, the same pixels (at most 2 levels of 255 apart, from rounding). The picture's scaling on a dedicated card is `catmull_rom` (was `spline36`). Per frame at the 4K opening window, in the engine's renderer on the processor: 919 → 695 (−24%); 300 → 251 at a laptop's 1280 × 720 (−16%).
- The film's ambient light (1.4.1, 23/11): its shape is runs of one colour, not a list of pixels (the 4K opening window: 160 thousand runs, 1.8 MB, instead of 1.6 million pixels, 14.3 MB); a paint writes only the runs whose colour changed and sends Windows only the changed part of each strip (`UpdateLayeredWindowIndirect` with a dirty rectangle), none when a strip did not change. The same light (`tools/benchmark_halo.cpp`: what each strip window holds, hashed after every tick). Per tick at the 4K opening window under Wine, a calm light: 2.0 → 0.4 ms of Cue's own work, 750 → 93 thousand pixels sent.
- A song minimized or under a full-screen window is drawn the plainest way (`bilinear` scaling, no light show; its pictures and its sound untouched): 514 → 309 per frame minimized at the 4K opening window. The preview helper ends while music plays.
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
g++ -std=c++20 -O2 tools/benchmark_frames.cpp src/ui/*.cpp src/core/*.cpp -o frames-benchmark
./frames-benchmark 600            # home and player at 1080p and 4K: ms per frame, allocations, bytes to the engine
bash tools/test.sh
```

Use the same machine, compiler, benchmark, and media to compare revisions. Additional logic benchmarks are in `tools/benchmark_ui.cpp`; buffer/invalidation checks are in `tests/performance_test.cpp`.

`cue.log` timestamps first paint, DLL mapping, engine initialization, open request, file load, first video frame, and iris start. Measure native Windows launch, pause/resume, fullscreen, resize, DPI changes, and playback under representative GPU/media load. Portable tests, Wine runs, and timing simulations cannot establish actual 60 fps or end-user startup time.
