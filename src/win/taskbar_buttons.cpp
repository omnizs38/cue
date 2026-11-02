#include "taskbar_buttons.h"
#include <shobjidl.h>
#include <algorithm>
#include <cstring>
#include <vector>
#include "../ui/gfx.h"

TaskbarButtons::~TaskbarButtons() {
  if (list_) list_->Release();
  free_icons();
  if (com_) CoUninitialize();
}

void TaskbarButtons::free_icons() {
  for (HICON& i : icons_) if (i) { DestroyIcon(i); i = nullptr; }
}

// The dock's own pixel glyphs, white, whole pixels, centred: the same marks at any DPI.
static HICON glyph_icon(const char* name, int size) {
  const BitmapDef* b = bmp(name);
  if (!b) return nullptr;
  int k = std::max(1, size / 8);
  if (b->w * k > size) k = std::max(1, size / b->w);
  int ox = (size - b->w * k) / 2, oy = (size - b->h * k) / 2;
  std::vector<uint32_t> px((size_t)size * size, 0);
  for (int y = 0; y < b->h; ++y)
    for (int x = 0; x < b->w; ++x)
      if (b->cells[y * b->w + x] == '#')
        for (int yy = 0; yy < k; ++yy)
          for (int xx = 0; xx < k; ++xx) px[(size_t)(oy + y * k + yy) * size + ox + x * k + xx] = 0xffffffffu;
  BITMAPV5HEADER h{};
  h.bV5Size = sizeof h; h.bV5Width = size; h.bV5Height = -size; h.bV5Planes = 1; h.bV5BitCount = 32;
  h.bV5Compression = BI_BITFIELDS;
  h.bV5RedMask = 0x00ff0000; h.bV5GreenMask = 0x0000ff00; h.bV5BlueMask = 0x000000ff; h.bV5AlphaMask = 0xff000000;
  void* bits = nullptr;
  HDC dc = GetDC(nullptr);
  HBITMAP color = CreateDIBSection(dc, (BITMAPINFO*)&h, DIB_RGB_COLORS, &bits, nullptr, 0);
  ReleaseDC(nullptr, dc);
  if (!color || !bits) { if (color) DeleteObject(color); return nullptr; }
  std::memcpy(bits, px.data(), px.size() * 4);
  HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
  ICONINFO ii{TRUE, 0, 0, mask, color};
  HICON icon = CreateIconIndirect(&ii);
  DeleteObject(mask); DeleteObject(color);
  return icon;
}

void TaskbarButtons::make_icons() {
  int size = GetSystemMetrics(SM_CXSMICON);
  if (size == icon_size_ && icons_[0]) return;
  free_icons();
  icon_size_ = size;
  const char* names[4] = {"prev-file", "play", "pause", "next-file"};
  for (int i = 0; i < 4; ++i) icons_[i] = glyph_icon(names[i], size);
}

void TaskbarButtons::created(HWND hwnd) {
  hwnd_ = hwnd;
  if (!list_) {
    if (!com_) { HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); com_ = SUCCEEDED(hr); }
    if (FAILED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&list_))) || !list_) { list_ = nullptr; return; }
    if (FAILED(list_->HrInit())) { list_->Release(); list_ = nullptr; return; }
  }
  added_ = false;  // a new taskbar button has none of them yet
  make_icons();
  apply(true);
}

void TaskbarButtons::sync(const State& s) {
  if (s == state_ && added_) return;
  state_ = s;
  if (list_) apply(!added_);
}

void TaskbarButtons::apply(bool add) {
  if (!list_ || !hwnd_) return;
  THUMBBUTTON b[3]{};
  const UINT ids[3] = {kPrevious, kPlayPause, kNext};
  for (int i = 0; i < 3; ++i) {
    b[i].dwMask = (THUMBBUTTONMASK)(THB_ICON | THB_TOOLTIP | THB_FLAGS);
    b[i].iId = ids[i];
    b[i].hIcon = icons_[i == 0 ? 0 : i == 2 ? 3 : state_.playing ? 2 : 1];
    const std::wstring& tip = i == 0 ? state_.previous : i == 2 ? state_.next : state_.playing ? state_.pause : state_.play;
    wcsncpy(b[i].szTip, tip.c_str(), 259);
    bool enabled = i == 1 || (i == 0 ? state_.has_prev : state_.has_next);
    b[i].dwFlags = (THUMBBUTTONFLAGS)((state_.shown ? 0 : THBF_HIDDEN) | (enabled ? THBF_ENABLED : THBF_DISABLED));
  }
  HRESULT hr = add ? list_->ThumbBarAddButtons(hwnd_, 3, b) : list_->ThumbBarUpdateButtons(hwnd_, 3, b);
  if (add && SUCCEEDED(hr)) added_ = true;
}
