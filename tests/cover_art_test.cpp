// Covers found without the engine: ID3v2 (2.2, 2.3, 2.4 with unsynchronisation), FLAC,
// Ogg Opus (a comment packet across pages), MP4 (moov after mdat), and pictures beside a song.
#include "../src/core/image_trim.h"
#include "../src/core/cover_art.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

static std::string be32(uint32_t v) { return {(char)(v >> 24), (char)(v >> 16), (char)(v >> 8), (char)v}; }
static std::string be24(uint32_t v) { return {(char)(v >> 16), (char)(v >> 8), (char)v}; }
static std::string le32(uint32_t v) { return {(char)v, (char)(v >> 8), (char)(v >> 16), (char)(v >> 24)}; }
static std::string ss(uint32_t v) { return {(char)((v >> 21) & 0x7f), (char)((v >> 14) & 0x7f), (char)((v >> 7) & 0x7f), (char)(v & 0x7f)}; }
static std::string jpeg(char fill, size_t n = 40) { std::string s("\xff\xd8\xff\xe0", 4); s += std::string(n, fill); s += "\xff\xd9"; return s; }
static std::string png(char fill) { std::string s("\x89PNG\r\n\x1a\n", 8); s += std::string(40, fill); return s; }
static std::string b64(const std::string& in) {
  const char* a = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string o; size_t i = 0;
  for (; i + 2 < in.size(); i += 3) {
    uint32_t v = (uint8_t)in[i] << 16 | (uint8_t)in[i + 1] << 8 | (uint8_t)in[i + 2];
    o += a[v >> 18]; o += a[(v >> 12) & 63]; o += a[(v >> 6) & 63]; o += a[v & 63];
  }
  if (i + 1 == in.size()) { uint32_t v = (uint8_t)in[i] << 16; o += a[v >> 18]; o += a[(v >> 12) & 63]; o += "=="; }
  else if (i + 2 == in.size()) { uint32_t v = (uint8_t)in[i] << 16 | (uint8_t)in[i + 1] << 8; o += a[v >> 18]; o += a[(v >> 12) & 63]; o += a[(v >> 6) & 63]; o += '='; }
  return o;
}
static std::string flac_pic(int type, const std::string& img) {
  return be32(type) + be32(10) + "image/jpeg" + be32(0) + be32(500) + be32(500) + be32(24) + be32(0) + be32((uint32_t)img.size()) + img;
}
static void write(const std::filesystem::path& p, const std::string& d) { std::ofstream(p, std::ios::binary).write(d.data(), (std::streamsize)d.size()); }

int main() {
  using cover_art::embedded_in;
  const std::string front = jpeg('F'), back = jpeg('B');

  // ID3v2.3: a back cover first, then the front cover, which wins; padding after
  {
    std::string apic_b = std::string("\0", 1) + "image/jpeg" + std::string("\0", 1) + "\x04" + "back" + std::string("\0", 1) + back;
    std::string apic_f = std::string("\0", 1) + "image/jpeg" + std::string("\0", 1) + "\x03" + std::string("\0", 1) + front;
    std::string frames = "TIT2" + be32(6) + std::string("\0\0", 2) + std::string("\0song!", 6) +
                         "APIC" + be32((uint32_t)apic_b.size()) + std::string("\0\0", 2) + apic_b +
                         "APIC" + be32((uint32_t)apic_f.size()) + std::string("\0\0", 2) + apic_f + std::string(64, '\0');
    std::string tag = std::string("ID3\x03\x00\x00", 6) + ss((uint32_t)frames.size()) + frames + "\xff\xfbmp3 audio";
    assert(embedded_in(tag) == front);
  }
  // ID3v2.4: UTF-16 description, frame-level unsynchronisation, data length indicator
  {
    std::string img = jpeg('\xff', 24);  // FF bytes that unsynchronisation must restore
    std::string body = std::string("\x01", 1) + "image/jpeg" + std::string("\0", 1) + "\x03" + std::string("\xff\xfe" "c\0v\0" "\0\0", 8) + img;
    std::string syn;
    for (size_t i = 0; i < body.size(); ++i) { syn += body[i]; if ((uint8_t)body[i] == 0xff) syn += '\0'; }
    std::string data = ss((uint32_t)body.size()) + syn;
    std::string frames = "APIC" + ss((uint32_t)data.size()) + std::string("\0\x03", 2) + data;
    std::string tag = std::string("ID3\x04\x00\x00", 6) + ss((uint32_t)frames.size()) + frames;
    assert(embedded_in(tag) == img);
  }
  // ID3v2.2 PIC
  {
    std::string pic = std::string("\0", 1) + "PNG" + "\x03" + std::string("\0", 1) + png('2');
    std::string frames = "PIC" + be24((uint32_t)pic.size()) + pic;
    std::string tag = std::string("ID3\x02\x00\x00", 6) + ss((uint32_t)frames.size()) + frames;
    assert(embedded_in(tag) == png('2'));
  }
  // not a picture: a tag whose APIC holds text is no cover
  {
    std::string apic = std::string("\0", 1) + "-->" + std::string("\0", 1) + "\x03" + std::string("\0", 1) + "http://example/cover.jpg";
    std::string frames = "APIC" + be32((uint32_t)apic.size()) + std::string("\0\0", 2) + apic;
    std::string tag = std::string("ID3\x03\x00\x00", 6) + ss((uint32_t)frames.size()) + frames;
    assert(embedded_in(tag).empty());
  }
  // FLAC: STREAMINFO, then PICTURE (last block); and one behind an ID3 tag
  {
    std::string pic = flac_pic(3, front);
    std::string f = "fLaC" + std::string("\x00", 1) + be24(34) + std::string(34, '\x11') + std::string("\x86", 1) + be24((uint32_t)pic.size()) + pic + "frames";
    assert(embedded_in(f) == front);
    std::string id3 = std::string("ID3\x03\x00\x00", 6) + ss(16) + std::string(16, '\0');
    assert(embedded_in(id3 + f) == front);
  }
  // Ogg Opus: the comment packet spans pages (a ~90 KB picture)
  {
    std::string big = jpeg('O', 90000);
    std::string comment = "METADATA_BLOCK_PICTURE=" + b64(flac_pic(3, big));
    std::string tags = "OpusTags" + le32(5) + "cue!!" + le32(2) + le32(9) + "TITLE=abc" + le32((uint32_t)comment.size()) + comment;
    std::string head = "OpusHead" + std::string(11, '\x01');
    auto pages = [](uint32_t serial, int& seq, const std::string& packet) {
      std::string out; size_t at = 0; bool first = true;
      while (true) {
        std::string segs, body;
        while (segs.size() < 255) {
          size_t n = std::min<size_t>(255, packet.size() - at);
          segs += (char)n; body += packet.substr(at, n); at += n;
          if (n < 255) break;
        }
        bool done = (uint8_t)segs.back() < 255;
        out += "OggS" + std::string(1, '\0') + std::string(1, first ? '\0' : '\x01') + std::string(8, '\0') + le32(serial) + le32((uint32_t)seq++) + le32(0) + std::string(1, (char)segs.size()) + segs + body;
        first = false;
        if (done) break;
      }
      return out;
    };
    int seq = 0;
    std::string file = pages(77, seq, head) + pages(77, seq, tags) + pages(77, seq, std::string(100, 'a'));
    assert(embedded_in(file) == big);
  }
  // MP4: ftyp, mdat, then moov/udta/meta/ilst/covr/data
  {
    auto box = [](const std::string& type, const std::string& body) { return be32((uint32_t)(8 + body.size())) + type + body; };
    std::string data = box("data", be32(13) + be32(0) + front);
    std::string ilst = box("ilst", box("\xa9nam", box("data", be32(1) + be32(0) + "song")) + box("covr", data));
    std::string meta = box("meta", std::string(4, '\0') + box("hdlr", std::string(25, '\0')) + ilst);
    std::string moov = box("moov", box("mvhd", std::string(100, '\0')) + box("udta", meta));
    std::string file = box("ftyp", "M4A " + std::string(4, '\0')) + box("mdat", std::string(5000, 'x')) + moov;
    assert(embedded_in(file) == front);
  }
  // pictures beside a song: its own "<name>.jpg" beats cover.png; then the folder's cover
  {
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "cue_cover_art_test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    write(dir / "Song One.mp3", "\xff\xfbno tags here");
    write(dir / "Song Two.opus", "OggSbroken");
    write(dir / "Cover.PNG", png('c'));
    write(dir / "song one.jpg", jpeg('1'));
    write(dir / "notes.txt", "x");
    auto u8 = [](const fs::path& p) { auto s = p.u8string(); return std::string(s.begin(), s.end()); };
    assert(cover_art::find(u8(dir / "Song One.mp3")) == jpeg('1'));
    assert(cover_art::find(u8(dir / "Song Two.opus")) == png('c'));
    // an embedded picture wins over the folder's
    std::string pic = flac_pic(3, front);
    write(dir / "Three.flac", "fLaC" + std::string("\x86", 1) + be24((uint32_t)pic.size()) + pic);
    assert(cover_art::find(u8(dir / "Three.flac")) == front);
    fs::remove(dir / "Cover.PNG");
    assert(cover_art::find(u8(dir / "Song Two.opus")).empty());
    assert(cover_art::find(u8(dir / "missing.mp3")).empty());
    fs::remove_all(dir);
  }
  // truncated and hostile inputs never read past the end
  {
    std::string tag = std::string("ID3\x03\x00\x00", 6) + ss(1000) + "APIC" + be32(900);
    assert(embedded_in(tag).empty());
    assert(embedded_in("fLaC\x06\xff\xff\xff").empty());
    assert(embedded_in(std::string("\0\0\0\x08" "ftyp", 8)).empty());
    for (size_t n = 0; n < 64; ++n) (void)embedded_in(std::string(n, '\xff'));
  }
  // a video's frame boxed in black bars is cut out of them; dark art is left alone
  {
    auto pic = [](int w, int h, auto f) { std::vector<uint32_t> px((size_t)w * h); for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) px[(size_t)y * w + x] = f(x, y); return px; };
    const uint32_t black = 0xff000000, sky = 0xff3070c0;
    // 400x400: a 16:9 frame (400x225) between bars of 87 and 88, a speck of noise in a bar
    auto boxed = pic(400, 400, [&](int x, int y) { return y >= 87 && y < 312 ? sky + (uint32_t)(x % 7) : (x == 200 && y == 10 ? 0xffffffffu : black); });
    TrimBox b = trim_dark_bars(boxed, 400, 400);
    assert(b.x == 0 && b.w == 400 && b.y == 87 && b.h == 225);
    int w = 400, h = 400; crop_pixels(boxed, w, h, b); assert(w == 400 && h == 225 && boxed.size() == 400u * 225 && boxed[0] == sky);
    // pillarbox too (a portrait frame in a square)
    auto pillar = pic(300, 300, [&](int x, int) { return x >= 75 && x < 225 ? sky : black; });
    b = trim_dark_bars(pillar, 300, 300); assert(b.x == 75 && b.w == 150 && b.y == 0 && b.h == 300);
    // a night sky above the art (dark at the top only): untouched
    auto night = pic(300, 300, [&](int, int y) { return y < 120 ? black : sky; });
    b = trim_dark_bars(night, 300, 300); assert(b.x == 0 && b.y == 0 && b.w == 300 && b.h == 300);
    // a thin black frame drawn into the art: untouched (thinner than a bar)
    auto framed = pic(300, 300, [&](int x, int y) { return x < 4 || y < 4 || x >= 296 || y >= 296 ? black : sky; });
    b = trim_dark_bars(framed, 300, 300); assert(b.w == 300 && b.h == 300);
    // all black: nothing left to show, untouched
    auto dark = pic(64, 64, [&](int, int) { return black; });
    b = trim_dark_bars(dark, 64, 64); assert(b.w == 64 && b.h == 64);
  }
  std::puts("cover art: passed");
}
