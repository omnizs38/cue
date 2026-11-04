# The Cue Manifesto

**The video is the interface.**

Cue started from a simple wish: to watch a film or listen to an album without thinking about the player. It exists to step aside when it isn't needed and to appear, whole and in the right place, when it is. These are the principles behind every decision, from a 0.2-second animation to the way Cue writes a file.

[Português](MANIFESTO.pt-BR.md)

---

## 1. Performance and quality go together

There is no such thing as a beautiful animation that stutters, or a light player that looks cheap. Neither is an excuse for the other.

Every effect in Cue has to earn its cost: the ambient light is drawn by the graphics card, the interface redraws only when something has actually changed, and when the window is minimized or covered, Cue rests. If something makes Cue heavier without making it better, it goes.

## 2. The computer belongs to the person using it

Cue is a guest on your PC, and it behaves like one.

- It writes to disk only when it has to: a film's position is saved when you pause, close it or reach the end, and at most once a minute while you watch. A state that hasn't changed is never rewritten.
- It never asks for administrator rights, never makes itself the default player unless you ask, and leaves nothing running once it's closed.
- Heavy work, like reading a folder or decoding a cover, happens in the background, so the window never freezes.

## 3. Motion with a purpose

Animation in Cue isn't decoration: it explains what is happening. The control bar rises, and the subtitle rises with it, on the same frame. The timeline preview follows your pointer instead of trailing behind it. Nothing jumps, nothing flickers, nothing moves without a reason.

People who prefer less motion are respected too: when Windows asks for fewer animations, Cue listens.

## 4. No surprises

Cue does what you expect, and only that.

- Clicking to bring the window back doesn't pause your video.
- Up next only offers what makes sense: your own playlist, the next episode of the same series, or the album's next song. Never a random file that happened to be in the same folder.
- A click on the music doesn't stop the music.

When Cue isn't sure, it doesn't act.

## 5. Local and private

Your files are yours. Cue has no account and no cloud, and it never sends what you watch anywhere.

Its only conversation with the internet is an update check on GitHub every three days. The phone remote works over your local network, paired with a code, with no app to install.

## 6. Whole pixels

Cue has a face: crisp pixel art, with care in every icon, every cursor and every curve. Whole pixels, at any display scale, from 100% to 8K. A crooked detail is a bug, even if nobody complains.

## 7. Less, but better

Every feature has to earn its place. It's better to do a few things very well than many things halfway. Before adding anything, the question is: does this help someone watch or listen better?

## 8. Open, careful and grateful

Cue is free and open source (GPL-3.0). Anyone can read it, learn from it and improve it.

Every contribution is read carefully before it's merged: is it safe, is it needed, and does it respect these principles? And everyone who helps, with code, a translation or a bug report, is thanked by name.

---

If you'd like to contribute, start here. A pull request that makes Cue lighter, clearer or kinder to the computer it runs on is always welcome.

*.tinypeep*
