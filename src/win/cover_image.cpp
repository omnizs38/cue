#include "cover_image.h"
#include "../core/image_trim.h"

#include <wincodec.h>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <thread>
#include <filesystem>
#include <fstream>

#include "../core/cover_art.h"

// A song's own cover (embedded or beside it), decoded by Windows' imaging codecs to BGRA,
// at most max_side px a side: big covers are scaled while decoding, not after. COM must be
// initialised on the calling thread.
bool cover_image(const std::string& path, std::vector<uint32_t>& px, int& w, int& h, int max_side) {
  if (!decode_image(cover_art::find(path), px, w, h, max_side)) return false;
  // a video's frame boxed in black bars: the phone shows the picture, not the bars
  crop_pixels(px, w, h, trim_dark_bars(px, w, h));
  return true;
}

bool decode_image(const std::string& encoded, std::vector<uint32_t>& px, int& w, int& h, int max_side) {
  if (encoded.empty()) return false;
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
    if (fw > 0 && fh > 0 && fw <= 16384 && fh <= 16384) {
      double k = std::min(1.0, (double)max_side / std::max(fw, fh));
      UINT tw = std::max(1u, (UINT)std::lround(fw * k)), th = std::max(1u, (UINT)std::lround(fh * k));
      IWICBitmapSource* src = frame;
      if ((tw != fw || th != fh) && SUCCEEDED(wic->CreateBitmapScaler(&scaler)) &&
          SUCCEEDED(scaler->Initialize(frame, tw, th, WICBitmapInterpolationModeFant)))
        src = scaler;
      else { tw = fw; th = fh; }
      if (SUCCEEDED(wic->CreateFormatConverter(&conv)) &&
          SUCCEEDED(conv->Initialize(src, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) {
        px.assign((size_t)tw * th, 0);
        if (SUCCEEDED(conv->CopyPixels(nullptr, tw * 4, (UINT)(px.size() * 4), reinterpret_cast<BYTE*>(px.data())))) {
          w = (int)tw; h = (int)th; ok = true;
        }
      }
    }
  }
  for (IUnknown* u : std::initializer_list<IUnknown*>{conv, scaler, frame, dec, stream, wic}) if (u) u->Release();
  return ok;
}


std::shared_ptr<CoverColorsJob> request_cover_colors(std::string path) {
  if (path.empty()) return {};
  try {
    auto job = std::make_shared<CoverColorsJob>();
    std::thread([job, path = std::move(path)] {
      SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
      HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
      try {
        std::vector<uint32_t> px;
        int w = 0, h = 0;
        job->bytes = cover_art::find(path);
        if (decode_image(job->bytes, px, w, h, 96)) {
          job->colors = analyze_frame(px.data(), w, h);
          job->found = job->colors.valid;
        }
      } catch (...) {}
      if (SUCCEEDED(co)) CoUninitialize();
      job->done.store(true);
    }).detach();
    return job;
  } catch (...) { return {}; }
}

std::shared_ptr<CardStillJob> request_card_still(std::wstring file) {
  try {
    auto job = std::make_shared<CardStillJob>();
    std::thread([job, file = std::move(file)] {
      SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
      HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
      try {
        std::string data;
        {
          std::ifstream f(std::filesystem::path(file), std::ios::binary | std::ios::ate);
          auto n = f ? (long long)f.tellg() : -1;
          if (n > 0 && n < (16 << 20)) { data.resize((size_t)n); f.seekg(0); f.read(data.data(), n); if (!f) data.clear(); }
        }
        std::error_code ec;
        std::filesystem::remove(std::filesystem::path(file), ec);
        std::vector<uint32_t> px;
        int w = 0, h = 0;
        if (decode_image(data, px, w, h, 64)) {
          double sum = 0;
          for (uint32_t v : px) sum += ((v >> 16) & 255) * .2126 + ((v >> 8) & 255) * .7152 + (v & 255) * .0722;
          if (!px.empty() && sum / px.size() > 18) job->bytes = std::move(data);  // not a black frame
        }
      } catch (...) {}
      if (SUCCEEDED(co)) CoUninitialize();
      job->done.store(true);
    }).detach();
    return job;
  } catch (...) { return {}; }
}
