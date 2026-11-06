# Validation · Cue 1.2.3

Validation date: 2026-11-03 (1.2.3). Environment: Linux x86_64, GCC, MinGW-w64 GCC 13
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
62. **Minimize and back.** Play a song (and later a film), minimize Cue, wait, restore it a few
   times. Expected: the sound never stalls or pauses for a moment, the picture is live at once.
63. **A new look.** Switch cover + spectrum → spectrum only → cover only while a song plays.
   Expected: the sound dips softly and comes back, never cut; the volume slider never moves.
64. **Cover only.** Right-click a song, choose the third icon. Expected: the cover over a still
   light, no spectrum; Task Manager shows Cue clearly lighter than with cover + spectrum.
65. **Vertical video menus.** In a portrait window open ···, CC and up next. Expected: each
   about as wide as the control bar, not covering most of the picture.
66. **The cover's light.** A song whose cover is bright on one side. Expected: the soft light is
   centred around the cover, in its colours, not off to one side.
67. **Cover only from the start.** Choose cover only, skip to the next song. Expected: it starts
   at 0:00, from its first note.
68. **Tilt off.** Right-click a song, turn off "tilt the cover", point at the cover. Expected: it
   stays flat; the choice is kept after reopening Cue.
69. **An update over an update.** Install 1.2.3 over 1.2.2 while Cue plays. Expected: Cue closes,
   updates, opens; no `.cue-install-*` folder is left in Cue's folder.
70. **Back to Cue.** Play a film, click another window, then click the picture of Cue once.
   Expected: the controls appear and it keeps playing; a second click pauses; clicking the
   play button on the bar while Cue is in the background acts at once.
71. **A song opening.** Open a song, then the next, then go back. Expected: the cover, its light
   and the media card's picture come together and quickly; going back is instant. In cue.log,
   "first video frame (open N ms)" is no longer than before.
72. **Preview and subtitles with the bar.** Sweep the pointer along the timeline: the preview
   stays under it. With a subtitle on screen, move and then rest the pointer: the subtitle rises
   and settles together with the bar, never after it.
73. **Up next, only the series.** Play a film in Downloads beside songs and other films. Expected:
   no up next. Play *Show S01E01* beside *Show S01E02*: up next offers it.
74. **The media card's picture.** Play a film for 10 seconds, press a media key. Expected: the
   card shows a frame of the film; nothing named `cue-media-card-*` appears in %TEMP%.
75. **A slow folder.** Open a film from a network share or a sleeping external disk. Expected:
   the window opens without freezing; up next appears a moment later.
76. **The subtitle's hand.** Point at a subtitle with the style panel open. Expected: the hand
   is about the size of the arrow.

77. **A bar that fits.** Narrow a film's window slowly from wide to small. Expected: repeat and up
   next go first, the volume line shortens, speed and audio leave, the volume becomes an icon
   (a click opens a column upward), subtitles last; play never moves off the centre; nothing
   flickers when the window stops at an edge; everything that left is in "..."'s first row.
78. **Vertical video.** Open a phone video. Expected: two rows (controls, timeline), no third.
79. **Mini player.** Right-click a film > mini player (or Ctrl+M). Expected: the item sits under
   its own line in the menu; a small window in the bottom-right corner, with subtitles readable;
   its bar holds play, volume (a click opens the column upward), the timeline, back to full size
   and "..."; no ambient light while it is small. Drag it to the middle of the screen and let
   go: it stays there. Let go a finger's width from a corner: it glides into it. Drag it to the
   top or a side of the screen: Windows' Snap does not resize it. Click another app: it goes
   behind; pin it: it stays above, and the next mini player starts pinned. Esc: back to the normal
   window (a second Esc goes home). Play a 4K film in it for ten minutes while working in another
   app: no stutter; cue.log says "mini player off (N late or dropped frames)" and no "engine:"
   lines about underruns.
80. **Super resolution.** On an RTX PC, with RTX Video Super Resolution on in the NVIDIA app,
   play a 720p or 1080p film full screen on a larger screen. Expected: the NVIDIA app reports it
   active; cue.log says "super resolution on". A 1440p or 4K film: never. With it off in the
   NVIDIA app: no change at all.
81. **Smooth motion.** A 24 fps film with a slow pan on a 60 Hz screen (also a 4K one). Expected:
   "smooth motion" in "..." is off; turn it on: cue.log says "smooth motion on (23.976 fps on 60.00
   Hz)" and the next "video ..." line says "sync display-resample"; the pan moves in even steps.
   A 60 fps video, or a 144 Hz screen: the row is absent.
82. **Music and the taskbar.** Play a song, minimize and restore Cue twenty times, quickly and
   slowly. Expected: the sound never stops, not even for a moment.
83. **Square music windows.** A song in a square window. Expected: the title and artist under
   the cover; the spectrum's wings whole inside the window; no black band at any edge.
84. **Small music windows.** Narrow a song's window. Expected: the bar shrinks to previous, play,
   next, the timeline, volume and "...", centred, and grows with the window; full screen and
   repeat wait in "...". A wide, short window: no dark band on the left. Resize the window, then
   play the next song: the window keeps its size.
85. **Moving the bar.** "Without background" on: drag the bar from a gap between its buttons to
   the bottom-left corner: it goes there; a click in the same gap pauses. A phone video in a
   narrow window: drag the bar left and let go, then widen the window: the bar is centred.
87. **Into the mini player.** Ctrl+M on a 4K film and on a song, with a menu open just before.
   Expected: the picture breaks into pixels where it is, then the small window flies into the
   corner, sharp before it lands; never the big frame squeezed into the small window, a cut
   picture or black edges. Ctrl+M again (and the arrow at the top-left, and from a maximized
   window): the small window breaks up drifting toward its old place and opens there out of the
   pixels. In the mini player F, a double-click and ? do nothing; "..." has no "back to full size".
   Pin on: drag from the picture and from the bar's empty places: it moves.
88. **A song's mini player.** Expected: a card about twice as wide as tall: the cover on the left,
   no spectrum wings, the title, artist and list at the top beside it, the timeline and volume ·
   previous · play · next · "..." at the foot, level with the cover; the song's light behind it.
   The cover does not tilt under the pointer. "N tracks": the list opens in the column, readable,
   on the song playing, the words stepping aside. Skip to the next song: still the card, no
   spectrum, a light quick dissolve. Drag a corner:
   it keeps its shape within its sizes.
89. **Small music windows.** A song in a portrait window (600 x 1200) and a small landscape one.
   Expected: cover, words and bar as one group a little above the middle, the bar right under the
   words (no hole), staying up while the pointer rests.
90. **Jumps without a pop.** A song with a steady tone: click ahead on the timeline ten times,
   press the arrows. Expected: no pop, no stall; the sound comes back softly at the new place.
91. **The light and the window.** Ambient light on, a film: drag the window around: the light
   moves with it, never goes out. Drag an edge: the light goes at once, no outline of it stays;
   let go: it comes back around the new size.
92. **The smallest window.** A film: narrow and shorten the window as far as it goes. Expected: it
   stops while the bar still leaves room for the picture; a song stops a little smaller.
95. **A long title, wide window.** A song with a long title in a 16:9 window, then resize it.
   Expected: the composition of 1.2.3 (the light all the way to the right edge, no black fade
   beside the title); the title ends in "..." only at the window's edge.
93. **The data folder.** Update a Cue 1.2.3 that has settings. Expected: after the first start,
   `%APPDATA%\tinypeep\Cue` holds state.json, cue.log and the shader cache (recents, positions,
   language kept), `%APPDATA%\tinyoffice\Cue` is gone; Cue is still the default player where it
   was. Uninstall without keeping settings: both folders gone.
94. **Saving under an antivirus.** Use Cue for an hour with Windows Defender on. Expected: no
   "could not save state" in cue.log.
86. **A small log.** Use Cue for a day. Expected: cue.log grows by a few lines per film (start,
   video, smooth motion, mini player, bye), never past 256 KB; a log whose first line is more
   than 5 days old is gone after the next start.

96. **Into the mini player and back, one movement.** A 4K film, a 1080p film and a song, in a
   normal and a maximized window, Ctrl+M then Esc, five times each, also quickly in and straight
   out. Expected: in, the picture shrinks continuously from the window into its corner, still
   playing, and lands sharp with a little settle; out, it opens out to the old place and size
   (maximized again). Never a blink, a black or stretched picture, a frame of the big bar in the
   small window, square corners for more than the flight, or a second window in Alt+Tab or the
   taskbar. cue.log: no error. On a PC where it cannot (remote desktop), the plain flight.
97. **Songs in the card.** In the card, next, previous, a song from the list, the phone's next,
   and a song that ends, ten times quickly. Expected: the cover turns away and the next turns in;
   never the spectrum's wings, a black room, the whole scene small, or the old cover flat for a
   moment; the sound fades out the old song and starts the new one at once, never silent, never
   a click; the room's light crosses to the new cover's colours. Leave the card on a song opened
   there: the full player shows its cover without the spectrum until the next song.
98. **The card's list and menu.** On 100%, 150% and 200% screens, open the card's list and
   "...", move and resize the card, open them again. Expected: one line per song at the size of
   the artist line, a click on a row's cover plays it, the dots at its left drag it; the menu as
   big as the card's words, before and after the move.
99. **The halo past the screen's edge.** A film with the ambient light, drag the window half past
   the left (then right, top) edge, let go, and drag it back; also across to a second monitor.
   Expected: the halo is whole around the window on the way back and on the other monitor, never
   cut on a straight line inside the screen.

100. **No outline in the mini player's flight.** A film into the mini player and back, on Windows 11
   (light and dark accent colours). Expected: the flying picture has no grey 1 px frame at any
   point; the window that lands has none either.
101. **Opening a film in its own shape.** From the home, open a 16:9 film, then a 2.39:1 film,
   then a vertical phone video, each from a window of another shape; also in 4K. Expected: the
   black window takes the film's shape first, then the picture opens in its pixel ellipse,
   centred on the picture, without a stutter; a film opened maximized or in full screen opens at
   once.
102. **Full screen without the L.** Enter and leave full screen (F, double-click) from a window in
   the middle of the screen, from a maximized window and on a second monitor, with a film and with
   music. Expected: the screen goes black in one step and the picture comes back out of the pixels;
   no frame shows a black L over the desktop or the desktop through the new size.
103. **Music in a strip.** A song with a cover; drag the window into a wide, short strip (about
   4:1, and 5:1), then back to 16:9. Expected: the cover and its wings in the left half, the
   title, artist and "N tracks" from the middle, level with the cover; no black bars beside the
   picture (the window around it takes the room's far light, in the cover's colours, grey in
   black and white); the change glides.
104. **One list, everywhere.** The album's list beside the cover (a roomy window), in a strip and in
   a small window (the popover). Expected: the same rows, the same last line ("add tracks / or
   drag them here") and the same footer (+ files, + folder, clear) in all of them.
105. **Settings.** "..." > settings: point at each row. Expected: the picture at the top shows that
   setting at work (the light off and on, a pan with and without smoothing, the bar at the size
   pointed at, the bar with and without its sheet, the language in itself) with one line that says
   what it does; a short window shows the settings without the picture. The language list opened
   from Settings comes back to Settings (its arrow and Esc).
106. **Full screen, again.** As 102, with the pin on as well, on a 4K screen at 150%. Expected:
   not one frame of the desktop or a black L between the window and full screen.
107. **Music's Settings and list.** A song: "..." ends with settings; Settings starts with the
   looks (pictures for each), the right-click menu has no looks. Open the list in a roomy window,
   a strip, a small window and a tall one. Expected: never a popup; over the cover the cover dims
   and comes back when the list closes; in a tall window the title stays about the cover's width.
108. **The card's flip.** Next and previous in the card, several times, on 60 and 144 Hz. Expected:
   the cover turns out and in without a jump at the start of the turn-in.
109. **The card at 60.** A song in the card: hover the cover, open and close the list, skip songs
   several times. Expected: every movement is smooth; the turn starts as soon as the song does.
110. **The list in every window.** A song with a short title ("DNA") and a long one; open the list
   in a roomy window, a wide one, a strip, small, tall, square and narrow windows. Expected: never
   a popup; beside the words the picture slides left to make room; over the cover, no band where
   the picture ends.
111. **Tight subtitles.** Line spacing at -16 with two-line subtitles in each typeface, with and
   without background. Expected: lines close, accents and descenders never cut.
112. **The list's header.** A song with a cover and an album; open the list in a small, a tall, a
   square and a narrow window, and close it. Expected: the cover glides into the header's thumbnail
   (and back), its words beside it, the list under it and the bar under the list, the group centred
   when the list is short; no wings or lit box round the thumbnail.
113. **The turn, once more.** As 108 and 109, at 60 and 144 Hz. Expected: one continuous turn
   through the edge, no stop at edge-on, a small overshoot as it lands.
114. **Settings on the sheet.** Open Settings for a film and for a song, in a roomy and a short
   window. Expected: the same black sheet as the other menus, no grey blocks; a hairline between
   the groups; the segmented choices readable.
115. **The spectrum back from the card.** Cover and spectrum look: a song in the full player, go to
   the mini player, skip a few songs (and back), return to the full player; skip again. Expected:
   no spectrum or black flash in the card at any skip; back in the full player the spectrum is
   there at once, on the song that was playing and on the next ones; the sound never stops.
116. **The music player.** A song with a cover in a maximized window, a 16:9 window, a wide strip and
   a tall one, with and without the bar's background. Expected: maximized and 16:9, the cover in the
   middle, its words under it, the bar along the foot with the timeline over the buttons; the
   list's button shows the number of songs; opening the list glides the cover and its words to the
   left half and the list fills the right; closing it glides them back. Strip and tall as before.
117. **The card.** A song in the mini player at its smallest and largest sizes, at 100% and 150%.
   Expected: the words at the cover's top, the controls at its foot; the time ends where "..."
   ends; play/pause shows no sign over the cover; opening the list moves previous/play/next down
   beside the volume, smoothly.
118. **The card's turn, drawn by Cue.** Next and previous in the card, quickly and slowly, with a
   cover and with a song without one, pinned and not, at 60 and 144 Hz. Expected: one smooth turn,
   no step at any point, no flash of the old cover or a missing one at the start or the end, no
   change in the shadow's darkness; the window dragged during a turn: the turn ends at once,
   cleanly.
119. **The film's mini player.** Expected: no bar behind the controls; two rows (volume, back, play,
   forward, "..."; the timeline with both times); the full player's bar with its background again
   when it comes back.
120. **Settings, short windows.** Settings for a song and a film in a short, wide window. Expected:
   the picture beside the settings, large, the two states one over the other, its words under it.
121. **The room.** A song with a colourful cover in a maximized and a 16:9 window. Expected: the
   cover's colours over the whole window, the same left and right of the cover, no band where the
   picture ends; under the bar a little darker.
122. **The list, level.** Open the list in a maximized window with 3 and with 20 songs. Expected: a
   short list stands level with the cover and its words; a long one fills the height.
123. **Settings' examples.** Open Settings: no picture. Press the i: the picture appears and
   follows the pointer; close Cue and open it again: still on. Resize the window while Settings is
   open, in a short window: the rows never cover the picture.
124. **The turn's shadow.** Skip songs in the card on a light cover. Expected: the shadow turns with
   the cover, never doubled or missing at the start or the end; skipping right after a song
   starts and a second later both turn through the edge without stopping.
125. **Wide, centred.** Make the music window very wide and short (split). Expected: the cover with
   its wings and the words beside it centred together, the same margin left and right.
126. **The card's words.** A song in the card: the title close above the artist, a size smaller
   than before, nothing cut at the smallest card.
127. **Mini subtitles.** A film with three subtitle tracks in the mini player: the subtitles entry
   opens the short panel; the arrows go round the tracks; the three sizes grow the subtitles in
   the mini player only (back in the full player: the size as before); close and reopen Cue:
   the size is kept. The panel never covers the bar.
128. **Home.** Hover each recent: the highlight has the same air above and below its lines, and
   left and right. Open "···": its right edge under the "···", the icons on one column; open
   the languages in the smallest window: every language inside the window.
129. **Skipping songs.** In the full music player (with and without the bar's background), skip
   songs quickly, then resize the window while skipping. Expected: the bar never moves or
   changes between songs; after a resize it is centred again.
130. **Music bar with background.** The compact bar like a film's, centred under the cover's
   words, the list's count as a small tag beside its icon (also with 100 songs: "99+").
131. **Mini player, quick look.** Right click on a film's mini player: nothing opens. Its "...":
   no up next. Near a song's end in the card or an episode's end in the film's: no notice.
132. **Mini subtitles.** With three subtitle tracks, the arrows change the subtitle on screen
   each time, and the name follows.
133. **Home, centred.** Maximized and in a small window: the eyes block and the list have the
   same margin left and right, and both sit on the window's middle.
134. **List count.** Music with 4 songs: "≡ 4" on the list button, the number the icon's height;
   with 120 songs: "99+", nothing touching repeat.
135. **Clearing music's list.** Open the list beside the cover and clear it. Expected: the list
   closes; no popover over the cover.
136. **Mini player, cover.** Look "spectrum alone", go to the mini player, skip a song. Expected:
   the card shows the cover; back in the full player the next song has the spectrum alone.
137. **Music light.** Settings for a song: no ambient light switch; the room's light is on (off
   with the cover alone). For a film the switch and the strength are there.
138. **About.** Settings > about Cue: it opens where Settings was, as wide, above the bar; its
   back arrow returns to Settings.
139. **Empty music list.** Clear the list beside the cover, close it, open it again: the same
   place, one line "add tracks / or drop them here" (a click opens the files), "+ files", "+ folder".
140. **Card, spectrum alone.** Look "spectrum alone", a song playing, go to the mini player:
   the card shows the cover at once (no cut in the sound), no spectrum anywhere in the card, the
   room's light round it; skip: it turns, still no spectrum; back to the full player: the spectrum.
141. **Survey.** Start with `--survey` (or a week after installing): on the home, the card in the
   corner, writing twice then resting; "responder" opens tally.so/r/8NzQZA, "não, obrigado"
   closes it; restart: never again. A file opened straight from Explorer: no card.
142. **Picking a song.** Music's list open beside the cover (and in the card): click another song.
   Expected: it plays, the list stays open, its row lit; Esc closes it. A film's up next: picking
   an episode closes the list.
143. **Card at large sizes.** Settings > interface "maior" (and "grande"), a song in the mini
   player: the card keeps a mini player's size, the title and the artist whole above
   previous · play · next, nothing under the window's buttons; resize it to its smallest: the
   same. Ctrl + / Ctrl − in the mini player: the card's window does not change.
144. **Settings' examples.** A fresh state: Settings opens with the picture on top. At "maior"
   (2560x1440, 1920x1080): the picture on top or beside the rows, never missing while there is
   room; change the size with Settings open: the layout is decided again. The "i" hides and shows
   the pictures. The music looks' pictures stay inside their frame at 2.5x, 3.5x and 4.5x.
145. **Credits.** Settings > about Cue > credits and licenses: the first page says "Cue 1.3",
   the release v1.3 and Cue-1.3-Engine-Source.zip.
146. **Ambient light, paused.** A film playing with the light on: pause, turn the light off, then
   on again. Expected: it fades back in at once, still paused.
147. **Spectrum in the card.** Look "spectrum alone", mini player, skip ten songs quickly (both
   ways): never the spectrum behind the cover, never a black cover.
148. **Clearing music's list.** Beside the cover and in the card: the rows leave top to bottom,
   the list closes up round its middle, the line that adds songs fades in.
149. **Mini player button.** A song: the button on the bar (beside shuffle) opens the card; a film:
   no button on the bar, right-click > mini player.
150. **Shuffle.** A list of 10 songs, shuffle on: next ten times plays every other song once;
   previous goes back through them; repeat-all: a new order after the round; shuffle off: next
   is the list's order again.
151. **Shift.** Drag the bar to a side, press Shift (pointer still): it centres; move up and down
   with Shift held and let go: it lands centred. Without the background too.
152. **Song picked in the list.** Music in a roomy window, list open beside the cover: pick
   another song. Expected: the cover stays in the left half, the list on the right, no moment
   of the cover centred under the list. In the card: no films' popover while the cover turns.
153. **Scale while music plays.** A song playing in a big window at "maior": switch to "média"
   (and back) in Settings and with Ctrl + / Ctrl −. Expected: the title and the artist always
   under the cover, the composition right for the new size, at once.
154. **Interface page.** At "maior" in a window where Settings shows "interface ›": open it and
   pick "média". Expected: all of Settings comes back with the sizes in it (if it now fits).
155. **Smallest window.** At "maior", shrink a film's window: it gets as small as at "média".
156. **Shuffle never ends.** A list of 3 songs, shuffle on, repeat off: next ten times. Expected:
   it keeps playing, every song once a round.
157. **Style at large sizes.** A film with subtitles at "grande" and "maior" (100% and 150%), CC >
   style: the panel under the window's buttons and the back arrow, above the bar, whole; in a
   small window it is a size smaller. The subtitle list and up next: clear of the bar.
158. **Mini player button, film.** The film's bar has the mini player's button; narrow the window:
   it moves into "···" first. In the mini player, "···" > back to full size.

159. **Mini player at large sizes.** "maior" and "grande", a film and a song in the mini player
   at its first size: the same size as at "média" (controls, window buttons, menus); the menus
   (subtitles, "···", volume) at the bar's own size, whole, under the window's buttons. Larger mini
   window: a little larger.
160. **Opening a film.** From the home, open a 16:9 film, a 21:9 film and a phone video (rotated):
   the window takes the shape while it loads, the opening effect starts with the first picture,
   smooth, no second glide. cue.log: "picture opens (open N ms)"; a song: "song plays (open N ms)".
161. **Esc to the home.** A film playing (with subtitles on), a paused film, a song, a film in
   fullscreen (Esc twice: out of fullscreen, then home): Esc. Expected: on the next frame the bar
   and the subtitles are gone, the picture closes in the pixel ellipse (0.22 s) as the sound fades,
   then black for the engine's stop, then the home's eyes come in. No frozen frame, no click in the
   sound, no subtitle over the black. Esc twice quickly: home without the rest of the ellipse.
   Esc during the opening ellipse: it turns around from where it is. Reduce motion on: straight home.
162. **Close interrupted.** Esc, and drop a file on the window (or pick one on the phone) before the
   ellipse closes: the new file plays with its sound at full level and its subtitles as saved.
163. **Resume is quick.** A 4K film with long keyframe gaps (HEVC from a camera or a remux), left in
   the middle: open it from Recentes. Expected: it starts a few seconds before the place (the last
   keyframe), noticeably sooner than before; clicking the timeline afterwards still lands exactly.
   cue.log "first video frame (open N ms)" before and after.
164. **4K at the screen's rate.** A 4K 24 fps film (the 12.6 GB test file), plugged in: hover the
   timeline back and forth, open it from the home. Expected: the hover bubble and the opening iris
   glide at the screen's rate like a 1080p film; the film plays without new drops (cue.log "video
   ... sync display-resample"; no "too slow" engine warnings). GPU use in Task Manager stays
   reasonable at 60 and at 144 Hz. On battery: "sync audio", as before. Unplug while it plays:
   no stall in the sound.
165. **Previews of a 4K film.** The same film, after its first seconds: hover the timeline across
   its length. Expected: the pictures follow the pointer sooner than before; playback unaffected.
166. **Mini player from the right-click, music.** A song in the full player: right-click the
   picture > mini player: the song's card. Not in full screen; the card itself has no menu.
167. **Weaker PCs.** cue.log's "graphics ..." line on this PC (dedicated), on a laptop with Intel
   graphics only (integrated) and in a VM or with the display driver disabled (software). On the
   integrated laptop: a 1080p film and a song play smoothly, the picture is sharp; a 4K film plays
   (its line "sync audio"). Software: films and music play, the interface stays responsive; smooth
   motion stays off (cue.log). A 4-thread processor: music's spectrum moves as on a strong PC.
168. **Volume without a pop.** A film and a song with a steady low tone (a bass note, a hum): ↑ and
   ↓ several times, the wheel, a fast drag of the volume bar, the phone's volume. Expected: no click
   or crackle, only the level changing; the bar follows the pointer with no jump back. Paused:
   the change is immediate. Above 100% loud music may still distort (that is the boost itself).
169. **Music into the mini player.** A song in a big window (cover in the middle), each look
   (cover and spectrum, spectrum alone, cover alone), with and without the list open: Ctrl+M and
   back. Expected: the cover never jumps; it flies from where it is into the card's cover (and
   back out to the middle), the card's words and controls come in around it; no frame of the old
   picture squeezed. Maximized and on a 4K screen too. A film's flight unchanged.
170. **Vertical video bar.** Vertical videos at 9:16, 3:4 and 4:5 (and a window a little taller
   than wide), at the four interface sizes and maximized: the bar and its menus are about 15%
   smaller than in the first 1.3 builds, still easy to hit; a square or landscape window unchanged.
171. **A click on the timeline (1.3.1).** A long film (H.264/HEVC with keyframes a few seconds
   apart), paused and playing: click the same spot of the timeline several times, then other
   spots. Expected: the picture goes straight to the clicked frame, never a different frame for a
   moment first (it used to show the keyframe's picture for about 0.3 s and come back). Dragging
   still previews while the hand moves and lands on the exact frame on release. The mini player too.
172. **A film of another shape (1.3.1).** From the home, open a 2.39:1 film (and a 4:3 and a vertical
   one), with a resume point and subtitles on; Esc back home; open it again. Expected: the window
   glides to the film's shape in one even movement (no big first step), no subtitle over the black
   window before the picture: the line comes in with the iris. Going home glides back evenly.
   Subtitles off for that file stay off; C during the black wait is respected.
173. **The welcome maximized (1.3.1).** `"C:\Cue\Cue.exe" --welcome`, maximize (button, double click on
   the top, Win+Up) and restore, on each page: a single "skip"/"back" and nothing left from the old
   size. The home maximized and restored: nothing left behind either.

## Subtitle selection follow-up — 2026-11-06

- Portable regression: `subtitle_selection_test` covers full/mini A-B-A clicks before replies,
  delayed observer updates, obsolete replies, coalesced changes, rejected commands, panel
  close/reopen and file replacement with reused track IDs. The existing mini subtitle and
  subtitle-list tests remain part of the full portable suite.
- Native checks still required: embedded SRT/ASS/bitmap and external subtitles while playing
  and paused; rapid A-B-A and next/previous cycles in both players; changing audio and
  entering/leaving mini/fullscreen with subtitles on/off. Confirm the displayed line matches
  the menu, playback does not jump, and toggles during transitions stay as chosen.

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

## Restored video audio mix transition

Reopen a video with two tracks mixed: expect the fullscreen pixel fade (0.11 s out, black hold during graph restoration, 0.20 s in), without changing window mode. Check paused/resumed positions, mini/fullscreen, reduced motion and failures. Manual audio changes retain their own effect. Native visual verification remains pending.

## Opening follow-up

Video mixes now initialize in on_preloaded; successful preload restoration skips the runtime graph rebuild. Fallback opening waits for the audio switch to complete before starting the iris. Shader-option commands allow one in flight and recompute the latest state on completion. The iris fast path uses squared distance and avoids sampling black pixels. Mask arithmetic checked across HD, 4K, 8K and portrait; existing audio mix, startup, fullscreen and subtitle regressions passed. Native shader compilation, actual 4K GPU performance and playback with restored mixes still require Windows validation; no measured FPS improvement is claimed.
