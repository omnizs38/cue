# cue-monitor

A small read-only monitor for Cue on Windows. It never touches Cue, its settings or its files.

Every 2 seconds it samples each `Cue.exe` (the player and its preview helper):

- memory: private bytes, working set
- handles, GDI and USER objects, and threads
- CPU (% of one core)
- GPU: load, video decode, and dedicated memory
- I/O operations
- TCP connections

A folder watcher counts every write in `%APPDATA%\tinyoffice\Cue`, file by file.

The monitor writes almost nothing itself. It produces two files in a `cue-monitor` folder next to the program:

- `cue-monitor-<date>-<time>.txt`, the timeline: one line per process per minute, plus events (start, exit or crash, window changes, CPU or I/O spikes, new connections, the files written that minute).
- `cue-monitor-<date>-<time>-summary.txt`, the summary. It is rewritten every 5 minutes and when the monitor closes, and contains:
  - start, end, minimum and maximum of every counter
  - growth per hour, with R² (a leak grows steadily)
  - CPU by window state
  - writes per file per hour
  - connections and crashes
  - a list of what looks excessive

## Use

1. Run `cue-monitor.exe`.
2. Use Cue as usual.
3. Close the window (or press Ctrl+C) to finish.

Windows may warn about an unknown program: choose **More info → Run anyway**.

Options:

- `--interval 1`: seconds between samples (0.5 to 60; the default is 2)
- `--minutes 90`: stop by itself after this many minutes
- `--out <folder>`: where to write the reports

## Build

```
x86_64-w64-mingw32-g++ -std=c++20 -O2 -s -static -municode cue_monitor.cpp -o cue-monitor.exe ^
  -lpsapi -lpdh -liphlpapi -lws2_32 -lversion -lole32 -lshell32 -luuid
```
