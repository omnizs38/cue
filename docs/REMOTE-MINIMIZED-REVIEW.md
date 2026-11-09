# Phone remote while minimized — Cue 1.3.2

This revision keeps playback control independent of the PC's visual frame loop.
Minimized and covered windows continue to suspend local animation. The remote
server, 500 ms state publisher and input event handling already stayed active;
the failures were in work downstream of those commands.

## Findings and changes

| Path | Previous behavior | Updated behavior |
|---|---|---|
| Seek/15-second jumps during playback | The command waited inside an audio fade advanced only by `frame`; hidden `frame` returned before sending it | A finite timer advances the same 8 ms out/30 ms in envelope independently of rendering |
| Restart after a hidden seek | A stopped visual loop could leave the engine gain attenuated | Playback restart arms the gain recovery timer; a 1.5 s watchdog handles missing restart |
| File replacement during a delayed seek | The old fade could retain a command for the departing file | Open, close and home transitions cancel the timer, envelope and stored command |
| Minimize during an audio transition | Pre-graph stages and timeout recovery stopped with rendering | Hidden control steps finish the pre-graph stages without shader animation; the existing slow poll runs the wait watchdog |
| Open a remote file from the hidden home | The player could wait forever for the home's departure animation before switching logical view | Hidden loads reveal the logical playback view immediately, so snapshots and controls describe the loaded file |
| Phone timeline thumbnails | The shared dispatcher rejected every hidden window, and its retry poll returned too early | Connected-phone requests can open and use the same bounded decoder while hidden; local hover and color sampling remain suppressed |
| Repeat a cached phone preview gesture | The last delivered key could suppress the same image for a later gesture | A new preview request starts a new delivery opportunity, still coalescing pending decoding |
| Restore after a hidden file open | The local iris could have elapsed while the window was hidden | Start it only when visible, after pending/active geometry is settled |
| File aspect changes while minimized | `adapt_window` discarded the requested ratio | Keep the pending ratio and consume it on restore, including the same-size restore path |

The decoder, preview cache limits and network command limits are unchanged. Hidden
work runs only for an explicit request from a connected phone, after the loaded
file has a first frame; it does not depend on finishing the local iris animation.
There is one active preview decode and one replaceable target, using the existing
`PreviewDispatch`. No second engine or background visual loop was introduced.

The seek timer requests 8 ms during the short gain ramps, uses 50 ms while waiting
for restart, and is removed when idle or cancelled. Win32 clamps `SetTimer` periods
below its minimum to 10 ms and delivery is message-driven, so those requested
periods are not real-time latency guarantees. Volume keeps its existing separate
timer. Audio graph completion continues to use engine events; a minimized window
uses the existing 250 ms control poll for its timeout safeguard.

## Validation and evidence

`tests/seek_fade_test.cpp` checks timer-only dispatch, gain limits/recovery, exactly
one dispatch per fade, restart, timeout and idle at 8/10/16/250 ms sampling.

`tests/hidden_playback_test.py` extracts the production App transport/preview
methods and executes them against a deterministic fake engine/timer. It checks:

- Hidden seek dispatch and gain recovery without any renderer call.
- A replaced destination, cancellation before dispatch and decoder-timeout recovery.
- Minimize during the pre-graph stages; audio reopen, graph replacement, completion
  and failure watchdog without hidden shader animation.
- Closing playback when the window is hidden.
- Cold preview-helper start while the local reveal is blocked; explicit phone
  decoding, latest-target replacement, no hidden local/color work and no decoding
  without a connected phone.
- A second gesture requesting the same cached preview.
- The production iris gate: no hidden reveal, no reveal with a pending ratio or
  an active geometry glide, and reveal after restoration geometry settles.

The previous production `seek_soft` and `thumb_next` methods fail their respective
new regressions when substituted into the same harness. Evidence is in
`validation-remote-minimized-1.3.2/`. The tests use real production method bodies
with fake platform dependencies, not a native Windows message loop or decoder.

All 96 portable C++ executables, the production-method fake-engine harness and
both JavaScript suites passed. The Windows x64 cross-build completed without
warnings. Results are recorded with the release evidence. Installer verification covers checksum, payload, staged bytes,
unchanged engine DLL and 16 source archives.

## Native limits

There is no native Windows/physical phone run here. The engine, actual Win32 timer
queue, minimization/restore compositor, real media files and Wi-Fi still require
physical integration validation. The hidden home/load and Win32 restore-aspect wiring
were code-reviewed and cross-compiled; the local iris gate is exercised in the
fake-engine harness, but the native restore messages and actual window resizing
are not. Pause/volume/subtitle/queue/timer network paths were reviewed, with no new
minimization gate found in the existing remote event/state timer paths. There is
no measured FPS, CPU, RAM or end-to-end network latency claim.

References: [Win32 SetTimer](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-settimer)
and [libmpv playback events](https://github.com/mpv-player/mpv/blob/master/include/mpv/client.h).
