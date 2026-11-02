# Changelog

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
- Native installer, shortcuts, Windows uninstall integration, and GitHub update checks every 15 days with SHA-256 download verification.
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
