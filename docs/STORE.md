# Microsoft Store edition

The Store edition is the same Cue, built with `STORE=1` and packaged as an MSIX. Partner
Center product: **Cue by .tinypeep**, Store ID `9PJS1Q8KH61S`.

## What differs from the GitHub edition

| | GitHub edition (`make OFFICIAL=1`) | Store edition (`make OFFICIAL=1 STORE=1`) |
|---|---|---|
| Installed by | `Cue-<version>-Setup.exe`, in `C:\Cue` | the Microsoft Store (MSIX), in `WindowsApps` |
| Updates | GitHub releases, every 3 days or from About | the Microsoft Store only: no GitHub check, no update offer on the home, in More or About, no `Cue-Setup.exe` |
| File types, "Open with" | written to HKCU by Cue (`register_open_with`) | declared in `AppxManifest.xml` (`windows.fileTypeAssociation`); Cue writes nothing |
| Explorer's pictures of the files Cue opens | Cue's types name Windows' handler `{9DBD2C50…}` (`register_open_with`) | the manifest's types name it (`desktop2:ThumbnailHandler`, 1.4.1) |
| Subtitle fonts | also registered for the user | read from the package's `fonts\` (`sub-fonts-dir`) only |
| Data | `%APPDATA%\tinypeep\Cue` (moved once from `tinyoffice\Cue`) | `%APPDATA%\tinypeep\Cue`, kept by Windows in the package's per-user folder and removed with the app; no migration (a Store install starts with default settings) |

Interface, playback, the phone remote, saved frames (Pictures\Cue) and every preference are
the same. The GitHub edition's updater, installer and registration are untouched.

## Build and package

```bash
make OFFICIAL=1 STORE=1      # build/Cue/Cue.exe, the Store edition (libmpv-2.dll beside it)
make msix                    # the same, then the package layout in build/msix-layout
```

The package for Partner Center is packed on Windows by Microsoft's own tools, because only
MakePri can write the `resources.pri` that indexes the taskbar/Start icons (without it Windows
puts the icon on a plate of the accent colour). Needs the Windows SDK features "Signing Tools
for Desktop Apps" and "Windows SDK for UWP Managed Apps":

```powershell
powershell -ExecutionPolicy Bypass -File tools\make_msix.ps1 -Layout build\msix-layout -Msix dist\Cue-1.4-Store-x64.msix
```

(`dist/*.nopri.msix` from `make msix` is the Python packer's output, for checks only: it has
no resources.pri.)

`make msix` runs `tools/make_msix.py`, which needs only Python 3 and Pillow (for the logos,
drawn by `tools/gen_icon.py`). It refuses a `Cue.exe` that is not the Store edition. With MSVC:
`build.bat store`, then `python tools\make_msix.py --build build\Cue --version 1.4.0.0 --out dist\Cue-1.4-Store-x64.msix`.
Run `make OFFICIAL=1` again before `make setup`: the GitHub installer refuses a Store build.

On Windows, `tools/make_msix.ps1` checks the package with MakeAppx (`-Validate`), can pack
`make_msix.py --layout` with MakeAppx instead (`-Layout`), signs a separately named copy for
this PC (`-TestSign`, `*.TESTE-assinado-local.msix`) and installs it (`-Install`). Remove the
test install (`-Uninstall`) before installing Cue from the Store.

## Identity (never change after the first publication)

| Field | Value |
|---|---|
| Identity Name | `tinypeep.Cueby.tinypeep` |
| Publisher | `CN=5166F7F3-1280-4595-9F07-1BE4D4704F31` |
| PublisherDisplayName | `tinypeep` |
| Display name | `Cue by .tinypeep` (Start menu: `Cue`) |
| Application Id | `Cue` |
| Architecture / family | x64, `Windows.Desktop`, Windows 10 1809 (`10.0.17763.0`) or later |
| Version | `<CUE_VERSION>.0`: the Store reserves the fourth number, which stays 0 |
| Capabilities | `runFullTrust` only (Cue runs as an ordinary desktop program with the user's rights) |
| Firewall | inbound TCP 7878-7889 on private networks, for the phone remote |

The package is uploaded unsigned; Partner Center signs it.

## Before a submission

On Windows, with the signed test copy installed: Cue opens from Start; a video and a song play;
"Open with" lists Cue for both; subtitles use Cue's fonts; the phone remote pairs; About shows no
update row; `state.json`, `shader-cache\` and `cue.log` appear under
`%LOCALAPPDATA%\Packages\tinypeep.Cueby.tinypeep_*\LocalCache\Roaming\tinypeep\Cue` and leave
with the app (try it with and without the GitHub edition installed beside it).
Run the Windows App Certification Kit on the package if it is installed.
