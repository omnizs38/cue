# Performance

Cue keeps playback independent from interface work. Hidden/idle UI avoids redraws, minimized windows suspend UI animation, and large temporary buffers are released after engine acknowledgement. Previews run in a low-priority helper; the main decoder and codec support are unchanged.

## Current approach

- Paint the window before loading the engine DLL; overlap engine startup with the home exit animation.
- Cache compiled shaders; register fonts and file associations after launch; enable verbose mpv logging only with `--log`.
- Coalesce engine events and save immutable state snapshots on a worker.
- Redraw only changed home regions and avoid formatting/layout work when visible state is unchanged.
- Pace animation through compositor vblank where available, with timer fallback. Playback UI targets approximately 60 Hz independently of video cadence.
- Delay color sampling by 1.5 seconds and thumbnails by 3 seconds after the first frame. During window dragging, reuse lighting images and defer preview work.
- Reduce hover-thumbnail transfer/cache pixel count without reducing scene-color sampling resolution.
- Use two separable Gaussian passes for the frameless control cloud, with half-resolution intermediate textures; disable them when hidden. Visible blur still has a GPU cost.
- Treat a window covered by another app's full-screen window on the same monitor (or cloaked on another virtual desktop) like a minimised one: no UI frames, no shader-parameter updates, the cover pass off, and a film presented per frame (`video-sync=audio`, file-local) instead of per display refresh. Checked twice a second; activation wakes it at once.
- Music's light show: two passes at a quarter of the output size (the dots' mask, 4 to 6 taps; their glow, 32 taps, only where the way back can meet a lit dot: about a third of the window with the cover, under a third for the spectrum alone, the rest returning at once) and one at full size (most pixels return at the first tests), parameters sent at 30 Hz only while the show moves and the window can be seen; the meter is `astats` on the song's own audio (a few sums per sample). The halo windows are not used for music.
- Music's cover scene: spectrum at 60 fps on tiny pictures (24x20 cells, 32x18 light); the wings' dot grid is a single frame multiplied into their alpha. Measured with FFmpeg on 30 s of audio, single-threaded: 6.9 s → 5.1 s for the whole scene.

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
