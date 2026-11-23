# Changelog

## 1.4.1 — 2026-11-22

- **the welcome on a laptop**: at 125% or 150% display scale it came at the 100% screen's size, a step smaller than the home and its own controls. the window's first size reaches the interface from inside `CreateWindowExW`, before Cue had asked the screen's DPI (`dpi_` was 96 there), and the welcome kept the page scale it made then (it was made again only for a new window size). the DPI is now read from the screen before the window is made, and the welcome's page scale is made again when the interface's own scale changes under the same window (`OnboardingView::page_s_base_`). at 1920 × 1080 and 125% the welcome is 2 px per art px, as the home (it was 1.5); 100% is as before. checked in the real Cue under Wine at 120 DPI.
- **check for updates from the home's settings**: the answer ("you already have the latest version", no connection, GitHub busy) took the place of **open a file** on the home's line, so **open a file** and **settings** went away, and it stood under the settings panel, where it could not be read. with the panel open over the home the answer now takes the place of **check for updates** in its own row for 3.5 s (after "checking for updates…" there: the question, then its answer, in one place), with a tick when Cue is up to date, then the row comes back (`App::update_reply`). the sheet keeps its width (its pages are measured with the row at rest); a newer version is said by the row itself ("update to x"). any other message over the panel comes at the top of the window, centred on it (the player's view is given the window's size first: it could still have an older window's width). the home's line stays as it was.
- **a song's long title** under the cover (the centred and the close compositions): it was cut at 1.7 × the cover's width (1.2 × in the close one) with the window's width free on both sides. a title that does not fit there even at the type's own size now takes the window's width, at that size, before it is cut; a title that fits is drawn as before. `tools/preview_music.cpp` with an 80-character title.
- **audio tracks by name**: an MP4 keeps its tracks' names apart from their titles (OBS writes the names given to its audio tracks there: Settings › Output › Audio), and the audio menu said "track 1", "track 2". the name is read from the engine's track metadata (`name`, or a handler that says something; never a muxer's stock "SoundHandler", nor an encoder's id such as `adv_record_audio_0`) and shown where there is no title. only the menus' words change: which tracks play (a recording's tracks mixed, the one chosen last time) is decided as before. titled tracks with no language ("und") no longer get a number after each name ("Game 1", "Mic 2"): only a name used twice does. `logic_test`, and in the real Cue under Wine with an MP4 named like OBS's.
- **a film opened from a home that was resized**: the film's window grows from the window's centre, so a home made smaller or wider by an edge opened the film off the screen's centre (at 1920 × 1080, a home narrowed to 900 × 560 from its right edge put a 1280 × 720 film at 124, 96). from the home a film's window now grows from the middle of the screen the window is on, as the home itself opens and comes back, however the home was sized or moved: the home is a place, in the middle (`App::film_centre_`; a first version kept a dragged home's place for the film, and the home then went back to the middle without it: the author's look at it, 22/11). files after the first, from the queue or the phone, grow from the window as before.
- **the preview helper when memory is short**: while Windows' own low-memory signal is on (`CreateMemoryResourceNotification`, raised only when memory is really short), the helper ends (its own engine and the decoder's frames, about 100 MB with a 4K film). the film plays on; the light keeps its last colours; the timeline's tip shows the time alone, never an empty card (also when a file's previews failed: it waited there for a picture that would not come). 30 s after the signal ends the helper comes back as for a new file. with memory to spare nothing changes (`App::check_memory`, once a second while something plays; `--low-memory-test` raises the signal for 20 s, 10 s in).
- **Explorer's pictures with Cue chosen by hand** (GitHub edition): Cue chosen through "Open with → Choose another app → Look for another app on this PC" is the `Applications\Cue.exe` entry, which did not name Windows' thumbnail handler as Cue's two file types do (since 1.4), so films and albums opened that way showed plain icons in Explorer (a report of 21/11). the entry names it now; `CueMediaTypes` `video-audio-6`, so a Cue already installed writes it again at its next start. the Store edition is unchanged (its package declares the types).
- **the list's names** ("the filename is cut short", a report of 21/11): a film's row says what it is, not its file name, as the home and Windows' media card already did (`card_words`): "Severance.S02E04.Woes.Hollow.1080p.ATVP.WEB-DL.mkv" is **Severance** with "S02E04 · playing" on the grey line, "Blade.Runner.2049.2017.2160p.UHD.mkv" is **Blade Runner 2049** with "2017 · 3 / 6"; dots and underscores become spaces and the release's labels go. a music video named "Artist - Title" shows the title, with the artist on the grey line. a title is cut at its end (the file name was cut in the middle, for its episode number, which the grey line now says); the remove button's room is kept only on the row that shows it; in a narrow list (a portrait window's, under 260 art px) a film's picture is a step smaller (44 × 25, was 60 × 34) and its words start closer to it: in a portrait window's list the name has about 60% more room. the pointer (or the keyboard's focus) resting on a row for half a second shows a cut name whole over the row's two lines, the grey line stepping aside, as a Mac list does (`QueueView::Item::dwell`, `expand`). the panel keeps its width. songs keep their file names.
- **subtitles as the window changes size and shape** (two studies of 22/11: "Cue · tamanho da legenda quando a janela muda", BBC, Netflix, VLC, the browsers; and "Cue · legenda maior sem virar três linhas"; then two looks of the author's: a first version that measured the subtitle file gave small words and lines over one another, a second that zoomed the words with the picture made them too small in smaller windows and took them out of the picture; the rule chosen is from his own drawing over his screenshots, 22/11 18:04): in a window taller than the film (half a 4K screen) the words were sized by the window's height and broke into three or four lines over the film. the words now follow the picture's reading side (the 16:9 frame inside the picture for a landscape film, the picture's height for a 4:3 one, its width for a portrait one; `subtitle_typography_scale`): larger than the window Cue opens in, in proportion; smaller, at a third of the rate (the cube root: a picture three times smaller has words 2.1 times smaller), never so large that a line of 64 letters would not fit the width the engine breaks at, which is a share of the picture's width (`subtitle_wrap_width`): two lines stay two in every window. a landscape subtitle keeps 12 of side margin at the opening window (it was 25); a portrait one takes 92% of the picture's width. the default style is size **36** with letter spacing **+1** (it was 44 and 0; a test build's 52 was too large, the author's look of 22/11 20:16): a line 5% of the picture's height at the opening window, the letters a little apart with the outline. a 44 kept from before (the old default, also a file's own style) becomes 36, and its letter spacing +1 when it was 0; a size or a letter spacing chosen stays (`state.json` schema 7; a test build's 52, schema 6, becomes 36 too). on a 4K screen at 175%, at 36: the opening window 72 px, maximized 104, full screen 108, half the screen 66, a third 52, a 960 x 540 window 39, a portrait video 51; at size 32 (the author's): the opening window 64 px, half the screen 58 (48 with the zoom of the same day), a square window 54 (37), a narrow one 35 (20); at 52: the opening window 105, maximized 151, full screen 156, half the screen 83, a third 56, a 960 x 540 window 42, a portrait video 74 (60, four lines then; now three). the background box, outline and shadow are the style's, as before. ASS kept in its own style and picture subtitles (PGS, VobSub) are not resized; the mini player keeps its own rule.
- **the subtitle style's spacing in every window**: **line spacing** was added in screen pixels, the same at full screen and in a small window, so a small window's lines ran into each other (−16) and a large one's drifted apart (+24); it is now a share of the letter's size (`subtitle_line_gap_share`: +44 is one letter's height, −16 the tightest each face allows, measured on its descenders and accents: Inter −0.23, the serif −0.30, the mono −0.26), the same look at every size. **letter spacing** above +10 was refused by the engine as the window grew (its limit is ±10 at its own reference size), so +10 gave two lines in one window and three in another: the size now reaches the engine as its scale (`sub-scale`, the reference size 44 kept), and letter spacing grows with the letters, always in range: the same lines in every window.
- **subtitles in the picture**: in a window with black bands above and below a landscape film, the words stood at 93% of the window's height, far under the film (half a 4K screen: 137 px below it, a third: 292). the position is now the picture's (`subtitle_window_pos`): at 93 the words stand inside the picture where they stand at the window Cue opens in, whatever bands the window adds, and rise over the controls when these cover the picture, as before. dragged into the band under the picture, they stay under it in every window with a band (a quarter of a line under its foot and lower in the band, as dropped) and at the foot of a window without one (`subtitle_picture_pos`, positions 100 to 200 in `state.json`); crossing the picture's foot while dragging they go from one side to the other, never cut by its edge. dragged anywhere else, they keep that place in the picture. ASS in its own style and picture subtitles are not moved.
- **where the engine draws a subtitle** (`subtitle_baseline`, `subtitle_block_bottom`): Cue's model of it was half a line too low (the hand showed under the words), and a first fix fitted at the default position (93) put the words 35 to 100 px too high elsewhere: the engine moves a subtitle by a share of the window's height less a part of the letters. measured again in the shipped engine, the last line's baseline at five positions (30 to 100), letter sizes 64 to 209 px, window heights 1447 and 2088 and with one to three lines: the model is within half a pixel of every capture, and the block grows upward one letter size per line. the hand, the lift over the controls, the dimming under a panel and the picture's position all use it.
- **the controls over a subtitle** (the author's look, 22/11 19:10: "a legenda muito próxima da barra de controle"): with the model too high, the controls did not see a subtitle under the picture in a resized window and covered its second line; a subtitle now rises to half a line of its own letters above the controls (it was 5 art px, close to the words at every size), so a larger subtitle keeps a larger gap.
- **contributed robustness ([#21](https://github.com/tinypeep/cue/pull/21), omnizs38)**: a broken engine sample (a NaN or infinite position or length) never reaches `state.json` (it was written as a bare `nan`, which made the next start read the whole file as corrupt and reset every setting); a second corrupt state is kept beside the first (`.corrupt-1`); the installer's `MZ` is read across HTTP reads; a folder of files dropped together is queued whole (only the first folder was); a queue entry is not called missing when the check itself could not run; queue paths match without case; a phone cover that failed is tried again; very large times are bounded before the phone page; a file named `-x.mp4` opens (only `--` starts an option); the window-restore code switched off on 15/11 is removed; tests in GitHub Actions and natively on Windows (`tools/native-tests.bat`). one change on top: a `state.json` that is there but cannot be read (an antivirus holding it) is asked for again three times over a quarter of a second, then the backup's settings are shown (not the welcome and the defaults), and nothing is written over the real file in that session.

- **the crash at a worker's start** (once in the 95-minute monkey of 17/11, `execute_native_thread_routine+6`, left open in 1.4): the MinGW thread library's, when a short worker made and let go (`std::thread(...).detach()`) starts while others end: it can start with its work missing (reading address 0). a program that only makes such workers, the way Cue does, crashed with the same address and instruction in 6 of 7 runs under Wine. Cue's sixteen workers that are let go now come from Windows itself (`core/detached_thread.h`, `start_detached`: the same contract, through `CreateThread`); the same program through it ran clean. `detached_thread_test`.
- **Explorer's pictures in the Store edition**: the package's two file types now name Windows' thumbnail handler (`desktop2:ThumbnailHandler` `{9DBD2C50…}` under `uap3:FileTypeAssociation`, as the GitHub edition's types do since 1.4.1), so films and albums keep their pictures when Cue (Store) opens them.
- **the home's pictures after a low-memory rest**: a film left while the helper rested for memory made the home give up its new pictures until another film had played; they now wait, and come when the helper may start again (asked every 5 s on the home, `T_MEMORY_BACK`).
- **a `state.json` held when Cue started** (an antivirus, a sync client): nothing of that session was saved. at the first save after the file can be read again, what the session changed goes over what the file says (the rest is the file's; the list of files by path, newest first) and saving works again (`SettingsStore::unblock`). `state_limits_test`.
- **a folder of more than 512 files**: "the list is full" was said twice.
- **a lighter music player** (the author: "the music player feels heavy and stuck", 22/11; measured first, then the three changes he chose of a list of five): a song is a 1920 x 1080 picture made 60 times a second and drawn over the whole window with its light, and at the same window it gave the graphics card about five times a 1080p film's work on a 4K screen (3.8 on a laptop's 1280 x 720 window; Cue's own shaders run by the engine's renderer on the processor, per frame: the scaling 46%, the light show 25%, the room's light 12%, the cover's shadow 10%, the edges 7%). (1) **one pass instead of two, twice**: the room's light is drawn inside the light show's pass (each of its reads of the picture takes the room's light at that place, `bg_room`, `room`), and the cover's pass now also draws the edges, the list's veil and black and white, in that order (each only reads its own pixel): a centred song has 2 passes over the whole window instead of 4 (`tests/music_passes_test.cpp` evaluates the passes' `WHEN` as the engine does); the same picture: 23 pixels of 3.7 million differ by 2 levels of 255, the rest by 1 at most (rounding: the steps no longer go through a texture between them), compared at 4K in six states (centred, a tilted cover, the list open in black and white, the cover alone, the spectrum alone, the edges alone). (2) **the picture's scaling on a dedicated card** is `catmull_rom` (it was `spline36`: four taps a side instead of six; the author's choice, B, from three drawn side by side at 1:1 on 4K). Together: 919 → 695 per frame at the 4K opening window (−24%; the passes alone −10%), 300 → 251 at the laptop's (−16%). (3) **a song nobody sees** (minimized, or under a full-screen window) was still drawn as on screen, 60 times a second: it is now drawn the plainest way (`bilinear`, and the light show stays out, as it already faded while unseen), and seen again it is drawn in full (`App::update_music_rest`, `send_music_scale`). Only how the engine draws changes: its picture's chain is not touched (a first version of the same day added a filter step after it, `fps=2`: the song paused and came back on minimizing and the spectrum came back broken, the author's look of 23/11 00:26). The room's light, the cover's shadow, the edges, the list's veil and black and white stay, so the window comes back as in 1.4. Minimized at the 4K opening window: 514 → 309 per frame.
- **the preview helper during music**: the helper kept from a film (or started for the home's pictures) stayed open for the whole of the music that followed, idle, its own engine in memory (92 to 111 MB under Wine): music has no timeline pictures and its light comes from its cover. It ends as a song is set up (`configure_audio_mode`) and starts again with the next film, as for the first one.

- **a lighter ambient light around a film** (the author's list for heavy films, 23/11; measured first with 4K HEVC 10-bit HDR, 4K VP9, 4K60 H.264, a 2-hour MKV with three audio and three subtitle tracks, a 30-minute OBS-like recording with three named tracks and a light 1080p film): of what Cue itself does while a film plays, the halo around the window was by far the most (under Wine at 4K and 175%, a 1080p film: the window's thread busy 501 ms a second, 18 with the light off). Its picture was a list of every lit pixel (1.6 million at the 4K opening window, 14.3 MB), each one looked up again at every paint, and each of its four strips sent whole to Windows each time. Now the halo is a list of runs of one colour (along a row of the top and bottom strips, down a column of the sides: 160 thousand at that window, 1.8 MB); a run whose colour did not change is not written again (a light flowing slowly changes a few levels at a time, and its faint outer pixels often not at all); a strip none of whose pixels changed is not sent, and one that changed sends only its changed part (`UpdateLayeredWindowIndirect`, `prcDirty`). The same light: what each strip window holds, modelled from every picture Windows is given, hashed after every tick, is the same as before in five situations (a calm and a busy light at the 4K opening window, a window half off the screen, the laptop's 1280 × 720) and 0 of 8.3 million pixels differ (`tools/benchmark_halo.cpp`). The reveal's own pace still goes by what a whole halo costs (`paint_ms_` from whole paints only): a cheap partial paint does not make the light's entrance believe this PC is faster. Per tick at the 4K opening window, under Wine: a calm light 2.0 → 0.4 ms of Cue's own work and 750 → 93 thousand pixels sent (12 → 4 ms in Wine's own part); a busy one 5.4 → 3.7 ms, 1.87 → 1.59 million pixels. In the real Cue under Wine, the same session twice each: a 1080p film playing, the window's thread 501 → 159 ms a second, its poll 19.7 → 5.0 ms on average and 86 → 27 ms at the 99th percentile; a 4K HEVC film 118 → 79 ms a second.
- **the controls' background in the welcome** (the author's question, 23/11: "does the welcome let me choose controls with or without a background?" it did not: only **more › interface**): a screen between the light and the size, **controls background**, "with a background, or just the buttons over the video", with **solid** and **none** (the panel's own words) under the same scene as the other screens, the player's real bar over it: with the black backdrop, or the buttons alone with their soft shadow and the scene softened under them, as the player draws them. the choice moves between the two in the same spring as the light's, is saved at once (`ObAct::ControlsBg`, `store_.controls_free`) and the size screen after it shows the bar the way it was chosen. ← → choose, Enter goes on; four dots. of three drawings the author chose the one that keeps the other screens' look (A, without the size screen's menu; 23/11). `onboarding_test`, `i18n_layout_test`.
- **up next at a film's end**: the notice counted in words ("Up next in 8s", the number changing every second). it now says **Up next** and a thin line under the words runs out with the film's time left, as the line of **skip intro** does: still while paused, twice as fast at 2×, gone with reduced motion; the episode starts as the line ends, 10 s as before, and a click still goes at once. when the next episode does not start by itself the notice is as it was. the line is drawn on the notice's own kept layer (`next_line_`): the words are not made again each frame. `next_count_test`.
- **a song into the mini player** (the author: "a animação de entrar no miniplayer de música ficou meio travada e feia", with a screen recording of 23/11, 30 frames a second, two entries, read frame by frame): the bar went and nothing moved for 100 ms (the picture's pixels at 4K are too small to see); the flight's first frame showed the full player squeezed into the card (the engine draws the card's composition a frame or two after the window's new size: the cover up and small, back in place the next frame, in both entries); the full player's room vanished at once around the card, whose pixels were its own size magnified with it (from 11 to 24 px on the screen in one frame). Now (of two drawn from his recording, the author's choice, A): the start is shorter (0.08 s, was 0.12); a frame or two before the window changes, the engine is asked once for its picture of the window (`screenshot-raw window`: what it draws there, the controls already gone; the switch waits for it, 0.12 s at most, then flies without it), and that picture, held over the flight in a click-through layered window with the window's round corners (`StillLayer`, `flight_ghost.cpp`), scales with the card so the two covers stay one, opaque for 40 ms (the card's late frames under it) and gone at 0.22 s: the room gathers into the card; the card's pixels start the size the full player's were on the screen and get smaller as it comes sharp. The live card flies as before (`FlightGhost`, now kept under the copy). Films and the way out are unchanged. The picture is scaled in one pass with whole pixels (2.4 ms at the 4K opening window, less as it shrinks; Windows' own copy for the layer on top); cue.log says how long the engine took to give it. (A first version copied the screen with GDI: on the author's PC, his recording of 12:09, the copy was a few frames old and brought back the bar and the title the start had just taken away, for the first fifth of a second. The flight itself was already smooth there: the largest change between two frames 15 against 45 before, no frame shown twice, one frame still at the start instead of three.)
- **the home's settings in a smaller window** (the author's screenshots of 23/11, 12:44): the panel rises from its line while there is room, and in a shorter or narrower window stands beside it or, wider with its picture beside the rows, above it: over the list, which showed in pieces round it ("continue" and "recent" cut under it, the end of a name beside it). The list now steps out while the panel stands over any of it (the fade it already comes in with at the start, 0.12 s out, 0.16 s back) and comes back when the panel leaves it or closes (`HomeView::cover_list`); where the panel stands clear of it, as in a large window, it stays. The player's Settings over a film was checked at the same sizes: on its bar's right edge, above it, the picture beside the rows in a short window, as before. `home_settings_test` (160 windows: the list gone whole wherever the panel stands over it, 105 of them, and back after), `home_partial_test`.

## 1.4 — 2026-11-17

- **music, five shapes** (`PlayView::cmode_`, `App::music_frame_for`, `core/ui_scale.h`): **micro** (a square window under 300 px at 100%, 220 at least: the cover is the window; with the pointer a 0.55 veil, large ⏮ ⏸ ⏭ in whole pixels, title and time, the timeline on the foot's edge; without it the cover, a 4-bar equalizer and a thin progress line; kept square while resizing), **strip** (under 120 px tall and 3× as wide: a small cover, title and artist, ⏮ ⏸ ⏭), **compact** (the old cover frame: the cover in the middle, the timeline along its foot, ⏮ ⏸ ⏭), **wide and short** (cover and wings on the left; words, timeline and transport beside it; `kWideArtH` 124: the pixel goes down a step until the column fits) and **tall** (cover on top, the title on up to two lines, the queue with the current song marked, the controls at the foot). the controls keep the interface's normal size (`music_small_scale`); small windows have no backdrop behind the controls; no cover tilt in micro, strip and compact; micro and strip have no list. `tools/audit_music.cpp` (44 windows × list open/closed × short/long title × 4 sizes × 4 scales × 8 languages: ink outside the window, layers over one another, words over the cover, a cut cover, jumps), `music_header_fit_test`, `adaptive_bar_test`.
- music: the bar at the window's foot without a backdrop folds by its own measures (prev/next 36 from the middle, right buttons 25 apart, the volume from 24; it counted the film's 30 and 22: at 100%, **larger**, 960 × 520 the list's "≡4" sat over **next**); its volume line is the film's 49 art px (it was held at 30), going down to 30 with the rest when room runs out. micro: a button's tip sits right above it (a bar that fills the window had no "outside" and put it past the edge); the cover and the controls' veil are measured from the window's pixels (at half scales they were 1–2 px short).
- **the film's bar, after the mockup** (`film_bar_`): `kFilmDockH` 41, `kFilmTimelineY` 31, the bar 7% of the height above the edge (14 art px at least), ⏪ ⏩ 27 art px from pause; pause in cells of 1.4 art px, ⏪ ⏩ ⏮ ⏭ in 1.2, rounded to whole screen pixels. ⏪ ⏸ ⏩ and the times white; speed and the right-hand buttons quiet grey, white under the pointer; on (subtitles, loop, list) lit; the volume as before (orange above 100%). `controls_revision_test`, `blur_cloud_test`, `free_controls_test`.
- **keyboard notices** (`flash_kind_`): space shows the bar's play/pause glyph in the middle, ▶↔⏸ in 0.14 s; ← → "◀◀ 5 s" / "5 s ▶▶" at the edge, 5% in (14 art px at least), presses within 0.6 s add up, shift 30 s; the bar's ⏪ ⏩ show "15 s" the same way; ↑ ↓ and the wheel a 10-block meter at the right edge (orange above 100%). no box: white with the controls' soft shadow; same timings. play/pause pressed on the bar with the pointer shows nothing; songs show none (the bar shows it). `music_notices_test`, `bar_click_sign_test`.
- **speed in one click**: a click on **1×** cycles 1 → 1.25 → 1.5 → 2 → 0.5 → 1 (from a speed off the cycle, the next one up); right-click or shift+enter opens the full menu; one physical click is one step, never full screen. the number rolls 8 art px in whole steps (0.18 s; down when slower), clipped to the row; instant with reduced motion. `[` `]` `\` are read as characters, so they work without a click on the video. `speed_cycle_test`.
- **time left**: with both times on the bar the left one is the time played and a click on the right one swaps length ↔ −left (player, music, mini player); one time alone switches as before. `time_left_test`.
- **skip intro** (`core/skip_parts`, `core/intro_match`, `win/sound_parts`): chapters named as an opening, ending, preview or recap (8 languages and common tags); otherwise the longest sound in common between two episodes (`Cue.exe --sound-print`: 8 kHz, 32 bits every 32 ms, idle priority, background mode, in a job that dies with Cue; the opening in the first 5.5 min, the ending in the last 4.5, 15 to 180 s). only an episode (another one in the folder), 8 min or longer, on the PC's own disks; starts 8 s after opening; never on 2 threads or less, software rendering, or battery saver/under 20%. the offer: the menus' black plate with ▶▶| at the bottom right, 8 s with a counting line, Enter skips; picture and sound dim 0.16 s and come back 0.24 s (cut with reduced motion); right size in full screen; leaves in 0.2 s when clicked. on the phone a card with the PC's words, a tap skips. never skips by itself. `skip_parts_test`, `intro_match_test`, `skip_plate_test`, `remote_skip_test`, `remote_skip_browser.cjs`.
- **from the start**: the resume notice stays 3.2 s (it was 1.1) and waits under the pointer; pointed at, it turns into |◀ **from the start** at the same width; a click (or Home while it shows) goes to 0:00 through the skip's dim. videos under 10 min keep no resume point (still marked watched). `resume_notice_test`, `logic_test`.
- **ambient light** (`core/light_timing.h`, `core/light_flow.h`, `ui/ambient_reveal.h`, `atmosphere` `LightBreath`, OKLab): the same samples as before, none more; a change is mixed in OKLab without greying and runs at a steady pace; it comes in spreading from the window's edge (opening a film, back from minimised, out of full screen, turned on) and draws in going out (closing in 0.15 s, turned off, full screen). minimising hides it at once, before Windows' animation (no trail). Cue measures a paint of the halo: from 6 ms the spread runs at 15 steps/s, from 14 ms (and with software rendering) only the opacity moves; during a reveal the 1 ms clock is asked for. reduced motion: only the opacity. `tools/benchmark_reveal.cpp`, `tools/ambient_measure.sh`.
- mini player: the film's pause and ⏪ ⏩ in the player's cells (1.4, 1.2); menus breathe (rows of 18 art px) where they fit; the top row of icons on the rows' icon column, with equal air above and below; the subtitle size control 2 art px lower. icons: back to the normal window = a 9 × 7 frame filled; back to the home = a 2 px chevron (also in the menus' titles).
- **settings on the home**: a **settings** line under **open a file** (also on first use) opens the player's own panel anchored to it, with **video ›**, **music ›**, **interface ›**, **language ›**, **about Cue ›** and **show pictures**. `home_settings_test`. the panel's switch rows change with every click (**show pictures**, **chapters**: the row kept its state from when the panel opened); **show pictures** swaps with its animation under the panel (only the list's band is drawn). `home_covers_switch_test`, `chapters_switch_test`.
- phone: the film's chapters (2 or more, at most 250, names cut at 80 bytes), only when chapters are on in the PC. `remote_test`, `remote_chapters_browser.cjs`.
- the taskbar thumbnail's buttons are black with Windows' light theme (`SystemUsesLightTheme`) and white with the dark one, switching with the theme.
- opening: several files in the open dialog; an Explorer selection of several + Enter becomes the queue (deliveries within 0.6 s are one selection); a file from Explorer while a song plays with its list open plays at once; a folder from the command line goes through Cue's list (name order, the file's title, next works). with no video Cue opens centred at the default size; esc goes home at the default size, centred. the window no longer goes back and forth when a film changes, nor jumps at the end of its animation; a black cover grows with the window going home, so nothing is left behind it.
- **slow disks**: a file on an external drive reads 20 s ahead (`demuxer-readahead-secs`, inside the 32 MiB already allowed): no 1.95 s stop after a pause while the drive wakes. the UI thread no longer asks the disk: the home's picture check, the recent pictures (read from the kept copy first), the window's shape from the header (0.3 s at most: a sleeping drive 2645 → 349 ms), "is it a folder?" (a dying disk: ~20 s → 156 ms), the queue's "does it exist?", the cover's size and date (`cover_cache_test`), saving a frame, the sound parts, Ctrl+V of a path, the phone's folder names. the latest choice wins over a slow answer. `tools/slow-disk/`.
- `SetErrorMode(SEM_FAILCRITICALERRORS)` in Cue itself (no "no disk in the drive" box); out of memory in a window message is written to `cue.log`. the iris shader and `input.conf` are written beside and swapped (all or nothing: a full disk no longer empties them). `write_whole_file_test`.
- a click on the video no longer gives the keyboard focus to the engine's window: on Windows, destroying it with the focus in it held Cue while the UI thread waited for the engine (confirmed on Windows with `focusdeadlock.exe`).
- lighter: minimised or covered with a film playing, the engine's time is asked 4×/s instead of observed (Wine: 26.3 → 10.7 messages/s on the UI thread); a film playing with no ambient light and nothing moving polls 10/s (30.3 → 10.0). the sound device is reopened when the engine loses it (headphones out or in, up to 4 tries), not between two songs (the engine closes and reopens it then).
- a .mp4 that is really an MPEG-TS seeks back like any file (`demuxer-seekable-cache`, absolute targets for the arrows). **about**: "code" is now "GitHub". in French the subtitle style's unavailable outline says "pas avec un fond", and the reason may run over the values' column: no menu cuts its words (`audit_menus`, 1,472 menus).
- fixes: a button under a pointer at rest lights up when the bar comes back (`hover_at_rest_test`); **clear queue** no longer makes the bar slide aside and back; the **i** tips and the song's title no longer vanish at the large sizes while resizing; the music's up next list no longer closes when skipping from the bar.
- **.ogg**, **.oga** and **.opus**, checked against the engine (`ogg_engine_test`, with real Vorbis, Opus and FLAC-in-Ogg songs and a Theora film): all three play, and the cover kept in their tags (METADATA_BLOCK_PICTURE) is the music's picture. the cover of a FLAC song inside Ogg (.oga) is now read by Cue too, for the home's list and the phone (its tags are FLAC blocks, not Vorbis comments: it had no picture); so is the song's cover in an Ogg that starts with another stream (a Skeleton). an **.ogg that holds a film** (Theora) is a film for Cue, not a song: the engine played it as a film already, but the home listed it as a song (no frame, no **continue**), and it opened from the start; its first 8 KB say what it is (only on this PC's own disks, never a file kept in the cloud; kept per file). `ogg_kind_test`, `cover_art_test`.
- **show pictures** on or off moves only the right column's contents: the column starts on the same left edge in both (the list of words started 4% of the composition further right, the rule of the old list), so the titles stay where they are; and it starts at the same height in both (where the taller of the two puts it: in a short window or at a large scale, each centred itself on its own height, and the top moved: 25 px at 1280 × 720 and 250%); the eyes and **open a file** never moved. `home_continue_test` checks the left edge and the top on 9 windows and 6 scales, with and without a **continue**.
- panels, menus, cards and switches at a half pixel (1.5, 2.5 px per art px: 100% and 150% at the medium size): their border, corners and the switch's outline are the same width all round (one whole pixel at 1.5, two at 2.5). laid cell by cell, a border was 2 px on the left and top and 1 on the right and bottom, and the switch's outline thicker on the left. whole scales draw as before, to the pixel. `raster_test`.
- drawing: a full-window clear or fill writes four pixels at a time, and a picture laid over another (the lights, the pictures' soft edges) is blended four at a time where it allows it: the same pixels, a quarter less time for the home at 4K (3.05 → 2.34 ms a frame; with the pointer on the one to continue, 2.92 → 1.85), the welcome 17 to 21% less, what's new 15 to 24% less (`tools/benchmark_redesign.cpp`, portable, no GPU). `raster_test` compares them with one pixel at a time.
- a song's opening, measured where it is felt: in its first moments (until 1.5 s after it plays), when anything holds the window for 25 ms or more, `cue.log` says what and when ("song open: the window waited 40ms on a frame at +350ms"); with `Cue.exe --log`, every step of 12 ms or more and each slow engine event by name.
- HDR films' previews, the home's frame and the ambient light are in their real colours: the preview helper reads an HDR10 (PQ) or HLG film's frame at 16 bits and tone maps it itself (the signal to light, BT.2020 to BT.709, HLG's system gamma as BT.2100 says, a gentle roll-off above the SDR white), where it read the PQ or HLG signal as if it were SDR: against the same picture in SDR, measured with the shipped engine, HDR10 went from 56% less saturation and another accent colour to 4% less and the same accent; HLG from 41% less to 25% less, the same accent. two tables per transfer, made once (0.1 ms a 320 × 180 frame); SDR films are read as before. (the engine's quick colour conversion gave every other row of a 16-bit frame the wrong colours: an HDR film's frames use bicubic.) the engine itself is unchanged. `hdr_preview_test`, `hdr_engine_test` against the engine.
- a song opening no longer holds the window for 70 ms at times (two of five songs in a person's `cue.log`): the artist for the home's list was read from the engine as the file loaded, a read that waits for the engine's own thread, busy opening the sound right then; it is asked for now and taken when it comes. so is what `cue.log`'s "video …" line says (codec, pixel format): its four reads, a moment after a song or film starts, waited for the engine too (40 to 63 ms in the same log). `cue.log` (with `--log`) also says when a song's sound is set up, when its cover's picture is ready, and which part of the poll takes the time.
- the pixel transitions (full screen in and out, a song coming in): in big blocks, the last block at the right and at the foot, cut by the picture's edge, took its colour from beyond the picture (its outermost pixel, the darkest of the room) and showed as a black band for a moment; each block now takes the colour of the middle of what is seen of it.
- music, the pixel transitions (full screen in and out, a song coming in): the window around the picture, where it stops short of the window (under and beside it), stayed lit in the room's colour while the picture went grey and dark: a band at the foot and the right for a moment. it now greys and darkens with the picture.
- the home's pictures: a film's is never asked for past its middle (a clip of a few seconds was asked for after its end), and a film with no picture where it was asked is tried once more at its start; when the preview helper goes in the middle of one, the others go on at once (they waited for the home to be left and opened again). `recents_view_test`.
- a recent iPhone's video (.MOV: HEVC 10-bit, HLG with Dolby Vision 8.4, AAC and a second sound in Apple Positional Audio) did not open: its two sounds, with no language, looked like a recording in parts and were mixed, and the engine has no decoder for the spatial one, so the whole file failed ("could not open"). Cue reads the engine's decoders once: a track it cannot play is not offered (the audio menu, the phone) and never mixed, also when a mix was chosen for the file before; the film opens with its AAC sound, as other players do.
- **load audio track**: the added sound is found also when the engine spells its file another way (a short name, a network path: it was added but never chosen, with no word said); a sound the engine cannot decode is said ("could not load the sound") instead of being chosen and heard as silence; a file with several sounds gives its first.
- a song waiting for its cover (the engine held at its first moment) is let go at once when the player closes to the home, instead of at the end of its wait.
- the home has a **continue**: the latest film, episode or long audio you started (10 s or more) and did not finish, even with songs or a quick look at another file opened after it, on top with its picture twice as large (128 × 72 where the column has room, 96 × 54 in a smaller one), its progress along the picture's foot, its title and what is left beside it, and a soft light behind it in the picture's own colours (made once per picture and size: about 8 ms at worst, at 4K with the largest interface). songs never go there; they start over. **recent** (now in lowercase) holds three more under it, newest first, without repeating it. in a short window it leaves fewer rows to **recent**. `home_continue_test`.
- **…** > **show pictures** (on by default): turned off, the home is the list of words it was, up to six entries, with no **continue** and no pictures read or made. kept between starts. switching, the right column goes out where it is (0.16 s) and the other comes in rising 4 px (0.26 s, 0.08 s later); the eyes and **open a file** stay still. instant with reduced motion.
- the welcome, drawn again: one grid for its three screens (picture, title, line, choice and footer in the same places). screen 1: larger eyes lit blue and amber over a lit line with its reflection, the title typed (from 0.7 s, 45 ms a letter) and "video and music player" after it. from 1 to 2 the eyes close, their light goes out and the window's real glow lights in the scene's colours. screen 2's switch turns that glow on and off (at the light's default strength, 15%, so it is what the person will get; the saved strength is not touched), drawn inside the window when it is maximised, the switch in the scene's colour. screen 3: the player's controls over the scene at each size, with their **…** menu open above them (speed, chapters, mini player: the words and rows make the size plain, the bar alone changed little from one to the next), changing with a 0.18 s cross-fade in whole pixels; the screen around them stays still, so the choices never move under the pointer, and the home behind already has the size chosen. pages slide 28 px without passing their place, the words leaving gone before the new ones come. the footer sits under the choice, on the picture's edges: **continue** / **start** on a plate 18 px tall, the installer's and the menus' rows (96 at least, the same width for both), **back** / **skip** as words, the page marks between them; it stays still while the pages change. the welcome's pixel is the home's medium one, a half step larger (×1.25); its picture keeps its size. **start** (and Esc on screens 2 and 3) darkens the screen in 0.22 s and opens the home through the film's iris (0.36 s). the scene changes mood every 5 s. the glow's shape is made while screen 1 rests (it was made in the middle of the slide to screen 2, the stutter there), from a table instead of a square root and an exponential per pixel (films' glow too). the glow no longer flickers on screen 2: the light of a film coming back to the home and the welcome set it in turns. `onboarding_test` (8 languages, 4 sizes, 10 windows).
- what's new, drawn again: happy eyes (^ ^) with the welcome's light, six pixel stars around them blinking slowly, a larger title and the version on a white stamp. three equal cards (black, thin border, stepped corners, a line of colour on top), side by side or in a column, their pictures drawn with Cue's own art (nothing from your files). it comes in once, in 2.6 s (stars flying out of the eyes, the title falling letter by letter, the stamp, the cards rising one after another and each picture playing once), then rests. a card under the pointer rises 2 px, lights in its colour and plays its picture again; poking the eyes throws sparks (34 at most). at rest it draws only when a star changes (82 draws in 10 s instead of 300): the window that shows what's new and the phone pairing asks before drawing again. at 4K about 4.4 ms a frame coming in, 3.7 ms after. everything in place, still, with reduced motion. `news_view_test`.
- one panel for settings and its pages, subtitles, style, up next, chapters and file info: one width (what the widest needs in the language in use, 204 px of art at least, up to 223 in Russian), the right edge at the end of the control bar, 4 px above it (under it when the bar is dragged to the top). panels and menus are black with a thin border and stepped corners, every row of one height; labels white, values grey, what is unavailable dark grey. the film's colour (with a floor so it never vanishes on a dark film) marks only a switch turned on; sliders are white or grey. when the window is short, a panel brings its rows closer before its pixel gets smaller. audio, speed and ··· still open over their button, the right-click menu at the pointer; the shortcuts sheet stays centred. **file info** opens at the video's top right (under the bar when the bar is at the top).
- one switch everywhere (the welcome, the panels, the mini player, music): a track with stepped corners, a black knob and a tick when on; off, the track is an outline and the knob grey. it moves in whole pixels. the values on the right end on the switches' and arrows' line.
- a subtitle under an open panel dims to 28% (text, back and outline) in 0.16 s, and comes back in 0.24 s when the panel closes, instead of hiding under it; it keeps its place. not for the right-click menu, nor for picture subtitles or ASS kept in its own style (the engine does not colour those). only the visible steps go to the engine (about a dozen a fade), nothing between two lines, and font and position are not sent again. a frame saved with a panel open has the subtitle at full colour. `subtitle_dim_test`.
- subtitle style: **restore** left the subtitle menu for the style's footer, on the right beside **use as default**, and turns into **undo** right after; it no longer moves the subtitle. the sample is drawn by Windows in the real font (Omni, Clean or Readable) with the chosen opacity, back or outline and spacing, twice as large and brought down so small letters are smooth, about 17% larger than before, over a darker scene; it is drawn again only when something changes (the pixel font if Windows cannot). changed values are white, not in the film's colour, centred in the column of **default**.
- loop, shuffle, subtitles and mute: turned on, the button's colour fades in with a short flash of light, without moving; the dot or the "1" fades in and out.
- the installer, a little larger and with every stroke the same width: its pixel is always a whole one (2 px per art px at 100% and 125% display scale, 3 at 150% and 175%, 4 at 200%, 5 at 250%), with its art drawn more compactly (356 × 208 art px: the eyes' panel 144 wide instead of 168, margins of 24 instead of 28, the eyes a little smaller), so its window is 712 × 416 at 100% and 1068 × 624 at 175% (it was 784 × 472 in 1.3; 1.5 px per art px, tried during this release, left it too small and mixed 1 and 2 px strokes). `i18n_layout_test` checks its words and buttons at 100, 125, 150, 175, 200 and 250% in every language.
- songs open sooner: the cover is read already reduced by Windows (a half, a quarter or an eighth of a JPEG) for the black-bars check, the mini player's card and the lock screen; other formats are read as before. a song waiting for its cover no longer holds the window: the wait (90 ms at most) runs on a timer, and `cue.log` notes "song waited X ms for its cover". a large cover gets a small copy (512 px JPEG) for the mini player's card. the music view opens on a spring.
- the home's pictures come in with the list when Cue starts, instead of one by one over their stand-ins. the list waits for the pictures already on disk (at most 0.2 s), and the engine is loaded only after them: mapping its DLL holds Windows' loader lock, which kept the pictures' reader (and the window) waiting, for seconds on a cold start. a picture that takes longer still fades in alone. a song's cover is kept the first time it is read (a small JPEG of at most 96 px, one per recent entry, deleted with it) and read again from the song only when the file changes: the next start reads the small picture instead of the cover inside the file. measured in Wine from a cold disk cache: the list and all its pictures in 20 to 36 ms after the first paint (before, the stand-ins first, then each picture fading in).
- "saiu do forno" (what's new) tells 1.4: the home with pictures, chapters, and lighter and faster, once, to people who update from 1.3.x.
- the welcome has three screens: "vem mais coisa por aí" (what is coming) is gone. the phone remote it announced has arrived, and the rest is not close enough to promise. interface size is now the last screen, with **começar**.
- a film opened from Explorer with Cue closed opens in its own shape: the window was made in the home's 16:9, drew the home for a moment, then reshaped itself to the film's (with a frame of nothing in between on some PCs). the film's shape is now read from its file's header before the window is first shown (a few small reads of an MKV/WebM's Tracks or an MP4/MOV's video track, only from this PC's own disks and never from a file kept in the cloud), and the home is not drawn at all: the window's first picture is the film's black. other formats (TS, AVI) open as before, without the home; the engine's measure corrects a header that said something else. `media_header_test`, and in the real Cue under Wine (2.39:1 MKV, 9:16 MP4).
- phone remote: « on a song goes to the song before again. the page stepped through its own copy of the queue, loaded once: from the third song, « went to the second and then asked for the second again, which the PC took as play/pause (the song paused); at other times it did nothing or went to the wrong song, and on a film in up-next mode « could do nothing. the page now asks the PC for its previous file (`prev`), the one P opens there, shuffle included, as » already asked for the next. found with the real Cue under Wine; `remote_side_buttons_browser.cjs`, `remote_test`.
- phone remote: music has two modes on the wheel, time and volume (its « and » already step songs, and the whole list is in **up next** below); films keep up next. in volume mode « and » are − and +, 5% a press (0 to 125%, like the ring), for films and music.
- timeline previews come back sooner and cost less: the preview helper answers a second of the timeline that lands on a keyframe it already decoded with that picture instead of decoding it again, knowing it from its own answers and from the film's index (Matroska Cues, MP4 sync samples, used once the engine has agreed with it twice). HEVC keyframes decode with their rows shared out (slice threading), 18% faster. with the real helper under Wine: the same pictures and times for every request; a 30 s sweep over a 4K HEVC film decoded 0 to 4 pictures instead of 30 (6.0 s to under 0.1 s); hovering the same way in the real Cue showed 2.7 times as many pictures with the helper busy 2.2 s instead of 7.2 s. `keyframe_index_test`.
- a film's picture opens with its first frame. the window took the film's shape from what the file says only when the engine named how the film is turned, which it does only for a turned film (a phone video): every other film waited for its first frame, 60 ms, then the 0.26 s glide, and opened about 0.38 s later (measured in Wine with the shipped engine: 367 to 391 ms; now 0). `media_geometry_test`.
- a film whose data runs out long before its end (a USB stick pulled out, a network share gone, a download that never finished, a cloud file that failed to arrive) keeps its place and does not start the next episode; it used to count as finished and start over next time. its end is its last 3% (30 s at least). songs keep the old rule. `episode_advance_test`.
- the room's light and the accent take a film's colours as often as the film allows: a film with a keyframe every ten seconds (x264's default) made the preview helper decode a quarter of a core for the whole film; now it works a tenth of the time at most (at the longest once every 15 s), and light films keep the usual pace. measured in Wine: 12.0 s of decoding in 52 s of such a film before, 6.6 s now. `colour_pace_test`.
- nothing to do, Cue wakes less: a paused film with its controls away 4 times a second instead of 30 (back at once on a pointer, a key, the engine or the phone); the home in view with the pointer still 10 times a second after 3 s; the home entirely behind the window in front 4 times a second, its eyes no longer following (and drawing) a pointer nobody can see them follow. measured in Wine: 30.3 → 4.0, 30.3 → 10.4, and no eye frames behind another window instead of 30 a second. `poll_pace_test`.
- the home draws again only the eyes' corner of the window when nothing else changed: at 4K a frame of the eyes following the pointer cost 8.5 to 11.5 ms (the whole window cleared, drawn and compared) and now about 0.4 ms, the same pixels. `home_partial_test` (32,400 frames of random sessions, each identical to a whole new frame), `benchmark_frames`.
- the preview helper kept from a film goes after a minute on the home with nothing to do; the next film starts it again, as the first one does. `helper_rest_test`.
- the phone remote's server, on with no phone, sleeps until a phone connects (it woke twice a second); turning the remote off no longer holds the window up to half a second (measured in Wine: 8 to 429 ms before, 1 to 2 ms now). `remote_idle_test`.
- a song kept only in the cloud (OneDrive "online only") is no longer read for its cover by the phone browsing its folder or by the "up next" list: that brought the whole song down. the home already did this. `system_events_test`.
- full screen covers its monitor again after the screens change at the same scale (another resolution, a monitor unplugged); only a change of scale did it before.
- going to sleep writes where the film is (a battery that ran out in the night lost up to five minutes of it), only when the place changed.
- the home's recent list shows each file's picture. a film shows a small frame (160 × 90 at most, about 2 KB) of where it was left, taken by Cue's own preview helper (the timeline's, already running after a film: no second process): at the nearest keyframe, below normal priority, one file at a time, on the home only, and handed back to the player the moment a file opens. the film just left goes first and at once (its picture is there as the home comes back); the others wait 1 s with the home on screen, and while the pointer is on an entry (a click there opens a file; at most 3 s). a picture on its way when a file opens is dropped on the spot (the helper is ended, so it reads and decodes nothing more), and asked again the next time. only files on the PC's own disks get a picture taken: not from the network, a disc, or a file kept only in the cloud (OneDrive would download all of it), which keep their stand-in; for a cloud song the cover is not read from its tags either. nothing is ever taken from the engine that plays, and nothing happens on pause or while playing. a frame older than the film's last opening is taken again; a film that was never watched gets one from a tenth in. frames live in `%APPDATA%\tinypeep\Cue\thumbs`, named by a hash of the path, one per recent entry and deleted with it (all of them with **clear recent**). songs, podcasts and books show their cover (tags or a picture beside them; else the one the helper reads, once) whole and square, on the left of the picture's place; a song's record peeks out from behind it (pixel grooves, two soft reflections, a label in the cover's most vivid colour), and under the pointer slides out a few pixels more, turning as it goes. a file with no picture gets a stand-in in the phone remote's pixel family, always the same for the same file: a little landscape for a film, a note for a song, a microphone for long audio, a book for an audiobook.
- each entry says only what helps: its title without what is only technical in brackets (**(audio)**, **(320 Kbps)**, **[1080p x265]**) and, for a song, without its artist in front (**Jane Remover - Music Baby** becomes **Music Baby**); under it, on one line, what it is and what is left: the episode (**S01E04 · 53 min left**), the artist for a song, the chapter for a book (**chapter 4 of 19**), or **watched** for one seen to its end. no "when" any more, and nothing on the right. an entry with nothing under its title has it in the middle. the title rule is careful: a bracket goes only when every word in it is technical, the artist only when it is the whole first part; `recents_view_test`. the progress lies along the foot of the picture. at rest a picture is a touch darker so the words lead; under the pointer (or the keyboard's focus) it comes up to full light and a small play sign rises into it. a picture that arrives late fades in over its stand-in.
- **clear recent** no longer drops the list at once: the menu goes at the click, the entries leave one after another from the top (each fading as it drifts a few pixels right, 35 ms apart), the eyes follow them down and blink once they are gone, then **Recent** glides to its new place and "nothing here yet" fades in. about 0.7 s in all; immediate with reduced motion.
- opening a file from Explorer with Cue closed: the recent files' check and their pictures wait for the home, and a file opening puts off the pictures not yet read, so neither competes with the film for the disk. **clear recent** draws the list's last look into the window's spare buffer (about 7 ms instead of 23 ms at 4K at the click).
- **load audio track**: a sound longer than the film (a dub with longer credits, another cut) no longer keeps the film going on its last frame with the clock past its length (00:20 of 00:12): the film ends at its own end. the same file chosen twice is selected in the list instead of being added again (the menu showed "dub, dub"). `audio_add_test`, `tracks_engine_test` against the engine.
- **load audio track**: the film may end while the file dialog is open (the next episode starts); the chosen file is no longer added to the film that came after. a new file starts without the last film's chapters (they were shown until the engine reported the new list).
- the list fits the window: 48 × 27 px pictures where there is room, 32 × 18 in a smaller window, and in a narrow one the list without pictures as before. the pictures are scaled once per size (box average from the kept frame, so they are sharp at any display scale) and their corners cut like the rest of the interface.
- long audio is come back to where it was left: 20 minutes or longer (a podcast, a talk, a book) resumes like a film, with its progress on the home; songs still start over. `.m4b` audiobooks are recognised as audio (and offered by **open file**); a file with chapters remembers the one it was left in.
- `home_thumbs_test` (sizes per window, fade-in, rest and hover light, tiles, stand-ins, 168 window/scale cases), `recents_view_test` (kinds, how long ago in calendar days, picture names), `store_io_test` (artist and chapters kept), and `recent_thumbs_win_test` under Wine (JPEG written, read back, black frames refused, pruned with the list).
- a film's own audio file (a dub, a commentary) can be added to its tracks: **…** > **load audio track** for a film with a single sound, or from the audio menu. it is chosen with the usual soft switch and keeps the film's original track beside it.
- phone remote: a paired phone no longer sends its credential after pairing. it answers a fresh challenge on each connection, and its folder, thumbnail, cover, queue and preview requests are signed by a session key that is derived on both sides and never sent; each request is accepted once. credentials are sealed with DPAPI on the PC. phones paired with 1.3.2 stay paired, and a 1.3.2 page left open on a phone reloads into the new one. based on pull request #20 by omnizs38 (review in SECURITY.md).
- pairing codes are drawn without bias; codes, proofs and signatures are compared in constant time.
- updates (GitHub edition) are fetched only from github.com, api.github.com and GitHub's download hosts; redirects are followed one at a time and checked.
- fades over the whole window (the home coming back from a film, the phone pairing screen, what's new) cost a fraction of what they did: on a 4K window a home fade frame takes about 6 ms instead of about 24 ms, so it no longer drops frames. the same pixels as before, bit for bit.
- the settings backup (`state.json.bak`) is refreshed again on Windows: the copy failed every time an unchanged state was saved, which also wrote a line to `cue.log`.
- "up next" no longer freezes Cue in a short window. at 100% display scale, in a window under 200 px tall (a small mini player, or the smallest window with the small interface size) with one or two files in the list, its dotted "add" box was drawn with a step of 0 px and the loop never ended. found by the new random-input runs (`tools/fuzz.sh`); `queue_view_test` covers it.
- chapters: a film's chapters (MKV, MP4, M4B) break the timeline where each begins, and the timeline's preview names the chapter of that moment. **…** > **chapters** lists them with where each begins, the one playing marked; a click goes there, and a long list scrolls. **page down** / **page up** go to the next chapter or back to the start of this one (the one before within its first 3 s), with a note naming it. a file with one chapter or none looks as before. the timeline's preview names the chapter under the pointer (where a click goes), also in files with few keyframes, whose pictures come from further back. **settings** > **chapters** turns all of it off (kept between starts). `chapters_test`, and `tracks_engine_test` against the engine, through Cue's own parser.
- settings' **i** (its examples on or off): the menu glides to its new size instead of jumping, its examples fading in as it opens and out as it closes. the corner it hangs from stays still, so the **i** stays under the pointer (beside the rows, in a wide window, the menu used to be centred on the bar and jumped sideways at each click). `settings_morph_test`.
- settings is a short list of one rhythm: the film's (or the song's) settings, then **interface**, one row that names what is chosen (**medium · without background**) and opens the size and the control background a step deeper, then language and About Cue. the interface's page is a page of Settings: as wide as Settings, its groups named with their icons (**size**, **controls background**; it no longer says "interface" twice), and with the examples on, Settings' picture where Settings has it (on top, or beside in a short window), showing a size or the background as you point at it. it stays open when the window or the size changes. a choice's two pictures beside a short page sit side by side instead of crossing.
- phone remote: the wheel's centre shows a note for music and a film for a video in its time mode (a clock spoke of seconds where the side buttons step songs); the icon and its mode dots are centred together in the button. play/pause is drawn at the side buttons' size (5 px of art, as tall as « and »).
- the music note now marks **load audio track** in the menus (it had the folder of **open file**).
- times from a file that reports an absurd length are formatted without overflowing on Windows.
- tests: the portable tests also run as Windows builds under Wine (`tools/wine-tests.sh`); the phone remote's real server is driven end to end from Chromium (`tests/remote_e2e_harness.cjs`); the external audio track and chapters are checked against the shipped engine (`tests/tracks_engine_test.cpp`). `tools/fuzz.sh` runs the player, the home, the scenes, the welcome and everything the network can send the remote with random input under AddressSanitizer and UBSan.

## 1.3.2 — 2026-11-09

- the phone clock anchors before changing play/pause state, ignores opposing stale pause positions while awaiting confirmation, and starts replay from zero. unavailable gestures no longer change optimistic playback state.

- phone seeks and previews work independently of the minimized PC's animation loop. audio transitions finish without hidden shader animation, and remote file opens no longer wait for the hidden home animation.
- delayed seeks are cancelled across file changes; file shape changes made while minimized apply when the window is restored.

- returning from a portrait video to the home avoids halo geometry rebuilds during resizing and prepares recent rows before the window starts moving.

- phone remote recovers after idle, screen lock, browser suspension and network return; retries keep one active attempt and never replay old commands.
- transient pairing failures retain the unconfirmed code within its validity window. a page's replacement connection retires its older session.
- paused phone controls avoid redundant UI rebuilds; the server shares current state frames and skips unnecessary snapshot copies.

- timeline previews load on demand, reuse a bounded cache and keep their image and size stable while you move the pointer. each preview shows its actual sample time; clicks still seek precisely.
- split-screen and portrait windows keep usable controls across interface sizes and display scales. mini-player previews stay inside the window.
- repeat and shuffle keep a visible active marker, including in compact menus. repeat-one also shows a small 1.
- number keys 0–9 jump to 0–90% of a video. music, mini players, open panels and active drags keep their existing controls.
- seeking recovers when the engine rejects a request. changing files or making a precise jump clears older pending destinations.
- music titles and artists reuse their rendered text during animation. lighting reads audio levels asynchronously and discards old readings after track changes, seeks and pauses.
- cover loading uses bounded background workers and reuses current requests. failed cover jobs stop requesting animation frames; hidden players do not start card-cover prefetches.
- minimizing during a music-card turn settles the animation and keeps the selected track change.
- late screenshots from a previous video cannot replace the current windows media-card image.
- settings hints stay inside a stable panel, respect reduced motion and enter with a short animation. volume boost above 100% keeps its orange indication with either control style.


## 1.3.1 — 2026-11-06

- A click on the timeline is one exact jump. The press used to send a quick keyframe jump (meant
  for dragging) and the release the exact one, so a plain click showed the keyframe's picture for
  about a third of a second and then the right frame: a frame that skipped and came back, playing
  or paused. Keyframe previews now start only once the hand drags more than 3 px, never twice for
  the same point (Windows repeats a move at the same place); the release is still the single
  exact jump. `audio_test` (click, click with 1 and 4 px of jitter, paused and playing).
- Opening a film of another shape (2.39:1 after the home's 16:9, say): the window jumped about 70%
  of its way to the film's shape in one frame and eased the rest. The glide starts while the
  engine opens the file, and the window's thread can wait on it a few frames; on a wall clock that
  wait became a jump. The glide's clock now pauses through a stall (a gap over 2.5 frames moves its
  start forward), so the window goes on from where it was. Going home uses the same glide.
  `core/glide_clock.h`, `glide_clock_test`.
- A film's subtitles wait for its picture: the engine draws them after the shader, so the line
  sat over the black window before the film, moved when the window took the film's shape, and
  only then did the iris open behind it. They are held while the film opens (the person's choice
  kept, the menus show it) and come in when the iris is half open; songs, reduced motion and a
  file that never opens are untouched (`release_open_subs`).
- The welcome (and the home) after maximizing: two "skip" buttons. Windows repaints only the strip
  a bigger window uncovers and keeps the old pixels elsewhere; the welcome's frame drawn there then
  stood for the whole screen, so the next frames saw nothing to change and the old "skip" stayed.
  A new size now repaints the whole window.

## 1.3 — 2026-11-06

- Esc to the home: the controls and subtitles go on the next frame and the picture closes in the
  same pixel iris that opens it (0.22 s, from just inside the corners, also from a half-open
  opening iris) while the sound fades out under the person's volume; only then does the engine
  stop, behind black, and the home comes in. Before, the frame froze while the engine stopped,
  went black and the home popped in. Esc again finishes at once; a file arriving meanwhile plays
  normally (gain and subtitles restored); reduced motion goes straight home. `close_iris_test`.
- Music into the mini player (and back): the flight starts from where the new composition's cover
  lies exactly over the old one (`music_cover_on_screen`), computed right after the switch from the
  new composition as it will be drawn (its words measured, its framing sent). It used to crop the
  full player into the card's shape and show the card in that crop: the cover jumped from the
  middle to the left and changed size as the flight began. (A first try held the whole window
  until the engine reported its new size: the screen recording showed the card blown up to the old
  window for two frames, the engine draws sooner than it reports.) Coming back, the room was framed
  on the card's words while they were hidden and jumped to the right size on landing: the words
  are now measured during transitions too. Films are unchanged.
- Vertical videos: the bar (and its menus, half way from the window's scale) about 15% smaller
  (`portrait_shrink`: 0.85 to the half art px below unless the one above is within a quarter, at
  least one half step from 2 px up, never under 1), whatever set it: the share of the width or
  the window's own scale, which is the larger one in wider portrait windows (4:5, 3:4); also in
  windows a little taller than wide (5:6 to 1:1). Measured 12 to 22% across sizes and scales.
  `portrait_bar_size_test`.
- Volume without a pop: the engine applies the volume to the sound as it leaves, at once, so a
  5% step (keys, wheel, the bar, the phone) cut the wave in one jump, a small click, and a dragged
  slider a row of them. The engine's volume now glides to the new place in 80 ms (smoothstep, on
  its own 8 ms timer, also with the window hidden; restarted from the current value when asked
  again); the bar shows the new place at once and the engine's reports of the steps are ignored
  until the last one. Paused or muted: set directly. `core/volume_glide.h`, `volume_glide_test`.
- Weaker PCs: Cue reads the graphics (dedicated, integrated, software) and the processor's
  threads once at start (`core/device_tier.h`, cue.log "graphics ..."). Integrated graphics get
  lighter scaling passes and 4K timed by the sound; software rendering gets the engine's fast
  scalers, no dithering, every film timed by the sound, no smoothing and the interface's pictures
  at up to 30 a second; a weak processor (4 threads or fewer) gets music's picture at 30 a second
  (same spectrum motion: analysis step and decay follow) and the film's colours sampled half as
  often. Dedicated cards are unchanged. `device_tier_test`.
- 4K films are timed by the display (`video-sync=display-resample`) like every other film,
  except on battery; beyond 4K (8K) they stay timed by the sound. Timed by the sound, the engine
  only showed the film's own frames, so everything Cue draws over a 4K film (the timeline's
  hover, the bar, the opening iris) moved at 24 per second. Chosen once in on_preloaded and never
  switched while the film plays (`clock_fixed_`: unplugging the laptop mid-film changes nothing).
- Timeline previews: the helper decodes each picture with slice threads (up to 4, half the cores,
  still at background priority) instead of one core, so 4K previews follow the pointer sooner.
- The picture's right-click menu offers the mini player for songs too (music's card).
  `context_menu_test`.
- Opening: a resumed film lands on the keyframe at or before its place (`hr-seek` off for that
  start only, back on at the first restart) instead of decoding every frame up to the exact
  second; the heavy-video clock (`video-sync`) is chosen in on_preloaded from the file's size,
  before the decoder starts, instead of being switched at the first picture; `interpolation` is
  sent only when it changes (each file sent it twice while opening).

- Less repeated work during playback: Windows media-card words are cached until the file or music tags change; opening and list animations avoid redundant shader calculation before the overlay update; matching cover decode requests reuse work through a four-entry weak cache.

- Video audio mixes are prepared before the first frame, avoiding a second audio transition during opening. Opening shader updates are bounded to one request in flight, and the iris skips video sampling outside its visible area.

- Reopening a video with multiple audio tracks enabled uses the fullscreen-style pixel fade while restoring the mix.

- Subtitle choices in the full and mini player are sent even when the last observed track
  still matches the new choice. Rapid A/B/A clicks keep the last choice, stale replies cannot
  overwrite it, and the menu reconciles with the actual engine result when the command ends.
  Audio-refresh suppression is cleared for manual choices and new files; visibility changes
  during a transition are kept when the subtitles return.

- The cover stands in the middle of the music's picture (x 700): a roomy window's picture reaches
  as far on both sides of it, so the room's light is the same left and right (it was a lit band
  on the right and the window's colour on the left). The room's light gained a wash over the whole
  room (a wide, faint glow in the cover's border colours) and the window's colour around the
  picture matches its far end (music_backdrop 0.13), so the window reads as one room of the
  cover's colours, like a blurred cover.
- Music's list beside the centred cover is as tall as its songs and stands level with the cover
  and its words (it hung from the top of the window).
- Settings is short by default: the "i" in its header shows each setting at work (the picture
  and its line), remembered (`settings_examples`). Beside the rows, the picture's column and the
  rows' now agree at half-step scales (a resize left the rows over the picture).
- The card's turn: the window draws the shadow too, cross-fading with the engine's over 0.12 s
  at the start and at the end (`cov_hide` now says how much of the engine's shadow is gone), so
  the shadow turns with the cover even while the engine draws nothing; the next cover turns in
  as soon as it is in hand, through the edge in one movement, and the covers of the songs either
  side are read ahead at the card's size a second after a song starts; the drawing thread runs
  at the highest priority.
- The news say more: the mini player for films and songs, the bar and the new music layout, the
  settings' examples.
- Wide, short windows (the split composition): the cover with its wings and the words beside it
  are centred as one group, with an even gap between them (the group sat to the left).
- The card's words: the title a size smaller (at most 1.5x) and the artist right under it.
- The film's mini player opens a short subtitles panel instead of the full one: on or off, the
  track by its arrows (round the list), and three sizes for the mini player alone (as the full
  player's, larger, largest; `mini_sub_size`), with a line that leaves the style and the sync to
  the full player.
- Music's list beside the words is laid out again when the composition moves (it kept the width
  it opened with and could pass the window's edge); the list's icon keeps a word's space before
  its count at every scale.
- Music's bar no longer jumps between songs: the app redrew the interface while it set the next
  song up, with the scene off for that moment, and the full bar took the film's place and came
  back. The scene stays as it was until the song's own is decided, and the bar now follows a
  scene change on its own (it waited for the next change of framing).
- With the bar's background, music has the film's compact bar (buttons over the timeline,
  centred low); the bar along the window's foot is for the style without background.
- The list button shows its count beside the icon, in the bar's own type a shade quieter, its
  capitals the icon's height ("99+" past that), in both bars; its slot grows with it.
- Music's list is always music's (beside the cover, in the card), empty too: it ends on its line
  that adds songs ("add tracks, or drop them here"), never the films' popover over the cover.
- The card in the spectrum-alone look shows the song's cover, drawn by Cue over the card's square
  (the cover read ahead at the card's size, with its shadow); nothing is reopened, the sound never
  stops, and back in the full player the spectrum is there. Skipping turns it like any cover.
  The whole spectrum steps out of the card there (`wing_all`), and the card has the cover look's
  room light.
- A song picked in music's list plays and the list stays open (Esc, its button or a click
  elsewhere closes it); a film's up next over the picture still closes on a pick.
- The survey's invitation: a small card on the home (Cue's eyes writing on a notepad, "help make
  Cue better?", answer / no thanks), for people who updated or a week after Cue first ran; only
  on the home, only where it covers nothing; answered or declined, never again. It opens the
  survey in the browser; Cue sends nothing.
- Shuffle for music: a button on music's bar (and in "..." when the bar is short) plays every
  song of the list once a round in a random order, never the one playing next; back is the songs
  played, newest first; songs added join the round, songs removed leave it; when a round ends a
  new one is dealt, repeat or not, so shuffle never runs out (`ShuffleDeck`, `shuffle` in
  state.json).
- A song's mini player has its own button on music's bar (and in "..."); a film keeps the menu
  on the picture (its bar is full enough). Right-click in the mini player does nothing.
- The card in the spectrum-alone look: the spectrum is kept out of the card on every path, song
  to song included (it came back behind the cover after a skip). The card's cover is the last one
  until the next is read (no black instant), and only the songs either side are read ahead, at
  the card's size, and dropped when the mini player ends.
- Clearing music's list: its rows leave top to bottom where they are, the list closes up round
  its middle (no jump of its top), and the line that adds songs fades in after them.
- Settings shows its examples by default (saved under a new name, `settings_hints`, so the old
  "off" from when they were off by default does not carry over). Too short for them on top, the
  picture goes beside the rows where the window is wide enough, then the rows close up (the
  interface one row deeper, then the groups closer), and only then the settings go alone; a new
  window or interface size decides it again (at the large sizes the examples did not come back).
  The interface's own page (Settings short of room): a size picked there that leaves room for all
  of Settings brings it back, the sizes in place (it stayed on the page).
- Settings' pictures keep inside their frame at every scale: they were measured in whole art px
  (2.5x as 2x, 4.5x as 4x), so at the half-step sizes the art filled its strip to the edge, and
  the spectrum's dots and the light crossed it.
- The credits' summary (the license reader's first page) and the installed sources note said
  1.2.3; they say 1.3, and a test now checks every version written in them.
- A song's card at the large interface sizes keeps a mini player's window, and its pixels step
  down by half art px until the card fits (`card_scale`: 250 x 130 art px at least). The title
  used to vanish (no room above the playback) and the controls climbed under the window's buttons.
- A new interface scale (a size picked in Settings, Ctrl + / Ctrl −, an edge let go, another
  screen) is laid out like a new window size (`App::rescale`/`relayout`): music's composition is
  chosen again for it (going from "maior" to "média" left the cover framed close, its words gone
  under a compact bar), the overlays are sent at once, the welcome scene and the news follow; a
  move to another screen keeps the mini player's rule; a new language frames music again.
- The smallest player window is the medium size's at every size (at "maior" a film's window could
  not get under 1280 x 720 px at 150%); a window that small steps the larger sizes down to fit.
- A song picked in music's open list: the list stays music's while the song changes (in the card
  it became the films' popover over the turning cover for a moment), and beside the cover the new
  song is framed with the list beside it (it was framed centred, the list over the cover, and
  stayed so: the first framing came before the frame was known).
- "..." folded at a large size unfolds again when a smaller size is picked; About is as wide as
  all of Settings, its picture's column included.
- Menus fit at every interface size (`panel_fit_`, `panel_room_h`): a menu that still cannot sit
  whole in the window, clear of the window's buttons and of the control bar, steps its pixels
  down by half art px. The subtitles' style has its own band (under the window's buttons and the
  back arrow, clear of the bar; it reached under them and over the bar's volume at "grande" and
  "maior"); the subtitle list and up next step down in a small window instead of opening over the
  bar; the picture's menu opened by the top edge goes under the window's buttons and the back
  arrow. `menu_fit_test`: every menu, four sizes, twelve windows.
- The mini player's button is on a film's bar too (the first to step into "..."); the mini
  player's "..." has "back to full size" by name.
- The mini players keep their pixels small enough for their window at every interface size
  (`mini_scale`: the chosen size's base scale, stepped down by half art px until a film's mini
  player has 288 x 162 art px and a song's card 250 x 130): at "grande" and "maior" the controls,
  the window's buttons and the menus were far too big for the little window, and the menus came
  out at another size than the bar. One scale for the whole mini player, its menus included
  (they may reach over its floating controls, never over its window's buttons).
- A film opens sooner and smoother: its window takes the film's shape at load, from what the
  file says (its size, `demux-par`, `demux-rotation`), while the engine starts its decoder; the
  picture opens with its first frame. It waited for the engine's first picture, 60 ms more and
  the 0.26 s glide (about 0.4 s of black), and the glide (and the engine's switch of video-sync
  for a large film) happened while the film had started. The engine's own measure, a moment
  later, changes nothing when it agrees within 0.4%. cue.log notes "picture opens (open N ms)"
  for a film and "song plays (open N ms)" for a song.
- Reliability fixes from omnizs38 (#17): a song's cover is decoded within bounds (no fallback to
  the full frame when scaling fails: a crafted cover could ask for about 1 GB; inputs over 4 GiB
  and non-positive sizes refused); the phone remote's thumbnail and cover renders wait 8 s at most
  for a free slot and are cancelled when sharing stops or changes (503 meanwhile; `remote_gate.h`,
  `remote_gate_test`); the engine's DLL is let go when it is too old and never loaded twice; the
  installer checks every destination before it moves anything (no escaping, duplicate or linked
  paths); a worker thread that cannot start is cleaned up; a failed settings save leaves no stale
  `.tmp`, and off Windows the good file is never replaced without its backup.
- A menu closed and faded lets go of its pixels (about 10 MB at 4K in two copies); it is drawn
  again when it opens.
- The ambient light turned off and on again over a paused film shows at once (the halo's
  windows stayed hidden until the next frame changed its colours).
- Shift held while the bar is dragged keeps the bar's centre on the window's middle: it moves
  only up and down and lands centred (pressed mid-drag, the bar centres at once).
- Music's room light is part of its look: always on at 30%, off only with the cover alone; the
  ambient light's switch and strength in Settings are for films.
- About Cue is a page of Settings: it opens in Settings' place, as wide, over the "..." (the
  shortcut sheet stays centred).
- The card: the artist close under the title (over the title's descenders' room when the title
  is larger), the list's line further down, closer again in a small card before it goes.
- The mini player is a quick look: no menu on the picture, no up next in a film's mini player
  (its "..." and its shortcut too), no "up next" notice in either.
- The mini player's subtitle arrows change the track: the panel set the interface's track at
  once, so the app took the pick for the current track and asked the engine for nothing.
- Home: the eyes with their line and the recent files are centred in the window as one group,
  the same air on both sides, each block's middle on the window's middle.
- Home: entries further apart, their lines and rule in the middle of their highlight, the "···"
  ending where the entries end, level with "Recentes"; its menu is the player's menu (icons,
  18 px rows, a hairline before "clear recents", the same sheet), its right edge under the "···",
  and in a short window its rows close up and it rises to fit.

- Music in a roomy window: the cover with its wings in the middle, the title (twice the type) and
  the artist (a size up) centred under it, and the bar along the window's foot, nearly edge to
  edge: the timeline across it with the time at each end, under it the volume and speed on the
  left, previous, play (larger) and next in the middle, the list, repeat, full screen and "..." on
  the right (`MusicFrame::Centre`). The list's count moved from under the artist to the list's
  button. The album's list opens beside the cover: the cover and its words glide into the left
  half, the list takes the right. Wide, short windows keep the split composition (the words beside
  the cover), with the same bar; tall and small windows keep the stacked one. The bar's background
  follows the setting.
- The song's mini player (the card): the cover low on the left, three quarters of the height, and
  one centred column beside it: the title, the artist and the list's line from the cover's top;
  previous, play and next, the timeline with the time where it ends, the volume and "..." at the
  foot, level with the cover's bottom edge (the volume's icon starts where the timeline starts,
  "..." ends where the time ends). A small card brings the words a size down rather than crowd
  the controls. With the list open, previous, play and next step down beside the volume (they
  glide there) and the list takes the column down to the timeline. No play or pause sign over the
  cover any more (the film's mini player keeps it).
- The film's mini player: two rows along its foot, with no bar behind them (the bar's background
  comes back with the full player): the volume, back 15 s, play, forward 15 s (previous and next
  with a list) and "...", then the timeline with the time at each end.
- The card's turning cover is drawn by Cue itself (`CoverTurnWindow`): a click-through window over
  the cover, its own thread, one picture per screen refresh from the clock alone, the same camera
  and rim as the engine's cover pass (`core/cover_turn.h`). The engine leaves the cover out
  meanwhile (`cov_hide`) and keeps the room and the shadow, which follows the turn. The next
  song's cover is read at the card's size while the old one turns away; it turns in once the song
  starts (or a moment after the edge), lands, and the window fades once the engine shows the same
  cover. Before, the turn could only move when the engine drew a picture, which around a song
  change it barely does: it stepped however its curves were tuned. The engine's own turn stays as
  the fallback.
- Settings in a window too short for its picture on top: the picture beside the settings, on the
  left, as tall as the column, larger (the two states one over the other), with its words under
  it. Narrow and short windows still drop it.
- Version 1.3 (it was going to be 1.2.4).

- Music's list where the words' column cannot hold it (small, tall, square and narrow windows)
  has a composition of its own instead of lying over a dimmed cover: one column centred in the
  window, a header (the cover glides down to a thumbnail on the column's left, the title and
  artist beside it), the list under it, the bar under the list; when the list is short the three
  stand together a little above the window's middle, when it is long the list takes the height.
  The wings step out under the header and the room's light goes quiet round the thumbnail.
  Closing the list glides everything back. A window too short even for that keeps the list over
  the dimmed cover. The list opens on a whole row above the song playing, never a cut one.
- The card's turn is one movement through the edge: away with an ease-in that is fastest at the
  edge (0.20 s), in with an ease-out-back that leaves the edge at that same speed, passes flat by
  about 5 degrees and lands (0.42 s), from the new song's first picture (no head start: its
  pictures come at 60 a second from the first). Edge-on is 88.5 degrees, a hair instead of a lit
  sliver hanging while the next song opens.
- Settings is grouped like a settings page, on the same black sheet as every other menu: the
  groups (the picture and its words, music's looks, the light, the interface, language and
  about) a little further apart, a hairline in the middle of the space between them, segmented
  choices on the usual quiet track. (Grey cards behind the groups were tried and dropped: the
  sheet turned muddy.)
- Back from the mini player, a song opened in it has its spectrum again at once (see the card
  below); it used to stay off until the next song.

- The cover-only picture (the cover-only look) runs at 60 pictures a
  second again. While a song plays the engine draws only when a picture comes (a redraw asked for
  in between waits for the next one), so at one or two a second the card's turning cover, the
  cover's tilt and the interface over the picture moved in jumps. Each of these pictures is a
  black frame with the cover laid on it: still far lighter than the spectrum.
- The card's turn: the old cover eases away (cubic, 0.17 s), the new one turns in on a damped
  spring that passes flat by a whisper and settles (0.46 s), a beat after the song starts.
- Music's list, wherever it opens, is the same list: two lines a song and the footer wherever
  they fit (one line a song only where they do not: a strip, a small window). Beside the words
  the composition glides to make room for it (the words' width it centres on is the list's
  column while the list is open): a short title used to leave the list no room at the window's
  edge, and it popped up. Where the words' column cannot hold it, it opens over the cover. Placed
  by the composition the window is gliding to, so it never starts in one place and jumps to the
  other. Over the cover, a short list stands in the middle of its room.
- The window's colour around the music's picture dims with the cover under the list (it stayed
  lit: a band at the picture's edge).
- Subtitles' style: three groups (the typeface; size, line spacing and letter spacing; text,
  background and outline), the typeface chooser is a track like the menus' other choices, every
  slider has a notch at Cue's own value (the handle sticks to it), and the panel breathes like the
  others. Line spacing goes down to -16 (was -10).

- "..." is the file and places to go: open, close, the phone remote and **settings >**, last.
  Settings holds everything Cue keeps between files: for music, its looks first (cover and
  spectrum, the spectrum alone or the cover alone; colour or black and white; the cover's tilt,
  each with its picture, dimmed when the look in use has nothing for it); the ambient light and its
  strength; for films, smooth motion; the interface's size and the controls' background; the
  language; and **about Cue**, at the end. Esc walks back the same way (About, then Settings, then
  "..."). A window too short for the whole panel drops the picture, then folds the interface's
  two choices into **interface >**.
- The picture's menu is the file's for music too (about the file · up next, mini player · close):
  its looks moved to Settings.
- Music's list never pops up. Beside the cover as before; in a strip, one line per song (the
  card's rows) from the words down to the bar; in the close compositions (the cover alone, or the
  words under it) it opens over the cover, which steps back into the dark (a `veil` pass), the
  words under the cover stepping out; one line per song where two lines and the footer do not fit.
- Music in a tall window kept the film's portrait bar scale (raised to fill the width) and its
  title at twice the type: both far too big. Music keeps the screen's own size, and under the
  cover (and in the card) the title is one and a half times the type at most and no wider than
  a little past the cover.
- A song's card turns its new cover in a beat (0.12 s) after the song starts: the engine holds
  redraws for 0.1 s after a start, and the turn's quick first part was lost there (a jump). The
  cover-only picture is one frame a second instead of two (the engine also holds a redraw asked
  for in the last 0.1 s before its next frame: the card's movements stalled a fifth of the time,
  now a tenth), lighter as well.
- Full screen: the black cover is painted and on screen (one composition pass) before the window
  changes size; shown and not yet painted, it was composed empty and the L still showed through.
  A pinned window rising with the switch no longer covers it.
- Settings (first round): what Cue keeps between files moves to **settings >**: the ambient light and
  its strength, smooth motion (always there now: a choice for every film, acting only where the
  film's rhythm and the screen's differ), the interface's size, the controls' background and the
  language. The row under the pointer (or the keyboard) shows itself at work in a small picture
  at the top: the light off and on round a little screen, a pan that jumps and one that glides,
  the bar at the size pointed at (before it is taken), the bar with and without its sheet, the
  language written in itself; one line under it says what the setting does. A short window gets
  the settings without the picture. The language list opened there comes back to Settings.
- The picture's menu (right-click) in three groups instead of a rule between every line: the
  picture (about the file, save frame), where it goes next (up next, mini player), and closing it,
  last and apart. Its lines breathe like the other menus when the window has room.
- Music in a wide, short window (a strip at the edge of the screen, about 2.2:1 and wider) keeps
  its words: half and half, the cover with its wings centred in the left half, the title, artist
  and list from the middle. It used to show the cover alone between two dark sides. The window
  around the music's picture now takes the room's far light in the cover's colours (the engine's
  background, and the edge pass fades into it), grey in black and white, black for films.
- The album's list is one list wherever it opens: beside the cover it has the popover's footer
  (+ files, + folder, clear), and the popover ends with the same "add tracks / or drag them here"
  line as the list beside the cover. End scrolls to that last line.
- Mini player: the flying picture had Windows 11's grey 1 px outline (every rounded popup gets
  one); it has none now, like the window itself.
- Full screen: Windows composed a frame or two of the window at its new place with its old
  contents (a black L over the desktop) while the window changed size. A black cover holds the
  screen still for that moment, and leaves as soon as the engine has drawn the new size (0.6 s
  at most).
- Opening a film: the picture opens once the window has taken the film's shape (it used to open
  while the window was still gliding to it, so the iris was drawn in the old shape, moved every
  frame and the engine drew a new size every frame: the stutter). At most 0.9 s after the first
  frame it opens anyway. A window already at the right size does not glide at all.
- The opening iris is drawn by the video shader, on the picture, in the picture's own
  proportions. It was a full-window bitmap made and sent to the engine 60 times a second (33 MB
  a frame in 4K) while the film was starting.

- The control bar fits every window: one row of controls and the timeline, always. As a window
  narrows, the bar first shortens the volume line, then hands its least needed buttons to "..."
  (speed, repeat, up next, audio, subtitles, in that order), folds the volume into its icon (a
  click opens it upward, as a column), and with a playlist lets the 15-second steps go while
  previous and next stay. Play stays centred and "..." at the right end; buttons glide to their
  new places and fade where they leave; a window held at a threshold never flickers between two
  folds. Every key keeps working. Portrait and square windows lose their old three-row bar.
- What leaves the bar waits at the top of "..." as one row of the bar's own icons, lit when on.
  In a window too short for the whole menu, its interface section folds into "interface >".
- Mini player: right-click the picture (the item has its own line, under up next) or press Ctrl+M,
  and the video goes to a small window in a corner of the screen. Its bar is play, volume (a
  click opens the column upward), the timeline and "..." (no "back to full size" row: the arrow
  in its corner is that, and the space goes to the controls); subtitles grow to
  stay readable and the controls keep a comfortable size. The whole picture moves the window, and
  so do the bar's empty places (with the pin on, the bar stays shown and covered most of a small
  window, so a drag there was dead; the pin is only "above other windows"),
  and it stays where it is let go (kept whole on the screen); let go right beside a corner and it
  glides into it (within 40 px at 100%, so a window let go near an edge stays put). Windows' Snap never resizes it while it moves. It stays above other windows
  when its pin is on (remembered for the next time). Esc brings the normal window back (a second
  Esc goes home). Full screen (F, a double-click) and the shortcuts sheet (?) belong to the full
  player: the mini player ignores them. The ambient light rests while the window is
  small. The next file keeps the mini window where it is. The flight is one continuous
  movement: the picture breaks into Cue's pixels where it is (0.12 s, the controls stepping
  aside), then a live picture of the window (a DWM thumbnail, composed by Windows on the graphics
  card, the film still playing in it) shrinks from the window's place into the corner, landing a
  hair past its place and settling (0.46 s), sharpening out of the pixels; the window itself is
  already there, out of sight, at its new size, and shows where the picture lands. Coming back,
  the picture opens out from the mini player to the old place and size (maximized too, 0.40 s,
  no overshoot past it). The window changes size once, out of sight (changing it on every frame
  of the flight left the engine's picture a frame behind: black edges, a cut picture, worse with
  4K), and the picture is always cropped to its own shape, never stretched. Where Windows gives
  no thumbnail, the flight is the plain one (out of sight for 80 ms, then a spring into the
  corner). A film's mini
  player shows the time on each side of its timeline when it has room. In the mini player the
  arrow at the top-left is "back to full size" (an expand arrow; home is a step further), the
  cover does not tilt under the pointer, and a song's list opens in the card's column, scrolled to the song playing, the words stepping
  aside for it. The card's list has its own rows, made for its narrow column: one line per song at
  the card's type size (the artist line's size), a small cover, the title and the artist dimmed
  beside it ("Artist - Title" file names are read as such; labels at a title's end such as
  "(audio)", "[Official Video]" or "(320 Kbps)" are left out there), the song playing lit with
  its play or pause mark on its cover. About three songs show at once, readable (the full list's
  two-line rows, shrunk to fit, came out smaller than the card's words). A song change in the card
  turns the cover over: it turns away edge-on about its upright axis as the song breathes out
  (0.15 s), the next song opens while it is edge-on, and as it plays its cover turns in from the
  other side, settling with a little overshoot (0.42 s); the words step out with the old cover and
  the next ones slide in. The cover's shadow narrows as it turns, and the room's light fills the
  place it uncovers from all four sides (the light borrowed from one side made a hard cross). The
  sound is never held: the old song fades for those 0.15 s, the new one starts at its full level.
  The card never shows the spectrum (its wings are masked, also through a song change, where it
  used to show for a moment with a black room and the whole scene small); a song opened in the
  card keeps the person's look underneath, so back in the full player its spectrum is there at
  once (it used to stay off on that song until the next). A song that
  follows keeps the card (it used to fall back to the wide composition and its spectrum). A song's mini player is
  its own card: the cover on the left, the title, artist and list at the top of the column
  beside it, and at its foot the timeline over volume · previous · play · next · "...", with the
  song's light behind (the spectrum's wings step out, drawn away by the graphics card; the sound
  is never touched). The mini player keeps its shape when an edge is dragged, and stays a mini
  player: a film's long side between 480 and 800 px at 100% (540 to start), a song's card between
  440 and 640 (480 to start). The card's cover starts under the window's top strip, clear of the
  back arrow. Leaving gives back the window's place,
  size and pin. A window made very small becomes the mini player's bar on its own.
- A song opening after a song (in the card or the full player) no longer flashes the last song's
  bare picture while the engine reads the next: the passes over it (the card's mask on the
  spectrum, the room's light, the cover) and its framing stay as they were (the framing is the
  engine's own zoom and pan now, set by every file as the engine reads it, a film back to none),
  and the room's light keeps its colours and crosses to the new cover's in 0.45 s instead of going
  black and fading in again.
- The mini player's menu and list keep the interface's own size after the window is moved or
  resized, and after a size chosen in the menu (they dropped to the small window's scale: on a
  150% screen, 2 instead of 2.5, about a fifth smaller than the card's words).
- While something plays, Windows never slows Cue down for not being the window in front (a mini
  player beside other work, a film on a second screen); at home Windows decides again. The
  scene's colour samples for a film in the mini player (no ambient light there) are taken every
  4 s instead of every 0.9 s.
- Super resolution: on a PC with an NVIDIA RTX card and "RTX Video Super Resolution" turned on in
  NVIDIA's app (that switch decides; Cue has none), a film smaller than the screen is upscaled by
  the driver, decided as the film opens (no pause), never on battery. Only films up to 1080p:
  1440p and 4K are left as they are. When Cue cannot read that switch, it leaves the picture as
  it is.
- Smooth motion: films whose frames do not land evenly on the screen's refreshes (24 fps on a
  60 Hz screen) blend frames at their changes, no judder on pans and no "soap opera" frames. Off by
  default (the effect is subtle by design); "smooth motion" in "..." appears only for such films,
  and cue.log says when it is on. A film with it on is timed by the display whatever its size (the
  engine ignores the blend otherwise, which is why it did nothing on 4K films). Never on battery.
- Music in square windows keeps its title and artist, under the cover, and the spectrum's wings
  always fit inside the window (the cover is a little smaller in narrow windows instead).
- Small and portrait music windows get a compact bar: previous, play, next, the timeline, volume
  and "..." (full screen and repeat wait there).
- The wide music composition is back to its original place (the cover 406 px in, the words from
  1262): moving it further in pushed long titles to the window's edge, and the picture was let
  stop short of the right edge (a black fade beside the title where the light should be). The
  picture now always covers the window; close framings of a wide, short window let the cover sit
  up to 10% right of the middle instead of leaving a dark band on the left.
- Music framed close (a small window, a portrait one, and a song's mini player) is one
  composition: the cover, its title, artist and list under it (when there is room), and the bar
  right under them, its timeline above previous · play · next, all of it centred a little above
  the window's middle, the bar centred on the cover and staying up (it is part of the picture).
  No more hole between the words and a bar pinned to the window's floor. Its size is the
  screen's standard one (what the wide composition shows): a small window, whose own scale
  shrinks, gets it back; a bigger one never gets more.
- Music framed close (a small or tall window) no longer leaves a dark band along one side: the
  cover sits further into the scene's picture, so the picture reaches the window's left edge, and
  in very wide, short windows the cover moves a little right of the middle instead.
- A song that follows a song keeps the window exactly as the person sized it.
- The wide composition is centred on what is there, from the wings to the end of the title as it
  is drawn (it was centred on the picture, so a long title ran to the window's edge while the
  left stayed empty); the picture may stop up to 7% short of the right edge for it, fading there.
  It glides to its new place when the next song's title is longer or shorter.
- The bar no longer stays off-centre when its width changes in place (a song's framing changing).
- The bar "without background" can be moved too: drag it from an empty place between its
  controls (a click there is still a click on the picture).
- In a narrow window (a phone video), dragging the bar can only centre it: left, centre and right
  are a few pixels apart there, and a corner chosen there threw the bar to the side once the
  window was widened.
- A jump in the sound (a click on the timeline, the arrows, the 15-second steps) no longer pops:
  the sound breathes out in 16 ms, jumps and breathes back in over 50 ms (the engine's gain,
  under the person's volume). The sound keeps 0.25 s ready (the engine's default is 0.2): every
  jump first fills that much, and the music's picture for it, so at 0.75 s it stalled.
- The light around the window: a resize takes it away at once (its old frame no longer stays
  behind as an outline) and it comes back for the new size when the edge is let go; moving the
  window keeps it, moving with it. A window dragged partly past the screen's edge and back keeps
  its whole halo: the halo's shape is made for the whole window once per size, and only what
  falls on a screen (any of them: the halo reaches onto a monitor beside) is sent to Windows;
  coming back into view, the part that was off screen is cut from that shape at once (it used to
  come back without that side, cut on a straight line in the middle of the screen, until the drag
  ended or the window was let go).
- A playing window stops shrinking where the player still looks right: 320 x 180 px of art for a
  film (below that the bar covered most of the picture), 260 x 220 for a song. The small window
  to watch beside other work is the mini player.
- Fewer writes: where a film is playing is saved every 5 minutes (it was every minute), and when
  it is paused, stopped or closed; a song's place is never saved. The colour samples for the
  light around the window are taken every 1.5 s, and only while that light shows (every 4 s
  otherwise): the preview helper used 40% of a core on a 4K film doing it every 0.9 s.
- Cue's folder moves from `%APPDATA%\tinyoffice\Cue` to `%APPDATA%\tinypeep\Cue`, the studio's
  name: the running Cue renames it once, at its first start, everything inside coming along (if
  something holds a file there, the old folder stays in use and the move waits for the next start).
  The uninstaller removes both. The names Windows knows Cue by (file associations, the
  single-window handoff, the update's handoff) do not change, so the default player and updates
  carry on.
- state.json is swapped in with one Windows call that keeps the previous file as the backup (a
  rename: one write less each time), and waits up to two seconds for an antivirus or indexer
  holding the file (it gave up after 0.3 s: "could not save state").
- cue.log names a film's codec and pixel format beside how it is decoded (the graphics card or
  the processor), so a film decoded by the processor can be explained.
- cue.log writes far less: routine steps only with --log, lines reach the disk together (once a
  minute, or at once for a failure), the file stays under 256 KB (the older half as cue.log.1,
  was 1 MB each) and a log older than 5 days is cleared when Cue starts. The engine's own
  warnings about the sound or picture falling behind are noted (a dozen per file at most).
- Music never stumbles when the window comes back from the taskbar: back at the same size,
  nothing is rebuilt (the engine's window is not resized and the overlays stay), and the sound
  keeps 0.75 s ready instead of 0.2 s, so a hiccup on the picture's side never reaches the ear.
  Volume stays instant (it is applied as the sound leaves).
- What's new in 1.3 opens once on the home screen.

## 1.2.3 — 2026-11-03

- Music is lighter to draw all the time: the room around the cover (its soft light) is no longer
  a full-size picture the processor scales 60 times a second. The music's picture is the cover and
  its wings on black, padded from a few pixels, and the soft light is drawn by the graphics card,
  centred on the cover in the colours of its own border (the old light was the whole cover
  blurred and stretched, so its bright parts glowed off to one side). About a fifth less work for
  the processor with cover + spectrum.
- A third music look, **cover only**, the lightest: the cover and its still light, no spectrum and
  no light show, two pictures a second.
- The cover's tilt under the pointer is a choice in the song's right-click menu (on by default).
- Changing the look no longer cuts the sound: it breathes out with the picture and back in after
  the song reopens, through the engine's own gain (the volume you set is never touched).
- Minimizing and restoring Cue no longer shakes playback (music or film): nothing in the engine's
  sound or picture is rebuilt any more when the window goes to the taskbar and back. Minimized,
  Cue's own poll runs 4 times a second instead of 30.
- A song always starts from its first sample (the engine no longer trims the start of the sound to
  meet the picture's first frame, which the cover alone, at two pictures a second, could delay).
- Portrait and square windows: the control bar stays big, the menus, the subtitle list and the
  shortcut sheet take a size half way between it and the window's own, and up next is exactly as
  wide as the bar in a portrait window.
- A song opens faster: its cover is read once (one read of the file, one small decode) for the
  picture's crop, the light's colours and Windows' media card, instead of three times, starting
  the moment the song is chosen. The last few songs' covers are kept in memory, never on disk, so
  going back to one is instant. The room's light fades in faster (0.3 s).
- A click that brings an inactive Cue window forward only wakes it (the controls appear); it no
  longer pauses. The next click pauses as always; a button on the control bar acts on that first
  click, and a double click is still full screen.
- The timeline's preview follows the pointer closely (it glided a little behind) and appears a
  touch sooner.
- The subtitle moves with the control bar: it starts rising or settling on the same frame the
  bar appears or leaves, and lands with it (a long way takes a little longer, never a jump). A
  new line that appears as the bar leaves is placed straight where it belongs, instead of
  sliding down from above.
- Up next, without a list of your own, offers only the next episode of the same series (by episode
  number, or the same name with the next number). A film no longer offers whatever file sits next
  to it in the folder (a song in Downloads, another film). Music still brings its folder's songs.
- A film's place is written to disk at most once a minute while it plays, instead of every 10
  seconds (pausing, closing, the end and Windows shutting down still save it at once).
- Windows' media card shows a film's picture without writing anything to disk: the frame is taken
  in memory (the engine no longer encodes a full-size JPEG into the temporary folder, which could
  also nudge playback), shrunk at once and turned into the card's small picture on a worker.
- The playing file's folder is read on a worker (one listing with the attributes Windows already
  returns, not a look-up per file): a network share, a sleeping disk or a Downloads folder of
  thousands of files never freezes the window or its animations; up next and a song's album
  appear as soon as it is read.
- The hand that moves the subtitle is about as tall as Windows' arrow (it was drawn the full size
  of the cursor's box, about twice the arrow).
- The installer (from omni38's pull request #13): an update is written to a staging folder beside
  the installed files and swapped in by renaming, with rollback if anything fails, so an
  interrupted update leaves the previous version whole; renames are retried for a moment when an
  antivirus holds a fresh file; folders left by an interrupted update are removed without ever
  following a link out of them; a damaged installer can no longer be mistaken for the
  uninstaller. The same number of bytes is written as before.
- Playing several audio tracks at once is remembered for the file again, and Windows' media card
  shows the right play / pause state after it comes back (also omni38's).

## 1.2.2 — 2026-11-03

- Music in black and white (issue #11): a second choice in the song's right-click menu, beside
  cover + spectrum and spectrum only: colour (the default) or black and white. The spectrum and
  the light turn to their own greys (each colour keeps its brightness) while the cover keeps its
  colours, also tilted under the pointer; the colour drains away or comes back in 0.3 s, without
  reopening the song. Saved for all music; films are never touched.
- Keyboard shortcuts (issue #12): ? opens a sheet with every key Cue answers to, in four groups
  (playback, subtitles, files and up next, window); ? again or Esc closes it. Two columns where
  the window is wide enough; in a small window its rows close up and it gets a little smaller,
  so it is always whole and clear of the window's buttons. About has a link to it, and a
  button's name now says its key, a shade dimmer (full screen  f).
- A maximized window stays maximized when the next file or song opens, or when going back home
  (issue #9): the new file's shape becomes the place the window restores to, as full screen
  already did.
- Up next never opens up under the window's own buttons (pin, minimize, maximize, close), in a
  window of any size (issue #10). Menus and the shortcut sheet keep clear of them too.
- Music: a cover that is a video's frame boxed in black bars (common in songs taken from video
  sites) is shown without the bars on the computer too, and its light is the picture's, not
  the bars'. The bars are found in the background while the song opens.
- Music: the spectrum's fading dots let the light behind them through instead of turning into
  dark holes over a bright cover, so the wings stay whole on white covers.
- Repeat (l), next and previous (n, p), the subtitle menu (c) and always on top (Ctrl+T) work
  after a click on the picture too (the engine's window had the keyboard and did not pass
  them on).
- A square window (1:1 video) gets bigger controls: the bar is raised until it fills about two
  thirds of the width (a share that follows the chosen interface size), easing in from 4:3, so
  it is not lost under a tall picture. 16:9 and 4:3 windows are unchanged.
- Changing the interface language redraws every word at once: the song's "N tracks", up next at
  the end of a song and the home's recent list ("25 min left", "3 episodes") kept the old
  language until they were touched.
- Thanks to DetroNot74 (@DetroNot74 on GitHub) for the reports and ideas behind this version.

## 1.2.1 — 2026-11-02

- Music: with ambient lighting on, a light show plays inside the window: a soft light around
  the cover and pools in the window's corners, in the cover's colours, that breathe in and
  out on a slow cycle (deeper and a little quicker when the music is full, shallow in quiet
  passages), follow the song's body, keep only a trace of each beat and turn round at a
  chorus; the spectrum's dots glow softly into the room as they rise, in their own colours,
  and take the cover's colour beside them (only the dots: a bright cover's own light around
  them is no longer tinted into dark patches). With the spectrum alone,
  the light sits behind it, round, and its dots take the light's colours. It never covers the cover or the words, rests while paused, and keeps to a slow
  breath when Windows asks for less motion. The halo outside the window stays the films'.
- Music: a window taller or wider than 16:9 is filled by the scene and its light (no more
  black bars above and below, where the light stopped on a straight line); where it cannot
  reach (very wide windows) it fades softly into the black. A long title ends with "..." at
  the window's edge, and when the interface changes size at the end of a resize the cover
  glides to its place.
- Closing: saving and the engine's end happen out of sight, after Windows' closing animation,
  and Cue opened again right after closing starts at once instead of waiting for the closing
  one to finish. Settings are saved even while another program reads the file for a moment
  (a few short retries), so the place in a film is not lost on closing.
- The subtitle menu and up next open over their buttons, like the audio, speed and "..."
  menus (centred on the button, or stopped at the bar's edge), instead of in the middle of the
  bar or pushed to the window's side.
- Starting the timeline previews' helper no longer shows Windows' "working in background"
  pointer for a few seconds after opening Cue.
- Phone: the wheel's outer ring follows the song's time again after a song is opened from the
  phone's folders or on the computer (it stayed on the old song's place in the queue). It shows
  the place in the queue only in up-next mode.
- Phone: on short phones nothing lands on the title any more. A short note (+15 s, a mode,
  the next episode) takes the title's row for a moment; the film's preview box and a song's
  record and offset shrink with the stage and stay above it; the place a turn of the wheel
  went to is no longer announced on top (the time under the bar says it), and a song skipped
  to no longer flashes its file name before its title.
- Phone: a song's cover made of a video frame boxed in black bars (common in songs taken from
  video sites) is shown without the bars.
- DV camcorder files (`.dv`, `.dif`) and `.divx` files are offered in Open, in Explorer's
  Open with and Default apps, and in folders on the phone.
- Music: switching between cover + spectrum and spectrum only is quicker: the song keeps
  playing while the picture goes out, comes back where it was without a second search, and
  the pixel transition is shorter.
- Lock screen and the volume flyout: Windows' media card shows what plays (a song's title,
  artist and cover; a film's name as people say it, "Twin Peaks · S01E03", with a still of
  it), its timeline, and previous / play-pause / next.
- The phone's pairing screen on the computer glows in a song's cover colour.
- Phone: songs show their title and artist (as the computer does) instead of the file name.
- Phone: turning the wheel on a song turns its cover into a pixel record that spins with the
  thumb and turns back upright on release.
- Phone: the time on the right of the bar is what is left or the whole length (a tap
  switches, remembered for films and songs); "ends at" is gone.
- Home: the seven-click surprises are now Christmas ones: first snow (some of it lands on the
  eyes), a Santa hat, and a string of lights.
- Music: a click on the scene no longer pauses; it brings the controls back. Pause with
  the bar's button, the space bar, a media key, the taskbar or the phone. A double click
  is still full screen; films keep click to pause.
- A folder dropped on the window brings its media: into the open list, or as a new list
  that starts playing (one folder at a time, not recursive).
- Music with its own cover: choosing another audio track, or mixing tracks, opens the
  song again at the same moment instead of rebuilding its picture mid-song; a remembered
  mix is part of the picture from the start.
- Phone: the click wheel's up next mode shows a song's cover while it is picked.
- Music: the control bar rests like a film's once the pointer is still, and comes back
  with any movement; a song without a picture keeps it.
- Music: the look choice in the right-click menu is two icons at the end of its row;
  the row names the look that is on, or the one under the pointer.
- Music: after a seek the bar stays where it was sent instead of jumping back to the
  old time while the engine restarts the picture.
- A queued file that is gone (deleted, moved, a drive unplugged) is skipped with a
  short note instead of being waited on; one that fails to open is skipped too (three
  in a row at most).
- The taskbar preview's buttons stay while the next file opens instead of vanishing and
  coming back.
- Phone: songs show their covers in the up next sheet and in the folders. Cue reads the
  picture embedded in the song (MP3, FLAC, M4A, Ogg and Opus) or one beside it (the song's
  own `.jpg`, then cover/folder/front), so covers that Windows Explorer does not show
  appear too; a queued song shows its cover wherever it lives. Its picture is square.
- Phone: the click wheel's centre icon (the check, the folder, the mode icons) sits in
  the middle of the button.
- What's new on the home is per version line: 1.2.1 does not bring 1.2's news back.

## 1.2 — 2026-11-02

- Give Up next a permanent control-bar button beside repeat, in wide and narrow
  layouts; repeat no longer disappears while a queue exists. Remove the duplicate
  entry from the … menu (right-click and Q still open it).
- Anchor the Up next panel to its button: the panel's list icon sits over the
  control bar's icon, about ten rows in wide windows and a compact scale in
  smaller ones, always clear of the controls.
- Add a previous-file button beside Play while a queue is active
  (previous · −15 · play · +15 · next) and the **P** shortcut. Previous and next
  stay in place and are dimmed at the ends of the queue.
- Rework the Up next panel: title and count on top, **+ files · + folder · clear
  queue** in a footer under the list, names without extensions, a **next** caption
  on the following entry, dimmed played entries, and an empty state that invites
  dropping files.
- Make queue motion follow Cue's motion primitives: eased hover on every control,
  a lifted drag row with a pixel shadow and a spring-driven landing outline,
  removals that slide toward their × button, highlighted additions that scroll
  into view, and thumbnails that appear in pixel steps.
- Space the control bar on one rhythm: 22 art px between icons of a group and
  32 between groups, centre to centre; playback controls share the beat.
- Replace the audio waveform with a mirrored pixel spectrum visualization: bass in
  the middle, fast rise and slow fall, on the interface's dot grid. It only draws
  the sound; the audible branch is untouched and no equalizer is applied.
- Redraw the application icon: the tile fills the icon box with a visible rim and
  a larger pixel play mark, so it holds its size on dark taskbars.
  `tools/gen_icon.py` regenerates `res/cue.ico`.
- Rebuild the what's-new screen around three items: Up next as the headline card
  with an animated queue icon, three new languages (a bubble whose letter changes
  script), and fixes and improvements, with a single **continue** button.
- Show contacts in About as one row of three pixel icons (LinkedIn, GitHub,
  e-mail) over a caption that reads *contact* and names the icon in use; when no mail app answers the e-mail
  link, the address is copied to the clipboard. **licenses** becomes **credits &
  licenses** and its reader opens with the acknowledgments (`docs/THANKS.md`).
- Tint the Up next button with the film's colour while the queue has entries,
  like **CC** with subtitles on (the separate marker pixel is gone).
- Recompose the control bar in three zones on one rhythm: volume and speed,
  playback centred on the bar, and one even row of secondary buttons. Lighter
  previous/next glyphs that match the 15-second ones, and a new Up next icon (a
  play mark over two lines).
- Home: a living Recent list. Each entry has the title and what is left, a quiet
  line with the episode (*S18E15 · 3 episodes*) or the kind of file, and a progress
  rule; episodes of one show become one entry that opens the latest. The home's
  composition is about 8% larger.
- Home: the ··· menu gains icons and a rule before **clear recent**, unfolds and
  folds with a short fade, eases its hover, and closes on any click outside it.
- Size the interface by the window's area as well as its width, so 4:3 and square
  video get the controls of a comparable 16:9 window.
- Phone: music never shows the end-of-file card over the face, and the time-mode
  card shows only the time (no frame or picture request) while the face steps back.
- Context menu: a rule above **up next**, like the other entries. A right click on
  the control bar no longer opens it.
- With a menu open, another button of the bar closes the menu and acts in the same
  click; entering or leaving full screen takes an open menu along on the first frame.
- … menu: section names carry icons on the icon column (interface size, control
  background), with even spacing around the segmented tracks.
- Phone: time steps on the wheel follow the file's length, so songs are no longer
  crossed in a few detents.
- Phone: a picked queue item without a picture (audio, or video outside the shared
  folders) shows its icon instead of an empty frame.
- Regroup the … menu in four parts (file, light, appearance, places) with
  language beside remote control and About; segmented choices sit in a quiet
  track aligned with the labels. In the subtitle style panel, values get their
  own column, **make default** sits under a rule on the labels' column, and the
  font names have more room.
- Light the corners of the ambient glow under Windows 11's rounded window
  corners, which left a dark wedge at each corner.
- In short, wide windows, open menus beside the control bar when there is no
  room above it, and lift About above the bar instead of covering it.
- Incorporate a community review of the Russian translation (pull request #3),
  keeping the wording of features added in this version.
- Render the queue panel about three times faster while scrolling (scaled
  thumbnails and the panel background are cached instead of redrawn per pixel).
- Starting a queue from a folder that contains the playing file no longer lists
  that file twice.
- Phone: one **+ add** entry point with +/✓ badges on files, a count of what was
  added, **+ add this folder**, and **done** back to the queue at its end.
- Phone: rows have a drag zone on the left and a × on the right; the secondary
  menu is gone. **clear queue** animates at once, top to bottom.
- Phone: the wheel gains an **up next** mode (previous/next file on the side
  buttons, the ring walks the queue and plays on release), shown by mode dots
  under the center icon. The off-center next-file button is removed.
- Music with a cover plays as a scene: the cover on the left with the spectrum as two
  wings tinted by the cover, the title and artist to its right, and a soft light in the
  cover's colours that swells with the music. The window frames that one picture with
  the engine's zoom and pan, live while an edge is dragged and without touching the
  sound: all of it in wide windows, the cover with the words under it in portrait
  windows, the cover and its wings alone in small ones; an album's next song keeps the
  window's size. Missing
  metadata falls back to the file name. The spectrum runs at 60 fps and ends cleanly
  (no solid wings when a song finishes). The cover leans away from the pointer (a GPU
  pass, idle when nothing moves). Right-click to choose **cover + spectrum** or
  **spectrum only**; the choice is kept.
- Music: *up next* drops in at the top in a song's last 12 seconds instead of sitting
  under the cover; the film's end-of-file card and its thumbnail are not used for music.
- Music: the control bar shows previous track · play · next track (no 15-second steps)
  and the subtitles button only when the song has lyrics, on a narrower bar that eases
  back to a film's width when a video follows. A song opened on its own brings its
  folder: the album becomes the queue, in name order, so previous, next and auto-advance
  follow the tracks.
- Music: "N tracks" under the artist opens the album's list beside the cover (square
  covers, a Cue tile for songs without one); its last line adds songs, by a click or by
  dragging them in (the list opens by itself while songs are dragged over the window,
  invites the drop and adds them to its end). Portrait and small windows open it as the
  usual popover.
- Music: songs without a cover wear Cue's music tile in **cover + spectrum**;
  **spectrum only** stands where the cover stands, with title, artist and album beside it
  and the same list. The album opens even when the path the song was opened with is
  spelled differently from the folder's listing, and one unreadable file no longer stops
  the folder scan.
- Music: switching between **cover + spectrum** and **spectrum only** opens the song again at the same moment behind the pixel
  transition, instead of rebuilding the engine's graph mid-song. Fixes the look switch
  skipping to the next song, or leaving the song silent with an empty cover frame.
- Music: a new song comes in through the pixels (big blocks resolving to its picture)
  and its title slides in; the wings' dot grid is drawn once instead of on every
  picture (about a quarter less work per frame).
- Repeat has two modes when there is a list: the whole list (the plain lit icon) and
  just this one (a small 1 beside the icon). Without a list it is on and off as before.
- The picture's menu never offers to save a frame of a song, even between two songs.
- While another app's full-screen window covers Cue (a game) or Cue is on another
  virtual desktop, the interface stops drawing, the cover's shader pass rests and a
  film is presented once per frame instead of at the display's rate; all of it returns
  as soon as Cue is in view. Playback never changes.
- Phone: with music, the song's cover is the face (its colour becomes the glow), the
  side buttons go to the previous and next song instead of ±15 s, and the time row shows
  the song's length instead of when it ends.
- The taskbar buttons' words are looked up once per language instead of on every poll.
- Music never resumes from where it stopped; it starts from the beginning. No timeline
  previews are decoded for music. Full screen uses the same transition as video.
- Previous, play/pause and next buttons under Cue's preview in the Windows taskbar;
  the media key for the previous track goes to the previous file of a queue.
- Music files get their own icon in Explorer (the Cue tile with a note) and type name;
  video files keep Cue's icon. Cue registers under **Settings → Default apps**, where
  each type is listed with its own icon.
- Phone: follow the computer's interface language; an open page reloads once
  when it changes, staying paired.
- Phone: sheets crossfade with the remote when opening and closing; closing now
  animates. Hide the end-of-episode card and its check in up next mode and until
  the new file reports its duration.
- Phone: fix a tile that blinked when another one was selected; retune motion
  (lighter press scale, one gentle spring, shorter distances and staggers, a
  stepped trail on the wheel's position dot).
- Save frames directly in `Pictures\Cue`. Existing `Pictures\Cue\Frames`
  folders are left untouched.
- Separate the compact dock's speed and More hit areas, which overlapped by
  two pixels. Remove six translation strings no longer used.

- Redesign the phone queue with authenticated on-demand thumbnails, drag reorder
  and short reduced-motion-aware transitions.
  Retain keyed rows and bound preview/exit resources for large queues.
- Use a dedicated phone browser sheet when adding media, with folder busy/result
  feedback, cancellation on clear and an explanation for folders with no direct media.
- Refine queue add/remove/clear feedback, bounded staggered exits, animated panel
  contraction and interrupted reopening; hide zero/single title counts and use
  a subtle dock marker without a numeral.
- Constrain the subtitle menu to the window, with a virtualized scroll area,
  pinned controls, selected-track reveal, ellipsis and full keyboard navigation.
  Preserve fractional native wheel input and improve region/title/forced/SDH
  naming without discarding long metadata.

- Redesign Up next as a complete scrollable list with on-demand video thumbnails,
  a now-playing row, drag reorder, edge scrolling and animated insertion gaps.
  Add keyboard toolbar navigation and append drops while the queue is open.
  Thumbnail memory stays bounded to 32 small images; no disk cache is created.

- Keep subtitle reading size consistent between 16:9 and wider windows at the
  same width. Preserve saved sizes, portrait behavior, original ASS and bitmap
  tracks; align drag/clearance geometry with the compensated text.

- Add a session Up next queue for audio/video, folders, reordering and removal,
  integrated with existing next-episode behavior and the phone remote.
- Use one reversible trajectory for control-bar entrance/exit; resolve subtitle
  clearance from actual layout with cached measurements and faster settling.
- Add Russian, German and Japanese across the application, installer and remote,
  including Russian plural rules and translated About labels.
- Keep timeline thumbnails on demand in the existing 12 MiB / 256-entry RAM LRU;
  no persistent thumbnail cache or speculative scan.
- Check for updates every three days, at most once automatically per session.
- Clean public documentation; explain original attribution, derivative branding
  and build identity without modifying the GPL or adding license restrictions.
- Remove failed downloads immediately, clean stale Cue update folders after seven days, and rotate the normal log at about 1 MiB with one backup.

## 1.1.1

### Final 1.1 polish

- The next file lives on the card at the end of the video, on the phone and on **N** (or the keyboard's media "next" key). The duplicate "Next episode" / "Up next" entry in the ⋯ menu is gone.
- Phone folders: the arrow beside the title goes one folder up and only appears when there is a folder above; a separate × leaves the folders from anywhere. With several shared folders, the top of one goes back to their list.
- Phone menus: an arrow beside each title goes one step back (a value to the subtitles list, the subtitles to the menu, the menu to the player).
- The phone remote no longer zooms on a quick double tap; fast taps on + and − all count.
- Embedded subtitles stay in sync at every speed: they are timed by the engine on the video's own clock (measured at 1×, 2× and 3× with zero drift).

### Next-file preview and series boundaries

- Show the destination filename and a small asynchronous thumbnail in the player's next card.
- Match normalized series title as well as season/episode; never automatically continue a different or unidentified show.
- Keep unrelated files as manual Up next suggestions, with matching labels in the player and phone.


### Episode handoff fix

- Preserve the next filename when opening it; `open` owns its argument before resetting discovery state.
- Capture automatic destinations across EOF/END_FILE/idle events, consume them once, and clear cancelled/stale requests.
- Reject empty open requests without disturbing playback.

### Remote layout and playback follow-up

- Enlarge portrait pairing independently of the playback dock scale; wrap folder actions.
- Prioritize Escape for the pairing overlay, preserve early cancellation and return keyboard focus on close.
- Add an exit timer fallback and avoid synchronous drag handoff to the player window.
- Keep system keyboard volume/mute separate from Cue's internal volume.
- Add Next episode to the player menu and a compact, cancellable countdown in the last 10 playback seconds; automatically continue numbered episodes at EOF, respecting loop and the end-of-episode sleep timer.
- Make the phone timeline fill follow the dragged position; dragging the active wheel dot seeks continuously while other wheel gestures retain detents.

All remote, memory and disk improvements are consolidated in this release.

### Stability and input handling

- Move preview pipe writes off the UI thread into a bounded, replaceable command queue.
- Give preview workers shared ownership of their process/pipe state; release stale posted frames on shutdown.
- Restrict child handle inheritance and require successful kill-on-close job assignment.
- Bound remote lifecycle events and keep disconnects from delaying remote shutdown.
- Make file-worker cleanup exception safe and acquire Winsock ownership before thread launch.
- Bound settings reads/JSON nodes and validate numeric conversions before casting; reject implausible saved window geometry.
- See [Additional stability review](STABILITY.md) for validation and remaining limits.

### Disk and RAM

- Generate timeline previews on demand instead of scanning distant timestamps in the background after a hover.
- Bound timeline pixel storage to 12 MiB / 256 entries with LRU eviction and capacity-based accounting.
- Keep colour-only samples transient while preserving ambient-light timing and sampling quality.
- Skip unchanged position/settings writes, preserve recovery-copy refresh and reuse identical blur-shader files.
- Exclude temporary build/sync leftovers from the installer.
- Add cache/real-filesystem I/O regressions and an optional read-only Windows resource probe. See [Disk and RAM review](RESOURCES.md) for scope and tradeoffs.

### Memory lifecycle

- Fix DIB/DC ownership when pairing/news windows resize; restore selection before deleting the bitmap.
- Release full-window scene surfaces and canvases after closing; destroy overlay windows and graphics resources explicitly.
- Give the display-pacing worker independent shared state so a late DwmFlush cannot access a destroyed owner or closed event.
- Stop caching unsupported Unicode code points. Known fallback glyphs remain cached and stable.
- Release the phone file browser’s DOM/blob URLs and abort pending requests on close; release timeline preview URLs when scrubbing ends.
- Add counted resource/allocation regressions, a Chromium memory stress test and a native Windows GDI-counter test. See [Memory review](MEMORY.md).

### Phone UI and security

- Reflow remote pairing for portrait desktop windows; wrap shared-folder chips and fit text.
- Adapt the phone remote to available width/height, browser bars and safe-area insets; keep primary targets at least 44 CSS pixels.
- Fix the folder bitmap and center icon clipping.
- Harden pairing: six digits, two-minute validity, one use, CSPRNG-only 256-bit remembered tokens, and revocation when turning the remote off. Existing phones pair again once.
- Validate Host/Origin and WebSocket handshakes; require first-message authentication and HTTP Bearer headers. Add strict input validation, script nonces, CSP and resource limits.
- Restrict file access to enabled local roots; reject special Windows paths and descendant reparse points.
- Limit listing work, worker count and lazy thumbnail requests; avoid redundant menu rebuilding and release sound nodes/object URLs.
- Require SHA-256 verification for automatic installer downloads and use unique temporary folders.
- See [Security](SECURITY.md) for remaining HTTP and platform-validation limitations.

## 1.1

The phone remote, and more.

### Highlights

- **Phone remote.** Open **… → remote control** (or **··· → remote control** on the home screen) and scan the QR code: your phone becomes a remote in its browser, nothing to install. A throwback click wheel moves through the film with a preview of where it will land; the center switches it to volume. Subtitles (on/off, timing, size, position), audio track and a sleep timer are in its menu. Browse your Videos and Downloads (or any folder you add) as a grid with thumbnails, and series move on to the next episode on their own. Phones that paired once reconnect by themselves.
- **Multiple audio tracks.** Recordings with separate tracks (like OBS: game, microphone and voice chat) can play every track at once, mixed at their own levels. Cue does it on its own when the tracks are clearly parts of one recording; anywhere else, the audio menu's **multiple tracks** lets you tick the ones you want. Remembered per file.
- **Subtitles back to default.** If a subtitle style or position gets out of hand, **CC → restore default** brings yours back in one click, and a second click undoes it.
- **Always on top.** A pin beside the window buttons (or **Ctrl+T**) keeps Cue above other windows while you work.

### Fixes and improvements

- A short **what's new** screen after updating, and a **what's new** line on the home until you close it.
- Interface sizes reworked: four even steps that always look different from each other, following Windows' scale up to 5K and 8K screens. **Ctrl + mouse wheel** changes the size, like **Ctrl +** / **Ctrl −**.
- The control bar lands exactly where its corner marks show when you drag it (a short drag no longer snaps back to the default place).
- Changing audio tracks keeps you at the same moment of the film, with a black-and-white transition while the sound switches; subtitles always come back afterwards.
- The pairing screen opens with its own animation and a soft sound; the film waits behind it, paused and in black and white.
- Lighter when idle: with the remote on and no phone connected, Cue does almost no extra work.

## 1.0

First public release. The 1.1.x beta numbers below were internal builds before it; they are older than 1.1.

- Five languages across the app, installer, and uninstaller: Brazilian Portuguese, English, Spanish, French, and Simplified Chinese. Automatic Windows display-language matching, saved manual selection, live switching, and pixel fallback glyphs.
- Native installer, shortcuts, Windows uninstall integration, and GitHub update checks every 15 days with SHA-256 download verification. (Later shortened to every three days; see 1.2.)
- Four-screen onboarding for Cue, ambient lighting, interface size, and upcoming features.
- Four adaptive interface sizes (small, medium, large, larger), with medium selected for fresh installations; consistent width/DPI scaling and a stacked vertical-video layout.
- Minimal playback controls with volume, speed, ±15-second seek, repeat, subtitles, conditional audio-track selection, and fullscreen.
- Two control styles, scene-driven ambient lighting, timeline previews, saved positions, and audio-only playback with a reactive waveform.
- Draggable subtitles with font, style, opacity, and synchronization settings; playback pauses during repositioning and resumes when appropriate.
- Shader-driven pixel fullscreen transitions, native window corners, and updated menus and input handling.

See the [user guide](GUIDE.md) for controls and the [release process](UPDATES.md) for packaging.

## Beta history

Condensed development history. Entries describe their respective builds; later versions may replace these behaviors or defaults.

| Build | Main changes |
|---|---|
| 1.1.21 beta | Subtler frameless blur cloud; reversible subtitle/control transitions; shared 60 Hz animation/upload deadlines; buffer-in-flight and reduced-motion protections. Native frame rate still requires Windows validation. |
| 1.1.20 beta | Independent subtitle return animation; window aspect ratio follows each new file; small UI temporarily became the default; revised resize targets; two-pass Gaussian blur. |
| 1.1.19 beta | Uniform white frameless controls with local blur cloud; reversible appearance transition; revised resize hit areas; reduced Win32 calls and preview sampling work. |
| 1.1.17 beta | Experimental local scene-contrast colors for all frameless controls, including timeline segments, with smoothing and hysteresis. |
| 1.1.16 beta | Scene-dominant frameless control color; removed extra luminance analysis; native border/corner adjustments. |
| 1.1.15 beta | Added saved control-background preference and frameless layout, using the existing scene analyzer. |
| 1.1.14 beta | Taller volume track/handle and larger window-button targets/icons. |
| 1.1.13 beta | Horizontal home screen without media; corrected vertical/horizontal volume proportions. |
| 1.1.12 beta | Vertical volume, speed, and More returned to the top; timeline moved to the bottom; removed permanent circular play background. |
| 1.1.11 beta | Intermediate vertical hierarchy and compact secondary controls. |
| 1.1.10 beta | Three-row vertical controls; horizontal layout and compact ±15-second icons retained. |
| 1.1.9 beta | “Cue beta” branding and Windows prerelease metadata. |
| 1.1.9 | Restored compact seek icons while retaining 15-second actions and tooltips. |
| 1.1.8 | Fullscreen/window-state fixes, vertical controls, C shortcut, playback feedback, synthesized saved-frame sound, Pictures/Cue/Frames migration, elapsed/remaining time toggle, and ambient lighting enabled for new preferences. |
| 1.1.7 | Six supplied action SVGs compiled into pixel assets; .tinypeep branding; compatibility identifiers retained. |
| 1.1.6 | File-information panel, in-app license reader and source access; no additional metadata decoder. Earlier packages bundled source ZIPs. |
| 1.1.5 | Supplied multi-resolution application icon; engine and corresponding sources remained build 1.1.4. |
