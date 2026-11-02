# Memory and resource lifecycle review: 1.1.1

Scope: Cue first-party code, particularly repeated scene open/close/resize, font fallback lookups, phone browsing, and worker shutdown. This does not certify all native playback, GPU, OS shell or codec allocations.

## Findings and fixes

| Finding | Classification | Change |
|---|---|---|
| Overlay resize calls DeleteObject while the bitmap is selected into its DC, ignores the return and loses the old handle | Incorrect GDI resource ownership; deletion is not guaranteed by the documented API contract | DIB and DC owned together; restore selection, destroy DC and delete bitmap; safe rollback on allocation failure |
| Overlay destruction stops its pacer but does not explicitly destroy its HWND, DC or bitmap | Missing cleanup | Idempotent destroy/destructor; explicitly invoked on app shutdown |
| Closing a scene keeps its DIB and full-window Canvas allocated | Retained buffers, not unbounded leakage by itself | Release both after the close animation finishes; recreate on next open |
| Font fallback caches every unsupported code point as an empty entry | Growing process-lifetime cache | Cache positive entries from the finite font table only; misses allocate nothing |
| Vsync stop times out after 200ms but its worker still references the owner and event | Potential use-after-free/invalid-handle access during slow compositor shutdown | Worker owns shared state/event independently until it actually exits; start/stop are idempotent |
| Phone browser keeps hidden file DOM, blob URLs and pending fetch closures after closing | Resource/cache retention | Clear file DOM/data, revoke blobs, disconnect observer, cancel listing/thumbnail requests; reject late completions |
| Timeline preview blob remains until replaced | One retained image, not an unbounded leak | Revoke on scrub end, next scrub start and media changes; abort obsolete preview requests |

A 3840×2160 BGRA buffer is 31.64 MiB. The scene has both a DIB and a CPU canvas: approximately 63.28 MiB of pixel storage at that size. This is a size calculation, not a measured reduction in total process RSS. Windows/allocators may retain freed pages for reuse.

## Verification

Current release evidence: [VALIDATION.md](VALIDATION.md).

## Intentional bounded storage

The timeline thumbnail cache, remote thumbnail cache, known fallback glyphs, fonts, synthesized pairing sounds and decoder buffers are intentional. A rise during first use or a high working set alone does not prove a leak. The important distinction is whether repeated identical operations continue to add live resources after warm-up.

## Reproduce

- Portable: `bash tools/test.sh` (`dib_surface_test.cpp`, `glyph_memory_test.cpp`).
- Browser: install Playwright/Chromium, then `node tests/remote_memory.cjs`; `CUE_CHROMIUM_PATH` may select an existing executable. This uses a mock player connection and real browser DOM/fetch/audio lifetimes.
- Native Windows: build and run `tests/overlay_engine_test.cpp` using the command in its header. It checks actual GDI counters across surface/scene cycles and prints process private bytes. Build output is `build/overlay-memory-test.exe`.
- Before release: repeat open/close/resize, fullscreen, file changes and phone browsing on a real Windows PC; record private bytes, GDI objects, handles and GPU memory after warm-up and again after sustained use. A long-run native profiler is still needed before claiming the whole player is leak-free.

Reference: [Microsoft DeleteObject](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-deleteobject) documents that an object currently selected into a DC is not eligible for ordinary successful deletion. [SelectObject](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-selectobject) describes restoring the previously selected object.
