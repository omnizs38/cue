# Validation · Cue 1.3.2

Date: 2026-11-09. Environment: Linux x86_64, GCC 13.3 and MinGW-w64 GCC 13-posix.
Input: the supplied Cue-1.3.2-Pacote(1).zip. Application and installer are rebuilt;
the supplied engine DLL/source ZIP are unchanged.

## Portable application checks

Detailed evidence files are supplied with the source repository rather than installed with the player. Final results are recorded in `validation-1.3.2/portable-tests.txt` and
`validation-1.3.2/sanitizers.txt`. All 93 portable C++ test programs passed,
including new background-work, music-render-cache and seek-dispatch regressions.
Translation/page generation checks are part of `tools/test.sh`.

Existing coverage includes video/audio, full/mini players, both control styles,
queue/panel lifecycle, playback pause/seek behavior, portrait and split windows,
four interface sizes, eight languages, DPI layouts and motion at multiple refresh
rates. Native window, device and shader execution is outside those tests.

The original 90 portable programs passed before changes. The before/after music
UI benchmark eliminated two allocations per steady audio frame and matched all
20 final layer digests. Video allocation counts were unchanged. Raw results are
in `validation-1.3.2/music-ui-{before,after}.txt`; this is not a native playback,
startup, end-to-end latency or process CPU/GPU/RSS benchmark.

## Windows build and native test preparation

The Windows x64 application and installer stub cross-compiled without compiler
warnings using `OFFICIAL=1`. The updated `audio_engine_test.cpp` cross-compiled
against the same wrapper and checks the asynchronous RMS/ZCR metadata map when
run with the real DLL. It was not executed here.

All 93 programs also passed AddressSanitizer/UBSan (including rechecks after fixing the test source-root lookup). These checks used `ASAN_OPTIONS=detect_leaks=0`.
LeakSanitizer was attempted but cannot inspect `/proc/<pid>/task` in this runtime;
its fatal setup error is retained in `validation-1.3.2/leak-sanitizer-limit.txt`.
No whole-application or native leak-free claim is made.

## Release and engine inputs

The local release verifier checks installer SHA256, LZMA decoding, CRC32, payload
entries, staged byte comparisons, the DLL manifest, source ZIP checksum and all
16 source archive hashes/tar structures. The outcome is retained in
`validation-1.3.2/release-verification.txt`.

Engine DLL SHA256:
`7bf665b49e5f60473c7338e3aebb6975a5b854129b47bb8f393ca78656277418`.
Unchanged engine source ZIP SHA256:
`6ce73f8f9b7209e390f45c16f6ec0fbdfab383eddb7a7726656f1f24b4fcb13e`.
The source ZIP's historical binary manifest records a different DLL hash; see
[provenance](../third_party/libmpv/SOURCES.md). No engine rebuild or binary
reproducibility comparison was performed.

## Pending on native Windows

Cold/warm launch, first frame, track-switch latency, physical input timing,
CPU/GPU/memory during actual playback, audio device behavior and image quality
remain unmeasured here. Exercise real video/audio, both bar styles, full/mini
players, split/portrait/resized windows, DPI changes, paused playback and panel
opening/closing. In particular, minimize during all cover-turn stages and confirm
that the selected track opens once, the turn worker stops, and gain/cover/shadow
recover on restore. Follow [the review procedure](PERFORMANCE-REVIEW.md).

No GitHub commit, tag or release was created or published.

## Remote reconnect follow-up

The updated recovery paths and limits are documented in
[REMOTE-STABILITY-REVIEW.md](REMOTE-STABILITY-REVIEW.md#follow-up-disconnect-after-idle-or-suspension).
Current evidence is in `validation-remote-reconnect-1.3.2/`. The recovery tests use
a virtual clock for 12 hours/200 resumes; this does not represent a native soak.
Browser startup is still blocked by the environment's Unix socket restriction.

## Home return follow-up

The portrait-to-home path defers halo rebuilds during the native resize, starts
its geometry clock after preparing the home and skips unchanged geometry samples.
See [HOME-RETURN-REVIEW.md](HOME-RETURN-REVIEW.md) and evidence in
`validation-home-return-1.3.2/`. The 260 ms easing and iris are unchanged; native
before/after frame pacing remains unmeasured.

## Minimized remote follow-up

Playback seeks now use a finite audio-control timer independent of the renderer;
hidden audio transitions, remote preview dispatch and hidden home loads have
separate completion paths. See [REMOTE-MINIMIZED-REVIEW.md](REMOTE-MINIMIZED-REVIEW.md),
`tests/seek_fade_test.cpp`, `tests/hidden_playback_test.py` and evidence in
`validation-remote-minimized-1.3.2/`. The fake-engine harness is not native Windows
validation. The engine and preview cache/decoder limits are unchanged.
