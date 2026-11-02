# Release process

1. Set the version in `src/core/version.h` and add its changelog entry.
2. Run `bash tools/test.sh`; regenerate and verify localization and embedded pages.
3. Build application and installer with MinGW-w64 or the documented Windows tools.
   Official .tinypeep releases use `make OFFICIAL=1` and `make OFFICIAL=1 setup`.
4. Exercise playback, audio tracks, subtitles, queue/EOF, rapid file changes,
   fullscreen, resize/DPI, remote pairing/revocation and installer/uninstaller on
   native Windows. Record results and remaining limits in `VALIDATION.md`.
5. Package the application source without build output, caches, local state,
   logs, sample media or credentials. Preserve third-party sources and notices.
6. Attach `Cue-<version>-Setup.exe`, its SHA-256 and the matching
   `Cue-<version>-Engine-Source.zip` to the same GitHub release. Tag the exact
   application source revision. Verify installed `sources/README.txt` matches it.
7. Keep source downloads available beside the binaries. Publish only after the
   native release checks are complete. Source/local builds identify themselves
   as unofficial by default; see [BRANDING.md](BRANDING.md).

Current build evidence and unexecuted checks: [VALIDATION.md](VALIDATION.md).
Cross-compilation and portable tests do not measure native GPU frame pacing,
codec memory, Narrator or real-phone behavior. Releases are unsigned unless
explicitly documented otherwise.
