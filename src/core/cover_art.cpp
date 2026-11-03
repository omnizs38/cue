#include "cover_art.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace cover_art {
namespace {

// ── sources: a file read in bounded pieces, or bytes already in memory ──
struct Src {
  virtual ~Src() = default;
  virtual uint64_t size() const = 0;
  // exactly n bytes at off, or false
  virtual bool read(uint64_t off, size_t n, std::string& out) = 0;
};
struct MemSrc final : Src {
  std::string_view b;
  explicit MemSrc(std::string_view v) : b(v) {}
  uint64_t size() const override { return b.size(); }
  bool read(uint64_t off, size_t n, std::string& out) override {
    if (off > b.size() || n > b.size() - off) return false;
    out.assign(b.substr((size_t)off, n));
    return true;
  }
};
std::filesystem::path fs_path(const std::string& s) { return std::filesystem::path(std::u8string(s.begin(), s.end())); }
struct FileSrc final : Src {
  std::ifstream f;
  uint64_t n = 0;
  explicit FileSrc(const std::filesystem::path& p) : f(p, std::ios::binary) {
    std::error_code ec;
    auto s = std::filesystem::file_size(p, ec);
    n = ec ? 0 : (uint64_t)s;
  }
  uint64_t size() const override { return n; }
  bool read(uint64_t off, size_t len, std::string& out) override {
    if (!f || off > n || len > n - off || len > kMaxCoverBytes) return false;
    out.resize(len);
    f.clear();
    f.seekg((std::streamoff)off);
    return (bool)f.read(out.data(), (std::streamsize)len);
  }
};

uint32_t be32(std::string_view s, size_t o) {
  return (uint32_t)(uint8_t)s[o] << 24 | (uint32_t)(uint8_t)s[o + 1] << 16 | (uint32_t)(uint8_t)s[o + 2] << 8 | (uint8_t)s[o + 3];
}
uint32_t be24(std::string_view s, size_t o) { return (uint32_t)(uint8_t)s[o] << 16 | (uint32_t)(uint8_t)s[o + 1] << 8 | (uint8_t)s[o + 2]; }
uint32_t le32(std::string_view s, size_t o) {
  return (uint32_t)(uint8_t)s[o] | (uint32_t)(uint8_t)s[o + 1] << 8 | (uint32_t)(uint8_t)s[o + 2] << 16 | (uint32_t)(uint8_t)s[o + 3] << 24;
}
uint32_t syncsafe(std::string_view s, size_t o) {
  return (uint32_t)(s[o] & 0x7f) << 21 | (uint32_t)(s[o + 1] & 0x7f) << 14 | (uint32_t)(s[o + 2] & 0x7f) << 7 | (uint32_t)(s[o + 3] & 0x7f);
}
// ID3 unsynchronisation: every FF 00 stands for FF
std::string unsync(std::string_view s) {
  std::string o;
  o.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    o += s[i];
    if ((uint8_t)s[i] == 0xff && i + 1 < s.size() && s[i + 1] == 0) ++i;
  }
  return o;
}
// a picture that looks like an image the decoder can try
bool looks_like_image(std::string_view d) {
  if (d.size() < 16) return false;
  auto u = [&](size_t i) { return (uint8_t)d[i]; };
  if (u(0) == 0xff && u(1) == 0xd8) return true;                                   // JPEG
  if (d.substr(0, 8) == std::string_view("\x89PNG\r\n\x1a\n", 8)) return true;     // PNG
  if (d.substr(0, 4) == "RIFF" && d.substr(8, 4) == "WEBP") return true;           // WebP
  if (d.substr(0, 3) == "GIF" || d.substr(0, 2) == "BM") return true;              // GIF, BMP
  return false;
}

// Keeps the best picture seen: the front cover (type 3) wins, else the first one.
struct Best {
  std::string data;
  bool front = false;
  void offer(std::string_view d, int type) {
    if (!looks_like_image(d)) return;
    bool f = type == 3;
    if (data.empty() || (f && !front)) { data.assign(d); front = f; }
  }
  bool done() const { return front; }
};

// ── ID3v2 (MP3, and the ID3 block some FLAC/AAC files start with) ──
// a text terminator for the given ID3 encoding, from `at`; returns the index after it
size_t skip_text(std::string_view s, size_t at, int enc) {
  if (enc == 1 || enc == 2) {
    for (size_t i = at; i + 1 < s.size(); i += 2) if (s[i] == 0 && s[i + 1] == 0) return i + 2;
    return std::string_view::npos;
  }
  size_t z = s.find('\0', at);
  return z == std::string_view::npos ? z : z + 1;
}
void id3_frames(std::string_view tag, int ver, Best& best) {
  size_t p = 0;
  const size_t head = ver == 2 ? 6 : 10;
  while (p + head <= tag.size() && !best.done()) {
    if (tag[p] == 0) break;  // padding
    std::string_view id = tag.substr(p, ver == 2 ? 3 : 4);
    uint32_t len = ver == 2 ? be24(tag, p + 3) : ver == 4 ? syncsafe(tag, p + 4) : be32(tag, p + 4);
    uint8_t fl2 = ver == 2 ? 0 : (uint8_t)tag[p + 9];
    p += head;
    if (len > tag.size() - p) break;
    std::string_view body = tag.substr(p, len);
    p += len;
    bool pic = (ver == 2 && id == "PIC") || (ver > 2 && id == "APIC");
    if (!pic) continue;
    std::string tmp;
    if (ver == 3) {
      if (fl2 & 0xc0) continue;                         // compressed or encrypted
      if (fl2 & 0x20) { if (body.empty()) continue; body.remove_prefix(1); }  // group id
    } else if (ver == 4) {
      if (fl2 & 0x0c) continue;                         // compressed or encrypted
      if (fl2 & 0x40) { if (body.empty()) continue; body.remove_prefix(1); }
      if (fl2 & 0x01) { if (body.size() < 4) continue; body.remove_prefix(4); }  // data length
      if (fl2 & 0x02) { tmp = unsync(body); body = tmp; }
    }
    if (body.size() < 4) continue;
    int enc = (uint8_t)body[0];
    size_t at;
    if (ver == 2) at = 4;                               // enc, "JPG", type
    else { at = body.find('\0', 1); if (at == std::string_view::npos) continue; ++at; }  // mime
    if (at >= body.size()) continue;
    int type = (uint8_t)body[at];
    size_t d = skip_text(body, at + 1, enc);
    if (d == std::string_view::npos || d > body.size()) continue;
    best.offer(body.substr(d), type);
  }
}
// returns the tag's total size (header included) when an ID3v2 tag starts at 0
uint64_t id3(Src& src, Best& best) {
  std::string h;
  if (!src.read(0, 10, h) || h.compare(0, 3, "ID3") != 0) return 0;
  int ver = (uint8_t)h[3], flags = (uint8_t)h[5];
  uint32_t size = syncsafe(h, 6);
  uint64_t total = 10ull + size + ((flags & 0x10) ? 10 : 0);
  if (ver < 2 || ver > 4) return total;
  std::string tag;
  if (!src.read(10, std::min<uint64_t>(size, src.size() - 10), tag)) return total;
  if ((flags & 0x80) && ver < 4) tag = unsync(tag);
  std::string_view t = tag;
  if ((flags & 0x40) && ver == 3 && t.size() >= 4) {      // extended header
    uint32_t ext = be32(t, 0);
    if (ext + 4 > t.size()) return total;
    t.remove_prefix(ext + 4);
  } else if ((flags & 0x40) && ver == 4 && t.size() >= 4) {
    uint32_t ext = syncsafe(t, 0);
    if (ext > t.size()) return total;
    t.remove_prefix(ext);
  }
  id3_frames(t, ver, best);
  return total;
}

// ── FLAC: a PICTURE block (also the payload of Ogg's METADATA_BLOCK_PICTURE) ──
void flac_picture(std::string_view b, Best& best) {
  if (b.size() < 32) return;
  int type = (int)be32(b, 0);
  size_t p = 4;
  uint32_t ml = be32(b, p); p += 4; if (ml > b.size() - p) return; p += ml;
  if (b.size() - p < 4) return;
  uint32_t dl = be32(b, p); p += 4; if (dl > b.size() - p) return; p += dl;
  if (b.size() - p < 20) return;
  p += 16;                                               // width, height, depth, colours
  uint32_t n = be32(b, p); p += 4;
  if (n > b.size() - p) return;
  best.offer(b.substr(p, n), type);
}
void flac(Src& src, uint64_t at, Best& best) {
  std::string h;
  if (!src.read(at, 4, h) || h != "fLaC") return;
  uint64_t p = at + 4;
  for (int guard = 0; guard < 128 && !best.done(); ++guard) {
    if (!src.read(p, 4, h)) return;
    bool last = (uint8_t)h[0] & 0x80;
    int type = (uint8_t)h[0] & 0x7f;
    uint32_t len = be24(h, 1);
    p += 4;
    if (type == 6) { std::string b; if (src.read(p, len, b)) flac_picture(b, best); }
    p += len;
    if (last) return;
  }
}

// ── Ogg (Vorbis, Opus): the comment packet's METADATA_BLOCK_PICTURE ──
std::string base64(std::string_view s) {
  static int8_t map[256];
  static bool init = [] {
    std::fill(std::begin(map), std::end(map), (int8_t)-1);
    const char* a = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; i < 64; ++i) map[(uint8_t)a[i]] = (int8_t)i;
    return true;
  }();
  (void)init;
  std::string o;
  o.reserve(s.size() * 3 / 4);
  uint32_t acc = 0; int bits = 0;
  for (char c : s) {
    int v = map[(uint8_t)c];
    if (v < 0) continue;
    acc = acc << 6 | (uint32_t)v; bits += 6;
    if (bits >= 8) { bits -= 8; o += (char)((acc >> bits) & 0xff); }
  }
  return o;
}
void vorbis_comments(std::string_view c, Best& best) {
  size_t p = 0;
  if (c.substr(0, 8) == "OpusTags") p = 8;
  else if (c.size() >= 7 && c[0] == 3 && c.substr(1, 6) == "vorbis") p = 7;
  else return;
  if (c.size() - p < 4) return;
  uint32_t vl = le32(c, p); p += 4; if (vl > c.size() - p) return; p += vl;
  if (c.size() - p < 4) return;
  uint32_t n = le32(c, p); p += 4;
  for (uint32_t i = 0; i < n && c.size() - p >= 4 && !best.done(); ++i) {
    uint32_t l = le32(c, p); p += 4;
    if (l > c.size() - p) return;
    std::string_view kv = c.substr(p, l);
    p += l;
    size_t eq = kv.find('=');
    if (eq == std::string_view::npos) continue;
    std::string key(kv.substr(0, eq));
    for (auto& ch : key) ch = (char)std::toupper((unsigned char)ch);
    if (key == "METADATA_BLOCK_PICTURE") flac_picture(base64(kv.substr(eq + 1)), best);
    else if (key == "COVERART") best.offer(base64(kv.substr(eq + 1)), -1);
  }
}
void ogg(Src& src, Best& best) {
  uint64_t p = 0;
  uint32_t serial = 0;
  bool have_serial = false;
  int packet = 0;
  std::string cur;
  for (int pages = 0; pages < 4096; ++pages) {
    std::string h;
    if (!src.read(p, 27, h) || h.compare(0, 4, "OggS") != 0) return;
    uint32_t s = le32(h, 14);
    int nseg = (uint8_t)h[26];
    std::string segs;
    if (!src.read(p + 27, (size_t)nseg, segs)) return;
    uint64_t body = p + 27 + (uint64_t)nseg;
    uint64_t total = 0;
    for (char x : segs) total += (uint8_t)x;
    if (!have_serial) { serial = s; have_serial = true; }
    if (s == serial) {
      uint64_t q = body;
      for (int i = 0; i < nseg; ++i) {
        size_t len = (uint8_t)segs[(size_t)i];
        if (packet == 1) {
          std::string piece;
          if (!src.read(q, len, piece)) return;
          cur += piece;
          if (cur.size() > kMaxCoverBytes) return;
        }
        q += len;
        if (len < 255) {                                 // a packet ends here
          if (packet == 1) { vorbis_comments(cur, best); return; }
          ++packet;
        }
      }
    }
    p = body + total;
  }
}

// ── MP4 / M4A: moov/udta/meta/ilst/covr/data ──
struct Box { uint64_t at, size, head; std::string type; };
bool box_at(std::string_view b, size_t at, Box& out) {
  if (b.size() - at < 8) return false;
  uint64_t sz = be32(b, at);
  uint64_t head = 8;
  if (sz == 1) {
    if (b.size() - at < 16) return false;
    sz = (uint64_t)be32(b, at + 8) << 32 | be32(b, at + 12);
    head = 16;
  } else if (sz == 0) sz = b.size() - at;
  if (sz < head || sz > b.size() - at) return false;
  out = {at, sz, head, std::string(b.substr(at + 4, 4))};
  return true;
}
// the children of a box's body [from, to)
void mp4_walk(std::string_view b, size_t from, size_t to, int depth, Best& best) {
  size_t p = from;
  Box x;
  while (p < to && depth < 8 && !best.done() && box_at(b.substr(0, to), p, x)) {
    size_t body = (size_t)(x.at + x.head), end = (size_t)(x.at + x.size);
    if (x.type == "udta" || x.type == "ilst" || x.type == "covr") mp4_walk(b, body, end, depth + 1, best);
    else if (x.type == "meta") mp4_walk(b, std::min(end, body + 4), end, depth + 1, best);  // full box
    else if (x.type == "data" && end - body > 8) best.offer(b.substr(body + 8, end - body - 8), 3);
    p = end;
  }
}
void mp4(Src& src, Best& best) {
  uint64_t p = 0;
  for (int guard = 0; guard < 256 && p + 8 <= src.size(); ++guard) {
    std::string h;
    if (!src.read(p, 8, h)) return;
    uint64_t sz = be32(h, 0), head = 8;
    if (sz == 1) { std::string l; if (!src.read(p + 8, 8, l)) return; sz = (uint64_t)be32(l, 0) << 32 | be32(l, 4); head = 16; }
    else if (sz == 0) sz = src.size() - p;
    if (sz < head) return;
    if (h.compare(4, 4, "moov") == 0) {
      if (sz > kMaxCoverBytes) return;
      std::string moov;
      if (!src.read(p, (size_t)sz, moov)) return;
      mp4_walk(moov, (size_t)head, moov.size(), 0, best);
      return;
    }
    p += sz;
  }
}

std::string from_src(Src& src) {
  Best best;
  uint64_t after = id3(src, best);
  if (!best.data.empty()) return best.data;
  std::string h;
  if (!src.read(after, std::min<uint64_t>(12, src.size() - std::min(after, src.size())), h) || h.size() < 4) return {};
  if (h.compare(0, 4, "fLaC") == 0) flac(src, after, best);
  else if (h.compare(0, 4, "OggS") == 0 && after == 0) ogg(src, best);
  else if (h.size() >= 8 && h.compare(4, 4, "ftyp") == 0 && after == 0) mp4(src, best);
  return best.data;
}

std::string lower(std::string s) {
  for (auto& c : s) c = (char)std::tolower((unsigned char)c);
  return s;
}

}  // namespace

std::string embedded_in(std::string_view bytes) {
  MemSrc m(bytes);
  return from_src(m);
}

std::string embedded_in_file(const std::string& utf8_path) {
  FileSrc f(fs_path(utf8_path));
  if (!f.size()) return {};
  return from_src(f);
}

std::string beside(const std::string& utf8_path) {
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::path p = fs_path(utf8_path);
  fs::path dir = p.parent_path();
  auto u8 = [](const fs::path& x) { auto s = x.u8string(); return std::string(s.begin(), s.end()); };
  std::string stem = lower(u8(p.stem()));
  static const char* exts[] = {".jpg", ".jpeg", ".png", ".webp"};
  static const char* names[] = {"cover", "folder", "front", "album", "albumart"};
  // rank: 0 the song's own picture, 1+ a folder picture by name order
  int best_rank = 1 << 30;
  fs::path best;
  int seen = 0;
  for (fs::directory_iterator it(dir, ec), end; !ec && it != end && seen < 4000; it.increment(ec), ++seen) {
    const fs::path& q = it->path();
    std::string e = lower(u8(q.extension()));
    if (std::find(std::begin(exts), std::end(exts), e) == std::end(exts)) continue;
    std::string s = lower(u8(q.stem()));
    int rank = -1;
    if (s == stem) rank = 0;
    else for (int i = 0; i < 5; ++i) if (s == names[i]) { rank = 1 + i; break; }
    if (rank < 0 || rank >= best_rank) continue;
    std::error_code fe;
    auto sz = fs::file_size(q, fe);
    if (fe || sz == 0 || sz > kMaxCoverBytes) continue;
    best_rank = rank;
    best = q;
  }
  if (best.empty()) return {};
  std::ifstream f(best, std::ios::binary);
  std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  return looks_like_image(data) ? data : std::string();
}

std::string find(const std::string& utf8_path) {
  std::string d = embedded_in_file(utf8_path);
  return d.empty() ? beside(utf8_path) : d;
}

}  // namespace cover_art
