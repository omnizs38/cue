# Remote and episode follow-up — 1.1.1

- Portrait pairing chooses an independent art scale using both available axes. At 1000×1780 with the playback scale at 2, pairing uses scale 4. QR modules remain integer pixels; folder chips wrap. Landscape retains the playback scale.
- Escape received through the player/engine prioritizes the active remote scene. Closing clears the first-frame reset flag and arms a fallback timer, so cancellation is not undone by a late frame. Drag handoff is posted, and keyboard focus returns to the owner after closure. Leaving the scene resumes a pause it initiated even if the pause acknowledgement was delayed. Going Home closes the scene without resuming media.
- System volume/mute key mappings no longer change Cue's own gain/mute. Arrow keys and the in-app controls still adjust Cue.
- The Up next panel integrates an explicit queue with next-file discovery. Numbered video episodes get a compact top toast in the final 10 seconds at the selected playback speed. Click the text to advance, the cross or Escape to cancel for this file. Pausing stops the countdown. Looping and the end-of-episode sleep timer prevent automatic advance. Advance happens at EOF, preserving the final seconds of content.
- The phone's linear timeline uses the dragged position for both its fill and knob. The lit outer-wheel dot has a generous angular hit area and starts continuous seeking; other wheel gestures retain their existing fine/coarse detents. Seeks commit at gesture release.

## Episode handoff correction

Opening media now takes ownership of its path argument before resetting discovery state. Previously, `open(next_ep_)` borrowed `next_ep_`, then cleared it before `loadfile`, sending an empty destination for player, phone and automatic requests. Empty open requests are now rejected before changing playback state.

Automatic advancement captures an owned destination at EOF, also handles the terminal END_FILE notification, and preserves a pending transition across idle notifications. Its timer consumes the captured path once; changing files, returning Home and cancelling invalidate it. The end-of-episode sleep timer continues to suppress advancement.

The portable EpisodeAdvance regression verifies ownership after discovery is cleared, duplicate EOF, one-shot consumption and cancellation. Windows playback itself still requires real-device validation.

## Series identity and next-file card

Next episode now compares the normalized title before the episode marker as well as season/episode numbers. It supports S01E02, 1x02 and episode-word markers, punctuation/spacing differences and leading bracketed release groups. A different show or an unidentifiable title is never an automatic continuation. Folder order can still supply a manual Up next suggestion. Number-only names and renamed series require manual selection; filename matching is conservative and is not a media database.

Player and phone distinguish automatic episodes from manual suggestions. The player card includes a single truncated filename line and a small 16:9 thumbnail beside its header. It reuses the shell thumbnail decoder used by the remote. One low-priority PC thumbnail worker at most is allowed globally; it starts within the last 90 seconds, never blocks the UI, and stale results cannot replace the new file's card. Only one 84×48 image is retained. If the Windows provider has no image, a folder placeholder remains visible.


## Queue transport and phone sheet — 1.2

An explicit queue with a next item adds a next-file button beside Play; +15 seconds
remains separate. At the queue end, only next-file disappears and Play stays put.

The phone's Up next is a temporary media sheet with visible-only thumbnails,
two-line names, current playback state and thumbnail-grip reordering. The entry
after the playing one is captioned **next**. Secondary move/remove actions live
behind More rather than competing with every title.

Adding has a single entry point, **+ add**, in the sheet's footer next to
**clear queue**: the thumb reaches it without stretching. It opens the shared
folders in adding mode, a full-height sheet with its own scroll area; the wheel is
hidden. The header reads *folder path · done*. File tiles carry a **+** badge that
turns into a check mark when tapped; a marked tile does not send a second request
and answers a repeat tap with a small bounce. The footer shows *added to queue · N*
and **+ add this folder**. Messages from the computer (reading, limit, failure)
take priority over the count. Folder completion is acknowledged by the computer
before returning to Up next; **done** returns at any time. The queue reopens
scrolled to its end, with the new entries briefly highlighted. Empty parent
folders point toward subfolders rather than silently appearing to do nothing;
folder imports remain nonrecursive. The folder button on the main screen still
opens files for immediate playback and shows no badges.

### Rows and clearing

Each row is laid out for a thumb: a drag zone on the left (a visible dotted handle
plus the thumbnail, about 110 CSS px wide), the title in the middle (tap to play),
and a 44 px × on the right that removes the item. There is no secondary menu.
Ctrl+Up/Down on a focused handle moves the row. **clear queue** acts at once on
the phone: rows leave top to bottom (32 ms apart, 240 ms each, sliding 28 px),
then the empty message rises in; the computer's update only confirms it.

Wheel steps in time mode scale with the length: the slow, medium and fast steps are
`round(dur/100)` (1–5 s), `round(dur/40)` (2–15 s) and `round(dur/15)` (5–60 s), so a
song is not crossed in four detents while long media keep 5 / 15 / 60 s. The
determined eyes follow the fast step, whatever its size.

Audio files have no pictures: the page never requests peeks for them, the time-mode
card drops its frame (no border, tint or background) and the face fades out while the
wheel turns, so only the time shows; the end-of-file card is not shown for music, whose
short length would keep it over the face most of the time. A song with a cover wears it
as the face: the page fetches `/cover` (72 px, shown at 2x, pixel for pixel) when the
state's `cover` number changes, takes its average colour (lifted so a dark cover still
glows) for the glow, and dims it while paused; without a cover the eyes stay. With music
in a queue, the side buttons are previous/next song (the up-next icons) and the long
press "what?" is off: a song's neighbours matter more than fifteen seconds of it. Under
the bar a song shows its length (like any music player) where a film shows when it ends.

The page's words come from Cue's interface language. Every state message carries
its tag (`lang`); when it differs from the language the page was served in, the page
reloads once to receive the new words. Pairing survives (the token lives in local
storage), and a session flag prevents a second reload for the same language.

Sheets crossfade with the remote: the wheel, face and timeline fade out over
160 ms as a sheet rises in, and fade back over 220 ms while it sinks 16 px and
fades out (200 ms) on close.

### Wheel modes

The center button cycles the wheel's modes: time, volume and, while an explicit
queue exists, **up next**. Two or three 4 px dots under the center icon show the
modes and light the current one. In up next mode the side buttons become previous
and next file, and the ring walks the queue in 26° detents. The scrub card shows
`position / total`, the name and, after a 140 ms rest, the item's preview; lifting
the finger plays it (nothing happens if it is already playing). The queue is
fetched from `/queue` when the mode is entered and when its revision changes.
The former next-file button beside Play is gone; Play stays centered. In up next
mode the end-of-episode card and the center's check are suppressed (» already
advances), and they also wait until the new file reports its duration, so a fresh
file never shows a stray check.

### Motion

Presses answer on the first frame (scale 0.9 for wheel buttons and icons, 0.95 for
the center button, 0.97 for tiles and cards) and release on one shared spring with
about 3% overshoot over 240–300 ms. Entrances rise 8 px, sheets 10–14 px. Menu,
grid and wheel-icon staggers are 16–20 ms, capped so a full screen settles within
about 110 ms of the first item. Reordered rows spring to their new places; removed
rows slide 12 px toward their actions while fading. The wheel's position dot leaves
a two-step pixel trail. A tile's entrance animation runs once and is then removed,
so selecting another tile never replays it (the old cause of a blink). Reduced
motion removes these transitions.
