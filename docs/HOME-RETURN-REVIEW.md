# Home return geometry — Cue 1.3.2

The supplied 30 fps recording shows uneven movement at the start of the portrait
window's return to the 16:9 home. Its sampling rate cannot establish display
frame pacing or identify a native call as the sole cause.

## Changes

The return retains the existing 260 ms cubic ease-out, its centered target,
native corners and the pixel-iris close. Fullscreen and mini-player curves are
unchanged. No new raster cache, worker or per-frame bitmap is introduced.

- `go_home` prepares the title and recent rows before starting the geometry clock.
  The home layout is applied at its final size instead of once more at the old
  portrait size after the transition starts.
- Native move notifications now defer halo shape/surface rebuilds during animated
  geometry changes, a pending home return and fullscreen transitions. The periodic
  atmosphere update observes the same deferral. Previously only manual resizing
  deferred this work; a halo still fading out could allocate/rasterize geometry
  synchronously inside the first `SetWindowPos` of the return.
- Atmosphere color blending no longer updates the hidden playback dock while the
  home is active. The halo's existing fade/update path remains available.
- Initial and tail samples whose rounded bounds match the current native bounds
  skip redundant `SetWindowPos` calls. Animation time and completion still advance.
- Cursor tracking against the old home layout waits until the home is revealed.

Microsoft documents that `SetWindowPos` changes window geometry and can preserve
and copy valid client contents; redundant calls and synchronous move/size handlers
are therefore avoided without changing drawing flags or disabling native corners.
[SetWindowPos reference](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos).

## Verification

Ten existing regressions cover ambient pixel output, the closing iris, fullscreen
mapping/curves, stall-aware glide clocks, per-media geometry, motion cadence,
window dragging, portrait bars/panels and mini-player placement. The portable/UI
implementation used by those tests is unchanged; the media-geometry integration
check reads the modified application source.

Windows x64 application and installer are cross-compiled. Release verification
checks the payload, checksum, staged bytes, unchanged engine DLL and 16 source
archives. Evidence is in `validation-home-return-1.3.2/`.

The changes remove specific synchronous work from the movement, but no native
Windows execution, compositor trace or before/after frame-time measurement is
available here. The visible improvement still needs observation on the machine
that produced the recording. Existing interruption, reduced-motion, maximized,
fullscreen and new-file paths retain their previous behavior and safeguards.
