# Performance review — 2026-11-06

## Applied

- Windows media card: parse video filenames only when the path/type changes. Music title/artist changes still update immediately. The normal 33 ms poll previously parsed the same video name about 30 times a second. Playback state, timeline, cover completion and still capture are still checked on every poll.
- Animation: remove three early shader recomputations from the player frame. The existing overlay upload computes the current iris/list shader state after the UI tick, once at that point. Other callers still update when needed; the prior one-command-in-flight limit remains.
- Covers: one mutex protects cache lookup and insertion. A four-entry weak request cache reuses identical CoverJob/size decodes in flight or already owned by the player, including prefetch-to-playback. Weak entries retain no cover pixels and the existing four-file strong cache is unchanged.

## Validation

- Nine existing portable tests passed: performance, music_cover, card_art, cover_art, mini_card, music_light, audio_mix_persistence, startup, subtitle_selection.
- Windows x64 C++ build passed without warnings. These portable tests do not execute the changed Windows integrations; native card metadata updates, cover decode reuse and animation appearance require Windows checks.
- No Windows CPU/GPU/FPS benchmark was run, and no percentage improvement is claimed.

## Existing controls reviewed

State writes are debounced, large overlay buffers are freed after engine acknowledgment, frame caches are bounded, and ambient-light sampling is already delayed and throttled.

## Further profiling

Rapid skipping/resizing can still start distinct background cover jobs. A bounded worker queue with cancellation would need dedicated concurrency tests before changing that lifecycle. Measure sustained 4K playback, rapid track changes, slow storage and remote preview traffic on Windows before selecting that next change.
