#include "cover_image.h"
#include "../core/image_trim.h"
#include "../core/bounded_worker.h"

#include <wincodec.h>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <thread>

#include "../core/cover_art.h"
#include "platform.h"

#include <deque>
#include <mutex>

// A song's own cover (embedded or beside it), decoded by Windows' imaging codecs to BGRA,
// at most max_side px a side: big covers are scaled while decoding, not after. COM must be
// initialised on the calling thread.
bool cover_image(const std::string& path, std::vector<uint32_t>& px, int& w, int& h, int max_side) {
  if (!decode_image(cover_art::find(path), px, w, h, max_side)) return false;
  // a video's frame boxed in black bars: the phone shows the picture, not the bars
  crop_pixels(px, w, h, trim_dark_bars(px, w, h));
  return true;
}

bool decode_image(const std::string& encoded, std::vector<uint32_t>& px, int& w, int& h, int max_side, int* full_w, int* full_h) {
  if (encoded.empty() || max_side <= 0) return false;
  // InitializeFromMemory takes a DWORD length: a larger input would be
  // silently truncated and decoded as a different (prefix) image.
  if (encoded.size() > 0xFFFFFFFFull) return false;
  std::string data = encoded;
  bool ok = false;
  IWICImagingFactory* wic = nullptr;
  IWICStream* stream = nullptr;
  IWICBitmapDecoder* dec = nullptr;
  IWICBitmapFrameDecode* frame = nullptr;
  IWICBitmapScaler* scaler = nullptr;
  IWICFormatConverter* conv = nullptr;
  if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic))) &&
      SUCCEEDED(wic->CreateStream(&stream)) &&
      SUCCEEDED(stream->InitializeFromMemory(reinterpret_cast<BYTE*>(data.data()), (DWORD)data.size())) &&
      SUCCEEDED(wic->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &dec)) &&
      SUCCEEDED(dec->GetFrame(0, &frame))) {
    UINT fw = 0, fh = 0;
    frame->GetSize(&fw, &fh);
    if (full_w) *full_w = (int)fw;
    if (full_h) *full_h = (int)fh;
    if (fw > 0 && fh > 0 && fw <= 16384 && fh <= 16384) {
      double k = std::min(1.0, (double)max_side / std::max(fw, fh));
      UINT tw = std::max(1u, (UINT)std::lround(fw * k)), th = std::max(1u, (UINT)std::lround(fh * k));
      IWICBitmapSource* src = frame;
      const bool needs_scale = (tw != fw || th != fh);
      bool scaler_failed = false;
      if (needs_scale) {
        // Fail closed when scaling is required but unavailable: falling back to the
        // full frame would allocate up to 16384x16384 pixels (~1 GiB) for one crafted cover.
        if (!SUCCEEDED(wic->CreateBitmapScaler(&scaler)) ||
            !SUCCEEDED(scaler->Initialize(frame, tw, th, WICBitmapInterpolationModeFant)))
          scaler_failed = true;
        else
          src = scaler;
      }
      // Defense in depth: never assign an unbounded pixel buffer. The intended
      // output is at most max_side px a side (k scales down to it), so the
      // pixel count can never legitimately exceed max_side^2; the absolute
      // 67M px (~256 MiB BGRA) ceiling additionally bounds large max_side
      // callers.
      const uint64_t cap = std::min<uint64_t>(1ull << 26, (uint64_t)max_side * (uint64_t)max_side);
      if (!scaler_failed && (uint64_t)tw * th <= cap) {
        if (SUCCEEDED(wic->CreateFormatConverter(&conv)) &&
            SUCCEEDED(conv->Initialize(src, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) {
          try {
            px.assign((size_t)tw * th, 0);
          } catch (...) {
            px.clear();
            for (IUnknown* u : std::initializer_list<IUnknown*>{conv, scaler, frame, dec, stream, wic}) if (u) u->Release();
            return false;
          }
          if (SUCCEEDED(conv->CopyPixels(nullptr, tw * 4, (UINT)(px.size() * 4), reinterpret_cast<BYTE*>(px.data())))) {
            w = (int)tw; h = (int)th; ok = true;
          }
        }
      }
    }
  }
  for (IUnknown* u : std::initializer_list<IUnknown*>{conv, scaler, frame, dec, stream, wic}) if (u) u->Release();
  return ok;
}


namespace {
// The picture at half its size (2x2 averages): the light reads the cover at ~96 px a side.
void half_size(std::vector<uint32_t>& px, int& w, int& h) {
  const int nw = w / 2, nh = h / 2;
  if (nw < 1 || nh < 1) return;
  std::vector<uint32_t> out((size_t)nw * nh);
  for (int y = 0; y < nh; ++y)
    for (int x = 0; x < nw; ++x) {
      const uint32_t q[4] = {px[(size_t)(2 * y) * w + 2 * x], px[(size_t)(2 * y) * w + 2 * x + 1],
                             px[(size_t)(2 * y + 1) * w + 2 * x], px[(size_t)(2 * y + 1) * w + 2 * x + 1]};
      uint32_t v = 0;
      for (int c = 0; c < 32; c += 8) {
        uint32_t sum = 2;
        for (uint32_t p : q) sum += (p >> c) & 255;
        v |= (sum / 4) << c;
      }
      out[(size_t)y * nw + x] = v;
    }
  px.swap(out); w = nw; h = nh;
}

// the song's identity on disk (name, size, last change): a re-tagged file is read again
std::string cover_key(const std::string& path) {
  WIN32_FILE_ATTRIBUTE_DATA a{};
  if (!GetFileAttributesExW(widen(path).c_str(), GetFileExInfoStandard, &a)) return {};
  return path + "\n" + std::to_string(((uint64_t)a.nFileSizeHigh << 32) | a.nFileSizeLow) + "\n" +
         std::to_string(((uint64_t)a.ftLastWriteTime.dwHighDateTime << 32) | a.ftLastWriteTime.dwLowDateTime);
}

std::mutex g_cover_mu;
std::deque<std::pair<std::string, std::shared_ptr<CoverJob>>> g_covers;  // newest first
constexpr size_t kCoversKept = 4;
// Weak entries reuse work already owned by the player/prefetch without retaining any images.
struct ArtRequest {
  std::weak_ptr<CoverJob> cover;
  std::weak_ptr<CoverArtJob> job;
  int side;
};
std::deque<ArtRequest> g_art_requests;
// Independent lanes: a large card decode cannot hold up the current file's
// small colour/crop read. Both sleep when idle; no thread per skip or resize.
BoundedWorker& cover_reads() { static BoundedWorker worker(4); return worker; }
BoundedWorker& art_decodes() { static BoundedWorker worker(4); return worker; }
template<class Job> auto cancelled(std::weak_ptr<Job> weak) {
  return [weak] { if (auto job = weak.lock()) { job->cancelled = true; job->done.store(true); } };
}

}  // namespace

std::shared_ptr<CoverJob> request_cover(const std::string& path) {
  if (path.empty()) return {};
  try {
    const std::string key = cover_key(path);
    // Lookup and insertion share one lock: concurrent callers cannot start
    // duplicate reads for the same file between the lookup and the cache insertion.
    std::unique_lock<std::mutex> lock(g_cover_mu);
    if (!key.empty()) {
      for (size_t i = 0; i < g_covers.size(); ++i)
        if (g_covers[i].first == key && !(g_covers[i].second->done.load() && g_covers[i].second->cancelled)) {
          auto job = g_covers[i].second;
          g_covers.erase(g_covers.begin() + (ptrdiff_t)i);
          g_covers.emplace_front(key, job);
          return job;
        }
    }
    auto job = std::make_shared<CoverJob>();
    if (!key.empty()) {
      g_covers.emplace_front(key, job);
      while (g_covers.size() > kCoversKept) g_covers.pop_back();
    }
    lock.unlock();
    const std::weak_ptr<CoverJob> weak = job;
    try { cover_reads().submit({[weak, path] {
      auto job = weak.lock();
      if (!job) return;
      SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
      HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
      try {
        std::vector<uint32_t> px;
        int w = 0, h = 0, fw = 0, fh = 0;
        job->bytes = cover_art::find(path);
        // 192 px a side is plenty to find bars a few percent tall, and quick to decode
        if (decode_image(job->bytes, px, w, h, 192, &fw, &fh) && w > 0 && h > 0) {
          TrimBox b = trim_dark_bars(px, w, h);
          job->x = (double)b.x / w; job->y = (double)b.y / h; job->w = (double)b.w / w; job->h = (double)b.h / h;
          // the scaled picture's edges are soft: a pixel in from each cut keeps no grey line
          if (b.w < w) { job->x += 1.0 / w; job->w -= 2.0 / w; }
          if (b.h < h) { job->y += 1.0 / h; job->h -= 2.0 / h; }
          job->full_w = fw; job->full_h = fh;
          job->found = true;
          // the light is the picture's, not its bars'
          crop_pixels(px, w, h, b);
          if (std::max(w, h) > 96) half_size(px, w, h);
          job->colors = analyze_frame(px.data(), w, h);
          job->lit = job->colors.valid;
        }
      } catch (...) {}
      if (SUCCEEDED(co)) CoUninitialize();
      job->done.store(true);
    }, cancelled(weak)}); }
    catch (...) { job->cancelled = true; job->done.store(true); return {}; }
    return job;
  } catch (...) { return {}; }
}

std::shared_ptr<CoverArtJob> request_cover_art(std::shared_ptr<CoverJob> cover, int side) {
  if (side <= 0) return {};
  try {
    std::unique_lock<std::mutex> lock(g_cover_mu);
    if (cover) for (auto it = g_art_requests.begin(); it != g_art_requests.end();) {
      auto owner = it->cover.lock();
      auto existing = it->job.lock();
      if (!owner || !existing) { it = g_art_requests.erase(it); continue; }
      if (owner == cover && it->side == side && !(existing->done.load() && existing->cancelled)) return existing;
      ++it;
    }
    auto job = std::make_shared<CoverArtJob>();
    job->side = side;
    if (cover) {
      g_art_requests.push_front({cover, job, side});
      while (g_art_requests.size() > kCoversKept) g_art_requests.pop_back();
    }
    lock.unlock();
    const std::weak_ptr<CoverArtJob> weak = job;
    try { art_decodes().submit({[weak, cover, side] {
      auto job = weak.lock();
      if (!job) return;
      SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
      HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
      try {
        std::shared_ptr<const CoverArt> art;
        if (cover) {
          // (the song's own read, usually done by now: a little patience, never forever)
          for (int i = 0; i < 1500 && !cover->done.load(); ++i) Sleep(2);
          if (cover->done.load() && cover->cancelled) {
            job->cancelled = true;
          }
          if (cover->done.load() && cover->found && !cover->bytes.empty()) {
            std::vector<uint32_t> px;
            int w = 0, h = 0;
            // decoded a little larger than the card needs (scaled while decoding), then cut and
            // brought to size in one smooth step
            const double share = std::max(0.05, std::min(cover->w, cover->h));
            const int max_side = std::clamp((int)std::lround(side * 2.0 / share), side, 4096);
            if (decode_image(cover->bytes, px, w, h, max_side) && w > 0 && h > 0) {
              auto a = std::make_shared<CoverArt>(cover_art_square(px.data(), w, h, side, cover->x, cover->y, cover->w, cover->h));
              if (a->ok()) art = a;
            }
          }
        }
        if (!job->cancelled) {
          if (!art) art = std::make_shared<CoverArt>(cover_art_tile(side));
          job->art = std::move(art);
        }
      } catch (...) {}
      if (SUCCEEDED(co)) CoUninitialize();
      job->done.store(true);
    }, cancelled(weak)}); }
    catch (...) { job->cancelled = true; job->done.store(true); return {}; }
    return job;
  } catch (...) { return {}; }
}

std::vector<uint32_t> shrink_bgr0(const uint8_t* data, int w, int h, int stride, int max_w, int max_h, int& out_w, int& out_h) {
  out_w = out_h = 0;
  if (!data || w < 2 || h < 2 || stride < w * 4) return {};
  const double k = std::min({1.0, (double)max_w / w, (double)max_h / h});
  const int ow = std::max(1, (int)std::lround(w * k)), oh = std::max(1, (int)std::lround(h * k));
  std::vector<uint32_t> out((size_t)ow * oh);
  for (int y = 0; y < oh; ++y) {
    const int sy = std::min(h - 2, (int)((y + 0.5) * h / oh));
    const uint8_t* r0 = data + (size_t)sy * stride;
    const uint8_t* r1 = r0 + stride;
    for (int x = 0; x < ow; ++x) {
      const int sx = std::min(w - 2, (int)((x + 0.5) * w / ow)) * 4;
      uint32_t v = 0xFF000000u;
      for (int c = 0; c < 3; ++c) {
        const unsigned sum = r0[sx + c] + r0[sx + 4 + c] + r1[sx + c] + r1[sx + 4 + c] + 2;
        v |= (sum / 4) << (8 * c);
      }
      out[(size_t)y * ow + x] = v;
    }
  }
  out_w = ow; out_h = oh;
  return out;
}

namespace {
// BGRA pixels to a JPEG in memory (Windows' own encoder), or "" if anything fails
std::string encode_jpeg(const std::vector<uint32_t>& px, int w, int h) {
  std::string out;
  IWICImagingFactory* wic = nullptr;
  IStream* stream = nullptr;
  IWICBitmapEncoder* enc = nullptr;
  IWICBitmapFrameEncode* frame = nullptr;
  IPropertyBag2* props = nullptr;
  std::vector<uint8_t> rgb((size_t)w * h * 3);
  for (size_t i = 0; i < (size_t)w * h; ++i) {
    rgb[i * 3] = (uint8_t)(px[i] & 255); rgb[i * 3 + 1] = (uint8_t)((px[i] >> 8) & 255); rgb[i * 3 + 2] = (uint8_t)((px[i] >> 16) & 255);
  }
  WICPixelFormatGUID fmt = GUID_WICPixelFormat24bppBGR;
  if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic))) &&
      SUCCEEDED(CreateStreamOnHGlobal(nullptr, TRUE, &stream)) &&
      SUCCEEDED(wic->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &enc)) &&
      SUCCEEDED(enc->Initialize(stream, WICBitmapEncoderNoCache)) &&
      SUCCEEDED(enc->CreateNewFrame(&frame, &props)) &&
      SUCCEEDED(frame->Initialize(props)) &&
      SUCCEEDED(frame->SetSize((UINT)w, (UINT)h)) &&
      SUCCEEDED(frame->SetPixelFormat(&fmt)) && IsEqualGUID(fmt, GUID_WICPixelFormat24bppBGR) &&
      SUCCEEDED(frame->WritePixels((UINT)h, (UINT)w * 3, (UINT)rgb.size(), rgb.data())) &&
      SUCCEEDED(frame->Commit()) && SUCCEEDED(enc->Commit())) {
    HGLOBAL mem = nullptr;
    if (SUCCEEDED(GetHGlobalFromStream(stream, &mem)) && mem) {
      STATSTG st{};
      if (SUCCEEDED(stream->Stat(&st, STATFLAG_NONAME)) && st.cbSize.QuadPart > 0 && st.cbSize.QuadPart < (16u << 20)) {
        if (const void* p = GlobalLock(mem)) {
          out.assign(static_cast<const char*>(p), (size_t)st.cbSize.QuadPart);
          GlobalUnlock(mem);
        }
      }
    }
  }
  for (IUnknown* u : std::initializer_list<IUnknown*>{props, frame, enc, stream, wic}) if (u) u->Release();
  return out;
}
}  // namespace

std::shared_ptr<CardStillJob> request_card_still(std::vector<uint32_t> px, int w, int h) {
  if (px.empty() || w <= 0 || h <= 0 || px.size() != (size_t)w * h) return {};
  try {
    auto job = std::make_shared<CardStillJob>();
    std::thread([job, px = std::move(px), w, h] {
      SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
      HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
      try {
        double sum = 0;
        for (uint32_t v : px) sum += ((v >> 16) & 255) * .2126 + ((v >> 8) & 255) * .7152 + (v & 255) * .0722;
        if (sum / px.size() > 18) job->bytes = encode_jpeg(px, w, h);  // not a black frame
      } catch (...) {}
      if (SUCCEEDED(co)) CoUninitialize();
      job->done.store(true);
    }).detach();
    return job;
  } catch (...) { return {}; }
}
