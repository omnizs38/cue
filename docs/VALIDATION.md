# Validation · Cue 1.2.2

Validation date: 2026-11-03. Environment: Linux x86_64, GCC, MinGW-w64 GCC 13
cross-compiler and Chromium Headless Shell 134 through Playwright 1.51.1.
The engine DLL is unchanged (SHA-256
`26fa28434e30aa319a8ab39fba76dbc4a84757b89349c1fc69b5e55ea6c02bca`).

## Automated coverage

- 46 portable core/UI test programs cover cache bounds/LRU, unchanged settings I/O, state
  limits, parser/security fuzzing, episode ownership/EOF, geometry, audio graph
  switching commands, subtitles, overlays, input and shutdown-related primitives.
- Localization: 337 keys across eight complete catalogs, placeholders, Russian
  one/few/many forms, regional Windows language mapping, persistence, and glyphs.
- Layout matrix: 47,088 checks across eight languages, four UI sizes and ten
  window/DPI combinations, including portrait, fullscreen and installer.
- Queue regression: audio/video ordering, stable IDs, repeated paths, current-item
  removal, explicit end, 512-item cap, 10,000 mutations, strict remote commands
  and transient panel bounds in all eight languages.
- Queue sheet: a 512-item list, absolute stable-ID drag moves, animated insertion
  gaps, edge auto-scroll, mid-gesture revision/resize cancellation, scrollbar
  capture, keyboard toolbar/navigation, current-row transport, image cache/failure
  bounds and 96 additional short/portrait/wide layouts. Reduced motion settles
  immediately; a wheel over the panel never changes volume.
- Refinement regressions cover zero/single counts, bounded clear snapshots,
  mid-clear edits, interrupted reopening and finite exit/height motion at
  60/120/144 Hz. Large subtitle lists cover 0/1/2/500 tracks and 96 additional
  locale/scale/window layouts, fixed controls, full metadata, selected visibility,
  fractional wheel input, keyboard and unchanged rows on playback ticks.
- Motion: continuous reversal at three points in the exit interval at
  60/120/144 Hz, reduced motion, and bounded subtitle return in both styles.
- Chromium: existing remote layout regression at six viewports, menu and folder
  regressions; queue actions, text escaping, fetch cancellation and 44 CSS-pixel
  touch targets across eight languages at 320×568, 390×844 and 844×390.
- Queue transport: 288 native UI layouts keep Play stable and next-file distinct
  from +15 seconds across scales, window widths, audio counts, styles and speeds.
  Another 288 docks (three scales, eight widths from compact to wide, one or three
  audio tracks, with and without a queue or next item, two speeds) keep both the
  Up next and repeat buttons present, with no overlapping hit areas, and open the
  panel from the button.
- Queue seeding: a batch that contains the playing file (any ASCII case or slash
  direction) adds it once; unrelated batches are unchanged.
- Phone adding: in every language/viewport scenario, adding mode shows **done**
  instead of close and the folder footer; a tapped file sends one `queue-add`,
  gains the queued mark, ignores a second tap and is counted in the status line;
  **done** returns to the queue. After a folder add, the queue reopens at its end
  with the new row highlighted.
- Mobile refinement: another 24 language/viewport scenarios cover keyed rows,
  drag reorder, optimistic placement, next-file versus seeking, folder busy/result
  feedback, empty/single counts and failed-fetch backoff. A 512-row stress checks
  32 preview URLs, 12 exit rows, reduced motion and ten rapid reopen cycles.
  Mock-server previews validate the browser lifecycle, not Windows providers.
- Browser lifecycle stress: 10 warmups, 100 browse cycles and 20 rapid-close
  cycles end with zero tracked blob URLs, hidden file images or connected sound
  edges. Forced-GC heap delta: +248,672 bytes. This is not whole-player leak proof.
- FFmpeg audio-visual fixture (FFmpeg 6.1): the spectrum-visualization graph
  generates a 1024×576 output with a 384×200 mirrored spectrum and reflection from a real
  signal. The audible PCM branch is byte-identical to the source (same MD5).
  Every filter it uses is present in the shipped libmpv-2.dll.
  This checks filter behavior in the available FFmpeg, not native GPU pacing.
- AddressSanitizer/UndefinedBehaviorSanitizer pass the queue-sheet interaction/
  rendering regression, the 500-track subtitle viewport, and the remote security/parser regression (12,000 malformed inputs). Leak detection is disabled because native
  process inspection is restricted; this is not a LeakSanitizer result.
- Application and installer are cross-built for Windows x64. Packaging checks
  cover payload CRC, required documents/fonts, engine identity and source hygiene.

Generators: `python3 tools/gen_i18n.py --check` and
`python3 tools/gen_remote_page.py --check`. Main suite: `bash tools/test.sh`.
Browser tests require Playwright and Chromium; `CUE_CHROMIUM_PATH` can select
an installed executable. Queue browser tests: `node tests/remote_queue_browser.cjs` and
`node tests/remote_queue_polish_browser.cjs`. Language follow-up:
`node tests/remote_language_browser.cjs` (one reload into the new language, no loop). Music: `node tests/remote_audio_browser.cjs` (no end-of-file card over the face).

## Subtitle aspect regression

`subtitle_geometry_test.cpp` verifies constant reading size at equal window widths
from 16:9 through 32:9, widths through 4K, all four representative subtitle sizes,
text/outline/padding geometry, authored ASS/bitmap preservation, invalid dimensions
and repeated format changes. An FFmpeg/libass fixture at 1920×1080 and 1920×800
produced the same 842-pixel text width and 45/44-pixel glyph height (one pixel of
rasterization difference). This is not a native Windows player capture.
The existing normal-window motion, style and layout
regressions are retained. Actual Windows render comparisons remain a native check.

## Native checks for 1.2 interface changes

Run these on Windows with the release build:

1. **Up next button.** Play a single file with no queue. Expected: the control bar
   shows the Up next button beside repeat; repeat still toggles with a click and L.
   Click Up next. Expected: the panel opens above (or below) the bar with the empty
   state and the add/clear footer. Resize the window down to the narrow layout.
   Expected: both buttons remain, without overlapping other controls.
2. **No duplicate seed.** While a file is playing with no queue, open Up next and
   choose **add folder** on the folder that contains the playing file. Expected:
   the playing file is the first entry and appears once; the next entry is
   captioned **next**.
3. **Drop to queue.** With the panel open, drop two files from Explorer onto the
   window. Expected: both are appended, highlighted briefly, and the list scrolls
   to them if they land out of sight. Playback is not interrupted.
4. **Drag motion.** Drag a row by its grip across several rows and release.
   Expected: the row lifts with a shadow, the dotted outline glides to the landing
   slot, the order is committed once, and Escape during a drag restores the order.
5. **Saved frames.** Right-click → save frame. Expected: the PNG is written to
   `Pictures\Cue` (not `Pictures\Cue\Frames`), and the confirmation's folder
   button selects it in Explorer. An existing `Pictures\Cue\Frames` folder and its
   contents are unchanged.
6. **Panel anchor.** Maximize the window on a 1080p or larger screen and press Q,
   with and without a following file. Expected: the panel's list icon sits directly
   over the control bar's Up next icon, about ten rows are visible, and the bar is
   not covered. Restore a small window (for example 1280×720 at 200%). Expected: the
   panel still rises from the same button, at a compact scale.
7. **What's new.** Update over an earlier version, or open it again from the home
   screen (**what's new**). Expected: Up next is the headline card with a moving list icon, the languages
   bubble cycles A, Я, あ, Ü, the third item reads *fixes and improvements*, and the
   only button is **continue**.
8. **Contacts.** About → the row of three icons (an "in" badge, `</>`, an envelope).
   Expected: the caption under them reads *contact*. Hover each. Expected: the
   caption crossfades to LinkedIn, GitHub and the e-mail address, and back to
   *contact* when the pointer leaves. Click the
   envelope. Expected: the default mail app opens a message to
   tinypeepstudio@gmail.com with the subject "Cue 1.2"; with no mail app
   associated, a toast says the address was copied, and pasting yields it. The row
   below reads **credits & licenses** and its reader lists the acknowledgments.
9. **Icon.** Pin Cue to the taskbar (dark and light Windows themes, 100% and 150%
   scale). Expected: the tile reads at the same size as neighbouring icons. Windows
   may keep the old icon until its icon cache is refreshed.
10. **Spectrum visualization.** Play an MP3 or FLAC. Expected: a mirrored dot
   spectrum centred in the window moves with the music, bars fall back slowly, and
   pausing freezes it. Sound is unchanged; there is no equalizer control.
11. **Phone adding.** On a paired phone, open Up next and tap **+ add**. Expected:
   shared folders open with **done** in the header. Tap two files. Expected: each
   badge turns into a check mark, the computer queue gains exactly two entries,
   and the footer reads *added to queue · 2*. Tap **done**. Expected: the queue
   reopens at its end with both entries highlighted.
12. **Phone rows and clearing.** Drag a row by its left zone (handle or thumbnail).
   Expected: the row lifts and the order changes on the computer. Tap × on a row.
   Expected: it slides out and leaves the computer queue. Tap **clear queue**.
   Expected: rows leave top to bottom immediately, then the empty message appears.
13. **Phone wheel modes.** With a queue, tap the center until the third dot lights.
   Expected: « / » go to the previous / next file; turning the ring shows a card
   with position, name and preview; lifting the finger plays that file. Without a
   queue, only two dots appear and the center toggles time and volume. In the
   third mode, play to the last 90 seconds of a file. Expected: no next card and no
   check button appear; after » the new file never shows a check.
14. **Phone sheets.** Open and close Up next on the phone. Expected: the wheel fades
   out as the sheet rises and fades back as the sheet sinks and fades; with Reduce
   Motion on, both changes are immediate.
15. **Previous and next buttons.** Start a queue and play its first file. Expected:
   the bar shows previous · −15 · play · +15 · next, with previous dimmed and not
   clickable. Press N, then P. Expected: Cue moves to the second file and back, and
   previous is dimmed again on the first file; on the last file next is dimmed.
16. **Ambient light corners.** On Windows 11, in a restored (not maximized) window,
   enable ambient light on a bright video. Expected: no dark wedge at any of the
   four rounded corners; the glow meets the window edge continuously. Maximize.
   Expected: unchanged glow, no light inside the frame.
17. **Short, wide windows.** Resize the window to about 2560×720 (or 3440×900) and
   open the … menu, the speed menu and About. Expected: each opens beside the
   control bar or above it without covering it, and stays inside the window.
18. **Queue colour.** Play a video with a strong colour and start a queue.
   Expected: the Up next icon takes the film's colour (no extra marker), like **CC** with
   subtitles on; clearing the queue returns the icon to grey.
19. **Phone language.** With a phone paired and the remote page open, change the
   language in **… → language**. Expected: within a second the phone page reloads
   in the new language and stays paired; it reloads only once.
20. **Menu rhythm.** Open the … menu at each interface size, in a landscape and a
   portrait window. Expected: four groups separated by rules; the size and control
   background names carry their icons on the icon column, their tracks start exactly
   at the labels' column, keep air below them, and end where the rules end
   (rule to rule in narrow windows); a section name right after a rule sits close
   to it, as close as the first entry of any group; language, remote control and About are together at the
   end. Open CC → style. Expected: values never touch the end of their slider,
   **make default** sits under a rule on the labels' column, and the three font
   names have room on both sides.

21. **Control bar.** Play a video with and without a queue, at 100% and 200%, in a
   wide and a narrow window. Expected: Play sits on the bar's centre; the right-hand
   buttons are evenly spaced; with a queue the order is previous · −15 · play · +15 ·
   next. The Up next icon is a play mark over two lines, with no dot.
22. **Living Recent list.** Watch part of two episodes of one show (named like
   `Show - S01E02.mkv`) and part of a film, then return home. Expected: one entry for
   the show with its latest episode and *2 episodes*, the film without its extension,
   *N min left* on the right, and a progress rule under each started entry. Click the
   show. Expected: its latest episode resumes. Shrink the window. Expected: fewer
   entries, never overlapping the window's edge.
23. **Phone: audio in up next.** With a queue of audio and video files, switch the
   wheel to the third mode and turn it onto an audio file. Expected: the card shows a
   note, the position and the name, not an empty picture.

24. **Home menu.** Click ··· beside Recent. Expected: the menu unfolds with a short
   fade and drop; each entry has an icon, **clear recent** sits under a rule, hover
   eases in. Click an empty part of the window. Expected: the menu folds away (the
   window does not start a drag).
25. **Phone with music.** Play an audio file in a queue and let it reach its last
   90 seconds. Expected: no card covers the face. Turn the wheel in time mode.
   Expected: only the time and the difference show, without a frame, and the face
   fades out until the finger lifts.
26. **Square and 4:3 video.** At 100% and 125% display scale, open a 4:3 or square
   video and compare with a 16:9 window of similar area. Expected: the same control
   size. Right-click. Expected: **up next** has a rule above it like the others.

27. **Menus and full screen.** Open the … menu (and, separately, CC) and press F,
   or click the full-screen button. Expected: the menu leaves with the controls on
   the first frame and the snap plays cleanly; with the menu open, one click on the
   full-screen button is enough. Right-click the control bar. Expected: nothing opens.
28. **Phone wheel on a song.** Play a 3-minute song and turn the wheel quickly.
   Expected: steps of about 13 seconds at full speed, 2 seconds turning slowly.

29. **Music with a cover.** Play an MP3 or FLAC with embedded cover art in a 16:9 window.
   Expected: the cover sharp on the left, two dot wings that move smoothly (60 fps) with
   the music in the cover's colours, title and artist to the right, level with the cover,
   and a soft light behind that swells with the bass. Let the song end. Expected: the
   wings keep their dots (no solid grey blocks). In the last 12 seconds of a song with a
   next track, *up next* drops in at the top; no card appears near the control bar.
   Drag an edge to make the window portrait, then small (about 600x450). Expected: the
   picture follows the drag without a pause or a transition; portrait shows the cover with
   the words under it, small shows only the cover and its wings above the bar; the window
   keeps the size given, also when the next song starts. Seek several times. Expected: the picture stays, the
   time never jumps to zero. Point at the cover's corners. Expected: it leans away from
   the pointer by a few degrees with a band of light, and settles flat with a soft spring
   when the pointer leaves. With Reduce Motion on, it does not tilt.
30. **The two looks.** Right-click the picture. Expected: a choice with icons between
   **cover + spectrum** and **spectrum only**; choosing changes the picture after a short
   pixel transition, the sound continues, and the next song keeps the choice. A file
   without a cover shows no such choice. An OGG/Opus file with a `cover.jpg` beside it
   shows the cover.
31. **File icons.** Open **Settings → Apps → Default apps → Cue**. Expected: Cue is listed
   with each type it opens. Set `.mp3` and `.mkv` to Cue there. Expected: in list and details views the MP3 shows the
   dark tile with a note and the MKV Cue's play tile; the Type column reads *Áudio do
   Cue* / *Vídeo do Cue* (in Portuguese).

32. **An album.** Open one song from a folder with several songs. Expected: the queue
   button is lit; the control bar shows previous · play · next (no −15/+15), the
   subtitles button is absent for a song without lyrics; previous and next walk the
   folder in name order and the next song starts by itself at the end. The queue panel
   lists the songs with square covers. Close and reopen the song. Expected: it starts at
   0:00. Hover the timeline. Expected: only the time, no picture.
33. **Taskbar buttons.** While a song plays, point at Cue's taskbar button. Expected:
   previous, pause and next under the preview; pause toggles to play; previous dims on
   the first track and next on the last. On the home the buttons are gone.
34. **Full screen with music.** Toggle full screen with the cover look. Expected: the same
   pixel transition as video, no frozen frame.
35. **Switching looks.** Mid-song, right-click → **spectrum only**, then back to **cover +
   spectrum**, several times, also while paused and after the song ended. Expected: each
   switch is a short pixel transition; the song stays at the same moment (paused stays
   paused), never skips to the next song, never goes silent; the spectrum-only look shows
   no empty cover frame and no captions.
36. **The list beside the cover.** In a 16:9 window with an album playing, click **N
   tracks** under the artist. Expected: the words rise to the cover's top and the songs
   appear under them, aligned with the words; songs without a cover show the black Cue
   tile; clicking a song plays it; clicking the line again closes the list. Make the
   window portrait. Expected: the same line opens the usual panel above the bar.
37. **Repeat.** With an album: click repeat three times. Expected: lit (the list), lit with
   a small 1 (this one), off. On "the list", let the last song end. Expected: the first song
   starts. On "this one": the song starts over and no *up next* appears.
38. **Track change and bar width.** Let a song end into the next. Expected: big pixels
   resolve into the new cover and the title slides in. Queue a video after a song.
   Expected: the bar eases from music's width to a film's.
39. **Covered by a game.** Play an album, then start a full-screen game (or any full-screen
   window) on the same monitor. Expected: music continues; Cue's log shows "covered by a
   full-screen window"; CPU/GPU use of Cue drops. Alt-Tab back. Expected: the interface is
   live at once.
40. **Phone with music.** Expected: the song's cover replaces the eyes (glow in its colour,
   dimmed while paused); the side buttons go to the previous and next song.
41. **Adding songs.** With an album playing in a 16:9 window, drag two MP3s from Explorer over
   the window. Expected: the list opens beside the cover, scrolls to its last line, which
   brightens, breathes and reads *drop to add to the list*. Drag them away: the list closes.
   Drag again and drop: the songs appear at the end of the list (briefly highlighted) and
   the list stays open. Click the last line: Windows' file picker opens.
42. **Songs without a cover.** Play an MP3 without cover art. Expected: Cue's black tile with
   its note, the wings, and the title beside it; right-click → **spectrum only**: the
   equalizer stands where the tile was, the title, artist and album stay beside it, and
   **N tracks** still opens the list.
43. **Seeking a song.** Click far along the timeline of a song with a cover, several times.
   Expected: the knob and the time stay where clicked; they never jump back to the old time.
44. **A queued file that is gone.** Queue an album, delete the next song in Explorer, press
   next. Expected: a note "couldn't open … · skipped" and the song after it plays at once;
   no busy cursor.
45. **Taskbar buttons across songs.** Point at Cue's taskbar button and press next several
   times. Expected: the buttons never disappear; the preview updates.
46. **Phone covers.** Open the up next sheet and a music folder on the phone. Expected: songs
   show their covers (square), songs without one the note.
47. **Click on a song.** Click the cover and the empty scene. Expected: the music keeps playing
   and the bar comes back; space pauses; a double click goes full screen without pausing.
48. **A dropped folder.** With an album's list open, drop a folder of songs on the window.
   Expected: "reading…", then its songs join the end of the list. On the home, drop a folder:
   a new list starts playing from its first file.
49. **Covers Explorer does not show.** Queue an `.opus` (or `.ogg`) song with an embedded
   cover and an MP3 kept outside the shared folders, then open up next on the phone.
   Expected: both show their covers, square; the wheel's centre check sits in the middle.

50. **Music's light show.** Ambient lighting on, play an album with covers in the cover look,
   then in spectrum only. Expected: soft light inside the window around the cover and in its
   corners, in its colours, breathing slowly in and out (deeper in full passages), the beats barely visible, the spectrum's dots glowing a short way into the room as they rise; with a bright cover, no dark patch beside the wings; in spectrum only, a round light behind the equalizer and no dark square where the cover was; switching the look keeps the song going until the pixels are out and comes back in about half a second; nothing outside the window; pausing settles
   it; ambient off removes it; Windows' "Show animations" off leaves only a slow breath.
51. **Lock screen.** Play a song with a cover, press Win+L. Expected: the card shows title,
   artist and cover; play/pause and next work from it and from the volume flyout; a film
   shows its name; going home clears the card.
52. **Phone record.** Turn the wheel on a song with a cover. Expected: the cover folds into a
   pixel record that turns with the thumb and the offset under it; on release it unfolds
   upright. The right-hand time switches between −left and length with a tap.
53. **Christmas.** Click the home's eyes seven times, three times over. Expected: snow (with
   caps on the eyes and a shake), a Santa hat, a string of lights; the eyes return to normal.
54. **Music in any window shape.** Ambient lighting on, a song with a bright cover; drag the
   window taller than 16:9, then much wider, then small, and let go each time. Expected: no
   black band above and below the scene in the taller window (the light reaches the edges);
   in a very wide one the scene fades softly into black at its sides, never over the cover or
   wings; a long title ends with "..." at the window's edge; when the interface changes size on
   letting go, the cover glides to its place; a film still letterboxes in black.
55. **Maximized stays maximized.** Maximize Cue on a song of an album, press next and previous,
   open a film from the home, go back home. Expected: the window stays maximized every time;
   restoring it gives a window shaped for the file playing.
56. **Up next in a small window.** Make the window small (about 500 x 320) and open up next,
   with a film and with a song. Expected: the list never reaches the pin, minimize, maximize
   and close buttons.
57. **Black and white.** Right-click a song with a colourful cover, choose black and white,
   then colour. Expected: the spectrum and the light lose their colour (the cover keeps it, also
   when it tilts under the pointer) and get it back in a moment, without the song stopping; the next song opens in the chosen one; a film stays in colour.
58. **Shortcuts.** Press ? while something plays, at a few window sizes and languages, then ?
   again; open it from About and press Esc. Expected: the whole sheet, nothing cut, clear of
   the window's buttons; ? closes it; Esc from About's link goes back to About. Click the
   picture, then press l, n and Ctrl+T: they work. Pointing at a button shows its key, dimmer.
59. **Cover in black bars.** Play a song whose cover is a video frame with black bars above and
   below. Expected: the cover is the picture alone, square, and its light has its colours.
60. **A square window.** Play a 1:1 video in a square window. Expected: the control bar about
   two thirds of the width; a 16:9 window of the same width keeps its usual size.
61. **Language, live.** With a song playing and its list (N tracks) showing, switch the language
   in ··· → language. Expected: "N tracks" and the home's recent list change at once.

## Native release checks not covered by portable tests

- Native thumbnail-provider availability/latency, queue drag/capture and touchpad
  scrolling across DPI/fullscreen changes.
- Actual Windows playback and EOF queue transitions with mixed audio/video,
  duplicate entries, failed files, very large/slow folders and rapid replacements.
- Real D3D11/vsync frame pacing, fullscreen while playing/paused, resize/DPI,
  large/multiline/ASS/bitmap subtitles, and multi-track audio synchronization.
- Real iOS Safari/Android pairing, reconnect, share revocation, folder junctions,
  timer/loop interactions and navigation while the computer changes files.
- Installer upgrade/uninstall, Windows display-language preference order,
  font fallback, native license reader, Narrator, offline updates and failed
  downloads/launches. Seven-day temp cleanup needs a native filesystem fixture.
- Sustained process/GPU/codec memory and handle profiling after warm-up.

The installer is a release candidate pending native release checks. No native
Windows test, real-phone test, complete security audit or legal enforceability
review is implied. Translation catalogs are complete and visually checked;
native-speaker review remains useful for phrasing. The GPL text is unchanged.
