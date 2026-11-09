# Remote and background stability review — Cue 1.3.2

Review date: 2026-11-09. Application version and playback engine are unchanged.
This review covers the phone page, WebSocket delivery, remote filesystem requests,
queue imports, background folder discovery and their shutdown paths.

## Findings, causes, corrections and validation

| Problem | Cause | Correction | Evidence |
|---|---|---|---|
| A final setting could be overwritten by an older value | A throttled timeout survived a later direct send; trailing sends did not update the throttle clock | One latest-value slot per continuous command, shared 80 ms cadence, final-value flush and cancellation across connections/media | Production JavaScript regression reproduces `10,30,20` on the original page and `10,30` after correction |
| Apparently open sockets could remain unusable after a network loss | Reconnect depended on `close`; no application-level deadline | 10 s connect/auth deadline; foreground ping every 15 s with 10 s response deadline; replace the socket on foreground/network restore | Deterministic socket/time tests cover silent OPEN/CONNECTING sockets, pong, backpressure and stopped retries |
| Obsolete commands could cross connection or file boundaries | Deferred sends captured values but not the connection lifecycle; filenames alone identified media | Clear pending sends on retirement; per-open media generation in snapshots/commands; reject stale media and revoked credentials on application | Transport tests, session parser/state tests; Windows application wiring cross-compiled |
| Same filenames could retain another file's optimistic position | Reconciliation compared basename and view only | Include the media generation in reconciliation and gesture invalidation | Production reconciliation regression uses identical basenames with different generations |
| Disconnected/hidden pages retained visual work | Fetches, idle timers and gestures had separate cleanup paths | Central cancellation of folder/queue/track/cover/peek requests, observers, animation frames, gesture state and face timers; resume open views from fresh data | Full-page simulated DOM lifecycle check; deterministic fetch tests; optional browser regression supplied |
| Queue wheel could show a superseded response | Queue revision was captured but not checked when a response arrived | Reject stale or aborted responses; refetch after the current revision changes | Production asynchronous regression fails on the original page and passes after correction |
| Cover cancellation could lose the newer controller; a failed cover never retried | An old `finally` cleared a shared variable; requested revision was marked complete before success | Controller identity checks, abort checks, 3 s retry backoff; release the URL when leaving music | Controlled out-of-order promises, HTTP failure and URL release tests |
| Picked preview could briefly show the preceding selection | Its generation changed after the debounce, leaving a stale-response window | Advance generation immediately when selecting; cancel timer/fetch on release | Controlled selection/debounce regression |
| A stationary scrub disc requested frames indefinitely | The RAF loop continued while the disc remained shown | Stop scheduling when shape/angle settle; retain detent redraws and the full release animation | Production cadence test: zero follow-up frames when settled; release schedules its animation |
| Folder discovery created an unbounded number of detached readers | Each new folder launched a thread while earlier IO kept running | One lazy IO lane, one running read and one replaceable waiting read; owned worker state survives a blocked call safely | 1,000 queued requests behind a barrier: 999 discarded, newest completed; nonblocking owner destruction |
| Remote filesystem checks could stall the UI | Handle/reparse and attribute checks ran inside `remote_command` | One low-priority validation lane; at most 128 outstanding jobs; permission/open/add epochs and stable-ID/path checks before applying results | FIFO/overload/cancellation worker tests, parser isolation tests and Windows cross-build; native latency remains unmeasured |
| Clearing a queue or revoking shares could admit an older import | Validation and enumeration could finish after the action that invalidated them | Cancel pending additions on clear; permission generation on scans; stop/share changes invalidate work and clear waiting validations | Worker cancellation tests; application paths reviewed and cross-compiled; Windows integration test remains required |
| Slow phones could accumulate application-level historical snapshots | State frames were appended while previous output remained pending | Append a fresh snapshot only once the application output buffer drains; retain existing outgoing caps and nonblocking sockets | Server code review and cross-build; real slow-LAN throughput is unmeasured |
| Back from the shared-folder root could target a menu instead | The root is the empty string and a truthiness check treated it as closed | Use an explicit null check for folder navigation | Source-path review; existing folder browser suite plus optional reconnect browser test |

## Resource and lifecycle policy

- Continuous command slots: volume, subtitle delay/size/position and preview.
  At most one pending value per kind. Settings share an 80 ms send cadence;
  previews additionally wait at least 250 ms. Discrete actions are never replayed
  on reconnect. Final gesture values can bypass the intermediate-value cadence.
- Browser socket output above 64 KiB retires the connection rather than enqueueing
  more commands. This is a congestion recovery policy, not exactly-once delivery.
- Heartbeats bypass the UI event queue and are answered by the authenticated
  server loop. Existing token/origin/path/frame/rate checks remain in place.
- Remote file validation: one active operation, 128 outstanding UI jobs at most.
  Obsolete waiting navigation jobs release their slots; independent additions
  retain FIFO order. Full queues report the existing limit notice.
- Folder discovery: one active read and one latest waiting destination. IO workers
  own their captures and synchronization state, never an App pointer or window.
  Shutdown discards waiting work without joining a blocked filesystem operation.
- Existing six HTTP workers, three image-generation slots, thumbnail cache bounds,
  512 queue entries, four cover-cache entries and preview cache bounds remain.
- Animations, artwork resolution, CSS layout, shaders and decoding options retain
  their existing quality. Paused/hidden/disconnected work is suspended; visible
  interactions retain their animations.

## Reproducible validation

`bash tools/test.sh` runs portable C++ tests and, when Node is present, the two
production JavaScript suites:

- `node tests/remote_lifecycle_test.cjs`
- `node tests/remote_fetch_test.cjs`

`tests/async_worker_test.cpp` exercises real threads with barriers, overload,
FIFO order, replacement, obsolete-job pruning and destruction during blocked IO.
`tests/remote_session_test.cpp` covers JSON round trips, numeric boundaries,
legacy messages and isolation of internal credential/validation fields.

The original page fails both new JavaScript suites: delayed volume order and
stale queue revision, respectively. The revised page passes both. A synchronous
burst of 1,000 volume inputs produced two outgoing messages and retained the final
value; this is a deterministic test workload, not measured Wi-Fi traffic.
A settled disc schedules no subsequent RAF in its cadence test; total phone CPU
or battery savings have not been measured.

Builds use the repository's MinGW Windows x64 Makefile, including the installer.
`python3 tools/verify_release.py` checks the installer hash, compressed payload,
CRC, staged bytes, unchanged engine DLL and corresponding engine source archives.
Validation evidence is stored in `docs/validation-remote-1.3.2/`. The portable
suite completed 95 C++ executables and both JavaScript suites. The new worker
and session tests also passed AddressSanitizer/UndefinedBehaviorSanitizer; leak
detection was disabled, so those runs do not establish leak freedom.

## Browser and native limits

Chromium 150 was installed, but startup was blocked by the execution environment's
Unix socket restriction (`socket() failed: Operation not permitted`). Therefore
no browser viewport, touch-layout, screenshot, or real browser performance result
is claimed. A supplemental full-page DOM simulation successfully opened folders,
suspended work and replaced the connection on resume without page exceptions;
that check is not layout or browser-engine validation.

`node tests/remote_reconnect_browser.cjs` supplies optional Playwright checks for
320×568, 390×844 and 844×390, folder restoration and orientation changes. It uses
a mock PC and must be run where Chromium is permitted. The existing browser suites
remain the broader layout, queue, menu, audio and memory regressions.

Real Windows and Android/iOS testing is still needed for lock/unlock, application
switches, Wi-Fi loss, BFCache, OS sleep, Explorer providers, slow/removable drives,
physical touch gestures and long-run CPU/GPU/memory measurements. A blocked OS
filesystem call cannot be forcibly cancelled; later file operations in its lane
wait for it, while ordinary UI/transport commands remain independent. There is
no claim of whole-process leak freedom or measured native playback improvement.

The existing UI-side cover-cache identity query (`cover_key`), local dropped-file
classification, local missing-queue-file checks and Shell folder display-name
lookups remain potential slow-storage costs. Moving the cover identity lookup
requires preserving retag invalidation and instant cache reuse; no speculative
cache policy change was introduced in this review. Native traces should establish
which remaining call sites warrant a further change.

Recent-file persistence already uses a debounced single writer and bounded reads;
no new defect was demonstrated there. No independent favorites store was found
in this source tree. Fullscreen/miniplayer composition and media decoder lifetime
were reviewed but require native profiling before further behavior changes.
HTTP remains unencrypted, and local processes can still race filesystem paths
after validation, as documented in SECURITY.md.

## Follow-up: disconnect after idle or suspension

The reported phone/Wi-Fi session was not available for inspection. Regression
checks did demonstrate recovery defects in the previous delivered page; these
are plausible contributors, not a measured diagnosis of that physical session.

- Hiding the page stopped timers but left its socket open. The phone now closes
  it immediately and cancels connection timers. Freeze/resume, page restore,
  online/offline, focus and the next touch share the same recovery path. A burst
  of return events keeps one attempt rather than replacing it repeatedly.
  An offline browser flag suppresses automatic retries but does not forbid a
  direct connection attempt on a functioning local network.
- A transport failure during first pairing lost the unconfirmed code, sometimes
  reporting it as wrong without an authentication refusal. The code now survives
  transient failures in memory for at most 120 seconds; explicit authentication
  refusals and PC shutdown still stop automatic attempts.
- Deadlines are bound to their socket. A callback already queued for an older
  socket cannot close a replacement. Old message handlers are also identity
  checked. Commands are still discarded across reconnections, never replayed.
- Each page sends a non-secret instance ID at authentication. An authenticated
  replacement retires older connections with the same credential and instance.
  Separate tabs keep separate IDs; legacy clients remain supported. Instance IDs
  grant no authority without the existing valid token or pairing code.
- The server preserves a readable socket on transient `WSAEWOULDBLOCK`. It copies
  and encodes state only when a client can accept a newer revision, sharing that
  encoded frame across clients in the current server iteration.
- Paused/home idle ticks no longer rebuild the complete phone interface twice
  per second. Blink/sleep animations and failed-cover retries remain. Fresh
  playback snapshots render the timeline; local interpolation runs if snapshots
  lag. Concurrent wake-lock requests are coalesced, and a late lock arriving
  after suspension is released.

Deterministic production-code tests cover 12 simulated hours of idle heartbeat,
200 background/foreground cycles, freeze/resume, offline/online, return-event
cascades, code expiry and transient pairing, obsolete callbacks, and a stale
OPEN socket after a clock jump. They assert one active socket/heartbeat timer
at the end and no connection timers while suspended. The previous delivered
page fails the immediate-background-socket-release regression. These scenarios
are virtual-clock tests, not a 12-hour physical-device soak.

The full-page DOM smoke check still opens folders and restores them after a
connection replacement without page exceptions. Chromium was retried but still
cannot create its required Unix socket in this environment; TCP loopback alone
is available. Browser layout, real browser lifecycle, Windows server session
replacement and real Wi-Fi loss remain unexecuted. No native FPS, process CPU,
RAM or battery improvement is claimed.

Browser lifecycle references: [Chrome Page Lifecycle API](https://developer.chrome.com/docs/web-platform/page-lifecycle-api)
, [MDN online-status limitations](https://developer.mozilla.org/en-US/docs/Web/API/Navigator/onLine)
and [MDN WebSocket client lifecycle](https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API/Writing_WebSocket_client_applications).
