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
