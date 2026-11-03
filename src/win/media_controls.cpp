#include "media_controls.h"

#include <inspectable.h>
#include <shcore.h>
#include <shlwapi.h>
#include <winstring.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

#include "platform.h"

// The few Windows Runtime interfaces used here, declared from Windows.Media's metadata in
// vtable order (only the calls Cue makes are typed precisely; the rest only hold their place).
// They live in a named namespace on purpose: in an anonymous one the compiler sees every class
// of the program that could implement them, finds none (Windows implements them), and treats
// every call through them as unreachable.
namespace cue_winrt {
typedef unsigned char rtbool;
struct Token { int64_t value; };

struct IHandler : IUnknown {
  virtual HRESULT STDMETHODCALLTYPE Invoke(IInspectable* sender, IInspectable* args) = 0;
};
struct IButtonArgs : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE get_Button(int* value) = 0;
};
struct IMusicProps : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE get_Title(HSTRING*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_Title(HSTRING) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_AlbumArtist(HSTRING*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_AlbumArtist(HSTRING) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Artist(HSTRING*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_Artist(HSTRING) = 0;
};
struct IUpdater : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE get_Type(int*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_Type(int) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_AppMediaId(HSTRING*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_AppMediaId(HSTRING) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Thumbnail(IInspectable**) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_Thumbnail(IInspectable*) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_MusicProperties(IMusicProps**) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_VideoProperties(IInspectable**) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_ImageProperties(IInspectable**) = 0;
  virtual HRESULT STDMETHODCALLTYPE CopyFromFileAsync(int, IInspectable*, IInspectable**) = 0;
  virtual HRESULT STDMETHODCALLTYPE ClearAll() = 0;
  virtual HRESULT STDMETHODCALLTYPE Update() = 0;
};
struct ISmtc : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE get_PlaybackStatus(int*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_PlaybackStatus(int) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_DisplayUpdater(IUpdater**) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_SoundLevel(int*) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_IsEnabled(rtbool*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_IsEnabled(rtbool) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_IsPlayEnabled(rtbool*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_IsPlayEnabled(rtbool) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_IsStopEnabled(rtbool*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_IsStopEnabled(rtbool) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_IsPauseEnabled(rtbool*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_IsPauseEnabled(rtbool) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_IsRecordEnabled(rtbool*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_IsRecordEnabled(rtbool) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_IsFastForwardEnabled(rtbool*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_IsFastForwardEnabled(rtbool) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_IsRewindEnabled(rtbool*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_IsRewindEnabled(rtbool) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_IsPreviousEnabled(rtbool*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_IsPreviousEnabled(rtbool) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_IsNextEnabled(rtbool*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_IsNextEnabled(rtbool) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_IsChannelUpEnabled(rtbool*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_IsChannelUpEnabled(rtbool) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_IsChannelDownEnabled(rtbool*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_IsChannelDownEnabled(rtbool) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_ButtonPressed(IHandler*, Token*) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_ButtonPressed(Token) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_PropertyChanged(IUnknown*, Token*) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_PropertyChanged(Token) = 0;
};
struct ITimeline : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE get_StartTime(int64_t*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_StartTime(int64_t) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_EndTime(int64_t*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_EndTime(int64_t) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_MinSeekTime(int64_t*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_MinSeekTime(int64_t) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_MaxSeekTime(int64_t*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_MaxSeekTime(int64_t) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Position(int64_t*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_Position(int64_t) = 0;
};
struct ISmtc2 : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE get_AutoRepeatMode(int*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_AutoRepeatMode(int) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_ShuffleEnabled(rtbool*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_ShuffleEnabled(rtbool) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_PlaybackRate(double*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_PlaybackRate(double) = 0;
  virtual HRESULT STDMETHODCALLTYPE UpdateTimelineProperties(ITimeline*) = 0;
};
struct IStreamRefStatics : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE CreateFromFile(IInspectable*, IInspectable**) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateFromUri(IInspectable*, IInspectable**) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateFromStream(IInspectable*, IInspectable**) = 0;
};
struct IInterop : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE GetForWindow(HWND, REFIID, void**) = 0;
};
}  // namespace cue_winrt

namespace {
using namespace cue_winrt;

const GUID IID_Smtc = {0x99fa3ff4, 0x1742, 0x42a6, {0x90, 0x2e, 0x08, 0x7d, 0x41, 0xf9, 0x65, 0xec}};
const GUID IID_Smtc2 = {0xea98d2f6, 0x7f3c, 0x4af2, {0xa5, 0x86, 0x72, 0x88, 0x98, 0x08, 0xef, 0xb1}};
const GUID IID_Interop = {0xddb0472d, 0xc911, 0x4a1f, {0x86, 0xd9, 0xdc, 0x3d, 0x71, 0xa9, 0x5f, 0x5a}};
const GUID IID_Handler = {0x0557e996, 0x7b23, 0x5bae, {0xaa, 0x81, 0xea, 0x0d, 0x67, 0x11, 0x43, 0xa4}};
const GUID IID_ButtonArgs = {0xb7f47116, 0xa56f, 0x4dc8, {0x9e, 0x11, 0x92, 0x03, 0x1f, 0x4a, 0x87, 0xc2}};
const GUID IID_Timeline = {0x5125316a, 0xc3a2, 0x475b, {0x85, 0x07, 0x93, 0x53, 0x4d, 0xc8, 0x8f, 0x15}};
const GUID IID_StreamRefStatics = {0x857309dc, 0x3fbf, 0x4e7d, {0x98, 0x6f, 0xef, 0x3b, 0x1a, 0x07, 0xa9, 0x64}};
const GUID IID_RandomAccessStream = {0x905a0fe1, 0xbc53, 0x11df, {0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda}};

// combase's runtime functions, found at run time
using RoGetFactoryFn = HRESULT(WINAPI*)(HSTRING, REFIID, void**);
using RoActivateFn = HRESULT(WINAPI*)(HSTRING, IInspectable**);
using CreateStringFn = HRESULT(WINAPI*)(LPCWSTR, UINT32, HSTRING*);
using DeleteStringFn = HRESULT(WINAPI*)(HSTRING);
struct Runtime {
  RoGetFactoryFn get_factory = nullptr;
  RoActivateFn activate = nullptr;
  CreateStringFn create = nullptr;
  DeleteStringFn del = nullptr;
  bool load() {
    HMODULE m = LoadLibraryExW(L"combase.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!m) return false;
    get_factory = reinterpret_cast<RoGetFactoryFn>(reinterpret_cast<void*>(GetProcAddress(m, "RoGetActivationFactory")));
    activate = reinterpret_cast<RoActivateFn>(reinterpret_cast<void*>(GetProcAddress(m, "RoActivateInstance")));
    create = reinterpret_cast<CreateStringFn>(reinterpret_cast<void*>(GetProcAddress(m, "WindowsCreateString")));
    del = reinterpret_cast<DeleteStringFn>(reinterpret_cast<void*>(GetProcAddress(m, "WindowsDeleteString")));
    return get_factory && activate && create && del;
  }
};
struct HStr {
  const Runtime& rt;
  HSTRING h = nullptr;
  HStr(const Runtime& r, const std::wstring& s) : rt(r) { rt.create(s.c_str(), (UINT32)s.size(), &h); }
  ~HStr() { if (h) rt.del(h); }
};
template <class T> void release(T*& p) { if (p) { p->Release(); p = nullptr; } }

// The ButtonPressed handler: called on a system thread, it only posts the button to the window.
struct Handler final : IHandler {
  std::atomic<ULONG> refs{1};
  HWND hwnd;
  UINT msg;
  Handler(HWND h, UINT m) : hwnd(h), msg(m) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override {
    if (!out) return E_POINTER;
    if (IsEqualGUID(riid, IID_IUnknown) || IsEqualGUID(riid, IID_Handler) || IsEqualGUID(riid, IID_IAgileObject)) {
      *out = static_cast<IHandler*>(this);
      AddRef();
      return S_OK;
    }
    *out = nullptr;
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
  ULONG STDMETHODCALLTYPE Release() override { ULONG r = --refs; if (!r) delete this; return r; }
  HRESULT STDMETHODCALLTYPE Invoke(IInspectable*, IInspectable* args) override {
    IButtonArgs* b = nullptr;
    if (args && SUCCEEDED(args->QueryInterface(IID_ButtonArgs, reinterpret_cast<void**>(&b))) && b) {
      int button = -1;
      if (SUCCEEDED(b->get_Button(&button)) && button >= 0) PostMessageW(hwnd, msg, (WPARAM)button, 0);
      b->Release();
    }
    return S_OK;
  }
};
}  // namespace

struct MediaControls::Impl {
  Runtime rt;
  ISmtc* smtc = nullptr;
  ISmtc2* smtc2 = nullptr;
  IUpdater* updater = nullptr;
  ITimeline* timeline = nullptr;
  Token token{};
  bool active = false, playing = false, prev = false, next = false;
  double last_pos = -1, last_dur = -1;
};

MediaControls::MediaControls() : d_(std::make_unique<Impl>()) {}
MediaControls::~MediaControls() { detach(); }

bool MediaControls::attach(HWND hwnd, UINT msg) {
  if (d_->smtc) return true;
  if (!d_->rt.load()) return false;
  IInterop* interop = nullptr;
  {
    HStr cls(d_->rt, L"Windows.Media.SystemMediaTransportControls");
    if (!cls.h || FAILED(d_->rt.get_factory(cls.h, IID_Interop, reinterpret_cast<void**>(&interop))) || !interop) return false;
  }
  HRESULT hr = interop->GetForWindow(hwnd, IID_Smtc, reinterpret_cast<void**>(&d_->smtc));
  interop->Release();
  if (FAILED(hr) || !d_->smtc) { d_->smtc = nullptr; return false; }
  d_->smtc->QueryInterface(IID_Smtc2, reinterpret_cast<void**>(&d_->smtc2));
  d_->smtc->get_DisplayUpdater(&d_->updater);
  {
    HStr cls(d_->rt, L"Windows.Media.SystemMediaTransportControlsTimelineProperties");
    IInspectable* t = nullptr;
    if (d_->smtc2 && cls.h && SUCCEEDED(d_->rt.activate(cls.h, &t)) && t) {
      t->QueryInterface(IID_Timeline, reinterpret_cast<void**>(&d_->timeline));
      t->Release();
    }
  }
  auto* h = new Handler(hwnd, msg);
  d_->smtc->add_ButtonPressed(h, &d_->token);
  h->Release();
  d_->smtc->put_IsPlayEnabled(1);
  d_->smtc->put_IsPauseEnabled(1);
  d_->smtc->put_IsEnabled(0);
  return true;
}

void MediaControls::detach() {
  if (!d_ || !d_->smtc) return;
  d_->smtc->remove_ButtonPressed(d_->token);
  d_->smtc->put_IsEnabled(0);
  release(d_->timeline);
  release(d_->updater);
  release(d_->smtc2);
  release(d_->smtc);
}

void MediaControls::set_active(bool active) {
  if (!d_->smtc || active == d_->active) return;
  d_->active = active;
  d_->smtc->put_IsEnabled(active ? 1 : 0);
  if (!active) {
    d_->smtc->put_PlaybackStatus(0);  // closed
    if (d_->updater) { d_->updater->ClearAll(); d_->updater->Update(); }
    d_->last_pos = d_->last_dur = -1;
  }
}

void MediaControls::set_playing(bool playing) {
  if (!d_->smtc || !d_->active || playing == d_->playing) return;
  d_->playing = playing;
  d_->smtc->put_PlaybackStatus(playing ? 3 : 4);  // playing, paused
}

void MediaControls::set_buttons(bool has_prev, bool has_next) {
  if (!d_->smtc) return;
  if (has_prev != d_->prev) { d_->prev = has_prev; d_->smtc->put_IsPreviousEnabled(has_prev ? 1 : 0); }
  if (has_next != d_->next) { d_->next = has_next; d_->smtc->put_IsNextEnabled(has_next ? 1 : 0); }
}

void MediaControls::set_media(bool music, const std::string& title, const std::string& artist, const std::string& cover) {
  (void)music;
  if (!d_->updater) return;
  IUpdater* u = d_->updater;
  u->ClearAll();
  u->put_Type(1);  // music: a title and an artist, for songs and films alike
  IMusicProps* m = nullptr;
  if (SUCCEEDED(u->get_MusicProperties(&m)) && m) {
    HStr t(d_->rt, widen(title)), a(d_->rt, widen(artist));
    m->put_Title(t.h);
    m->put_Artist(a.h);
    m->Release();
  }
  if (!cover.empty()) {
    // the cover's own bytes, as a stream the shell reads when it draws the card
    IStream* mem = SHCreateMemStream(reinterpret_cast<const BYTE*>(cover.data()), (UINT)cover.size());
    IInspectable* ras = nullptr;
    IStreamRefStatics* refs = nullptr;
    IInspectable* ref = nullptr;
    HStr cls(d_->rt, L"Windows.Storage.Streams.RandomAccessStreamReference");
    if (mem && SUCCEEDED(CreateRandomAccessStreamOverStream(mem, BSOS_DEFAULT, IID_RandomAccessStream, reinterpret_cast<void**>(&ras))) && ras &&
        cls.h && SUCCEEDED(d_->rt.get_factory(cls.h, IID_StreamRefStatics, reinterpret_cast<void**>(&refs))) && refs &&
        SUCCEEDED(refs->CreateFromStream(ras, &ref)) && ref)
      u->put_Thumbnail(ref);
    release(ref);
    release(refs);
    release(ras);
    release(mem);
  }
  u->Update();
  d_->last_pos = d_->last_dur = -1;
}

void MediaControls::set_timeline(double pos, double dur) {
  if (!d_->smtc2 || !d_->timeline || !d_->active || !(dur > 0)) return;
  // every few seconds, and at once after a jump or with a new length
  if (std::abs(dur - d_->last_dur) < 0.5 && d_->last_pos >= 0 && pos >= d_->last_pos && pos - d_->last_pos < 5.0) return;
  d_->last_pos = pos;
  d_->last_dur = dur;
  const double k = 1e7;  // 100 ns units
  d_->timeline->put_StartTime(0);
  d_->timeline->put_MinSeekTime(0);
  d_->timeline->put_EndTime((int64_t)(dur * k));
  d_->timeline->put_MaxSeekTime((int64_t)(dur * k));
  d_->timeline->put_Position((int64_t)(std::max(0.0, pos) * k));
  d_->smtc2->UpdateTimelineProperties(d_->timeline);
}
