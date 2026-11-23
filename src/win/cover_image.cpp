#include "cover_image.h"
#include "../core/detached_thread.h"
#include "../core/image_trim.h"
#include "../core/bounded_worker.h"
#include "../core/cover_cache.h"

#include <wincodec.h>

// The decoder's own reduced decoding (a JPEG at 1/2, 1/4, 1/8): in the Windows SDK's wincodec.h,
// missing from MinGW's. The same interface, its methods in the SDK's order.
#if !defined(__IWICBitmapSourceTransform_INTERFACE_DEFINED__)
MIDL_INTERFACE("3b16811b-6a43-4ec9-b713-3d5a0c13b940")
IWICBitmapSourceTransform : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE CopyPixels(const WICRect* prc, UINT uiWidth, UINT uiHeight, WICPixelFormatGUID* pguidDstFormat,
                                               WICBitmapTransformOptions dstTransform, UINT nStride, UINT cbBufferSize, BYTE* pbBuffer) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetClosestSize(UINT* puiWidth, UINT* puiHeight) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetClosestPixelFormat(WICPixelFormatGUID* pguidDstFormat) = 0;
  virtual HRESULT STDMETHODCALLTYPE DoesSupportTransform(WICBitmapTransformOptions dstTransform, BOOL* pfIsSupported) = 0;
};
__CRT_UUID_DECL(IWICBitmapSourceTransform, 0x3b16811b, 0x6a43, 0x4ec9, 0xb7, 0x13, 0x3d, 0x5a, 0x0c, 0x13, 0xb9, 0x40)
#endif

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
  IWICBitmapSourceTransform* xf = nullptr;
  IWICBitmap* reduced = nullptr;
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
      bool needs_scale = (tw != fw || th != fh);
      bool scaler_failed = false;
      // A JPEG (most covers) can be decoded straight at 1/2, 1/4 or 1/8 of its size: a 3000 px
      // cover read for a 192 px look used to be decoded whole first (tens of ms, and the song's
      // opening waits for that read). The decoder's own reduction, at least the size wanted, then
      // the same smooth scaling as before on the much smaller picture. Anything unexpected: the
      // whole picture, as before.
      if (needs_scale && SUCCEEDED(frame->QueryInterface(IID_PPV_ARGS(&xf)))) {
        UINT sw = 0, sh = 0;
        WICPixelFormatGUID f = GUID_WICPixelFormat32bppBGRA;
        // (the closest size it can make may fall below the one asked: then a larger one is asked)
        bool sized = false;
        for (UINT k = 1; k <= 8 && !sized; k *= 2) {
          sw = std::min(fw, tw * k); sh = std::min(fh, th * k);
          sized = SUCCEEDED(xf->GetClosestSize(&sw, &sh)) && sw >= tw && sh >= th;
        }
        if (sized && sw < fw && sh < fh &&
            (uint64_t)sw * sh <= 4096ull * 4096ull && SUCCEEDED(xf->GetClosestPixelFormat(&f))) {
          const UINT bpp = IsEqualGUID(f, GUID_WICPixelFormat32bppBGRA) || IsEqualGUID(f, GUID_WICPixelFormat32bppBGR) ? 32
                           : IsEqualGUID(f, GUID_WICPixelFormat24bppBGR) ? 24
                           : IsEqualGUID(f, GUID_WICPixelFormat8bppGray) ? 8 : 0;
          if (bpp) {
            const UINT stride = ((sw * bpp + 7) / 8 + 3) & ~3u;
            try {
              std::vector<BYTE> buf((size_t)stride * sh);
              if (SUCCEEDED(xf->CopyPixels(nullptr, sw, sh, &f, WICBitmapTransformRotate0, stride, (UINT)buf.size(), buf.data())) &&
                  SUCCEEDED(wic->CreateBitmapFromMemory(sw, sh, f, stride, (UINT)buf.size(), buf.data(), &reduced))) {
                src = reduced;
                needs_scale = sw != tw || sh != th;
              }
            } catch (...) {}
          }
        }
      }
      if (needs_scale) {
        // Fail closed when scaling is required but unavailable: falling back to the
        // full frame would allocate up to 16384x16384 pixels (~1 GiB) for one crafted cover.
        if (!SUCCEEDED(wic->CreateBitmapScaler(&scaler)) ||
            !SUCCEEDED(scaler->Initialize(src, tw, th, WICBitmapInterpolationModeFant)))
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
            for (IUnknown* u : std::initializer_list<IUnknown*>{conv, scaler, reduced, xf, frame, dec, stream, wic}) if (u) u->Release();
            return false;
          }
          if (SUCCEEDED(conv->CopyPixels(nullptr, tw * 4, (UINT)(px.size() * 4), reinterpret_cast<BYTE*>(px.data())))) {
            w = (int)tw; h = (int)th; ok = true;
          }
        }
      }
    }
  }
  for (IUnknown* u : std::initializer_list<IUnknown*>{conv, scaler, reduced, xf, frame, dec, stream, wic}) if (u) u->Release();
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

// the song's identity on disk (name, size, last change): a re-tagged file is read again. Asks the
// disk: only on a worker (see core/cover_cache.h)
std::string cover_key(const std::string& path) {
  WIN32_FILE_ATTRIBUTE_DATA a{};
  if (!GetFileAttributesExW(widen(path).c_str(), GetFileExInfoStandard, &a)) return {};
  return path + "\n" + std::to_string(((uint64_t)a.nFileSizeHigh << 32) | a.nFileSizeLow) + "\n" +
         std::to_string(((uint64_t)a.ftLastWriteTime.dwHighDateTime << 32) | a.ftLastWriteTime.dwLowDateTime);
}

std::mutex g_cover_mu;
CoverCache<CoverJob> g_covers;  // by path; identities checked on workers
constexpr size_t kCoversKept = CoverCache<CoverJob>::kKept;
double cover_clock() { return (double)GetTickCount64() / 1000.0; }
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
// the identity checks of covers already read (a disk asleep or gone holds up only these)
BoundedWorker& cover_checks() { static BoundedWorker worker(4); return worker; }
template<class Job> auto cancelled(std::weak_ptr<Job> weak) {
  return [weak] { if (auto job = weak.lock()) { job->cancelled = true; job->done.store(true); } };
}

}  // namespace

std::shared_ptr<CoverJob> request_cover(const std::string& path) {
  if (path.empty()) return {};
  try {
    // (18a, 17/11: no disk here. This runs on the window's thread when a song opens, is skipped
    // to, or the next one's cover is read ahead; asking the disk for the song's size and date
    // froze the window for as long as an external disk took to wake, or a dropped share to fail)
    // Lookup and insertion share one lock: concurrent callers cannot start
    // duplicate reads for the same file between the lookup and the cache insertion.
    std::unique_lock<std::mutex> lock(g_cover_mu);
    bool check = false;
    if (auto kept = g_covers.find(path, cover_clock(), check, [](const CoverJob& j) { return !(j.done.load() && j.cancelled); })) {
      lock.unlock();
      if (check) {
        // (the job is held while it is checked: its address names it in the cache)
        const CoverJob* id = kept.get();
        auto unchecked = [id, kept] { std::lock_guard<std::mutex> l(g_cover_mu); g_covers.unchecked(id); };
        try {
          cover_checks().submit({[path, id, kept] {
            const std::string identity = cover_key(path);
            std::lock_guard<std::mutex> l(g_cover_mu);
            g_covers.report(id, identity, cover_clock());  // (changed: the next request reads it again)
          }, unchecked});
        } catch (...) { unchecked(); }
      }
      return kept;
    }
    auto job = std::make_shared<CoverJob>();
    g_covers.add(path, job, cover_clock());
    lock.unlock();
    const std::weak_ptr<CoverJob> weak = job;
    try { cover_reads().submit({[weak, path] {
      auto job = weak.lock();
      if (!job) return;
      SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
      HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
      try {
        // the identity this read sees (before the read: a change during it is seen next time)
        const std::string identity = cover_key(path);
        { std::lock_guard<std::mutex> l(g_cover_mu); g_covers.report(job.get(), identity, cover_clock()); }
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
      job->done.store(true);
      // then, with the song already able to use the rest: the media card's small copy
      try {
        if (job->found && job->bytes.size() > (160u << 10)) {
          std::vector<uint32_t> px;
          int w = 0, h = 0;
          if (decode_image(job->bytes, px, w, h, 512) && w > 0 && h > 0) {
            std::string small = encode_jpeg_bgra(px, w, h);
            if (!small.empty() && small.size() < job->bytes.size()) job->card = std::move(small);
          }
        }
      } catch (...) {}
      job->card_done.store(true);
      if (SUCCEEDED(co)) CoUninitialize();
    }, [weak] { if (auto job = weak.lock()) { job->cancelled = true; job->done.store(true); job->card_done.store(true); } }}); }
    catch (...) { job->cancelled = true; job->done.store(true); job->card_done.store(true); return {}; }
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

std::string encode_jpeg_bgra(const std::vector<uint32_t>& px, int w, int h) {
  if (px.empty() || w <= 0 || h <= 0 || px.size() != (size_t)w * h) return {};
  return encode_jpeg(px, w, h);
}

std::shared_ptr<CardStillJob> request_card_still(std::vector<uint32_t> px, int w, int h) {
  if (px.empty() || w <= 0 || h <= 0 || px.size() != (size_t)w * h) return {};
  try {
    auto job = std::make_shared<CardStillJob>();
    start_detached([job, px = std::move(px), w, h] {
      SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
      HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
      try {
        double sum = 0;
        for (uint32_t v : px) sum += ((v >> 16) & 255) * .2126 + ((v >> 8) & 255) * .7152 + (v & 255) * .0722;
        if (sum / px.size() > 18) job->bytes = encode_jpeg(px, w, h);  // not a black frame
      } catch (...) {}
      if (SUCCEEDED(co)) CoUninitialize();
      job->done.store(true);
    });
    return job;
  } catch (...) { return {}; }
}
