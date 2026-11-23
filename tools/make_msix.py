#!/usr/bin/env python3
"""Packages the Microsoft Store edition of Cue as an MSIX (x64), without the Windows SDK.

    python3 tools/make_msix.py --build build/Cue --version 1.4.0.0 \\
        --out dist/Cue-1.4-Store-x64.msix [--layout build/msix-layout]

--build   the program folder: what `make OFFICIAL=1 STORE=1` (or `build.bat store`) leaves in
          build/Cue, with libmpv-2.dll beside Cue.exe. Cue.exe must be the Store edition
          (it carries "cue-edition:microsoft-store"); a GitHub build is refused, because its
          updater would download Cue-Setup.exe from inside a Store app.
--layout  also writes the unpacked package (AppxManifest.xml, Assets\\, the program), which
          MakeAppx.exe can pack on Windows: tools/make_msix.ps1 -Layout <dir>.
--out     the package. It is unsigned: Partner Center signs what it publishes. For a copy to
          install on your own PC, sign a copy with tools/make_msix.ps1 -TestSign.

The package's identity is Partner Center's reservation for "Cue by .tinypeep" (IDENTITY below).
Once a version has been published, Identity Name, Publisher and Application Id never change.

The file has the ZIP layout of MakeAppx and the MSIX SDK (data descriptors, ZIP64 end records)
and the OPC parts MSIX expects: AppxManifest.xml, AppxBlockMap.xml (SHA-256 of every 64 KiB
block; for a compressed file each block is a separate deflate segment, so its compressed size is
exact) and [Content_Types].xml. Output is reproducible: the same
input files and version give the same bytes.
"""
import argparse
import base64
import hashlib
import importlib.util
import os
import re
import shutil
import struct
import sys
import zlib
from xml.sax.saxutils import escape, quoteattr

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Partner Center: Product identity of "Cue by .tinypeep" (Store ID 9PJS1Q8KH61S)
IDENTITY = {
    "name": "tinypeep.Cueby.tinypeep",
    "publisher": "CN=5166F7F3-1280-4595-9F07-1BE4D4704F31",
    "publisher_display": "tinypeep",
    "display_name": "Cue by .tinypeep",  # the reserved name: Properties/DisplayName
}
APP_ID = "Cue"                 # Application Id: permanent once published
START_NAME = "Cue"             # the name in the Start menu (VisualElements/DisplayName)
DESCRIPTION = "Video and music player"
# "transparent": Windows then uses the target-size altform-unplated icons (indexed by
# resources.pri); a solid colour makes some surfaces ("Open with") fall back to a plate.
TILE_BACKGROUND = "transparent"
# (1.4.1, 23/11) No thumbnail handler on the package's file types: MakeAppx refuses one that is
# not a COM class the package itself declares (error C00CE1A2, keyref ComServer_SurrogateServer_
# Class_IdKey), and Windows' own handler is not the package's. A first 1.4.1 named it; the author's
# MakeAppx refused the manifest. The file types are declared as in 1.4 again.
MIN_VERSION = "10.0.17763.0"   # Windows 10 1809
MAX_TESTED = "10.0.26100.0"    # Windows 11 24H2
LANGUAGES = ["en-us", "pt-br"]  # the Store listings; the interface itself picks among its 8 languages

# app list / taskbar sizes Microsoft lists for Square44x44Logo, and the file type icon sizes
TARGET_SIZES = [16, 20, 24, 30, 32, 36, 40, 48, 60, 64, 72, 80, 96, 256]
SCALES = [100, 125, 150, 200, 400]

BLOCK = 65536
# the program folder holds no packaging parts of its own; installer-only files never go in
RESERVED = {"appxmanifest.xml", "appxblockmap.xml", "[content_types].xml", "appxsignature.p7x", "appxmetadata", "assets"}
SKIP = {"install.txt", "uninstall.exe", "cue.log", "cue.log.1"}
# 2026-11-09 12:08:52 (the 1.3.2 commit), in ZIP's DOS form: fixed, so the bytes are reproducible
DOS_TIME, DOS_DATE = (12 << 11) | (8 << 5) | (52 // 2), ((2026 - 1980) << 9) | (11 << 5) | 9


def die(msg):
    sys.exit("make_msix: " + msg)


# ── reading the program ────────────────────────────────────────────
def pe_machine(path):
    with open(path, "rb") as f:
        d = f.read(4096)
    if d[:2] != b"MZ":
        return None
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    if d[pe:pe + 4] != b"PE\0\0":
        return None
    return struct.unpack_from("<H", d, pe + 4)[0]


def release_name(version):
    """The version as Cue writes it (CUE_VERSION, the release tag): 1.4.0.0 -> 1.4, 1.3.3.0 -> 1.3.3."""
    parts = version.split(".")[:3]
    while len(parts) > 2 and parts[-1] == "0":
        parts.pop()
    return ".".join(parts)


def exe_version_strings(data):
    """StringFileInfo values of a PE's version resource (UTF-16 key/value pairs)."""
    out = {}
    for key in ("FileVersion", "ProductVersion", "ProductName", "CompanyName", "SpecialBuild"):
        k = (key + "\0").encode("utf-16-le")
        i = data.find(k)
        if i < 0:
            continue
        j = i + len(k)
        while data[j:j + 2] == b"\0\0":  # padding to the value's 32-bit boundary
            j += 2
        end = j
        while data[end:end + 2] != b"\0\0":
            end += 2
        out[key] = data[j:end].decode("utf-16-le", "replace")
    return out


def source_lists():
    """File types and the phone remote's ports, read from the same source as the GitHub edition."""
    plat = open(os.path.join(ROOT, "src", "win", "platform.cpp"), encoding="utf-8").read()
    def exts(name):
        m = re.search(r"static const wchar_t\* " + name + r"\[\] = \{([^}]*)\}", plat)
        if not m:
            die("cannot read %s from src/win/platform.cpp" % name)
        return re.findall(r'L"(\.[a-z0-9]+)"', m.group(1))
    proto = open(os.path.join(ROOT, "src", "core", "remote_proto.h"), encoding="utf-8").read()
    port = int(re.search(r"constexpr int kPort = (\d+);", proto).group(1))
    tries = int(re.search(r"constexpr int kPortTries = (\d+);", proto).group(1))
    return exts("kVideoExts"), exts("kAudioExts"), port, port + tries - 1


def collect(build):
    files = []
    seen = {}
    for dirpath, dirnames, filenames in os.walk(build):
        dirnames[:] = sorted(d for d in dirnames if not d.startswith("."))
        rel_dir = os.path.relpath(dirpath, build)
        if rel_dir == ".":
            for d in dirnames:
                if d.lower() in RESERVED:
                    die("build folder already has %s\\ (the packager makes it)" % d)
        for fn in sorted(filenames):
            if fn.startswith("."):
                continue
            rel = fn if rel_dir == "." else os.path.join(rel_dir, fn)
            rel = rel.replace(os.sep, "/")
            if rel_dir == "." and fn.lower() in RESERVED:
                die("build folder already has %s (the packager writes it)" % fn)
            if rel.lower() in SKIP or fn.lower().endswith((".tmp", ".part")):
                continue
            if not re.fullmatch(r"[A-Za-z0-9._/+-]+", rel):
                die("unexpected character in file name: " + rel)
            low = rel.lower()
            if low in seen:
                die("two files differ only in case: %s, %s" % (seen[low], rel))
            seen[low] = rel
            files.append(rel)
    return files


# ── assets: Cue's own tile, drawn per size by tools/gen_icon.py ─────
def make_assets():
    spec = importlib.util.spec_from_file_location("cue_gen_icon", os.path.join(ROOT, "tools", "gen_icon.py"))
    gi = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(gi)
    import io
    def png(im):
        b = io.BytesIO()
        im.save(b, "PNG", optimize=True)
        return b.getvalue()
    # Unqualified names, each at its scale-100 size, plus the target-size icons the shell
    # wants for the taskbar, Start and Explorer. Without the altform-unplated ones (and a
    # resources.pri that indexes them, made by MakePri: tools/make_msix.ps1 -Layout) Windows
    # draws the icon small on a plate of the accent colour.
    from PIL import Image
    def plated(im):
        # the plated variant sits on the plate Windows draws (the accent colour when the manifest
        # says "transparent"): opaque corners in the tile's own black, so no plate shows through
        back = Image.new("RGBA", im.size, gi.FILL + (255,))
        back.alpha_composite(im)
        return back
    out = {
        "Assets/Square44x44Logo.png": png(plated(gi.tile(44))),
        "Assets/Square150x150Logo.png": png(plated(gi.tile(150))),
        "Assets/StoreLogo.png": png(gi.tile(50)),
        "Assets/FileVideo.png": png(plated(gi.tile(44))),
        "Assets/FileAudio.png": png(plated(gi.music_icon(44))),
    }
    for pct in SCALES:
        out["Assets/Square44x44Logo.scale-%d.png" % pct] = png(plated(gi.tile(round(44 * pct / 100))))
        out["Assets/Square150x150Logo.scale-%d.png" % pct] = png(plated(gi.tile(round(150 * pct / 100))))
        out["Assets/StoreLogo.scale-%d.png" % pct] = png(gi.tile(round(50 * pct / 100)))
        out["Assets/FileVideo.scale-%d.png" % pct] = png(plated(gi.tile(round(44 * pct / 100))))
        out["Assets/FileAudio.scale-%d.png" % pct] = png(plated(gi.music_icon(round(44 * pct / 100))))
    # Three forms per size, as Microsoft asks: the default (plated), the dark theme
    # (altform-unplated) and the light theme (altform-lightunplated). The tile is monochrome, and
    # Windows checks a monochrome icon's contrast with the background: on dark backgrounds the
    # dark form carries a graphite rim (gen_icon.DARK_RIM), or "Open with" plates it.
    for n in TARGET_SIZES:
        for name, draw in (("Square44x44Logo", gi.tile), ("FileVideo", gi.tile), ("FileAudio", gi.music_icon)):
            light = draw(n)
            out["Assets/%s.targetsize-%d.png" % (name, n)] = png(plated(light))
            out["Assets/%s.targetsize-%d_altform-unplated.png" % (name, n)] = png(draw(n, dark=True))
            out["Assets/%s.targetsize-%d_altform-lightunplated.png" % (name, n)] = png(light)
    return out


# ── the manifest ───────────────────────────────────────────────────
def manifest(version, video, audio, port_lo, port_hi, test=False):
    def types(exts):
        return "\n".join("              <uap:FileType>%s</uap:FileType>" % e for e in exts)
    res = "\n".join('    <Resource Language="%s"/>' % lang for lang in LANGUAGES)
    ident = dict(IDENTITY)
    start = START_NAME
    if test:  # a packaging test: an identity Partner Center rejects, so it can never be submitted
        ident["name"] += ".PackTest"
        ident["display_name"] = "Cue pack test (not for the Store)"
        start = "Cue (pack test)"
    return f"""<?xml version="1.0" encoding="utf-8"?>
<Package
  xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
  xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10"
  xmlns:desktop2="http://schemas.microsoft.com/appx/manifest/desktop/windows10/2"
  xmlns:rescap="http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities"
  IgnorableNamespaces="uap desktop2 rescap">
  <Identity Name={quoteattr(ident["name"])} Publisher={quoteattr(ident["publisher"])} Version="{version}" ProcessorArchitecture="x64"/>
  <Properties>
    <DisplayName>{escape(ident["display_name"])}</DisplayName>
    <PublisherDisplayName>{escape(ident["publisher_display"])}</PublisherDisplayName>
    <Logo>Assets\\StoreLogo.png</Logo>
  </Properties>
  <Dependencies>
    <TargetDeviceFamily Name="Windows.Desktop" MinVersion="{MIN_VERSION}" MaxVersionTested="{MAX_TESTED}"/>
  </Dependencies>
  <Resources>
{res}
  </Resources>
  <Applications>
    <Application Id="{APP_ID}" Executable="Cue.exe" EntryPoint="Windows.FullTrustApplication">
      <uap:VisualElements DisplayName={quoteattr(start)} Description={quoteattr(DESCRIPTION)} BackgroundColor="{TILE_BACKGROUND}"
        Square150x150Logo="Assets\\Square150x150Logo.png" Square44x44Logo="Assets\\Square44x44Logo.png">
        <uap:DefaultTile ShortName={quoteattr(start)}/>
      </uap:VisualElements>
      <Extensions>
        <uap:Extension Category="windows.fileTypeAssociation">
          <uap:FileTypeAssociation Name="video">
            <uap:DisplayName>Cue video</uap:DisplayName>
            <uap:Logo>Assets\\FileVideo.png</uap:Logo>
            <uap:SupportedFileTypes>
{types(video)}
            </uap:SupportedFileTypes>
          </uap:FileTypeAssociation>
        </uap:Extension>
        <uap:Extension Category="windows.fileTypeAssociation">
          <uap:FileTypeAssociation Name="audio">
            <uap:DisplayName>Cue audio</uap:DisplayName>
            <uap:Logo>Assets\\FileAudio.png</uap:Logo>
            <uap:SupportedFileTypes>
{types(audio)}
            </uap:SupportedFileTypes>
          </uap:FileTypeAssociation>
        </uap:Extension>
      </Extensions>
    </Application>
  </Applications>
  <Capabilities>
    <rescap:Capability Name="runFullTrust"/>
  </Capabilities>
  <!-- windows.firewallRules is a PACKAGE extension (Package/Extensions). Inside <Application>
       Windows rejects the manifest and App Installer says "Erro ao analisar o pacote".
       The phone remote: its page and WebSocket on the home network (private profile). -->
  <Extensions>
    <desktop2:Extension Category="windows.firewallRules">
      <desktop2:FirewallRules Executable="Cue.exe">
        <desktop2:Rule Direction="in" IPProtocol="TCP" Profile="private" LocalPortMin="{port_lo}" LocalPortMax="{port_hi}"/>
      </desktop2:FirewallRules>
    </desktop2:Extension>
  </Extensions>
</Package>
"""


# Extension categories Windows accepts only in Package/Extensions, never in an Application's.
PACKAGE_ONLY = {"windows.firewallRules", "windows.desktopEventLogging", "windows.certificates",
                "windows.publisherCacheFolders", "windows.comServer", "windows.comInterface"}


def check_manifest(data):
    """What the Windows schema enforces and the MSIX SDK's simplified schema lets through."""
    import xml.etree.ElementTree as ET
    root = ET.fromstring(data)
    for app in root.iter("{http://schemas.microsoft.com/appx/manifest/foundation/windows10}Application"):
        for ext in app.iter():
            cat = ext.get("Category")
            if cat in PACKAGE_ONLY:
                die("%s is a package extension: it belongs in Package/Extensions, not in <Application>" % cat)


# ── OPC/ZIP writing ────────────────────────────────────────────────
UNRESERVED = re.compile(r"[A-Za-z0-9\-._~/]")


def part_name(rel):
    return "".join(c if UNRESERVED.fullmatch(c) else "%%%02X" % ord(c) for c in rel)


CONTENT_TYPES = {
    "exe": "application/x-msdownload", "dll": "application/x-msdownload",
    "png": "image/png", "ttf": "application/x-font-ttf", "json": "application/json",
    "md": "text/markdown", "txt": "text/plain", "xml": "application/xml",
    "py": "text/x-python", "sh": "application/x-sh",
}


def content_types(names):
    defaults, overrides = {}, []
    for n in names:
        base = n.rsplit("/", 1)[-1]
        if n == "AppxManifest.xml":
            continue
        if "." in base:
            ext = base.rsplit(".", 1)[1].lower()
            defaults.setdefault(ext, CONTENT_TYPES.get(ext, "application/octet-stream"))
        else:
            overrides.append(("/" + part_name(n), "application/octet-stream"))
    x = ['<?xml version="1.0" encoding="UTF-8"?>',
         '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">']
    for ext in sorted(defaults):
        x.append('<Default Extension=%s ContentType=%s/>' % (quoteattr(ext), quoteattr(defaults[ext])))
    for p, t in overrides:
        x.append('<Override PartName=%s ContentType=%s/>' % (quoteattr(p), quoteattr(t)))
    x.append('<Override PartName="/AppxManifest.xml" ContentType="application/vnd.ms-appx.manifest+xml"/>')
    x.append('<Override PartName="/AppxBlockMap.xml" ContentType="application/vnd.ms-appx.blockmap+xml"/>')
    x.append("</Types>")
    return ("\r\n".join(x) + "\r\n").encode("utf-8")


def deflate_blocks(data):
    """As MakeAppx does: each 64 KiB block is its own byte-aligned deflate segment (full flush),
    so the block map can give every block's compressed size; the stream then ends with the
    2-byte final block (03 00), which belongs to no block."""
    c = zlib.compressobj(6, zlib.DEFLATED, -15, 9)
    segs = [c.compress(data[i:i + BLOCK]) + c.flush(zlib.Z_FULL_FLUSH) for i in range(0, len(data), BLOCK)]
    return segs, c.flush(zlib.Z_FINISH)


class Writer:
    """The ZIP layout of Microsoft's packagers (MakeAppx, the MSIX SDK's ZipObjectWriter): every
    local header has bit 3 set and no sizes, each file is followed by a ZIP64 data descriptor,
    and the archive ends with a ZIP64 end of central directory, its locator and an EOCD whose
    fields all point to it."""

    def __init__(self, path, compress):
        self.f = open(path, "wb")
        self.compress = compress
        self.central = []
        self.blockmap = []

    def add(self, name, data, in_blockmap=True):
        # OPC: a ZIP item is named by its part name, percent-encoded, except the content types
        # stream, whose brackets stay literal
        zname = (name if name == "[Content_Types].xml" else part_name(name)).encode("ascii")
        crc = zlib.crc32(data) & 0xFFFFFFFF
        hashes = [hashlib.sha256(data[i:i + BLOCK]).digest() for i in range(0, len(data), BLOCK)]
        segs, tail = deflate_blocks(data) if (self.compress and data) else (None, b"")
        if segs is not None and sum(map(len, segs)) + len(tail) >= len(data):
            segs, tail = None, b""  # nothing to gain (PNG, already compressed): stored
        method = 8 if segs is not None else 0
        payload = b"".join(segs) + tail if segs is not None else data
        if len(payload) >= 0xFFFFFFFE or len(data) >= 0xFFFFFFFE or self.f.tell() >= 0xFFFFFFFE:
            die("file too large for this packager: " + name)
        offset = self.f.tell()
        lfh = struct.pack("<IHHHHHIIIHH", 0x04034B50, 45, 0x0008, method, DOS_TIME, DOS_DATE, 0, 0, 0,
                          len(zname), 0) + zname
        self.f.write(lfh)
        self.f.write(payload)
        self.f.write(struct.pack("<IIQQ", 0x08074B50, crc, len(payload), len(data)))
        self.central.append((zname, method, crc, len(payload), len(data), offset))
        if in_blockmap:
            self.blockmap.append((name, len(data), len(lfh), hashes, [len(s) for s in segs] if segs else None))

    def blockmap_xml(self):
        x = ['<?xml version="1.0" encoding="UTF-8" standalone="no"?>',
             '<BlockMap xmlns="http://schemas.microsoft.com/appx/2010/blockmap" HashMethod="http://www.w3.org/2001/04/xmlenc#sha256">']
        for name, size, lfh, hashes, sizes in self.blockmap:
            x.append('<File Name=%s Size="%d" LfhSize="%d">' % (quoteattr(name.replace("/", "\\")), size, lfh))
            for i, h in enumerate(hashes):
                b64 = base64.b64encode(h).decode()
                x.append('<Block Hash="%s"/>' % b64 if sizes is None else '<Block Hash="%s" Size="%d"/>' % (b64, sizes[i]))
            x.append("</File>")
        x.append("</BlockMap>")
        return ("\r\n".join(x) + "\r\n").encode("utf-8")

    def close(self):
        start = self.f.tell()
        for zname, method, crc, csize, usize, offset in self.central:
            self.f.write(struct.pack("<IHHHHHHIIIHHHHHII", 0x02014B50, 45, 45, 0x0008, method, DOS_TIME, DOS_DATE,
                                     crc, csize, usize, len(zname), 0, 0, 0, 0, 0, offset) + zname)
        size = self.f.tell() - start
        n = len(self.central)
        zip64_eocd = self.f.tell()
        self.f.write(struct.pack("<IQHHIIQQQQ", 0x06064B50, 44, 45, 45, 0, 0, n, n, size, start))
        self.f.write(struct.pack("<IIQI", 0x07064B50, 0, zip64_eocd, 1))
        self.f.write(struct.pack("<IHHHHIIH", 0x06054B50, 0, 0, 0xFFFF, 0xFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0))
        self.f.close()


# ── main ───────────────────────────────────────────────────────────
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--build", required=True)
    ap.add_argument("--version", required=True, help="package version, four parts, the last 0 (Store rule)")
    ap.add_argument("--out", required=True)
    ap.add_argument("--layout")
    ap.add_argument("--no-compress", action="store_true")
    ap.add_argument("--allow-github-build", action="store_true",
                    help="packaging test only: accept a GitHub-edition Cue.exe (never for the Store)")
    a = ap.parse_args()

    if not re.fullmatch(r"\d+\.\d+\.\d+\.0", a.version):
        die("version must look like 1.4.0.0 (the fourth part is reserved by the Store and must be 0)")
    build = os.path.abspath(a.build)
    exe, dll = os.path.join(build, "Cue.exe"), os.path.join(build, "libmpv-2.dll")
    for p in (exe, dll):
        if not os.path.isfile(p):
            die("missing " + p)
        if pe_machine(p) != 0x8664:
            die(os.path.basename(p) + " is not an x64 Windows program")
    exe_data = open(exe, "rb").read()
    vs = exe_version_strings(exe_data)
    want = release_name(a.version)
    if vs.get("ProductVersion") != want:
        die("Cue.exe is version %r, the package says %s" % (vs.get("ProductVersion"), want))
    if vs.get("ProductName") != "Cue":
        die("Cue.exe is not an official build (ProductName %r): build with OFFICIAL=1 STORE=1" % vs.get("ProductName"))
    store_exe = b"cue-edition:microsoft-store" in exe_data or vs.get("SpecialBuild") == "Microsoft Store"
    if not store_exe:
        if not a.allow_github_build:
            die("Cue.exe is not the Store edition (its GitHub updater would stay on): "
                "build with `make OFFICIAL=1 STORE=1` or `build.bat store`")
        print("WARNING: GitHub-edition Cue.exe packaged for a packaging test, as tinypeep.Cueby.tinypeep.PackTest "
              "(an identity Partner Center refuses). Not for the Store.")

    video, audio, port_lo, port_hi = source_lists()
    files = collect(build)
    assets = make_assets()
    # where this package's sources are (GPL/LGPL): replaces the installer's wording
    v3 = release_name(a.version)
    notes = {"sources/README.txt": (
        "Cue source code\r\n\r\n"
        "This is the Microsoft Store edition of Cue %s, built from the Cue %s source with\r\n"
        "`make OFFICIAL=1 STORE=1` (see docs/STORE.md). That source and the corresponding playback\r\n"
        "engine sources are on the GitHub release page:\r\n\r\n"
        "https://github.com/tinypeep/cue/releases/tag/v%s\r\n\r\n"
        "- Source code (zip): Cue application source (GPL-3.0-or-later), generated by GitHub.\r\n"
        "- Cue-%s-Engine-Source.zip: sources and build recipes for the included libmpv-2.dll.\r\n\r\n"
        "Licenses and notices: LICENSES.md and the third_party folder of this package.\r\n" % (v3, v3, v3, v3)).encode("utf-8")}
    files = [f for f in files if f not in notes]
    man = manifest(a.version, video, audio, port_lo, port_hi, test=not store_exe).encode("utf-8")
    check_manifest(man)

    if a.layout:
        lay = os.path.abspath(a.layout)
        if os.path.exists(lay):
            shutil.rmtree(lay)
        for rel in files:
            dst = os.path.join(lay, *rel.split("/"))
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copyfile(os.path.join(build, *rel.split("/")), dst)
        for rel, data in list(notes.items()) + list(assets.items()):
            dst = os.path.join(lay, *rel.split("/"))
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            open(dst, "wb").write(data)
        open(os.path.join(lay, "AppxManifest.xml"), "wb").write(man)
        print("layout:", lay)

    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    tmp = a.out + ".tmp"
    w = Writer(tmp, not a.no_compress)
    names = []
    for rel in files:
        w.add(rel, open(os.path.join(build, *rel.split("/")), "rb").read())
        names.append(rel)
    for rel, data in list(notes.items()) + list(assets.items()):
        w.add(rel, data)
        names.append(rel)
    w.add("AppxManifest.xml", man)
    names.append("AppxManifest.xml")
    w.add("AppxBlockMap.xml", w.blockmap_xml(), in_blockmap=False)
    w.add("[Content_Types].xml", content_types(names), in_blockmap=False)
    w.close()
    os.replace(tmp, a.out)
    digest = hashlib.sha256(open(a.out, "rb").read()).hexdigest()
    with open(a.out + ".sha256", "w", newline="\n") as f:
        f.write("%s  %s\n" % (digest, os.path.basename(a.out)))
    print("%s  %s  (%d files, %.1f MB, %s edition)" % (digest, a.out, len(names), os.path.getsize(a.out) / 1e6,
                                                        "Store" if store_exe else "GITHUB - test only"))


if __name__ == "__main__":
    main()
