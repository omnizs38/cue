# User guide

## Playback

Open a file with `Ctrl+O`, drag it onto Cue, or use File Explorer's **Open with** menu. Cue uses one window: opening another file replaces the current media and saves its position. Launching Cue again brings that window forward. Each file gives the window a comfortable shape for its picture (a square window gets a bigger control bar, so it is not lost under a tall picture); a maximized window stays maximized (the new shape is where it goes when restored), and so does full screen.

Cue plays common video formats through libmpv, including MKV, MP4, MOV, WebM, and AVI. Audio-only MP4, OGG, Opus, MP3, M4A, AAC, WAV, and FLAC show a pixel spectrum visualization: the bass rises in the middle, higher frequencies spread to both sides, and bars fall back slowly. Album artwork does not prevent audio mode. If visualization initialization fails, Cue retries without it.

Music plays as a scene. A song without a cover (embedded, or a `cover.jpg` beside it) wears Cue's music tile, a black square with its note, so both looks work for every song. With a cover: the cover, sharp, in the middle of the window; the spectrum as two wings beside it, the bass next to the cover; the title and artist under it; and behind them a soft light in the cover's colours that swells with the music. In a very wide, short window the title and artist stand to the right of the cover instead. The window frames the scene as it is resized, live and without touching the sound: a wide window shows all of it, filling the window (no black bars above and below, or beside it) as far as the cover, the wings and the start of the title still fit, a portrait window shows the cover with the title and artist under it, and a small window shows only the cover and its wings. Where the picture cannot reach the window's edges (a very wide window), it fades softly into the black. The next song of an album keeps the window's size. A song without a title tag shows its file name; without an artist, only the title. In a song's last 12 seconds, *up next* drops in at the top of the window. Point at the cover and it leans away from the pointer; it settles flat when the pointer leaves. Right-click the picture to choose the look with the two icons at the end of the row (the row names the one that is on, or the one under the pointer): **cover + spectrum**, **spectrum only** or **cover only** (the lightest: the cover over a still light, no spectrum and no light show); the choice is kept for all music. **Tilt the cover**, in the same menu, turns off the cover leaning under the pointer. The row under it chooses **color** (the default) or **black & white**: the spectrum and the light turn to their own greys while the cover keeps its colours, the colour draining away (and coming back) in a moment without reopening the song; also kept for all music, and films are never touched. A cover that is a video's frame boxed in black bars (common in songs taken from video sites) is shown without the bars. Like a film's, the control bar rests while the pointer is still and comes back with any movement. A click on the music scene never pauses: it brings the controls back (pause with the bar's button, the space bar, a media key, the taskbar buttons or the phone); a double click is still full screen. **Spectrum only** stands the equalizer where the cover stands, with the same title, artist and album beside it and the same list.

For music the control bar runs along the window's foot: the timeline across it with the time at each end, and under it the volume and speed, previous track · play · next track (instead of the 15-second steps), and the list, repeat, full screen and **…**, and the subtitles button only when the song has lyrics. In a small or portrait window the song becomes one composition: the cover, its title and artist under it (when there is room), and right under them the bar, its timeline over previous, play and next, with volume and **…** (which holds the rest) at its ends; the bar stays up, centred on the cover. A song opened on its own brings its folder: the folder's music becomes the queue, in name order, starting from that song, so previous and next walk the album and it plays on. A queue you built yourself is never replaced. Songs always start from the beginning; Cue does not remember where a song stopped.

The list's button on the bar (it shows how many songs there are) opens the album's list beside the cover, which moves into the left half with its words (in a small or tall window, **N tracks** under the artist opens it): each song with its cover (a black Cue tile with a note when it has none), the one playing marked; click a song to play it, drag to reorder. The list's last line adds songs: click it to pick files, or drag songs from Explorer onto the window. While songs are dragged over a playing song, the list opens by itself and its last line says *drop to add to the list*; dropped songs join the end of the list (and the list stays open to show them), and if they are dragged away again the list closes. The same line under the artist (or the Up next button, or Esc) closes it. In a tall window, or with the spectrum alone, the list opens as the usual panel above the control bar.

**Shuffle** (its button beside repeat, or in **…** when the bar is short) plays every song of the list once a round in a random order and never runs out: when every song has played, a new round starts. **Previous** goes back through the songs heard, newest first; songs added join the round. Music's bar also has the mini player's button (a film's is in the menu on the picture).

Repeat cycles through **off → the list → this one** when there is a list (the icon shows a small 1 for "this one"), and simply on and off for a single file. Switching between the two looks briefly reopens the song at the same moment behind a pixel transition. A new song comes in the same way: big pixels resolving into its cover while its title slides in.

While something plays, the taskbar preview of Cue (point at Cue's taskbar button) has previous, play/pause and next buttons. Previous and next follow the queue, and dim when there is nothing before or after.

At the end of a video, Cue pauses on the last frame. Play restarts it. Enable repeat to loop continuously.

The home screen lists recent files in two lines each: the title and what is left (or the length, when not started), then a quieter line with the episode or the kind of file, over a thin progress rule. Episodes of one show become a single entry, named after the show and opening its latest episode, with the episode count when there are several (*RuPaul's Drag Race · faltam 25 min*, *S18E15 · 3 episódios*). Names are shown without extensions. Small windows show fewer entries rather than shrinking them. The **···** beside Recent opens a small menu (open folder, language, remote control and, set apart, clear recent) that unfolds from the button; a click anywhere outside closes it.

## Controls

| Key / action | Behavior |
|---|---|
| Space / click video | Play / pause (a click that brings Cue forward only wakes it) |
| F / Alt+Enter / double-click | Toggle fullscreen |
| ← / → | Seek 5 seconds |
| Shift+← / Shift+→ | Seek 30 seconds |
| « / » buttons | Seek 15 seconds |
| L | Toggle repeat; preference is saved |
| P | Previous file in up next |
| ↑ / ↓ / mouse wheel | Volume, up to 125%; orange above 100% |
| M | Mute |
| N / media next key | Open the next file when Cue found one |
| [ / ] | Adjust playback speed |
| 1× button | Speed slider (0.25×–3×), 0.05 steps, and presets |
| \ | Reset speed to 1× |
| C | Subtitle panel |
| S | Toggle subtitles |
| Z / X | Subtitles 0.1 seconds earlier / later |
| Ctrl+O | Open a file |
| Ctrl+Shift+O | Open a folder |
| Ctrl+V | Open video/subtitle files copied in File Explorer |
| Ctrl+T | Always on top (the pin beside the window buttons) |
| Ctrl+M | Mini player, and back |
| Ctrl+plus / Ctrl+minus / Ctrl+0 | Next UI size / previous / default |
| Tab / Shift+Tab | Navigate controls and panels |
| Esc | Close panel, exit fullscreen, then return home |
| ? | All the shortcuts on one sheet; ? again or Esc closes it |
| Click either time display | Toggle elapsed / remaining time on the left |

Press **?** during playback for a sheet of every shortcut, in four groups (playback, subtitles, files and up next, window). It opens in two columns where the window is wide enough; in a small window its rows close up and it gets a little smaller, so it is always whole. **About Cue** links to it too. Pointing at a button shows its name and, a shade dimmer, its key (*full screen  f*). The keys work after a click on the picture as well.

The music icon beside **CC** appears only for files with multiple audio tracks. Its menu also offers **multiple tracks**, where any of the tracks can be ticked to play at once (or **all** of them): made for recordings in parts, like an OBS capture with the game, the microphone and voice chat on separate tracks. Cue mixes such files on its own when none of their tracks carries a language or is named like a dub or commentary.
Right-click to view file information, save a frame without subtitles, or close the video. The saved-frame confirmation can open its folder with the image selected.

## Subtitles and appearance

Open **CC → style** to change the font, size, opacity, background, and position. Subtitle formats may limit available styling. Drag subtitles to reposition them; playback pauses during the gesture and resumes if it was playing. If they end up hard to read, **CC → restore default** (it appears, with a round arrow, whenever the look differs from the default) brings back your default look and position in one click; click it again right away to undo.

The interface size follows the window's size, not only its width: a 4:3 or square video window gets the same controls as a 16:9 window of the same area, as long as its width holds the bar.

The control bar has three zones: volume and speed on the left, playback centred on the bar, and one evenly spaced row on the right (Up next, repeat, subtitles, audio when there are several tracks, full screen, and **…**). In a smaller window the bar keeps what matters most: the volume line shortens and then folds into its icon (a click opens it as a column above), and speed, repeat, up next, audio and subtitles step into **…**, whose first row holds them as the bar's own icons. Play stays in the middle, and every key keeps working.

**Mini player.** The mini player button on the control bar (in **…** when the window is narrow), right-click the picture and choose **mini player**, or press **Ctrl+M**: the video moves to a small window in a corner of the screen. Its controls float over the picture in two rows: volume (a click opens the column upward), back, play, forward and **…**, then the timeline with the time at each end (the arrow in its corner takes you back), and the subtitles grow so they stay readable. Its subtitles panel is a short one: on or off, the track by its arrows, and three sizes for the mini player alone (as in the full player, larger, largest); the style and the sync stay in the full player. The mini player is a quick look: no menu on the picture, no up next list for films, no notices about what comes next. Drag the picture (or the bar's empty spots) anywhere: it stays where you let it go; let go right beside a corner and it glides into it. The pin keeps it above other windows and you can still move it (Cue remembers it for the next mini player). A song's mini player is a small card: the cover on the left and, beside it, the title, the artist and **N tracks** at the top, then previous, play and next, the timeline with the time, and the volume and **…** at the foot, with the song's light behind. Skipping a song turns the cover over. Drag an edge or a corner to resize it: it keeps its shape, within a mini player's sizes. At the large interface sizes the mini player (a film's or a song's card) keeps its size and its controls and menus step down together to fit, so nothing crowds the little window. Full screen and the shortcuts sheet are the full player's. **Esc**, Ctrl+M, the arrow in its corner or **…** > **back to full size** brings the window back as it was; a second Esc goes home. The ambient light rests while the window is small and comes back with it.

**Picture.** With an NVIDIA RTX card and RTX Video Super Resolution turned on in NVIDIA's app, films smaller than the screen, up to 1080p, are upscaled by the driver (not on battery; 1440p and 4K films are left as they are). Films whose frame rate does not match the screen's (24 fps on 60 Hz) can play with **smooth motion**, off by default: turn it on in **…**. It evens out the judder of slow pans (a 24 fps film on a 60 Hz screen moves in uneven steps); it does not invent frames. cue.log says when it is on.

Explorer shows video files with Cue's icon and music files with the same tile and a note, once Cue is their default app. Windows keeps the choice made for each type: types that were set to Cue before 1.2 keep the old icon until Cue is chosen for them again. The quickest way is **Settings → Apps → Default apps → Cue**, which lists every type Cue opens; choosing Cue there for a music type applies the music icon.

Right-click the picture for the context menu; a right click on the control bar does nothing. With a menu open, clicking another button of the bar closes the menu and acts at once.

The **…** menu is grouped in four parts: the file (open, close), ambient lighting, appearance (interface size and control background), and the places it leads to (language, remote control and About Cue). **Settings** shows each setting at work as you point at it, a small picture on top (beside the rows in a short window); the **i** in its header hides the pictures for a shorter panel. Four sizes are available: small, medium, large, and larger; medium is the fresh-install default. Each is a quarter step from the next, and every step stays visibly different at any window or screen. **Ctrl +** / **Ctrl −** (or Ctrl and the mouse wheel) step through them from anywhere, and **Ctrl 0** goes back to medium; the size's name shows for a moment. Sizes adapt to window width and DPI, with a stacked layout for vertical video. Video and subtitle size are independent of UI size. Drag the bar by its background to move it: bottom centre, top centre, the bottom corners, or anywhere you let it go. Without background, drag it from an empty place between its buttons (a click there still pauses). Hold **Shift** while dragging and the bar keeps its centre on the middle of the window: it moves only up and down and lands centred. In a narrow window it can only be centred, so it never lands in a corner that only shows once the window is widened.

Ambient lighting follows scene colors and fades in after playback starts. It is hidden on the home screen and in fullscreen or maximized mode.

With music, the same switch turns on a light show inside the window instead of the halo outside it: a soft light around the cover, in the colours of the cover's own edges, that breathes in and out on a slow cycle, a soft glow that leaves the spectrum's dots as they rise, deeper and a little quicker when the music is full and shallow in quiet passages, and a pool of light in each corner of the window that breathes with it, barely touched by each beat in turn. With **spectrum only**, the same light sits round and soft behind the equalizer and its dots take the light's colours. A louder stretch such as a chorus turns the colours half way round, and the spectrum's dots take the colour of the cover beside them. It stays quiet: the light adds to the picture, never covers the cover or the words, and settles while paused. With Windows' **Show animations** off, only a slow breath remains. The ambient light's strength sets how bright it is.

Cue supports Brazilian Portuguese, English, Spanish, French, Simplified Chinese, Russian, German, and Japanese. Automatic mode follows Windows display-language preferences, then falls back to English. Change the language in **… → language**, or the **…** beside Recent on the home screen. Changes apply immediately and are saved.

## Phone remote

Open **… → remote control** while a film plays, or **··· → remote control** beside Recent on the home screen. Cue shows a QR code with its network address and a six-digit code. Point the phone's camera at the code (or type the address in the phone's browser and enter the code). The phone and the computer must be on the same network. Nothing is installed on the phone.

The phone follows Cue's interface language: when it changes on the computer, an open phone page reloads once to show the new language, and stays paired.

The first time, Windows may ask whether Cue can use the network: allow it on **private** networks. On a network marked as public, Windows blocks phones from reaching Cue.

| On the phone | Does |
|---|---|
| Turn the wheel | Time: move through the film; faster turns take bigger steps (5 s, 15 s, 1 min). Volume: set the level. Up next: walk the queue. Cue acts when the finger lifts |
| Center button | Switch the wheel's mode: time, volume and, while a queue exists, up next. Small dots under the icon show the mode. In menus and folders, confirm |
| « / » | 15 seconds back / forward (in up next mode: previous / next file). Hold « for ten seconds back with subtitles shown for that moment |
| ▶❚❚ or double-tap the eyes | Play / pause; resuming steps back 3 seconds |
| menu | Subtitles (on/off, timing, size, position), audio track, sleep timer |
| Folder button | Browse the shared folders and open a file on the computer |
| The bar | Tap or drag to seek (or to set the volume, in volume mode) |
| The time on the right | Tap to switch between what is left (−12:34) and the whole length. Films start with what is left, songs with their length; the phone remembers the choice for each |

With music, the phone shows the song's title and artist (as the computer does) and wears its cover; turning the wheel turns the cover into a little record that spins with your thumb.

In menus and folders the « button becomes a back arrow; **menu** closes. Subtitle position is dragged on a small screen on the phone, or nudged with the wheel and the − / + buttons.

**Next episode.** Without a list of your own, a film is followed only by the next episode of its series in its folder: by episode number (S01E04, 1x04, Ep 4, even when the names are spelled a little differently, like *Twin Peaks - S01E01* and *Twin Peaks S01E02*), or by the same name with the next number (*Show - 01*, *Show - 02*). Another film or a song that sits beside it is never offered. A song brings its folder's songs (only the songs), and a list you made plays in its own order. In the last 90 seconds the phone offers it. With a phone connected, it starts on its own a moment after the episode ends, unless the sleep timer is set to the end of the episode.

**Shared folders.** The pairing screen lists them as chips: Videos and Downloads start on, Desktop starts off, and **+** adds any folder. A phone can list and open only what is inside a folder that is on. Thumbnails are the ones Windows shows in Explorer, shrunk to the remote's pixel size; files without one get a pixel landscape.

**Privacy and pairing.** Cue listens on the local network only (port 7878, or the next free one) and answers only addresses of private networks. The code is accepted only while the pairing screen is open; several wrong codes lock pairing for a minute. A paired phone keeps a token in its browser; Cue remembers the last eight. **Turn off** on the pairing screen stops the server and tells connected phones; it stays off until the remote is opened again.

The phone's browser cannot use its volume buttons for the film, and only Android phones vibrate with the wheel (Safari gives web pages no access to the iPhone's motor); every phone gets the soft tick.

## Lock screen and media keys

What plays shows in Windows' own media card: on the lock screen and in the flyout over the volume keys, with the song's title, artist and cover (a film shows its name), the timeline, and previous, play/pause and next. Those buttons, and keyboard media keys, act like Cue's own.

## Files and settings

- Preferences, recent files, and positions: `%APPDATA%\tinypeep\Cue\state.json` (including the remote's shared folders and paired phones).
- Backup: `state.json.bak`; corrupted state is preserved as `.corrupt`.
- Saved frames: the Windows Pictures folder, directly under `Cue` (for example `Pictures\Cue\Show S01E02_00-12-34.png`). Frames saved by earlier versions in `Cue\Frames` stay where they are.
- Shader cache: `%APPDATA%\tinypeep\Cue\shader-cache`.

Before 1.3 this folder was `%APPDATA%\tinyoffice\Cue` (and, earlier still, `tinyoffice\Player`): Cue moves it to `tinypeep\Cue` once, at its first start, in one step, keeping everything inside. If something holds a file there at that moment, Cue keeps using the old folder and moves it at the next start. The internal names Windows knows Cue by (file associations, the single-window handoff) stay as they were, so the default player choice and updates are untouched.

| Command | Purpose |
|---|---|
| `Cue.exe --welcome` | Show onboarding again |
| `Cue.exe --register` | Register “Open with” for the current user; does not change the default app |
| `Cue.exe --log` | Enable `mpv.log` in the state folder |

About Cue includes credits, a row of contact icons (LinkedIn, GitHub and e-mail; the caption under them reads *contact* and names the icon that is pointed at or focused, and the arrow keys move between them; when no mail app is set up, the e-mail icon copies the address), **credits & licenses** (an in-app reader with acknowledgments and component licenses), source links, and update checks. Cue checks for updates every three days. See [installation and updates](UPDATES.md).

### Remote security

The pairing code works once and expires after two minutes; a waiting pairing screen refreshes it. Six wrong attempts in a minute lock pairing for one minute. Turning the remote off revokes all remembered phones. This update requires existing phones to pair again once.

The remote uses HTTP on private IPv4 networks. Pairing controls access; it does **not** encrypt traffic. Use a trusted home network. Cue does not request router port forwarding. Only explicitly enabled, local-drive folders can be browsed; UNC paths and junctions/symlinks below those folders are refused.

## Up next

Open it with the **up next** button in the control bar, beside repeat, or press
**Q**. The button is always there; while the queue has entries it takes the film's
colour, like **CC** when subtitles are on. Right-click → **up next** opens it too. Repeat keeps its own
button and the **L** shortcut. Add files or a folder without interrupting the
current file. When the queue starts while a file is playing, that file becomes
its first entry; a folder that contains it does not list it a second time. Selecting multiple
files in the normal Open dialog replaces the queue and starts the first file.
**Ctrl+Shift+O** or **Recent → … → open folder** starts a folder as a new queue.

In short, wide windows (ultrawide video), menus that would not fit above the
control bar open beside it instead, level with its edge, so the controls stay
visible. About moves above the bar when centring it would cover the controls.

The panel opens over its button, like the bar's other menus (centred on it, or stopped at the
bar's edge), on the side of the bar that has room. In roomy windows it shows about ten rows; in
small windows it compacts.

The panel has three parts: the title with the entry count, the complete
scrollable list, and a footer with **+ files**, **+ folder** and **clear queue**.
The footer sits under the list because additions go to its end. Rows show the
file name without its extension, a video thumbnail and a short caption:
**now playing** with the playback state, **next** on the entry that plays next,
and the position (`3 / 7`) on the others. Entries before the playing one are
dimmed. Click a row to play it; clicking the playing row pauses or resumes. Drag
the grip or the row to reorder: the row lifts, the others open a gap, and a dotted
outline marks where it will land; the list scrolls near the edges. Hover a row for
its × remove button. Escape cancels a drag, then closes the panel. Removing the
playing row keeps playback. **clear queue** restores automatic episode discovery.
An empty queue says so and invites dropping files onto the panel.

Keyboard: Tab moves through the list, + files, + folder, clear and close; arrows,
Home/End and Page Up/Down navigate the list. Enter plays, Ctrl+↑/↓ reorders,
Delete removes. Inside the panel, Ctrl+O adds files and Ctrl+Shift+O adds a
folder. Files dropped onto the player while the queue is open are appended.
Video thumbnails load only for visible rows, using the Windows thumbnail
provider; unavailable previews and audio files use a placeholder.

Explicit queues advance through both audio and video and stop at their end.
Repeat and the sleep timer take priority. Closing the file clears the queue.
A folder includes only supported files directly inside it, in natural order;
subfolders and links are not expanded. Queues hold up to 512 entries, and folder
reading stops after 4,000 scanned directory entries or 512 playable files.

While an explicit queue exists, previous-file and next-file buttons sit outside the
15-second steps: previous · −15 · play · +15 · next. At either
end of the queue the button that has nowhere to go stays in place, dimmed and
inactive, so Play never moves. **P** goes to the previous file and **N** to the
next. Seeking and changing files remain separate actions.

On the phone, **up next** opens a temporary full-height media sheet. Visible videos
load small previews; audio and unavailable/unshared previews use icons. Tap the
title to play, or pause/resume the current item. Each row has a drag zone on the
left (a dotted handle and the thumbnail) and a × on the right that removes it.
A focused handle supports Ctrl+Up/Down. **clear queue** empties the list at once,
rows leaving top to bottom; the computer's confirmation follows. Motion respects
reduced motion.

While a queue exists, the wheel has a third mode, **up next**: tap the center
until the third dot lights. « and » go to the previous and next file; turning the
wheel walks the queue with a card showing the position, name and preview, and
lifting the finger plays the chosen file. In this mode the end-of-episode card and
its check button do not appear, since » already goes to the next file. Opening
and closing the sheet crossfade with the remote underneath. While music plays, the
end-of-file card never covers the face, and turning the wheel in time mode shows only
the time (the face steps back meanwhile): an audio file has no picture to preview.
The wheel's time steps follow the file's length (about 5 / 15 / 60 seconds for an
episode, 2 / 5 / 13 for a three-minute song), so short files stay easy to aim.

**+ add**, at the bottom of the sheet, opens the shared folders in adding mode.
Each file shows a **+** badge: tap the file to add it, and the badge turns into a
check mark. A second tap on a marked file adds nothing. The footer counts what
this visit added and offers **+ add this folder**, which adds every supported file
directly inside the open folder. **done** (or Esc) returns to the queue, scrolled
to its end with the new entries briefly highlighted. After a folder request, the
sheet returns by itself once the computer confirms it. A folder containing only
subfolders asks you to open a subfolder; it is not scanned recursively. Busy,
limit and failure messages stay visible. Clearing the queue cancels an outstanding
folder request. The phone can only open/enqueue files inside currently enabled
shared roots. The folder button beside **up next** opens the same folders for
playing a file right away, without badges.

### Long subtitle lists

The subtitle menu keeps its toggle and loading/style/sync controls fixed. Only
the track list scrolls, within the available window height. The selected track
is revealed when the menu opens. Use the wheel or touchpad, the pixel scrollbar,
Up/Down, Page Up/Down or Home/End; Enter selects the focused track.
Long names are clipped with an ellipsis without changing their stored metadata.
Language/region and forced/SDH flags precede long authored titles. A generic track
number is used only when no useful language or title metadata is available.

### Queue feedback

The title shows a count only for two or more entries. The dock's Up next button
takes the film's colour while entries exist, never a badge. New rows rise into place with a
short highlight; if they land out of sight, the list scrolls to them. A removed
row slides a few pixels toward its × while it fades. Clearing commits
immediately; only the visible rows finish a short top-to-bottom exit before the
panel contracts. Adding new
files or closing the panel does not wait for that animation. Reduced motion
shows the resulting state directly.
