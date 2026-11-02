# Architecture

Cue uses **C++20, Win32, and dynamically loaded libmpv**. Portable UI/core code supports Linux previews and tests; Windows integration stays in `src/win/`.

## Source map

| Path | Responsibility |
|---|---|
| `src/core/` | State, persistence, subtitle settings, language catalogs, release parsing |
| `src/ui/` | Pixel renderer, Omni assets, animation, home, onboarding, playback panels |
| `src/win/` | Windowing, DPI, input, dialogs, clipboard, libmpv, updater, preview host |
| `installer/` | Native installer, uninstaller, LZMA unpacking |
| `i18n/` | Eight translation catalogs |
| `assets/`, `res/` | Fonts, icons, the phone remote's page, executable resources |
| `tools/`, `tests/` | Generators, previews, packaging, portable and engine tests |

## Playback and input

The UI thread owns application state and rendering. An engine thread loads libmpv after the first window paint, allowing initialization to overlap the home exit animation. libmpv handles demuxing, decoding, D3D11 output, audio, and subtitles through libass/FFmpeg. Hardware decoding uses `hwdec=auto-safe`, with software fallback.

The video occupies a child window. Mouse events are forwarded without blocking the engine; keyboard bindings reach the UI through mpv. Engine events wake the Win32 loop through `PostMessage`, with bounded batches to keep it responsive. Commands are asynchronous and seeking allows only one request in flight.

`SingleInstance` holds a per-session mutex. Later launches send UTF-8 paths through `WM_COPYDATA` and exit before loading the engine. Requests received during startup wait in `pending_open_`. Preview-host and registration helper modes bypass this path.

A single click sends pause/resume immediately. A recognized double-click restores the starting playback state before toggling fullscreen; a brief state change between clicks remains possible.

## Rendering and motion

The home screen uses a software BGRA canvas; playback UI uses premultiplied BGRA `overlay-add` buffers. Buffers are reused or freed only after engine acknowledgement. Idle/hidden UI avoids redraws; minimized windows suspend UI animations. Home presentation invalidates only the changed rectangle.

Omni supplies palette tokens, bitmap typography, icons, and motion conventions. Geometry resolves to physical pixels. UI scale depends on width, DPI, and the selected size; subtitle/video scale is independent. Layout waits until a manual resize ends to avoid scale jumps.

`Spring` uses closed-form damped motion. `VsyncPacer` waits on `DwmFlush` only while needed; playback overlays target approximately 60 Hz, independently of video frame rate. Timer fallback and Windows reduced-motion preferences are supported.

The frameless control style uses a masked, separable Gaussian shader before subtitle/OSD composition. Later beta tuning reduced the cloud to a 6 px radius, 35% maximum strength, and 95% brightness. Passes become inactive when hidden. Control-style changes use reversible interpolation. The dock uses a critically damped trajectory. Subtitles use a finite 300 ms lift / 360 ms return, preserving position and velocity on reversal. Cached target geometry prevents repeated layout retargeting.

Fullscreen uses shader parameters `cue_on`, `cue_dark`, `cue_px`, and `cue_gray`: exit, wait for the dark frame and overlay removal, resize, wait for the new renderer geometry, then enter. Native rounded corners are requested through DWM in windowed mode and disabled when fullscreen/maximized.

## Previews and ambient lighting

`Cue.exe --thumb-host` runs a separate low-priority, CPU-only engine for timeline previews and scene-color samples. Two pipes carry one request at a time; generation IDs reject stale responses. A job object ties its lifetime to Cue. Failure removes previews without stopping playback.

Color sampling starts 1.5 seconds after the first frame; thumbnails wait 3 seconds. Both restart on file changes. Hover output is reduced before pipe transfer/cache storage, while color-sampling resolution stays unchanged. As of 1.1.1, timeline requests are on demand with a 12 MiB / 256-entry LRU cache; colour-only probes are transient, and there is no speculative full-timeline scan.

`atmosphere.cpp` chooses representative scene colors and smooths changes. Ambient lighting uses click-through layered windows below the player, cached geometry and color ramps, and approximately 30 Hz updates. Dragging reuses existing images and defers sampling/rebuilds until the gesture ends.

## Audio-only files

`on_preloaded` inspects actual tracks, ignoring album artwork as video. `core/audio_mode.h` builds a `lavfi-complex` graph: `asplit` preserves sound and feeds a 24 kHz copy to `showfreqs`, which draws a spectrum visualization and applies no equalization (24 bands × 20 rows, 30 Hz, logarithmic frequency, square-root amplitude). The bands are mirrored around the centre (`hflip` + `hstack`, bass in the middle), `lagfun` lets bars fall slowly, and a dim bottom row marks silence. The 48×20 picture is scaled 8× with nearest-neighbour pixels, cut into dots by `drawgrid`, given an 18% reflection, and centred in 1024×576. All filters work on the tiny picture before scaling. No second audio decoder or whole-file analysis is needed.

Controls remain visible; video previews and ambient lighting stay off. Timeline dragging updates the displayed time and sends one exact seek on release. The graph drops two preparation frames to avoid flashing. Failed visualization initialization triggers one retry without it. File-local settings reset when media unloads.

## Phone remote

`core/remote_proto` holds everything portable: HTTP parsing, the WebSocket handshake (SHA-1, Base64) and frames, the JSON state and commands, folder filtering, natural order and next-episode detection, pairing-code throttling, strict command validation, resource limits and lexical path containment. `core/qr` encodes the pairing QR code (byte mode, versions 1-10, ECC M; verified against a reference encoder). `core/remote_sound` synthesizes the pairing sound as WAV bytes.

`win/remote_server` runs one thread with a `select` loop for HTTP and WebSocket clients, accepting only private IPv4 peers, exact local-endpoint `Host` headers and same-origin browser connections. Folder listings and thumbnails (Explorer's `IShellItemImageFactory`, reduced to 84×48 BMP) run on at most six short-lived workers that own their socket and copies of what they read. A same-origin WebSocket upgrades without exposing player state, then requires a credential in its first masked text frame within five seconds. HTTP data endpoints require a Bearer header. PINs have six digits, two-minute validity and one use; fresh tokens have 256 bits from Windows CSPRNG, with no predictable fallback. Turning the remote off revokes remembered tokens. WebSocket connections recheck token validity; frames, outgoing buffers, message rates and pending UI commands are bounded. HTML uses a per-response script nonce and CSP. Filesystem access rejects special Windows path syntax, checks resolved handles and refuses descendant reparse points. HTTP remains unencrypted.

Commands reach the UI thread through `WM_APP + 12`; App applies them as the same `PlayAction`s the dock produces, then publishes a state snapshot (~2 Hz while phones are connected, and after every command).

The page preserves its pixel grid while fitting width, laying out height separately and maintaining at least 44 CSS-pixel touch targets for primary controls. Short screens scroll. Thumbnails load lazily with at most three browser requests, and credentials stay out of URLs.

The page (`assets/remote/index.html`) is embedded by `tools/gen_remote_page.py` into `win/remote_page.inc`; the server injects the language's `remote.*` and `queue.*` strings. The pairing scene is `ui/remote_view` (portable, previewable with `tools/preview_remote.cpp`) inside `win/remote_scene`, which hosts it in `win/overlay`: a layered popup laid over the player's client area that runs any `ui/scene_view` (pointer, keys, its own frame timer, background drag moving the window). The once-per-version "what's new" splash (`ui/news_view`, previewable with `tools/preview_news.cpp`) uses the same host; `Store::news_seen` remembers the last version shown. `tools/remote_harness.cpp` runs the real server without the player for browser testing.

## Persistence and lifecycle

`SettingsStore` uses schema 2 (`prefs`, `session`, `media`), migration, defaults, atomic writes, a backup, and a 400 ms debounce. Immutable snapshots are saved by one worker at a time. Per-media state includes position, duration, selected tracks/languages, subtitle delay, visibility, and style.

State remains in `%APPDATA%\tinyoffice\Cue` for compatibility. Font and “Open with” registration run after startup. Compatible compiled shaders are cached. `cue.log` records startup milestones; verbose mpv logging requires `--log`.

On close, visible windows and lighting disappear first; playback pauses and state is saved before engine teardown. Preview work is terminated separately.

## Development

```bash
bash tools/test.sh
g++ -std=c++20 -O1 -DWITH_PLAY -o preview tools/preview.cpp tools/preview_play.cpp src/ui/*.cpp src/core/*.cpp
./preview out 2
```

Asset generators: `gen_assets.py`, `gen_action_icons.py`, `build_fonts.py`, and `gen_fallback_glyphs.py`. `brand_images.cpp` renders the cover, banner, icon, and layers; `brand_psd.py` assembles PSDs. Regenerate branding from the repository root and place outputs in `docs/`.

[Languages](LANGUAGES.md) · [Performance and evidence](PERFORMANCE.md) · [Release process](UPDATES.md)

## Session queue

`core/queue.h` owns up to 512 ordered audio/video entries with stable IDs. The
application owns the queue, not mpv's playlist: one decoder and the existing
load/EOF path remain in use. Explicit queue order takes priority over episode
discovery and stops at its end. Clearing the queue restores automatic discovery.
Removing the playing entry does not stop playback. Closing the video clears the
session queue; it is not written to preferences.

`win/app_queue.cpp` scans one folder on a low-priority worker, without recursion,
up to 4,000 directory entries / 512 playable files, in natural order. Results are
published through an atomic completion flag and consumed by a UI timer. Only one
scan exists at a time; cancellation discards results without dereferencing App.
OS filesystem calls can remain blocked; cancelled work retains its slot until it
finishes. File changes and shutdown discard results.

`ui/queue_view` renders only visible rows of a complete scrollable list. Stable-ID
row springs preserve positions during a reorder; one absolute move commits on
release. Resize, dismissal and external queue revisions cancel pending gestures.
Scroll and reorder motion respect the Windows reduced-motion preference.
Visible video rows share the existing single low-priority Windows thumbnail
worker with the next-episode card. A 32-image LRU holds 84×48 BGRA previews
(about 504 KiB of pixel data), with no disk cache or second decoder. Missing
previews are remembered until the entry is removed. Results use stable IDs,
so a late completion cannot update a replacement entry. The timer stops when
the panel closes. Only the visible viewport is composed; the queue stays bounded
to 512 entries. Each visible preview keeps a copy scaled to its row (released when
the row leaves the viewport), and the panel background is cached per size, so a
scroll frame copies rows instead of resampling previews pixel by pixel. The panel is a popover
anchored to the up next button: its list icon is placed over the button's icon and
only the window margin moves it. Windows at least 760 art px wide keep the interface
scale and allow about ten rows (320 px wide); smaller windows compact its scale.
Other menus keep their placement above (or below) the bar, but when that space is
too short for them (short, wide windows) they open beside the bar, level with its
outer edge; About rises above the bar instead of covering it.
The dock gives up next a permanent slot beside repeat (left of the
subtitle group in narrow docks); it takes the film's accent while the queue is not empty.
The wide dock has three zones on one 22 art px beat (centre to centre): volume and
speed, playback centred on the dock (previous · −15 · play · +15 · next with a queue),
and one even row of secondary buttons from up next to More. Play leaves the centre only
when the zones would otherwise collide.

Music uses `audio_cover_tail` or `audio_spectrum_tail` (`core/audio_mode.h`), chosen by the
look. The cover look takes the engine's album-art track (`audio_cover_track`) as `[vidN]`, or,
for a song without one, Cue's tile drawn in the graph (`audio_placeholder_cover`: a 26x26
`color` frame with the note in `drawbox` runs, scaled by 20 with nearest-neighbour). The
spectrum look places the 384x200 equalizer, doubled, where the cover stands in the same
1920x1080 scene. `cover_scene_` means the scene is on (either look); `cover_art_` means a
cover is drawn (the shader's pass and the tilt). The cover is scaled once with
Lanczos to 520 px and placed on one 1920x1080 scene (`MusicScene`), the cover on the left
and the words' column on the right. The window frames it (`MusicFrame`, `music_view`):
`Scene` shows all of it, `Tall` (portrait windows) the cover with the words under it,
`Cover` (small windows) the cover and its wings alone above the bar. The framing is mpv's
`video-zoom` / `video-pan-x/y` as file-local options (`mpv_framing`), updated on every
`WM_SIZE` including mid-drag and glided for 0.34 s across frames (`App::music_fit_window`,
`step_music_view`); nothing is rebuilt. The shader's `cov_*` coordinates are taken in the
shown part of the picture (`music_visible`). A reopened song, or the next song of an album,
keeps the window's size (no `request_video_geometry`). The 24x20 spectrum (`showfreqs` at 60 fps:
4096-sample window, 0.92 overlap) is turned sideways, mirrored around the bass and drawn
as two dot wings, each row tinted by the cover row beside it; the dots' transparency comes
from `colorkey` on the tinted cells and the gaps from `drawgrid` with `replace=1`, so when
the sound ends the last frame keeps its holes (an `alphamerge` would lose its alpha input
at end of stream and paint solid wings). Every two-input blend is led by the music stream,
so the scene runs at the spectrum's rate. The background is the cover
reduced to 32x18, blurred and dimmed, plus the blurred spectrum multiplied by those colours,
so the light swells with the music, then vignetted and scaled up. In this look the engine
scales smoothly (`spline36`/`mitchell`); the spectrum alone keeps nearest-neighbour. The
cover's shadow, tilt (up to 6°, a camera at four half-sides), light band and rim are a pass
of the output shader (`cov_*` uniforms, updated only while something moves); where the
turned cover uncovers the flat one, the light is borrowed from just outside it. The
captions are a UI layer (`L_MUSIC`), placed from the same `MusicScene`; *up next* in a
song's last 12 seconds is another (`L_NEXT`), dropped in at the top.

Look changes reopen the song rather than swapping `lavfi-complex` in place
(`App::reopen_music`): with the cover's attached-picture track in the graph, a runtime swap
could leave the graph waiting for a cover frame the demuxer does not send again (no sound)
or fail and end the file (the queue skipped). The reopen runs inside the audio-switch stage
machine: pixels out, `loadfile` of the same path with `start` at the saved position and pause
held (`music_reopening_` in the `on_load` hook), an exact seek after `PLAYBACK_RESTART`,
pixels in, pause restored. `on_file_loaded` keeps the interface (no reset) for a reopen. A
song that follows another in play mode starts at stage 5 of the same machine (`music_intro_`).
The wings' dot grid is one `color`+`drawgrid` frame multiplied into their alpha by `blend`
(repeated by framesync), not `drawgrid` per picture.

A song opened without a queue builds one from its folder (`App::music_album_queue`:
playable audio, natural order, at most 512 around the song). Music skips resume points,
timeline previews and the next file's thumbnail. Queue rows for music ask the shell for
a square cover (`make_thumb(path, square)`), and a list of only music uses the compact
album row; beside the cover, `QueueView` runs bare (rows only, no sheet, header or
footer) in the words' column (`PlayView::scene_list_rect`). Repeat is `loop` (this one,
`loop-file=inf`) or `loop_all` (`App::queue_following` wraps to the first item). While another
app's full-screen window covers Cue on its monitor, or Cue is cloaked on another desktop
(`App::covered`, polled twice a second), `hidden_` stops UI frames like a minimised window,
sends `cov_on=0` and sets a film's file-local `video-sync=audio`. The phone's face for a
song is `/cover` (the playing song's shell thumbnail at 72 px, cached), announced by the
`cover` field of the state. The taskbar's thumbnail toolbar (`win/taskbar_buttons.*`, `ITaskbarList3`)
is attached on `TaskbarButtonCreated` and synced from the poll timer; its icons are the
dock's pixel glyphs. File types register `Capabilities\FileAssociations` under
`Software\tinyoffice\Cue` and `RegisteredApplications`, so Default apps maps each
extension to its ProgID. If the cover graph fails, the file is
reopened once with the spectrum alone, and then without a picture.

`ui_scale` measures a squarer window by its area (as a 16:9 window of the same area),
bounded by its width at 1000 DIPs per reference step: 4:3 and square video keep the
controls of a comparable 16:9 window instead of dropping a step.

The home's Recent list is built by `core/recents_view.h`: recents (newest first) are
grouped by `remote::series_id`, so a show is one entry that opens its latest episode and
counts the others; `series_title` and `episode_label` give the readable name and
"S01E03". Rows are two lines (30 art px pitch); the window shows as many as fit.
When a queue starts from the playing file, `MediaQueue::without_first` drops that
file once from the added batch (ASCII case-insensitive, either slash), so a folder
containing it does not list it twice. The phone fetches an authenticated `/queue` snapshot only
when opened or its revision changes, not on every playback tick. The snapshot
contains bounded names, IDs and audio/preview flags, never filesystem paths. Remote playback/additions
are revalidated against currently shared roots, including after folder scans.

The remote server keeps a private ID-to-path map for `/queue-thumb?id=…`.
Authentication, current shared-root/reparse checks and existing bounded thumbnail
workers apply to every request. Queue/root changes invalidate the published
snapshot epoch. No preview route accepts a queue filesystem path from the phone.
The phone loads visible previews with at most two requests and 32 Blob URLs;
closing cancels work and revokes URLs after the short exit. No disk cache is added.

The mobile sheet retains keyed rows across clock ticks, batches mutations before
FLIP measurements, and animates only visible changes. Adding from the phone uses
the folder sheet in an adding mode: a per-visit set of sent paths marks tiles and
prevents duplicate sends from repeated taps; the set is discarded on exit. The
return to the queue scrolls to the end and highlights the entries added. Drag release commits one
absolute stable-ID move with optimistic placement until the authoritative update.
At most 12 outgoing row nodes remain briefly for exits. Dismissal, reduced motion
and page hiding cancel pending gestures/animations. Folder requests publish busy,
actual-added, empty and error results; clear cancels outstanding results. The
folder worker remains nonrecursive and retains its slot until cancellation finishes.

## Subtitle reading scale

Styled text uses the existing 16:9 height as its reading-size reference when a
landscape window becomes wider: `clamp(width * 9 / (16 * height), 1, 2)` compensates
mpv's height-based font scaling. 16:9 and taller windows retain their established
size. The continuous cap protects extremely short windows. Saved style values,
UI size settings, authored ASS and bitmap subtitles are unchanged. Font size,
tracking, outline and background padding share the compensation; libass line
spacing remains in physical frame pixels. Drag targets and control clearance
use the same geometry. Resize updates only changed properties asynchronously.

## Bounded subtitle menu and queue motion

`ui/play_tracks.cpp` lays out a clipped subtitle viewport between fixed controls.
The panel width and height are capped to client geometry; unusually small windows
compact the panel scale. Track IDs and full labels remain in rows, while fitted
labels are measured once per layout. Scroll renders only intersecting rows and
updates the custom scrollbar; it never rebuilds the track list. `tracks_revision`
invalidates metadata only when the engine publishes a new list, avoiding string
signatures over every title on playback ticks. Subtitle labels are built in one
batch with duplicate counts rather than rescanning the list for every row.
The native video-window subclass posts fractional wheel deltas to the UI thread;
the engine does not also process the same Win32 event.

Queue removal commits in the session model immediately. At most 12 visible row
rasters survive as noninteractive exit snapshots, including a clear of 512 items.
The stagger is capped at 80 ms, with 150 ms individual exits / 180 ms clear exits;
exits shift 6 art px toward the remove button. Additions reveal over 160 ms with a
700 ms highlight; additions outside the viewport scroll it to them. A drag lifts
the row with a hover-style step pose and a pixel shadow, and a spring-driven dotted
slot marks the destination. Panel height settles over 200 ms after exits finish.
Only container bounds change during this contraction; rows are not rebuilt.
New mutations supersede the displayed state without delayed model callbacks.
Closing, replacing the panel or reducing motion releases exit snapshots and
settles pending motion. No additional handles, threads or disk files are used.

## Motion language

Desktop motion uses the primitives in `ui/anim.h`: poses answer on the first frame
(`hover_step`), positions move on closed-form springs, and exits ease out from the
value on screen, so any motion can reverse mid-way. The phone page follows the same
rules in CSS: presses scale on the first frame (50 ms), then release on one shared
spring (`--spring`, about 3% overshoot) in 240–300 ms. Entrances travel 8–14 CSS px;
staggers stay under about 110 ms in total. Image fades and the click-wheel position
dot change in two or three pixel steps. `prefers-reduced-motion` removes transitions
and animations on both surfaces.

## Ambient light and rounded corners

The halo is four click-through layered strips around the player's visible frame.
Windows 11 rounds that frame by 8 px at 96 dpi, which used to leave a dark wedge at
each corner between the square halo and the arc. The top and bottom strips now
reach `C = 8 px × dpi / 96 + 1` under the window and light only the wedges outside
the arc, at full edge strength and with the corner's colour; the left and right
strips start and end `C` px further in, so no pixel is lit twice. With square
corners (Windows 10, maximized) those pixels stay hidden under the player.
