# Heavy films · Cue 1.4.1, third round

Review date: 2026-11-23. The author's list: 4K HEVC and VP9, high bitrates, HDR, large MKVs with many
tracks, 60 fps, long OBS recordings: opening, playing, seeking, processor, graphics card and memory,
nothing visible changed. Measured under Wine 9.0 (two cores, no graphics card, no video decoder) with
the real Cue, a measurement build (not shipped) and the real preview helper; the films and numbers
are in VALIDATION.md.

| Where | What was measured | Done |
|---|---|---|
| the halo around the window | nearly all of the window thread's work while a film plays (a 1080p film: 501 ms a second, 18 with the light off); a 14.3 MB pixel list at the 4K opening window, every strip sent whole at every paint | runs of one colour, only changed runs written, only changed parts sent: the same light, 501 → 159 ms a second in the real Cue under Wine; 14.3 → 2.4 MB |
| layers sent to the engine | 0.2 to 1.2 MB in every moment of every film; no window-sized layer while a film plays | nothing to gain |
| colour samples (the helper) | an exact seek decodes from the keyframe before it: 5 s of a 4K film with a keyframe every 10 s on one thread; `ColourPace` already spaces them | unchanged (see below) |
| the helper's memory | 112 MB (1080p) to 266 MB (4K HEVC 10-bit): its decoder's own frames | unchanged |
| opening, seeking, full screen | the engine's own decoding and drawing; under Wine also a Wine-only stall (no `atan2`) | unchanged |
| engine options | hardware decoding, buffers, extra surfaces, HDR peak detection and the frame cache for repeated frames were already set (1.3.2, 1.4) | unchanged |

Considered and left, with the reason:

- **colour samples decoded forward from the last one** instead of from the keyframe: the same pictures, but only cheaper while samples are closer together than keyframes, and on films with far-apart keyframes the pace already spaces them more widely than that. No gain where it matters.
- **colour samples decoded by the graphics card** (`hwdec`, the helper's diagnostic switch): far less processor on 4K films, but the helper skips the loop filter today (lighter), so its colours differ from a full decode: on the HEVC test film by up to 17 levels in an edge zone's average (VP9: none). A visible change and a native measurement (the card's video engine is shared with the film): left as a suggestion.
- **the arrows' exact seeks** (decoding from the keyframe up to the place): keyframe seeks would land elsewhere. Unchanged.

# Performance and bug review · Cue 1.4, second round

Review date: 2026-11-12. Priorities: playback first (opening, the first frame, nothing stalling,
seeking), then CPU, memory, disk and battery with Cue paused, on the home or minimized, then
reproducible bugs. Nothing visible changed except where the author chose (the colours' pace, the
home's poll when still). Measured with the shipped engine under Wine 9.0 (two cores) and a
counting build that logs, every 5 s, the messages that woke the window (not shipped), and with
`tools/benchmark_frames.cpp`. Wine is not Windows: the steps for the author's PC are in the
report.

| Case | Before | After | How |
|---|---:|---:|---|
| picture opening after the first frame (2.39:1 film) | 367 to 391 ms | 0 ms | `cue.log` with `--log`, 5 and 7 runs |
| a request to the picture (same file) | 1,401 to 1,604 ms | 1,083 to 1,181 ms | same |
| a 2:00 file whose data ends at 0:47: saved place | 0 (finished) | 47.9 s | `state.json` |
| colour samples of a film with a keyframe every 10 s, 52 s playing | 12.0 s decoding | 6.6 s | `cue.log` (decode_ms) |
| the same with a keyframe every second | 11 samples, 85 ms each | 10, 47 ms | same (unchanged pace) |
| wake-ups, paused, controls away, pointer still | 30.3 /s | 4.0 /s | counting build |
| wake-ups, home in view, pointer still | 30.3 /s | 10.4 /s | counting build |
| eye frames, home behind another window, pointer moving there | 30 /s | 0 (blinks only) | counting build |
| a frame of the eyes following the pointer, 4K / 1080p | 11.5 / 1.45 ms | 0.36 / 0.06 ms | `benchmark_frames` (same final pixels) |
| preview helper idle on the home | stays | ends after 60 s | counting build, process list |
| remote on with no phone: wake-ups of its server thread | 2 /s | 0 | thread context switches |
| turning the remote off: UI thread held | 8 to 429 ms (mean 211) | 1 to 2 ms | 12 starts and stops |

Causes and changes, in order of impact:

1. **The film's shape at load was never used for a plain film.** `configure_audio_mode` set
   `early_shape_` only when the track named its rotation, and the engine lists
   `demux-rotation` only for a turned film. `demux_shape` (core/media_geometry.h) treats a
   missing rotation as none.
2. **An early end counted as the end.** `O_EOF` zeroed the place and queued the next
   episode. `stopped_short` (core/episode_advance.h): more than 3% (30 s at least) before the
   length, a film keeps its place and does not hand over.
3. **Exact colour seeks on long GOPs.** `ColourPace` (core/colour_pace.h) measures what a
   colour sample costs this film (the helper's `decode_ms`) and stretches the interval so the
   helper works at most 10% of the time (at most 15 s apart).
4. **The 33 ms poll ran whatever happened.** `poll_pace` (core/poll_pace.h) sets its pace at
   the end of each poll; `ensure_anim`, engine events, the phone and activation bring it back.
   `under_active_window` (DWM frame bounds of the foreground window containing Cue's) stops the
   eyes following behind another app.
5. **Whole home frames for the eyes.** `Canvas` gained a clip; `HomeView::eyes_only_key` names
   everything on the home but the face, and `face_reach` bounds every pixel the face can draw
   (3.9% of the window). Same key: the app draws only that corner, in place, and gives Windows
   the rows that changed.
6. **Idle helper.** `T_HELPER_REST`: 60 s after the home has nothing for it.
7. **Remote loop.** A loopback datagram socket in its `select`: no timeout without phones;
   `stop` rings it before joining.

Bugs without a reproduction here (Windows only), fixed by reading: cloud-only songs read for
their covers by `make_image` (phone, "up next"), full screen not refitted on `WM_DISPLAYCHANGE`,
the place not written on `PBT_APMSUSPEND`. Not changed: paths over 260 characters (the
manifest is not long-path aware; to be confirmed on the PC first) and a read-ahead of the engine
DLL at a cold start (to be measured first).

# Performance review · Cue 1.4

Review date: 2026-11-11. Same rules as below: nothing visible changes, and no quality,
resolution or cadence is lowered to claim a gain.

## Measured

`tools/benchmark_frames.cpp` (new): the portable interface at 1920×1080 (scale 1.5) and
3840×2160 (scale 3), 600 frames per case, Linux x86_64, GCC 13.3 `-O2`, two cores. It reports
CPU time per frame, heap allocations and the bytes the player would hand to the engine. The
final pixels of every case are hashed; 1.3.2 and 1.4 give the same hashes.

| Case | 1.3.2 | 1.4 |
|---|---:|---:|
| home fading in (back from a film), 4K | 23.5 ms/frame | 6.4 ms/frame |
| home fading in, 1080p | 5.3 ms/frame | 1.7 ms/frame |
| one translucent fill over a 4K canvas | 34.0 ms | 4.5 ms |
| a 4K scene faded as a whole (`scale_alpha`: pairing screen, what's new) | 25.9 ms | 3.7 ms |
| home resting / eyes following the pointer, 4K | 4.4 / 4.2 ms/frame | 4.3 / 4.4 ms/frame (unchanged, within noise) |

At 4K a home fade frame took longer than a 60 Hz frame (16.7 ms), so the fade back from a film
dropped frames on large screens; it now fits with room to spare. The player's own interface
(controls moving, resting, the timeline preview, menus opening, music) costs 0.001–0.26 ms per
frame at 4K in the same benchmark and did not need changes.

Change: source-over of one premultiplied colour, and the whole-canvas alpha scale, run four
pixels at a time with SSE2 (every x64 processor), using the exact rounding of the scalar code:
`(x + 1 + (x >> 8)) >> 8` equals `x / 255` rounded for every value the blend produces. Verified
for every source alpha, source channel and destination channel (8.4 million pairs) and on
whole canvases (`tests/raster_test.cpp`). Sources that are not premultiplied keep the scalar path.

Reviewed and left as they are: the 30 Hz poll during playback (cheap Win32 queries, already
slowed when minimized), overlay uploads (only changed layers, capped at the display's divisor of
60 Hz), the ambient light (paints at 30 Hz only when its colours change), the home's dirty-rect
present, the engine's options (`video-sync=display-resample` is what keeps the interface at the
display's rate over a film; changing it would change behaviour).

## Native measurement

`TESTES-NO-PC\3-MEDIR-DESEMPENHO.bat` in the delivery (it runs `tools/measure_resources.ps1`)
records CPU, memory, threads, handles and I/O of Cue and its preview helper while a film plays:
run it with 1.3.2 and with 1.4 on the same film and window. GPU time needs Task Manager or a
graphics trace. None of this was measured here.

# Performance and stability review · Cue 1.3.2

Review date: 2026-11-09. Input: the supplied Cue-1.3.2-Pacote(1).zip.
The video player remains the primary playback path. Decoder options, shaders,
visual effects, audio samples, spectrum resolution/cadence and animation curves
are unchanged by this review.

## Findings and implemented corrections

| Problem and evidence | Cause | Change | Benefit and scope |
|---|---|---|---|
| Keyframe scrubbing could stop accepting destinations after an enqueue error (code path, covered by a failure-injection test) | The busy flag was set before checking the engine's return; a rejected command sends no reply | SeekDispatch acquires its slot only for an accepted command; file changes, relative seeks and exact release clear older pending destinations | Recovery without reopening; one seek in flight and one replaceable target remain bounded. Playback latency has not been measured on Windows |
| An old film's screenshot could become a new file's Windows media-card image (lifecycle path) | Screenshot replies used one fixed request ID across files | Per-request IDs invalidate screenshots on open/home; pending JPEG references are released | Old responses cannot start encoding for a new file; no change to actual video or saved frames |
| Music lighting performed two synchronous engine property reads per visible poll | The UI requested RMS and zero-crossing metadata separately | One asynchronous metadata-map read at a time, at the existing ~30 Hz poll; generation and 200 ms age checks reject obsolete replies | No synchronous engine reads in this recurring path; paused/hidden/cover-only modes submit none. One combined response keeps RMS/ZCR together |
| Song-to-song lighting retained unheard readings and a pending old cover-palette job | The visual state was intentionally retained, but its pending inputs were retained too | Drop pending inputs on seek, pause, minimize and file change; preserve the visible light state and palette crossfade | Old readings cannot trigger a new beat in the next passage; an old palette result cannot replace the new one |
| Music rebuilt a concatenated text key on each animation frame (measured) | Title, artist, dimensions and flags were formatted to detect unchanged rendered text | Cache text fields and numeric state separately; update the text surface only when fields change | Exactly two fewer allocations per steady audio UI frame in the supplied benchmark. Newline/field-boundary collisions also stop aliasing distinct text states |
| Rapid skips/resizes could create a thread for each cover read/decode (code path) | Detached threads had no global concurrency/queue limit | Two lazy workers: one for reads/colour/crop analysis and one for card decodes; four pending requests per lane; replace oldest queued work on pressure and retry still-needed requests | At most two active jobs and eight waiting jobs; orphaned art requests do not decode. The four-entry encoded cache and four weak decode-reuse entries remain unchanged |
| A failed card-art job could keep the frame timer alive | `sync_card_art` treated completed failure as still pending | Keep polling per animation frame only while a job is unfinished | Finished failure does not request perpetual redraws; queue-pressure cancellation is retried separately |
| Minimizing during a card turn could leave its DWM-paced thread running indefinitely | `frame` returned before the hidden-state cleanup in `step_card_flip`; the worker could wait in its flat stage for a UI finish signal | Settle the card turn, including any already-selected pending song, before suspending UI frames | The separate turn is stopped when hidden. This Win32 lifecycle path is cross-compiled; real compositor behavior remains a Windows check |
| Rebuilding a release could omit its checksum and publish without engine sources | The release script only attached the installer and treated missing sources as a warning | Require engine sources, verify the payload and pinned inputs, attach exactly EXE, EXE.sha256 and Engine-Source.zip | Repeatable local verification; manifests stay in the repository. Nothing was published by this review |

## Measurements and their limits

`tools/benchmark_music_ui.cpp`, Linux x86_64, GCC 13.3, `-O2`, 1280×720 at scale 2,
long metadata strings, five consecutive passes of 3,600 simulated 60 Hz UI frames
per mode/style. The baseline executable was built against the original portable
objects; the revised executable used the revised UI with the same other objects.
This exercises PlayView, not libmpv, the Windows compositor or the GPU.

| Mode | Background | Median allocations before | After | Difference per 3,600 frames |
|---|---|---:|---:|---:|
| Video | On | 124 | 124 | 0 |
| Video | Off | 342 | 342 | 0 |
| Audio | On | 7,324 | 124 | −7,200 |
| Audio | Off | 7,560 | 360 | −7,200 |

All 20 final rendered-layer hashes matched before/after, including layer placement
and visibility. This covers the benchmark's UI states, not native shader output
or every intermediate transition. Raw allocation, elapsed-time and hash output
is retained in `validation-1.3.2/music-ui-{before,after}.txt`. Timing samples were
noisy (the unchanged video path varied too); they do not support a claimed
percentage speedup. The deterministic allocation reduction is the reported gain.

A barrier-controlled test holds one worker active while submitting 1,000 tasks:
996 old queued tasks are discarded, the newest four finish in order, and peak
concurrency is one in that lane. A separate test suppresses 10,000 redundant
engine read attempts while one is pending. These are measured policy outcomes,
not playback measurements. Failure-injection seek tests cover rejected first and
coalesced commands, a 1,000-target sweep and file/exact-seek supersession.

## Resource ownership and bounds

- Cover workers: two active / eight pending total. Replaced work publishes a
  cancellation; consumers retry only the still-needed result. Running WIC or
  filesystem calls are allowed to finish; their result belongs to its original job.
- Encoded cover cache: four strong entries, each limited by the existing 32 MiB
  input cap; at most 128 MiB for those cache entries. Other current/pending job
  owners can retain their covers separately, so this is not a process-wide cap.
- Decode reuse: four weak entries, retaining no pixels by themselves. Queued jobs
  hold a weak result owner and do not decode after the result is abandoned.
- Crop/light analysis stays at 192 px, then ~96 px for colours. Card source decode
  keeps its existing 4096 px nominal cap and its original resampling. Large DPI
  covers can still be expensive individually; the concurrency limit prevents
  many decodes running together but does not promise lower steady-state RSS.
- Light reads: one outstanding engine operation, no cancellation/restart storm;
  invalidation retains the occupied slot until the old reply arrives. There is
  no timeout that could secretly admit an unbounded set of old reads.
- Existing preview cache (12 MiB / 256 entries), preview helper, media decoder
  budgets, bounded engine events and overlay acknowledgment rules are preserved.
- Music's existing filter graph continues during hidden audio playback. Rebuilding
  it solely to suspend visuals can interrupt the audio and was deliberately not
  introduced; UI sampling/updates and the independent turn worker are stopped.

A slow synchronous disk/codec operation can still occupy its lane until it
returns and delay subsequent covers. The queues bound the backlog, not the
latency of an OS call; this tradeoff needs native slow-storage profiling.

## Reviewed but requiring native profiling

These are remaining opportunities, not established performance regressions:

- `cover_key` and queue-file existence checks still perform filesystem metadata
  queries from UI call sites. Network/removable storage may block; no native
  trace quantified that delay here.
- The existing cover-crop hook allows up to 60 ms for the first read. Removing it
  without a replacement could expose black bars or change framing. It is retained.
- WIC codec work, actual GPU shader costs, fullscreen/miniplayer frame pacing,
  video-to-audio switches and driver allocations require Windows measurements.
- Cover-only artwork and astats/spectrum cost should be profiled with representative
  media; no quality/resolution/cadence reduction was used to claim an improvement.

## Native comparison procedure

Use the original and revised installers sequentially on the same Windows PC,
with the same DLL, saved settings, local files and window sizes. Keep the original
installer separately; the delivery contains the revised installer only.

1. Compare cold and warm opens (at least 10 each): 1080p H.264, 4K HEVC/AV1,
   portrait video, audio with/without embedded and external artwork. `cue.log`
   records first-frame/song-play elapsed open time; record medians and tails.
2. Use `tools/measure_resources.ps1 -Seconds 120 -OutputPath before.csv` and repeat
   as `after.csv`. It records per-process CPU (normalized by logical core count),
   threads, handles, private bytes, working set and logical IO. Include Cue's
   thumbnail helper when aggregating; first samples have zero delta counters.
3. Profile GPU/frame presentation separately with a native graphics trace. This
   script does not measure GPU utilization, physical disk active time or input
   latency. Use an input/display trace for pause, seek and panel response.
4. Exercise full/mini players, both bar styles, paused/playing, split/portrait and
   resized windows, four UI sizes, DPI changes, queue opening/closing and rapid
   file/track changes. Minimize during all card-turn stages, restore after 10 s,
   and verify the selected song plays once and cover/shadow/sound gain recover.
5. Build and run `tests/audio_engine_test.cpp` against the shipped DLL to validate
   the new asynchronous RMS/ZCR metadata map as well as decode/pause/seek/switches.

Completed checks and remaining execution limits are in [VALIDATION.md](VALIDATION.md).

## API references consulted

- [mpv property metadata](https://mpv.io/manual/stable/#command-interface-af-metadata-filter-label): audio-filter metadata follows the metadata map format.
- [libmpv client API](https://github.com/mpv-player/mpv/blob/master/include/mpv/client.h): asynchronous reads deliver GET_PROPERTY_REPLY events; a rejected enqueue sends no reply. Event data is consumed before the next event is fetched.

The sanitizer run also exposed three test entry points that assumed the binary
was always in `build/tests`. Their source-tree lookup now walks parent directories,
so the same assertions run from `build/asan/bin`; the application behavior checked
by those tests was not weakened. The LeakSanitizer default remains enabled in the
script, with an explicit `ASAN_OPTIONS` override for runtimes that cannot support it.
