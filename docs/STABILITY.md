# Additional stability review — Cue 1.1.1

The following lifecycle safeguards were introduced in 1.1.1 and remain in use. The playback engine is unchanged.

## Findings fixed in this pass

| Finding | Impact | Change |
|---|---|---|
| UI writes directly to the preview helper's synchronous pipe | A slow/stuck decoder or repeated file changes can fill the pipe and freeze the UI | Dedicated writer with a bounded mailbox: 16 pending commands, 64 KiB total, 16 KiB per line. New open/close commands discard obsolete queued work. Failure disables optional previews |
| Preview reader uses its owner's address and pipe handle after timed shutdown; exit path leaves the read handle open | Potential stale-owner/handle access during restart; inconsistent cleanup | I/O workers own shared state and handles until completion. Shutdown terminates the optional child, cancels queued work, prevents further posts and deletes already-posted frame messages |
| Child process inherits all inheritable handles; job assignment errors ignored | Unintended resource retention and weakened child cleanup guarantee | Explicit two-handle inheritance list; require successful kill-on-close job configuration/assignment before resuming the child |
| Remote commands bounded, but connection lifecycle events unbounded | A paired client repeatedly reconnecting while the UI is busy can grow the event vector | 256-event hard limit, authentication backpressure at 192, command limit 128; room reserved for all active clients to disconnect |
| Remote shutdown changes sockets to blocking mode and sends to each phone | Multiple non-reading phones can delay the UI's join | Best-effort shutdown notification on nonblocking sockets; remove the extra worker wait |
| HTTP worker cleanup uses only the success path | Exceptions can leave resource counters/gates inconsistent; Winsock ownership can race shutdown | Scope cleanup for socket/counter/Winsock and thumbnail gate; acquire Winsock reference before launching each worker; suppress obsolete work/replies |
| Settings values cast before range validation; version components multiply without overflow checks | Corrupt/extreme numeric values cause undefined behavior and implausible window restoration | Safe integer conversion before clamping, geometry validation, bounded stored media times, saturating version parsing and updater asset-size rejection |
| Settings/language readers consume whole files, and JSON tree count is unbounded | A corrupt oversized file or a compact array can amplify startup RAM use | 4 MiB state-file read limit, 65,536 JSON value limit, existing depth limit retained; backup recovery tested |

Additional preview checks reject oversized command lines, line/NUL injection and non-finite frame times. Screenshot result nodes are released even for unexpected types, and stride bounds are checked without overflowing a multiplication.

## Verification

Current release validation is recorded in [VALIDATION.md](VALIDATION.md).
Portable tests do not establish native GPU performance or whole-process leak freedom.

## Remaining limits

No claim of a complete security audit, leak-free process or measured speedup on native Windows is made. Real Windows playback, codec/GPU allocations, Winsock/COM integration, stalled-process cancellation and iOS Safari still need native validation. Explorer providers can stall within the fixed worker budget; a stuck provider is not forcibly terminated in-process. HTTP/WS remains unencrypted, and bearer tokens remain in per-user storage. See [Security](SECURITY.md), [Memory](MEMORY.md) and [Disk/RAM](RESOURCES.md).

The 12 MiB budget applies to the timeline pixel cache, not the entire player. The existing engine buffering and ambient-light sampling behavior are preserved. Settings remain compatible with the previous working builds.

## API references

- [Microsoft: anonymous pipe operations](https://learn.microsoft.com/en-us/windows/win32/ipc/anonymous-pipe-operations) — synchronous writes can block when the pipe is full.
- [Microsoft: handle inheritance](https://learn.microsoft.com/en-us/windows/win32/procthread/inheritance) — restrict inherited handles using an explicit attribute list.
- [Microsoft: nonblocking socket I/O](https://learn.microsoft.com/en-us/windows/win32/winsock/nonblocking-i-o-2) — operations return rather than waiting for a peer.
