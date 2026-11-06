# Subtitle selection fix — Cue 1.3

- All 77 portable test executables passed (`bash tools/test.sh`).
- The new subtitle regression exercises full/mini A-B-A clicks, stale observers and command replies, coalesced changes, failed commands, panel close/reopen and file replacement.
- Windows x64 official build: GCC 13.2.0 / MinGW-w64, `-Wall -Wextra`; no compiler warnings.
- The bundled playback engine is unchanged; its SHA-256 was checked against the original installer and `tools/fetch-libmpv.ps1`.
- The installer payload is independently decompressed and checked for file boundaries, count, size, CRC, and exact compiled player/engine bytes before delivery.
- Native Windows playback and visual checks were not run in this Linux environment. Use the follow-up cases in `docs/VALIDATION.md`, including paused/playing, embedded/external SRT/ASS/bitmap subtitles, audio changes and mini/fullscreen transitions.
