# Phone clock synchronization — Cue 1.3.2

The play/pause gesture previously flipped `S.paused` before sampling `livePos`.
Pausing therefore sampled the old snapshot instead of its extrapolated position;
resuming could count elapsed time spent paused. The gesture now samples the old
clock first and then changes its rate/anchor. Replay from an ended file anchors
to zero, matching the PC's replay action.

Reconciliation preserves both pause intent and the position anchor while an
opposing pause snapshot is still pending, rather than applying the delayed
position independently. Existing finite reconciliation deadlines remain; another
media generation and authoritative confirmation still take precedence. This is
not a new global smoothing filter, so normal PC-side seeks remain visible.

Toggle, skip and the ten-second subtitle replay now require a successful socket
write before changing optimistic playback state. A disconnected or congested
socket cannot show an unsent action as successful. No command replay is added.

Production JavaScript tests cover pause/resume after a 600 ms snapshot gap and
three seconds paused, 2x playback, delayed opposing state, media replacement,
end-of-file replay and disconnected gestures. Existing lifecycle and asynchronous
fetch suites remain enabled. Evidence is in `validation-remote-clock-1.3.2/`.

The tests use a virtual clock and fake socket. Windows/physical-phone frame and
network latency were not measured. Native code is unchanged apart from the
regenerated embedded phone page; the Windows installer is rebuilt and verified.
