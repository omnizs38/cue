# Validation · Cue 1.4.1

## 1.4.1: the home's settings in a smaller window (2026-11-23)

- **before** (the real Cue under Wine, 3840 x 2160 at 175%, two entries in the list, Settings open, the window made shorter and narrower: 2573 x 1447, 1150, 950, 780, then 1800 x 1447, 1500 x 900, 1300 x 650): as in the author's screenshots, beside the line at 1150 with the end of a name showing to its right, above the line with its picture beside the rows at 780 with "recent" and a title beside it.
- **after**: the same sizes: wherever the panel stands over the list the list is gone, whole; at 2573 x 1447 and 1800 x 1447 (clear of it) it stays; Esc: the list back as the panel fades. `home_settings_test` (every language, four interface sizes, five windows: the list's pixels gone wherever the panel stands over it, 105 of 160, drawn again after), `home_partial_test` (the panel over the list for a moment, now and then: still pixel for pixel with whole frames).
- **the film's Settings** at the same sizes (and a portrait 900 x 1300): on the bar's right edge, above it; the picture beside the rows in a short window; never under the window's buttons. Unchanged.
- **tests**: `tools/test.sh` passed whole; `tools/wine-tests.sh`: 143 of 143.

## 1.4.1: a song into the mini player (2026-11-23)

- **before** (the author's screen recording, 3838 x 2158 at 30 frames a second, two entries): the cover's bright figure tracked frame by frame: 3 and 2 frames with the bar gone and nothing moving, then one frame with the cover 0.6 times its size and about 150 px higher (the full player's composition in the card's window), back the next frame; two frames shown twice in the second flight. The flight itself (ease out with a little overshoot) runs as drawn.
- **the choice**: two versions drawn at 60 frames a second from frames of the recording (the full player, the card with and without its words, the screen behind), at real speed and four times slower; the author chose A.
- **the still layer under Wine** (a test program with the shipped `flight_ghost.cpp`, the first version that copied the screen): shown over the window it copies, then at half size elsewhere, then at half alpha, then gone, captured from the X screen at each step. Wine's screen copy holds no window's pixels (black) and Wine does not show a layered window with only a constant alpha (Windows does), so the copy is made opaque and drawn with its own alpha, which both show. Its cost: the scaling 2.4 ms at the 4K opening window natively, under 1 ms at half size; Wine's own layered windows are far slower (70 ms at full size), so the whole flight can only be timed on Windows.
- **after the first version** (the author's second recording, 12:09, a different song and desktop): the change between two frames at most 15 (45 before), decreasing frame by frame; no frame shown twice (two before); one frame still at the start (three before); the copy of the screen (GDI, at the time) was a few frames old: the bar and the title came back with it for a fifth of a second. Now the still picture is the engine's (`screenshot-raw window`), asked for once the pixels are at their size.
- **the engine's picture under Wine** (the real Cue, 3840 x 2160 at 175%, a song, Ctrl+M): 2573 x 1447, the window's size, no bar and no title in it (written to a file by a throwaway build), in 107 to 366 ms (Wine draws on the processor; past 0.12 s the flight goes without it, as it did once); kept in 150 ms (Wine's layered windows). On a graphics card it should come within a frame or two: cue.log says it (`mini player: the still picture ...`).
- **not under Wine**: the flight itself (Wine has no live window pictures, `DwmRegisterThumbnail`, so it takes the plain flight).
- **tests**: `tools/test.sh` passed whole; `tools/wine-tests.sh`: 143 of 143.

## 1.4.1: the controls' background in the welcome, up next with a line (2026-11-23)

- **the welcome**: `onboarding_test` (four screens by keyboard and by mouse: → picks **none** and ← **solid** on the new screen, each saved at once as `ControlsBg`, Enter goes on to the size; a click on each of the two words, found by scanning the screen as a person would); `i18n_layout_test` (48,616 checks: the new screen's title, line and two words fit at every size, scale and language, the four headings in order). `tools/preview_onboarding.cpp`: the screen with each choice and the size screen after it, at 4K. In the real Cue under Wine (3840 x 2160, 175%, no `state.json`): the four screens by keyboard, the bar with its backdrop and without it over the scene, the size screen showing the choice; `state.json` then said `"controls_free": true`.
- **up next**: `next_count_test` (new: the line half as long at half the time left, still while paused, twice as fast at 2×, none when the next file does not start by itself, the same words a second later draw nothing new). In the real Cue under Wine with two 40-second episodes (`Show.S01E01.mp4`, `S01E02`): **Up next**, the next file's name and the line under them getting shorter, then the second episode. Under Wine at 4K the film played slower than real time, so the notice went at its usual 12 s before the line ran out; on a real PC the 10 s fit inside it, as before.
- **tests**: `tools/test.sh` passed whole; `tools/wine-tests.sh`: 143 of 143 (one new: `next_count_test`). The installer: `make OFFICIAL=1 setup`, `tools/verify_release.py`; Store layout from `make msix`.

## 1.4.1: heavy films, a lighter ambient light (2026-11-23)

- **test films** (made with FFmpeg here): 4K HEVC Main 10 with HDR10 (3840 x 2080, 40 Mbps, a keyframe every 10 s), 4K VP9 (20 Mbps, 10 s), 4K60 H.264 (80 Mbps, 2 s), a 2-hour 4K HEVC 10-bit MKV with three audio tracks (E-AC-3 5.1, AAC, Opus), three subtitle tracks and twelve chapters (5.4 GB), a 30-minute 1080p60 OBS-like MP4 with three named audio tracks (2.8 GB), and a 1080p H.264 film for comparison.
- **measured before changing anything**: the real Cue under Wine at 3840 x 2160 and 175%, the measurement build (not shipped) with new steps for films (the arrows, the digits, pause, full screen, a resize, the mini player, minimized, an audio and a subtitle track), the window's thread timed message by message, the processes' processor time and memory sampled every 0.25 s; the real preview helper (`Cue.exe --thumb-host`) driven through its own pipe with 16 timeline requests and 8 colour samples per film. What Cue sends the engine while a film plays is small: its layers held 0.2 to 1.2 MB in every moment of every film (no window-sized layer; those are the home's and the scenes'). The window's thread: the halo was nearly all of it (a 1080p film playing: 501 ms a second, 18 with the light off). The helper: a colour sample decodes from the keyframe before it: 0.14 s on the 1080p film, 1.1 s on the 4K60 H.264, 5.0 to 5.2 s on the 4K VP9 and HEVC with a keyframe every 10 s (one decoder thread here; the pace already follows that cost, `ColourPace`); its memory 112 MB with the 1080p film, 129 MB with the 4K H.264, 157 MB with the VP9, 266 MB with the HEVC 10-bit. Opening and seeking are the engine's own decoding (software here: Wine has no video decoder); the open times under Wine also carry a Wine-only stall (its shader compiler lacks `atan2` and its report of the failure holds the engine for seconds; the measurement build works around it).
- **the halo, pixel for pixel** (`tools/benchmark_halo.cpp`, the real `Ambient` on real layered windows under Wine, the colours flowing for 6 s at 30 ticks a second, then going out): a model of what each strip window holds, built from every picture Windows is given (whole, or only its dirty part), hashed after every tick: the same hash before and after with a calm and a busy light at the 4K opening window (2573 x 1447, 168 dpi), with that window half off the screen (calm and busy) and at 1280 x 720; what the strips show at the end, laid on a screen: 0 of 8,294,400 pixels differ. (At 1280 x 720 under Wine a whole paint costs about the 14 ms at which the light's entrance turns to opacity alone, so the exit's look changes from run to run there, before as after.)
- **its cost** (same benchmark, several runs): per tick at the 4K opening window, a calm light: Cue 1.9 to 2.0 → 0.4 ms, Wine 11.5 to 12.2 → 3.6 to 4.1 ms, 750 → 93 thousand pixels sent; a busy light: Cue 5.2 to 5.6 → 3.3 to 4.0 ms, 1.87 → 1.59 million pixels; half off the screen and calm: Cue 1.4 to 1.5 → 0.3 ms, 442 → 54 thousand pixels; 1280 x 720 and calm: 0.5 → 0.2 ms, 193 → 24 thousand. The shape: 1,622,636 pixels in 14.3 MB → 159,793 runs in 1.8 MB (plus 0.6 MB for the colour each holds) at the 4K opening window; 21.2 → 1.9 MB maximized; 3.7 → 0.8 MB at 1280 x 720; made in about the same time (14 to 33 ms, before as after).
- **in the real Cue** (Wine, 4K at 175%, the same scripted session twice with each build): a 1080p film, the window's thread busy 501 → 159 ms a second playing, 459 → 190 with the controls up, 464 → 198 over the arrows, 529 → 125 after a pause; its poll 19.7 → 5.0 ms on average, 86 → 27 ms at the 99th percentile; a 4K HEVC film (its colours sampled every 15 s, so the light flows less): 118 → 79 ms a second playing, 135 → 43 after a pause. A screenshot of each while the film plays: the light is there, as before.
- **tests**: `tools/test.sh` passed whole; `tools/wine-tests.sh`: 142 of 142. The installer: `make OFFICIAL=1 setup` (107 files; only `Cue.exe` and the documents differ from the previous 1.4.1 build; the engine is the same file); installed under Wine with `--silent`, it starts as 1.4.1 and plays a film with its light. Store layout from `make msix`: `cue-edition:microsoft-store`, version 1.4.1.0.
- **only on Windows**: the halo's cost through DWM (Wine's part above is Wine's), the processor, the graphics card and the memory in Task Manager with the author's own films.

## 1.4.1: a lighter music player (2026-11-22)

- **measured before changing anything** (the author's list, 22/11): the real Cue under Wine at 3840 x 2160 and 175% and at 1920 x 1080 and 125%, a measurement build that times every message the window's thread handles and counts what it sends the engine (not shipped), one scripted session per screen: opening, playing, the next song, the list opening and open, a resize, the six shapes, minimized, under a full-screen window of another process, the ambient light off, the spectrum alone, the cover alone, then a film. The window's thread is not where the weight is: 2% busy playing, its longest wait under 10 ms; 18 to 28% while resizing (22 ms at most); a song change under 7% (once 254 ms on the laptop, waiting for the engine; on the author's PC, `--log`, 22/11 23:27, a song plays 145 to 230 ms after it is asked for and the window waits 16 ms at most). The engine's picture is: the graphics card's part measured with Cue's own shaders and scene in the engine's renderer on the processor (Wine's shader compiler has no `atan2`, so the music's passes never run under Wine), per frame at the 4K opening window 970 for a song (60 a second) against 464 for a 1080p film (24 new ones): about five times the work a second (3.8 at the laptop's 1280 x 720). The processor's part (FFmpeg, the graph Cue sends, one core): 2.8 s for 30 s of music. Minimized or covered, the picture was drawn as on screen.
- **the passes**: `music_passes_test` (new): the passes' `WHEN` evaluated as the engine does: a centred song 2 full-window passes (4 before: the test fails on the old shader), the cover alone 2, the spectrum with the list in black and white 2, the veil or black and white alone 1, nothing 0; the room's light never twice; the light passes read the picture through `room()`; the merged pass keeps cover, edges, veil, black and white in that order. The same frame (a detailed cover, its spectrum and wings) drawn by the old and new shader at 2573 x 1447 in six states: centred 23 pixels of 3,723,131 differ by 2 levels, the rest by 1 at most (a tilted cover 26; the list in black and white, the cover alone: 1 at most; the spectrum alone and the edges alone: identical).
- **the cost**: per frame at the 4K opening window, two runs each: 947 / 891 before, 692 / 699 after (the passes alone 825 / 823); at 1280 x 720: 310 / 291 before, 249 / 252 after. `device_tier_test`: `catmull_rom` on a dedicated card.
- **a song nobody sees**: a first version added a filter step after the picture's graph (`fps=2`); on the author's PC the song paused and came back on minimizing, and the spectrum came back broken (23/11 00:26). Now only how the engine draws changes (`bilinear`, the light show out). Natively, with a close version of the engine (0.37): three rounds of hidden and back while a song plays, the sound's clock (`audio-pts`, every frame) never off by more than 80 ms, no `video-reconfig`, no `audio-reconfig`, the core never idle; the old step, for comparison, reconfigured the picture each time. In the shipped engine under Wine: minimized twice and under a full-screen window of another process, only `file-local-options/scale` and `dscale` change in the engine's log (no pause, no restart, no filter), "music unseen" / "music in view" in `cue.log`. The window coming back drawn as it was hidden and half a second later, at 4K (`volta-da-janela.png`): the room's light, the shadow and the edges there at once, the light show coming in as in 1.4. Per frame minimized at the 4K opening window, two runs each: 510 / 518 before, 301 / 316 after. `smooth_playback_test`: no `vf`, no `@cuerest`, never the graph; a film's options never touched; the shadow and the room's light kept.
- **the helper**: film, then a song: the helper ended as the song was set up (92 MB → none, under Wine); a film after the song started it again (colour samples at 4 s).
- **tests**: `tools/test.sh` passed whole; `tools/wine-tests.sh`: 142 of 142 (one new: `music_passes_test`). The installer: `make OFFICIAL=1 setup`, `tools/verify_release.py`; Store layout from `make msix`.

## 1.4.1: subtitles in the picture, the crash at a worker's start (2026-11-22)

- **tests**: `tools/test.sh` passed whole. `tools/wine-tests.sh`: 141 of 141 (one new: `detached_thread_test`; `subtitle_lines_test` went with the file measurement it tested). `motion_cadence_test`: the lift's return takes the dock's own 0.2 s at least (the measured placement makes its way shorter). `subtitle_geometry_test`: the sizes on a 4K screen at 175% (at 52: the opening window 105 px, maximized 151, full screen 156, half the screen 83, a third 56, a 960 x 540 window 42, a portrait video 74; at 32, the author's, half the screen and a square window at the cube root's size); every size of the style from 24 to 80 with letter spacing −3 to 10, six shapes, windows from 320 x 240 to 3840 x 2160: never smaller than in proportion, a line of 64 letters inside the breaking width (or the opening window's breaks kept), a zoom above the opening window, continuous and growing with the window; the side margins (12 at the opening window, the mini player's 25 / 8 as before); line spacing a share that grows with the setting, the tightest within each face's measured clearance; letter spacing +10 at size 72 sent as size 44, spacing 10 and scale 72/44 times the window's, an ASS in its own style at scale 1; the engine's block position at two window heights; the picture's position at three letter sizes and one to four rows: unchanged with no band, inside the picture where it stands in it with bands, window and picture positions converting back and forth, under the picture from 100 to 200 (never over it, or at the window's foot when the block is taller than the band), no jump over 330 heights; a drag in the player at sizes 32, 52 and 80 from inside the picture into the band, then the opening, square, narrow and half-screen windows (under the picture, or at a bandless window's foot), and back up into the picture. `state_limits_test`: 44 with letter spacing 0 to 36 and +1 in the prefs and a file's own style, a chosen letter spacing kept with the new size, 60 kept, a test build's 52 (schema 6) to 36, the author's 32 from a test build kept, a 44 chosen with schema 7 kept; a held file readable again: the session's volume and played film over the file's language and its film the backup never knew. `logic_test`: the hand over the subtitle's real lines with the engine's placement.
- **the real Cue under Wine at 3840 x 2160 and 175%** (an SRT beside a 16:9 film, the author's line "Eu me viro do avesso Ou faço de joelhos" and a 95-letter one): the author's four windows (the opening window, half the screen, a square window, a narrow one), side by side at the same scale with his drawing: at his size (32) the words take 32, 38, 45 and 58% of the picture's width (his drawing: 32, 40, 44, 55); inside the picture in all four; dragged under the picture (saved at 134), under it in the three with bands and at the foot of the opening window; at 52, 72 and 80 (the largest) inside the picture everywhere, the 95-letter line on two lines in every window at 72.
- **the style's settings in every window** (with the zoom rule of the same day, the default background box): line spacing −16, 0, +12, +24: the same look at every size, −16 clear of the descenders; letter spacing −3, +4, +10: +10 gave two lines at the opening window and three at half the screen before the fix (the engine refused spacing over 10 as the window grew).
- **the laptop** (Wine at 1920 x 1080 and 120 DPI, size 32): the opening window, maximized, half and a third of the screen and the portrait video's 510 x 907 window, before and after, at the same scale: inside the picture, larger in the smaller windows (a 95-letter line, on one line at the opening window, takes two there); the portrait video as before.
- **where the engine draws a subtitle** (the real Cue under Wine, a line of capitals with no descenders): the last line's baseline at positions 30, 50, 73.4, 93 and 100, letter sizes 64, 93, 145 and 209 px, window heights 1447 and 2088 (20 captures): `subtitle_baseline` within 0.5 px of each; two and three lines at 30, 73.4 and 93 (9 captures): the last line in the same place, one letter size between lines. the first model (fitted at 93 alone) was 35 to 100 px too high away from 93.
- **the controls and a subtitle under the picture** (size 32, saved at 134, a two-line subtitle, the controls showing): before, at half the screen, the controls covered its second line; now the opening, half-screen, square and narrow windows have half a line of its letters between the words and the controls. `subtitle_geometry_test` checks it in the four windows at sizes 32, 52 and 80.
- **the crash at a worker's start**: a program making 160,000 short workers from four threads, at most 48 alive, under Wine: with `std::thread(...).detach()` the same access violation as the monkey's (reading 0 at `execute_native_thread_routine+6`) in 6 of 7 runs; with `start_detached` 3 of 3 runs clean (at the end of each, under full load, a few dozen workers were still waiting to run or could not be made: the same with `std::thread`; with 2,000 workers, all ran in 2.6 s).
- **the Store's thumbnail handler** (taken out again, 23/11): on the author's PC MakeAppx refused the manifest that named Windows' handler `{9DBD2C50…}` on the package's file types (C00CE1A2: the handler must be a COM class the package declares). The package is as in 1.4 there; `make msix` writes no `ThumbnailHandler`.
- **only on Windows**: the Store package's thumbnails and its acceptance, the subtitles on the author's 4K and laptop screens, a `state.json` held by a real antivirus.

## 1.4.1: the welcome on a laptop, the update answer, long titles, track names (2026-11-19)

- **tests**: `tools/test.sh` passed whole. `tools/wine-tests.sh`: 138 of 139 built and passed under Wine; `intro_match_test` needs `-D_USE_MATH_DEFINES` with MinGW (as in 1.4) and passed with it: 139 of 139. `logic_test` checks the new track names (an OBS recording's names, a name used twice, a track with none).
- **the welcome at 125%** (Wine at 120 DPI, 1920 × 1080, the installer's own run): in 1.4 its eyes measured 318 × 244 px, the 100% screen's page; in 1.4.1 426 × 320, the page `tools/preview` draws for that window. its three screens checked; 100% is unchanged (the same scale either way).
- **check for updates from the home's settings** (Wine, the check refused by the network: "GitHub asked for a break"): the row says "checking for updates…", then the answer for 3.5 s, then "check for updates" again; the sheet keeps its width; the home's **open a file** and **settings** where they were. the up-to-date answer with its tick drawn by `PlayView` in the 8 languages: every one fits the sheet.
- **continue on the home at 125%** (Wine, 1.4 and 1.4.1): the card shows for a 12-minute film left at its start. a film under 10 minutes has no **continue** by design (`resume_point`, `kShortClip`), in every window.
- **a song's long title**: `tools/preview_music.cpp` at 120 DPI over its 24 windows with an 80-character title: under the cover it now runs to the window's width at the type's size; titles that fit are drawn as before; the wide and low and the strip compositions keep their column (the playback is beside the words there).
- **track names**: an MP4 with three audio tracks named the way OBS names them (`trak/udta/name`): the engine gives them as `name`, and the audio menu says Game, Microfone, Discord. the recording's tracks are still mixed as before.
- **a film from a resized home** (the real Cue under Wine, 1920 × 1080): the home narrowed to 900 × 560 by its size, a 1280 × 720 MP4 opened into it: the previous build put the film's window at 124, 96, this one at 317, 178, the middle of the screen. the home dragged by its title to the left, then the same file: the film grows from the middle too (the first version kept it there; changed after the author's recording, 22/11).
- **the helper when memory is short** (the real Cue under Wine, `--low-memory-test`, a 3-minute 720p film): the helper started at 3 s, ended with the signal, and the timeline's tip showed "01:13" alone; 30 s after the signal it was running again and the tip showed the picture at 01:50. `hidden_playback_test` knows the new state.
- **`Applications\Cue.exe` and Explorer's pictures** (Wine): a 1.4.1 started over a registration of version 5 writes it again 5 s in: `CueMediaTypes` `video-audio-6` and both `ShellEx` handlers (`{e357fccd…}`, `{BB2E617C…}`) naming `{9DBD2C50…}`. the pictures themselves only on Windows.
- **the list's names**: the film list drawn with `QueueView` at 200% with six real-world names (two episodes, a film with its year, a music video, a trailer's long title, an underscored file): clean titles, the episode or year on the grey line; a long one whole on two lines after half a second under the pointer. then, after the author's look at a portrait window's list ("too cramped"), three ways drawn side by side at 205 art px and 300%; the one chosen (a smaller picture in a narrow list, the title cut at its end, the remove button's room only on its row, a music video's title and artist): every one of seven real names whole in the portrait list; the landscape list as wide as before.
- **subtitles as the window changes** (the real Cue under Wine at 3840 x 2160 and 175%, a two-line subtitle): half the screen, the opening window, maximized and a portrait video drawn side by side at the same scale, for each step of the study (the 1.4.1 first fix, the square root alone, the rule chosen); the opening window and maximized as in 1.4. `subtitle_geometry_test`: the sizes of the study's table (76, 58, 88 px; a portrait video 58 px on two lines), every window from 240 x 180 to 3840 x 2160 in five shapes with a 42-letter line inside the width, another face with letter spacing, the mini player's rule unchanged.
- **#21 applied on 1.4.1**: without conflicts; `tools/wine-tests.sh` 140 of 140 (`intro_match_test` now builds with MinGW as it is; `remote_path_win_test` is new: Wine 9's `mklink /J` answers 0 without making the junction, so the test now skips the link check when no folder is there, as it already did when `mklink` failed); `store_io_test` and `state_limits_test` with its cases plus one for the change on top (a state that cannot be read, with a backup beside it: the backup's settings, no save).
- **the release**: `make OFFICIAL=1 setup`, `tools/verify_release.py` (hash, LZMA, CRC32, 107 files byte for byte, the DLL's manifest, 16 engine sources). the installer opens under Wine as v1.4.1 · .tinypeep, installs, and opens the welcome. Store layout from `make msix`: `cue-edition:microsoft-store`, version 1.4.1.0.
- **only on Windows**: the welcome at the laptop's own DPI on real hardware, an OBS Hybrid MP4 straight from OBS, the GitHub updater from 1.4 to 1.4.1 once the release exists.

## Closing the release: music shapes, the film's bar, the light, slow disks (2026-11-17)

Reviewed by the author on his PC (Windows, 4K at 175%) from test builds of the real Cue, step by
step: the music player's five shapes, the film's bar, the keyboard notices, speed and time left,
skip intro, the ambient light's breath, flow and reveal, the mini player, the home's settings.
His last build in normal use (video and music): fluid, no problems found.

- `tools/test.sh` passed on the release source (every portable test, the catalogs in 8 languages,
  the phone page's JavaScript tests).
- Monkey under Wine and Xvfb: about 95 minutes, ~19,000 actions (files, songs, folders, the home,
  next, resizing, full screen). No hang from Cue: the stops over 3 s were Wine's own
  `DestroyWindow` wait on a child window of another thread and the engine drawing slowly with
  llvmpipe. **One crash, once** (seed 208): an access violation reading 0 at the start of a
  `std::thread` (`execute_native_thread_routine+6`); the same seed again and 30 more minutes: not
  reproduced. Left open, not changed.
- Memory and handles (Wine, 13–16 min sessions): the Windows side (Cue, engine, ffmpeg)
  175 → 197 MB and flat for the last 7 min; threads 24–25, stable; +2 handles a round, the COM
  RPC pipe Wine keeps per apartment (every `CoInitializeEx` has its `CoUninitialize`).
- Messages per second on the UI thread (Wine, relay): home at rest 18.7, film paused 6.9, song
  paused 7.6, paused and minimised 10.5; a film minimised 26.3 → 10.7; a film playing with no
  ambient light and nothing moving: poll 30.3 → 10.0/s.
- The hang on Windows when the engine's window was destroyed with the keyboard focus in it:
  confirmed on the author's PC with `focusdeadlock.exe` (focus in it: stuck 5 s; mouse capture
  only: fine) and fixed (the click on the video no longer moves the focus).
- Slow disk simulator (`tools/slow-disk/`, an `LD_PRELOAD` under Wine: 14 ms a seek, 25 MB/s, a
  drive that sleeps after 30 s and wakes in 2.5 s): no stop after a pause on an external drive
  (it stopped 1.95 s); opening with Cue closed from a sleeping drive: the window held 2645 → 349 ms;
  a disk at 5 s a query with Cue open: ~20 s → 156 ms at worst; the latest choice wins.
- `tools/audit_music.cpp` (~26,000 compositions of the music player) and `tools/audit_menus`
  (1,472 menus, 8 languages, 4 sizes, 100 and 175%): no ink outside the window, no layers over one
  another, no cut words after the fixes. Left as it is: the list's bar in German and Japanese at
  the large sizes still shortens "+ Ordner" / "+ フォルダー" (a shorter text would be needed).
- Ambient light, measured before and after under Wine (borders recorded at 60 fps,
  `tools/ambient_measure.sh`, `tools/ambient_analyze.py`, `tools/benchmark_reveal.cpp`): the same
  frames read; no darkening in the middle of a change; the halo's paints about the same.
- The release files: `tools/verify_release.py` on the installer (every file against `build/Cue`,
  the engine's manifest and its sources), the engine source archive byte for byte the 1.4 one
  (the same engine as 1.3.2), the installer run in Wine, **about Cue** showing v1.4 in the GitHub
  edition; the Store layout built with `STORE=1` (`cue-edition:microsoft-store`).
- Only Windows shows (not measured here): the sound coming back when headphones are unplugged or
  plugged in (no audio hardware here), the taskbar thumbnail's buttons in the light theme (Wine
  has no taskbar), frame pacing on a real GPU, dragging the window's edge live (Wine shows isolated
  black frames from the engine recreating its surface; a plain GDI window does not).

## Review of the redesign: Ogg, HDR, borders, speed (2026-11-14)

- `tools/test.sh` passed (with the new `ogg_kind_test` and the new checks in `raster_test`,
  `cover_art_test` and `home_continue_test`); `tools/sanitize.sh` (ASan, LSan, UBSan) and
  `tools/fuzz.sh 4`: clean. `tools/wine-tests.sh`: all 115 Windows builds passed.
- Against the shipped engine under Wine: `tests/ogg_engine_test.cpp` (new): Vorbis, Opus and FLAC
  in Ogg play and their cover (METADATA_BLOCK_PICTURE) is the engine's album art, and Cue's own
  reader finds it; a Theora film named .ogg is a film for the engine and for `ogg_kind`.
  `tests/hdr_engine_test.cpp` (new): HDR10 and HLG are named (BT.2020, PQ / HLG, peak, MaxCLL);
  the preview helper's pictures of them are washed out (HDR10: saturation -56%, luma -13%; HLG:
  saturation -41% against the same picture in SDR): it receives PQ/HLG with no tone mapping. A
  prototype in the test (16-bit read, tone mapped) brings the saturation back (0.94 against 0.97).
- The engine's libplacebo is built without Dolby Vision (`--auto-features disabled`: `dovi` off,
  `PL_HAVE_DOVI` undefined), so mpv ignores the RPU: profile 5 cannot show its real colours;
  8.1/8.4 play their HDR10/HLG base; 7 its base layer. Not changed (an engine rebuild).
- The real Cue (this build) under Wine and Xvfb: the welcome, a Theora .ogg playing as a film, Vorbis,
  Opus and FLAC-in-Ogg songs with their covers, and the home listing the Theora file with its frame
  ("watched") and the songs with their covers; `cue.log` writes the new "song open" lines.
- Portable benchmarks (`tools/benchmark_redesign.cpp`, new; `tools/benchmark_frames.cpp`), the
  same pixels before and after: the home at 4K 3.05 -> 2.34 ms a frame (pointer on the one to
  continue 2.92 -> 1.85; the list 2.36 -> 1.51), the welcome 4.17/2.93/3.05 -> 3.44/2.30/2.46,
  what's new at rest 3.56 -> 2.70, the home fading in from a film 8.36 -> 7.14.
- Borders at half scales: `raster_test` draws a panel and a switch at 1, 1.5, 2, 2.5, 3, 3.5 and
  4.5 px per art px at odd and even places and checks the same thickness on every side (it fails
  on the previous drawing: 2/1 px at 1.5).
- Only Windows shows: the real HDR display, a song's opening times on the author's PC (the new
  `cue.log` lines), the borders in the real window.

## The redesign: home, welcome, what's new, panels (2026-11-13)

Reviewed by the author part by part, on his PC, from test builds of the real Cue (the welcome,
what's new, the one panel, the home's **continue**, the subtitle's dimming and style, the
switches' light, **show pictures** and its switch).

- `tools/test.sh` (112 programs) passed, with the new `home_continue_test`, `news_view_test` and
  `subtitle_dim_test`; `onboarding_test` and `i18n_layout_test` over 8 languages, 4 sizes and
  10 windows. The touched views' tests and `tools/fuzz_views.cpp` also passed under
  AddressSanitizer, LeakSanitizer and UBSan.
- Cross-compiled with MinGW-w64 (`make OFFICIAL=1`, `make OFFICIAL=1 setup`, `make msix`);
  `verify_release.py` checked the installer against the build and the engine's sources.
- Not run here: Wine was not available for this round, so `tools/wine-tests.sh` did not run.
  What only Windows draws was seen on the author's PC, not measured here: the window's real glow
  in the welcome, the style's sample in the real font (GDI), the subtitle dimmed inside the
  engine, and covers read already reduced by WIC (`IWICBitmapSourceTransform`; any other format
  falls back to the full read). `TESTES-NO-PC\2-TESTES-NATIVOS.bat` runs
  `recent_thumbs_win_test`, whose JPEGs go through the same reader, on a real PC.

## A film opened with Cue closed (2026-11-12, night)

- The author's screen recording: the window appeared in the home's 16:9 (the home drawn in it for
  a few frames), went black, vanished for a frame and came back in the film's shape.
- The same in the real Cue under Wine, recorded at 30 frames a second (`x11grab`), 1.4 before
  and after the change: a 2.39:1 MKV and a 9:16 MP4 now open in their shape from the first frame
  with nothing of the home drawn; an MPEG-TS (no header read) opens in 16:9 without the home,
  then takes its shape as before. The log says `window in the film's shape from its header`.
- The header reader against ffprobe's display aspect on real files (ffmpeg: plain, 2.39:1,
  anamorphic in MKV and MP4, a phone video turned in its matrix, 9:16, WebM, a 4K HEVC MP4):
  the same shape for every one. `media_header_test` (built files, every truncation and 40,000
  corruptions, also under AddressSanitizer and UBSan).
- `tools/test.sh` (109 programs) and `tools/wine-tests.sh` (111 Windows builds) passed.

## The home's Recent list, with less text (2026-11-12, evening)

Option A of three shown to the author (pictures from Cue's own drawing code with his recents).

- `recents_view_test`: titles as the home shows them, including what must stay (*Song (Live at
  Wembley)*, *Film (2019)*, *Song (Official Video)*, *Abbaesque - Thing* by ABBA, a title that
  would come out empty); also under AddressSanitizer and UBSan.
- The real Cue under Wine with six recents like the author's (songs tagged with their artist, a
  film and an episode left halfway, one entry from yesterday): *Music Baby / Jane Remover*,
  *Crimson / Hunnygloss*, *DNA / Number Teddie · ontem*, *RuPaul's Drag Race / S18E15* with
  *faltam 1 min*, the film's title in the middle of its entry beside *faltam 2 min*; no song
  lengths, no *hoje*.
- `tools/test.sh` (108 programs) and `tools/wine-tests.sh` (110 Windows builds) passed.

## The phone remote's side buttons and the timeline previews (2026-11-12, later)

Same environment. What changed is in CHANGELOG.md (1.4: the first three entries).

- **The phone's « in a queue**, with the real Cue under Wine and the real page in Chromium (a
  phone paired through state.json): from a fresh page on the third song, « went to the second
  song and the next « paused it (`queue-play` of the playing song); after the change « asks the
  PC for its previous file (`prev`): third, second, first, then nothing; » the same way; a film
  in up-next mode back to the song before it, at once. `tests/remote_side_buttons_browser.cjs`
  fails on the page before the change (its first «) and passes after; `remote_test` covers the
  command and the state's `prev`.
- **The wheel**: music has time and volume; in volume mode « and » send the volume in steps of 5
  (0 to 125), for films and songs, checked in the real Cue (100 → 95 → 90 → 95 → 100 → 105 on a
  film, the film's time running on). `remote_ring_browser.cjs` updated: up-next mode is a film's.
- **Timeline previews**, with the real helper (`--thumb-host`) and the shipped engine under Wine,
  76 requests per file (16 jumps, a 30 s sweep right, a 30 s sweep left), the build before
  against the build after: the same time and the same pixels (hashed) for every answer, on
  H.264 and HEVC 10-bit MKV, MP4 with B-frames (an edit list's shift found), MKV, WebM VP9 and
  MPEG-TS (no index: answers from the engine's own only). Decodes in the two sweeps, before →
  after: 4K HEVC MKV and MP4 60 → 0 (the jumps had decoded those keyframes), 1080p HEVC 60 → 0,
  1080p H.264 MKV 60 → 12, VP9 60 → 2, TS 60 → 36. Decoder threads, the same 76 requests:
  slice threading for HEVC 1080p 66 → 54 ms and 4K 170 → 140 ms a request (median, 2 threads);
  H.264 kept frame threading (55 against 68 ms).
- In the real Cue, the same pointer path over the timeline (a slow sweep, a fast sweep back,
  small moves) with `--log`: pictures shown 90 → 135 (H.264 1080p), 86 → 157 (HEVC 1080p), 48 →
  129 (HEVC 4K); the helper busy with the timeline 4.5 → 2.3 s, 4.8 → 1.3 s, 7.2 → 2.2 s.
- `tests/keyframe_index_test.cpp`: Matroska and MP4 files built byte by byte, every truncation
  and 40,000 random corruptions (no read past the file, under AddressSanitizer and UBSan too),
  the span rule, calibration, trust and dropping.
- `tools/test.sh`: 108 C++ test programs and the JavaScript checks passed. `tools/wine-tests.sh`:
  the 110 Windows builds passed. The 12 phone-page browser tests passed.

## Second round (2026-11-12)

Same environment (Linux x86_64, two cores, GCC 13.3, MinGW-w64 GCC 13.2, Wine 9.0, Chromium,
Node 22). What changed is in PERFORMANCE-REVIEW.md, "second round".

- `tools/test.sh`: all 107 C++ test programs, the translation and phone-page checks and the
  JavaScript regressions passed. New: `colour_pace_test`, `poll_pace_test`, `home_partial_test`
  (36 random sessions, 32,400 frames, each drawn the way the app does and compared pixel for
  pixel with a whole new frame), `helper_rest_test`, `remote_idle_test`, `system_events_test`,
  `audio_add_test`;
  extended: `media_geometry_test` (`demux_shape`), `episode_advance_test` (`stopped_short`);
  `smooth_playback_test` now finds the poll's pace in `set_poll`.
- `tools/wine-tests.sh`: the 109 Windows builds of the portable tests passed under Wine.
- AddressSanitizer, LeakSanitizer and UBSan on the new and touched tests (the canvas clip,
  the home's partial frames, the raster, the paces): clean.
- The phone remote's browser tests and `tests/remote_e2e_harness.cjs` against the real server
  (with its new loopback bell) under Wine: passed.
- The real Cue with the shipped engine under Wine, the build of the previous delivery against
  this one: a film's picture opening at its first frame (5 and 7 runs), a file cut short keeping
  its place, the colours' pace on long and short GOPs, the window's wake-ups paused, on a still
  home and behind another window (a counting build, not shipped), the preview helper ending
  after a minute idle and starting again with the next film, the remote's server idle and its
  stop.
- `benchmark_frames`: the home's final pixels hash the same as before in every case.
- **Load audio track**, in the real Cue and in `tracks_engine_test` against the shipped engine:
  a sound longer than the film kept it on its last frame to 00:20 of 00:12 before, now the film
  ends at 00:12; the same file chosen twice is selected instead of listed twice; a file that is
  not audio is refused and said so; a sound added to one film is gone with the next file.

Not reproducible here, checked by reading and by source tests: cloud-only songs on the phone and
in "up next", full screen after a display change, the place written on sleep. Steps for the
author's PC are in the delivery's report.

## First round

Date: 2026-11-11. Environment: Linux x86_64 (two cores), GCC 13.3, MinGW-w64 GCC 13.2,
Wine 9.0, Chromium (Playwright), Node 22, Python 3.12. Input: the 1.3.2 package on the
author's PC (GitHub `main` plus the Store edition). The engine DLL and its source ZIP are
unchanged (SHA-256 below).

## Portable tests

- `tools/test.sh`: all 98 C++ test programs, the translation and phone-page checks and the
  JavaScript regressions passed. New: `remote_auth_test` (SHA-256/HMAC against FIPS and
  RFC 4231 vectors and 200 Python digests, the protocol's values, header parsing, replay window),
  `remote_auth_page_test.cjs` (the page's own SHA-256/HMAC against Node's crypto for every
  length 0–300 and keys of 0–129 bytes), the store's sealing, migration and stable saves in
  `remote_test`, allowed update hosts in `update_test`, and the exact fast blending in
  `raster_test` (8.4 million source/destination pairs).
- `tools/sanitize.sh` with AddressSanitizer, UndefinedBehaviorSanitizer **and LeakSanitizer**
  (`detect_leaks=1`; the 1.3.2 environment could not run LeakSanitizer): every test clean.
- `tools/wine-tests.sh` (new): the same 98 programs built for Windows and run under Wine, so the
  `_WIN32` paths run too (BCrypt, DPAPI, `ReplaceFileW`, wide paths). It found one real bug,
  fixed in 1.4: MinGW's `std::filesystem::copy_file` refuses to overwrite on Windows, so the
  settings backup was never refreshed and every unchanged save reported a failure.
- `tools/fuzz.sh` (new): random input under AddressSanitizer and UBSan. The player and the home
  with NaN, infinite, negative and huge times and volumes, broken UTF-8, 0–8 tracks, the mini
  player, full screen and every interface size, at sizes from 1×1 to 1920×1080 (about 1,100
  rounds of 400 events); the news, remote-pairing and welcome screens and "up next" at any
  size and scale (160 rounds); and everything the network can send the remote, mutated: HTTP
  requests, WebSocket frames, JSON commands, the signed header, file names for series and
  listings (1.5 million iterations). It found one real bug, fixed in 1.4: "up next" froze Cue
  in a window under 200 px tall at 100% (a dotted box drawn with a 0 px step). Nothing else.
- Chapters (`chapters_test`): breaks on the timeline, the preview's name, Page Up / Page Down
  (the 3 s rule, presses in a row, other jumps in between, the first and last chapters, files
  with one chapter or none), the "…" row only with chapters, the list (marked, following
  playback, click, Esc, a new file without chapters, 120 chapters scrolling to the one playing)
  and both menus whole (more than 300 cases) across every language, interface size, film and song.
  The shortcut sheet holds the new entry in all 480 of its windows (`shortcuts_test`).
  The preview names the chapter under the pointer even when its picture comes from a keyframe
  in another chapter; the Settings choice is saved and offered for films and songs; an open
  menu keeps Page Up / Page Down; the list closes when the window becomes the mini player.
- Settings' "i" (`settings_morph_test`): in 14 windows and scales, both ways, the corner the
  menu hangs from does not move by one pixel, the box only grows or only shrinks, and the
  glide's last frame is the menu itself; no glide with reduced motion.
- Settings' short list (`settings_examples_test`): for a film and a song, Settings shows one
  **interface** row naming the size and background in use; it opens the interface's page, which
  stays open across sizes. The page itself, in 180 windows, scales, languages, films and songs,
  with the examples on and off: the same width and place as Settings (and back), the picture where
  Settings had it and the size's first, two groups each with its icon and none named like the
  page. `ui_scale_test`, `free_controls_test`, `about_test` keep the page's
  choices working.
  `tools/fuzz_play.cpp` adds random chapter lists (empty, unnamed, zero-length, 140 long,
  changing mid-play) and Page Up / Page Down.
- `clang-tidy` (clang-analyzer and bugprone checks) over every source file, Windows code
  included (MinGW headers): no real defect; its leak reports are messages handed to
  `PostMessageW`, freed by their receiver or deleted when posting fails.

The 1.3.2 package as received failed one test (`bar_place_test`): it read `platform.cpp` for a
sequence the Store edition had moved inside `#if`. The test now checks the same order in the
new shape.

## Browser and end-to-end

- The eleven optional Chromium tests (`tests/remote_*_browser.cjs`, `remote_memory.cjs`) ran and
  passed (they could not run for 1.3.2), updated to the challenge and to signed requests, which
  their fake servers verify with Node's crypto.
- `tests/remote_audio_browser.cjs`: music's centre shows the 7x7 note, the icon and its mode
  dots are centred together in the button (8 px apart), and play/pause is as tall as «.
- `tests/remote_auth_browser.cjs` (new): file and folder names with apostrophes, brackets,
  accents, CJK and emoji are signed exactly as the browser sends them; replayed, moved and
  forged requests are refused.
- `tests/remote_e2e_harness.cjs` (new): the real Windows server (`tools/remote_harness.cpp`,
  under Wine) with the real page: pairing by the link's code, the challenge answered without
  the credential, signed folder and thumbnail requests accepted by the C++ server, replay /
  moved / unsigned / Bearer refused, a wrong proof closed with 4001, the derived session key
  accepted and ended with its connection, and a real 1.3.2 page reloading into 1.4 while
  staying paired.

## Engine (shipped DLL, under Wine)

`tests/tracks_engine_test.cpp` (new): `audio-add <file> auto <title>` adds an external track
without switching, lists its file and title (what Cue matches to choose it), plays it when
chosen, and reports a missing file as a failed command; chapter lists from MKV and MP4 carry
their titles and times, and chapter jumps and steps work.

## Builds

- Windows x64 application, GitHub and Store editions, and the installer stub cross-compiled
  without warnings.
- `tools/verify_release.py`: installer SHA-256, LZMA, CRC32, 107 payload files, staged bytes,
  DLL manifest and the 16 source archives of `Cue-1.4-Engine-Source.zip` (the 1.3.2 ZIP,
  byte for byte) verified.
- Store layout (`make msix`): the manifest is 1.3.2's with version 1.4.0.0 (identity, publisher,
  display name, architecture, file types, firewall rule, capabilities unchanged); the same 264
  files; `Cue.exe` carries `cue-edition:microsoft-store` and no GitHub update address.

Engine DLL SHA-256: `7bf665b49e5f60473c7338e3aebb6975a5b854129b47bb8f393ca78656277418`.
Engine source ZIP SHA-256: `6ce73f8f9b7209e390f45c16f6ec0fbdfab383eddb7a7726656f1f24b4fcb13e`.

## Pending on a real Windows PC and phone

MakePri/MakeAppx packing and the signed test install of the Store package, the native test
executables (DPAPI and CNG on real Windows), resource use while playing (CPU, memory, GPU),
the GPU path, a real phone on real Wi-Fi, and a look at the new "load audio track" rows and the chapters with a real film.
Wine and Chromium in a container are not Windows and a phone.
