# Timeline preview pipeline review

## Findings and changes

The previous dispatcher could send a replacement navigation seek every 33 ms,
even while a different navigation request was decoding. The helper's mailbox
treated that replacement as cancellation. A decode taking longer than the
pointer's update interval could therefore keep restarting without producing a
cache entry. Keeping the old image visible hid this problem but did not fix it.

Navigation now has one active operation. Pointer updates replace the desired
destination in App state, without invalidating that active operation. Its reply
is cached for the current file; only a reply matching the current requested
interval can update the display. On completion, the newest destination is sent.
There is no queue of intermediate pointer positions. Colour probes remain
immediately preemptible, and load/close still invalidate old media generations.

Finishing an operation has a cost: an uncached final position may wait for the
remaining active decode before its own decode begins. The change prevents
starvation and wasted restarts; it does not reduce an individual decode's cost
or guarantee an image at every uncached position during a fast sweep.

The helper also restores below-normal CPU priority on every shot exit. It no
longer stays at normal priority after an interactive request finishes.

## Pipeline and resource bounds

| Stage | Behaviour and remaining cost |
| --- | --- |
| Pointer | Video seek actions are unchanged. Hover requests update the latest destination; music emits no frame requests. |
| Cache | Stable one-second request intervals, actual frame timestamps, 12 MiB of pixel capacity / 256 entries, LRU. Hits bypass decode. |
| Startup | Helper preparation waits for the first frame and opening/reveal, plus the existing 250 ms warmup. Explicit hover may start after reveal. No new startup task was added. |
| Scheduling | At most one navigation request active, latest pending desktop/phone destinations, desktop priority, at most 30 dispatches/s. Colours yield. |
| IPC | Existing bounded mailboxes and worker-owned pipe I/O; the UI never waits for pipe writes or decoding. Process/media/request IDs reject stale replies. |
| Seek/decode | Existing reused libmpv instance and software decoding with 1–4 threads. Navigation uses keyframes; colours and main-player exact seeks retain their existing policy. Source resolution/codec/GOP still affect decode cost. |
| Preparation | The helper scales/sends small BGRA images. No image encoding or disk-cache write is added. |
| Presentation | The view retains the small source. Nearest-neighbour colour runs and repeated-row copies write directly into the tooltip canvas only when content/geometry changes. Moving an unchanged card repositions it without resampling. |
| Lifecycle | New hovers clear displayed pixels, matching cache hits may return immediately, and late unrelated intervals cannot replace the current destination. File changes clear cache/display state and interrupt old helper work; application close kills its helper job. Leaving hover/hidden windows prevents new local work; one active operation may finish for cache reuse. |

The 12 MiB cap covers cached pixel buffers, not the whole process or libmpv's
decoder memory. CPU priority classes do not impose total CPU, memory or I/O quotas.

## Mini-player and visual stability

Known video geometry reserves a correctly proportioned card before the first
image. Geometry is independent of the cached source's resolution and follows
window/scale changes. A large cached source fits a mini window without another
decode or an enlarged retained copy. Portrait cards stay below the window-button
strip and clear of the dock, including when the dock is at the top. Very narrow
images retain enough card width for the timestamp. Music stays time-only.

During a continuous hover, the displayed image and its actual timestamp stay
together until a matching replacement is ready. Cache misses do not collapse the
card. A new interaction clears old pixels while retaining per-file geometry.
No frame blending or automatic exact refinement was introduced.

## Alternatives considered

| Alternative | Decision |
| --- | --- |
| Reused decoder, demand-driven samples and latest-destination scheduling | Retained; fix scheduling before adding more decoding. |
| Restart active navigation on each movement | Removed; the deterministic model reproduces starvation. |
| Limited directional prefetch or progressive sparse preparation | Deferred. It adds seeks and contention; there is no native workload evidence justifying it yet. |
| Full storyboard / persistent disk cache | Not added. It requires substantial up-front work and disk/invalidation policy for local files; prepared server assets are a different workload. |
| Larger cache / more threads | Not increased. The issue occurs before images enter the cache; a bigger cache cannot resolve cancellation. |
| Hardware decoding | Existing opt-in diagnostic retained. Availability, copy-back cost and contention with main playback require native measurements before changing production policy. |
| Direct FFmpeg decoder | Not introduced. It would duplicate media/rotation/colour handling and dependencies without evidence that libmpv is the limiting stage. |

## Measurements and limits

### Scheduler model

`tools/benchmark_preview_dispatch.cpp` executes the actual dispatch policy with
125 pointer updates/s for two seconds, a reversal and a final uncached destination.
Decode duration is an input to the model, not a measurement of libmpv or any file.
The before build uses the previous dispatch header with the same sequence; only
its former four-argument `delay` call is adapted.

| Assumed decode | Completed while moving, before → after | Cancelled, before → after | Final uncached wait, before → after |
| --- | --- | --- | --- |
| 12 ms | 59 → 59 | 0 → 0 | 26 → 26 ms |
| 60 ms | 0 → 33 | 59 → 0 | 70 → 100 ms |
| 120 ms | 0 → 16 | 59 → 0 | 130 → 160 ms |
| 300 ms | 0 → 6 | 59 → 0 | 310 → 400 ms |

Completed samples can fill the cache; these counts are **not** visible updates at
the latest cursor position. The final-wait column makes the scheduling tradeoff
explicit. Matching-position updates during movement/reversal were 29 → 29 with
12 ms decoding, 0 → 16 at 60 ms, 0 → 7 at 120 ms, and 0 → 2 at 300 ms. These are
model events, not measured screen updates. Cached revisits perform no new decode
requests.

### Portable UI microbenchmark

`tools/benchmark_preview_render.cpp` drives the actual PlayView with identical
40×22 sources, a 1280×720 window at 2× scale and identical 83,328-byte tooltip
canvas capacity. It times 2,000 image replacements plus UI ticks per pass, five
passes per run, with three interleaved before/after runs on this Linux host
(GCC 13, `-O2`). The baseline uses the previous `play.cpp`, with the same current
headers and remaining portable objects; this comparison isolates that view's
implementation and does not reproduce the entire previous application.

- Median of the three run medians: 121.01 → 54.54 ms per 2,000 updates.
- Run-median ranges: 118.33–128.64 ms before, 53.81–65.67 ms after.
- Retained source allocation: 56,320 → 3,520 bytes. The tooltip canvas remains.

This is a small UI preparation cost, not extraction, startup or screen latency.
Compiler/system load affect timings. Memory values are buffer capacities, not
whole-player working-set measurements.

### Validation

88 portable C++ tests passed, including the progress regression, four simulated
decode durations, stable display/re-entry, timestamp pairing, cache bounds,
request/media generations, one exact click/release seek, and 144 combinations of
window size, DPI, aspect and control style. Layout checks include every generated
nearest-neighbour pixel, window-button clearance and audio-only behaviour.
Portable visual renders were inspected for landscape/portrait mini-player cards,
edge positions and the initial waiting card.

ASan/UBSan also passed the progress test and the layout test with an instrumented
PlayView and test driver (other linked portable objects remained uninstrumented).
Leak detection was disabled; this is not a whole-program leak measurement.

The application was cross-built for Windows x64. No native Windows runtime,
Wine or Windows GPU is available here. There are **no before/after native video
opening, frame-drop, CPU/GPU, I/O or end-to-end hover latency results**, including
long/4K/long-GOP files. Compilation, simulation and portable rendering do not
establish those results.

For native measurements, use the same local files and machine with `Cue --log`.
The log separates local queue wait, dispatch-to-reply, worker decode/preparation
and request-to-ready at the UI handoff. It does not measure physical display
latency. The supplied Windows helper diagnostic can compare CPU-fast, CPU-exact
and GPU-requested modes; it does not include simultaneous main playback or UI
painting. Useful native cases are cold/warm hover, cached revisits, sweeps,
reversals and stopping, in both full and mini windows, with playback active and
paused, then file replacement and close.

## Primary references

- [mpv stable manual](https://mpv.io/manual/stable/): keyframe/exact seeking, screenshots and hardware-decoding options.
- [FFmpeg decoder utilities](https://www.ffmpeg.org/doxygen/trunk/group__lavc__misc.html): codec buffers are reset on seeks; repeated seeks can discard decoding progress. The observed Cue cancellation comes from the code/model above, rather than a claimed measurement from this document.
- [Microsoft SetPriorityClass](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setpriorityclass): CPU priority classes and their distinction from background resource scheduling. The helper's existing classes are retained and restored when each shot ends.
