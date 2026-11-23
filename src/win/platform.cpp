#include "platform.h"

#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <cmath>
#include <mutex>
#include <winioctl.h>

#include "../core/build_identity.h"
#include "../core/i18n.h"
#include "../core/remote_proto.h"

std::wstring widen(const std::string& s) {
  if (s.empty()) return {};
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
  std::wstring w(n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
  return w;
}

std::string narrow(const std::wstring& w) {
  if (w.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
  std::string s(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
  return s;
}

std::wstring exe_dir() {
  wchar_t buf[MAX_PATH * 4];
  DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof buf / sizeof buf[0]));
  std::wstring p(buf, n);
  size_t k = p.find_last_of(L"\\/");
  return k == std::wstring::npos ? L"." : p.substr(0, k);
}

// Cue's folder in %APPDATA%. It is %APPDATA%\tinypeep\Cue (the studio's name); it used to be
// %APPDATA%\tinyoffice\Cue (and before that tinyoffice\Player). The running Cue moves the old
// folder once (migrate_data_dir); until it has, the old one is the one in use, so nothing is
// ever read from an empty folder while settings sit in the other.
static std::wstring roaming_dir() {
  PWSTR base = nullptr;
  std::wstring dir;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &base))) { dir = base; CoTaskMemFree(base); }
  else dir = exe_dir();
  return dir;
}
static bool dir_exists(const std::wstring& p) {
  DWORD a = GetFileAttributesW(p.c_str());
  return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring data_dir() {
  const std::wstring root = roaming_dir();
  const std::wstring now = root + L"\\tinypeep\\Cue", was = root + L"\\tinyoffice\\Cue", older = root + L"\\tinyoffice\\Player";
  if (dir_exists(now)) return now;
#if CUE_STORE_BUILD
  // The Store edition never used the old names: only tinypeep\Cue. (Windows keeps a packaged
  // app's %APPDATA% writes in the package's own per-user folder, removed with the app.)
  (void)was; (void)older;
  CreateDirectoryW((root + L"\\tinypeep").c_str(), nullptr);
  CreateDirectoryW(now.c_str(), nullptr);
  return now;
#else
  if (dir_exists(was)) return was;
  if (dir_exists(older)) return older;
  CreateDirectoryW((root + L"\\tinypeep").c_str(), nullptr);
  CreateDirectoryW(now.c_str(), nullptr);
  return now;
#endif
}

bool migrate_data_dir() {
  if (CUE_STORE_BUILD) return true;  // nothing of the GitHub edition's is moved from inside the package
  const std::wstring root = roaming_dir();
  const std::wstring now = root + L"\\tinypeep\\Cue", was = root + L"\\tinyoffice\\Cue", older = root + L"\\tinyoffice\\Player";
  if (dir_exists(now)) return true;
  const std::wstring from = dir_exists(was) ? was : dir_exists(older) ? older : std::wstring();
  if (from.empty()) return true;
  CreateDirectoryW((root + L"\\tinypeep").c_str(), nullptr);
  // one rename on the same disk: everything comes along at once, or nothing moves (something
  // still holds a file inside, such as a Cue that is closing during an update): then the old
  // folder stays in use and the next start tries again
  if (!MoveFileExW(from.c_str(), now.c_str(), 0)) return false;
  RemoveDirectoryW((root + L"\\tinyoffice").c_str());  // only if nothing else lives there
  return true;
}

std::vector<std::string> open_file_dialog(HWND owner, DialogKind kind) {
  std::vector<std::string> out;
  IFileOpenDialog* dlg = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) return out;
  // the dialog itself (buttons, places, columns) is Windows' own, in Windows' language;
  // only the title and the type names below are Cue's
  const std::wstring media = widen(tr(str::win_filter_media)), audio = widen(tr(str::win_filter_audio)),
                     all = widen(tr(str::win_filter_all)), subtitles = widen(tr(str::win_filter_subtitles));
  COMDLG_FILTERSPEC video[] = {
      {media.c_str(), L"*.mkv;*.mp4;*.m4v;*.mov;*.avi;*.webm;*.wmv;*.flv;*.ts;*.m2ts;*.mts;*.mpg;*.mpeg;*.ogv;*.3gp;*.vob;*.divx;*.dv;*.dif;*.ogg;*.oga;*.opus;*.mp3;*.m4a;*.m4b;*.aac;*.wav;*.flac;*.aif;*.aiff;*.wma;*.mka"},
      {audio.c_str(), L"*.mp4;*.ogg;*.oga;*.opus;*.mp3;*.m4a;*.m4b;*.aac;*.wav;*.flac;*.aif;*.aiff;*.wma;*.mka"},
      {all.c_str(), L"*.*"}};
  COMDLG_FILTERSPEC subs[] = {{subtitles.c_str(), L"*.srt;*.vtt;*.ass;*.ssa;*.sub;*.sup;*.idx"}, {all.c_str(), L"*.*"}};
  // a film's own audio file (a dub, a commentary): every container the engine reads sound from
  const std::wstring tracks = widen(tr(str::win_filter_audio_tracks));
  COMDLG_FILTERSPEC sound[] = {
      {tracks.c_str(), L"*.mka;*.mp3;*.m4a;*.aac;*.ac3;*.eac3;*.ec3;*.dts;*.thd;*.truehd;*.flac;*.opus;*.ogg;*.oga;*.wav;*.w64;*.wma;*.aif;*.aiff;*.mp2;*.mpa;*.mp4;*.mkv;*.webm"},
      {all.c_str(), L"*.*"}};
  if (kind == DialogKind::Video) dlg->SetFileTypes(3, video);
  else if (kind == DialogKind::Audio) dlg->SetFileTypes(2, sound);
  else dlg->SetFileTypes(2, subs);
  dlg->SetTitle(widen(tr(kind == DialogKind::Video ? str::win_open_title : kind == DialogKind::Audio ? str::win_audio_title : str::win_subtitle_title)).c_str());
  // films and songs: several at once (Ctrl and Shift, as in Explorer: 15/11); they go to the list
  // in the order Explorer gives them. A dub and subtitles stay one at a time
  if (kind == DialogKind::Video) {
    FILEOPENDIALOGOPTIONS o = 0;
    if (SUCCEEDED(dlg->GetOptions(&o))) dlg->SetOptions(o | FOS_ALLOWMULTISELECT);
  }
  if (SUCCEEDED(dlg->Show(owner))) {
    IShellItemArray* items = nullptr;
    if (SUCCEEDED(dlg->GetResults(&items)) && items) {
      DWORD n = 0;
      items->GetCount(&n);
      for (DWORD i = 0; i < n; ++i) {
        IShellItem* item = nullptr;
        if (FAILED(items->GetItemAt(i, &item)) || !item) continue;
        PWSTR path = nullptr;
        if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
          out.push_back(narrow(path));
          CoTaskMemFree(path);
        }
        item->Release();
      }
      items->Release();
    }
  }
  dlg->Release();
  return out;
}

std::string pick_folder_dialog(HWND owner) {
  std::string out;
  IFileOpenDialog* dlg = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) return out;
  DWORD opts = 0;
  dlg->GetOptions(&opts);
  dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
  dlg->SetTitle(widen(tr(str::remote_add_folder)).c_str());
  if (SUCCEEDED(dlg->Show(owner))) {
    IShellItem* item = nullptr;
    if (SUCCEEDED(dlg->GetResult(&item))) {
      PWSTR path = nullptr;
      if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
        out = narrow(path);
        CoTaskMemFree(path);
      }
      item->Release();
    }
  }
  dlg->Release();
  return out;
}

std::string folder_display_name(const std::string& path) {
  std::string out;
  IShellItem* item = nullptr;
  if (SUCCEEDED(SHCreateItemFromParsingName(widen(path).c_str(), nullptr, IID_PPV_ARGS(&item)))) {
    PWSTR name = nullptr;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_NORMALDISPLAY, &name))) {
      out = narrow(name);
      CoTaskMemFree(name);
    }
    item->Release();
  }
  if (out.empty()) {
    size_t k = path.find_last_of("\\/", path.size() > 1 ? path.size() - 2 : 0);
    out = k == std::string::npos ? path : path.substr(k + 1);
    while (!out.empty() && (out.back() == '\\' || out.back() == '/')) out.pop_back();
    if (out.empty()) out = path;
  }
  return out;
}

std::vector<std::string> default_media_folders() {
  std::vector<std::string> out;
  for (const KNOWNFOLDERID* id : {&FOLDERID_Videos, &FOLDERID_Music, &FOLDERID_Downloads, &FOLDERID_Desktop}) {
    PWSTR p = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(*id, KF_FLAG_DEFAULT, nullptr, &p)) && p) out.push_back(narrow(p));
    else out.push_back("");
    if (p) CoTaskMemFree(p);
  }
  return out;
}

void show_in_folder(const std::wstring& file) {
  // talking to Explorer can take a moment: on its own thread, so the video never waits
  auto* path = new std::wstring(file);
  HANDLE th = CreateThread(nullptr, 0, [](void* p) -> DWORD {
    auto* f = static_cast<std::wstring*>(p);
    bool init = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    bool ok = false;
    if (PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(f->c_str())) {
      ok = SUCCEEDED(SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0));
      ILFree(pidl);
    }
    if (!ok) {  // the file is gone: at least its folder
      std::wstring dir = f->substr(0, f->find_last_of(L"\\/"));
      ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    if (init) CoUninitialize();
    delete f;
    return 0;
  }, path, 0, nullptr);
  if (th) CloseHandle(th); else delete path;
}

std::vector<std::string> files_from_hdrop(HDROP drop) {
  std::vector<std::string> out;
  UINT n = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
  for (UINT i = 0; i < n; ++i) {
    UINT len = DragQueryFileW(drop, i, nullptr, 0);
    std::wstring w(len + 1, L'\0');
    DragQueryFileW(drop, i, w.data(), len + 1);
    w.resize(len);
    out.push_back(narrow(w));
  }
  return out;
}

std::vector<std::string> clipboard_files(HWND owner, bool* as_text) {
  std::vector<std::string> out;
  if (as_text) *as_text = false;
  if (!OpenClipboard(owner)) return out;
  if (HANDLE h = GetClipboardData(CF_HDROP)) {
    out = files_from_hdrop((HDROP)h);
  } else if (HANDLE t = GetClipboardData(CF_UNICODETEXT)) {
    // a path copied as text also works, if it points to a real file
    if (auto* p = (const wchar_t*)GlobalLock(t)) {
      std::wstring w = p;
      GlobalUnlock(t);
      while (!w.empty() && (w.back() == L'\n' || w.back() == L'\r' || w.back() == L' ')) w.pop_back();
      if (w.size() > 2 && w.front() == L'"' && w.back() == L'"') w = w.substr(1, w.size() - 2);
      // (18g, 17/11, varredura: whether it is a file is the caller's to ask, on a worker. Asked here,
      // on the window's thread, with the clipboard held, a path on a sleeping disk froze both)
      if (as_text && !w.empty() && w.size() < 32768 && w.find_first_of(L"\r\n\t") == std::wstring::npos) {
        out.push_back(narrow(w));
        *as_text = true;
      }
    }
  }
  CloseClipboard();
  return out;
}

[[maybe_unused]] static bool reg_set(HKEY root, const std::wstring& key, const wchar_t* name, const std::wstring& val) {
  HKEY k;
  if (RegCreateKeyExW(root, key.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) != ERROR_SUCCESS) return false;
  LONG r = RegSetValueExW(k, name, 0, REG_SZ, (const BYTE*)val.c_str(), (DWORD)((val.size() + 1) * sizeof(wchar_t)));
  RegCloseKey(k);
  return r == ERROR_SUCCESS;
}

// Two file types: video (Cue's own icon) and music (the same tile with a note, icon 2).
// Files Windows already opens with Cue keep the type they were chosen with until chosen again.
[[maybe_unused]] static const wchar_t* kVideoExts[] = {L".mkv", L".mp4", L".m4v", L".mov", L".avi", L".webm", L".wmv", L".ts", L".m2ts", L".mpg", L".mpeg", L".ogv", L".divx", L".dv", L".dif"};
[[maybe_unused]] static const wchar_t* kAudioExts[] = {L".ogg", L".oga", L".opus", L".mp3", L".m4a", L".aac", L".wav", L".flac", L".aif", L".aiff", L".wma", L".mka"};
static const wchar_t* kVideoProg = L"Software\\Classes\\tinyoffice.Cue.video";
static const wchar_t* kAudioProg = L"Software\\Classes\\tinyoffice.Cue.audio";
// Settings → Default apps → Cue lists every type below with its own file type, so music
// chosen there gets the music icon. "Open with" can only offer Cue; Windows keeps a choice made
// earlier (on an older version) until it is made again.
static const wchar_t* kCapabilities = L"Software\\tinyoffice\\Cue\\Capabilities";
static const wchar_t* kMediaTypes = L"video-audio-6";  // bumped when the registration's shape changes

bool register_open_with(std::string* err) {
#if CUE_STORE_BUILD
  // The package manifest declares Cue's file types (windows.fileTypeAssociation). A packaged
  // app's HKCU writes stay private to the package, and the path below would point into
  // WindowsApps, which Explorer cannot launch: nothing to write.
  (void)err;
  return true;
#else
  wchar_t exe[MAX_PATH * 4];
  GetModuleFileNameW(nullptr, exe, (DWORD)(sizeof exe / sizeof exe[0]));
  std::wstring cmd = L"\"" + std::wstring(exe) + L"\" \"%1\"";
  // the file type's name in Explorer, in the interface language of the last registration
  auto prog_set = [&](const wchar_t* prog, str::Id name, const wchar_t* icon) {
    return reg_set(HKEY_CURRENT_USER, prog, nullptr, widen(tr(name))) &&
           reg_set(HKEY_CURRENT_USER, std::wstring(prog) + L"\\Application", L"ApplicationName", L"Cue") &&
           reg_set(HKEY_CURRENT_USER, std::wstring(prog) + L"\\DefaultIcon", nullptr, std::wstring(exe) + icon) &&
           reg_set(HKEY_CURRENT_USER, std::wstring(prog) + L"\\shell\\open\\command", nullptr, cmd);
  };
  bool ok = prog_set(kVideoProg, str::win_progid, L",0") && prog_set(kAudioProg, str::win_progid_audio, L",-2");
  // Explorer's pictures of films and album covers: Explorer asks the file type that opens a file
  // for its thumbnail handler first. Windows' own handler for media is often registered only on
  // the type that opened the file before (Windows Media Player's), so files handed to Cue lost
  // their pictures on some PCs (reports of 14/11). Cue's two types name Windows' handler
  // themselves; Cue makes no thumbnails of its own and touches no other type's registration
  for (const wchar_t* prog : {kVideoProg, kAudioProg})
    for (const wchar_t* kind : {L"{e357fccd-a995-4576-b01f-234630154e96}", L"{BB2E617C-0920-11D1-9A0B-00C04FC2D6C1}"})
      ok = reg_set(HKEY_CURRENT_USER, std::wstring(prog) + L"\\ShellEx\\" + kind, nullptr, L"{9DBD2C50-62AD-11D0-B806-00C04FD706EC}") && ok;
  std::wstring app = L"Software\\Classes\\Applications\\Cue.exe";
  ok = ok && reg_set(HKEY_CURRENT_USER, app + L"\\shell\\open\\command", nullptr, cmd) &&
       reg_set(HKEY_CURRENT_USER, app, L"FriendlyAppName", L"Cue");
  // (1.4.1) Cue chosen through "Open with → Choose another app → Look for another app on this PC"
  // is this entry, not one of the two types above: it names Windows' handler too, or the files it
  // opens lose their pictures in Explorer (a report of 21/11)
  for (const wchar_t* kind : {L"{e357fccd-a995-4576-b01f-234630154e96}", L"{BB2E617C-0920-11D1-9A0B-00C04FC2D6C1}"})
    ok = reg_set(HKEY_CURRENT_USER, app + L"\\ShellEx\\" + kind, nullptr, L"{9DBD2C50-62AD-11D0-B806-00C04FD706EC}") && ok;
  // Keep an existing legacy ProgID working if Windows still uses it as a default.
  // Hide its duplicate recommendation and change only our own Player registration.
  const std::wstring legacy = L"Software\\Classes\\tinyoffice.Player.video";
  HKEY old;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, legacy.c_str(), 0, KEY_READ, &old) == ERROR_SUCCESS) {
    RegCloseKey(old);
    wchar_t old_command[MAX_PATH * 4]{};
    DWORD bytes = sizeof old_command;
    RegGetValueW(HKEY_CURRENT_USER, (legacy + L"\\shell\\open\\command").c_str(), nullptr,
                 RRF_RT_REG_SZ, nullptr, old_command, &bytes);
    wchar_t player_command[MAX_PATH * 4]{};
    bytes = sizeof player_command;
    const std::wstring player = L"Software\\Classes\\Applications\\Player.exe";
    if (old_command[0] && RegGetValueW(HKEY_CURRENT_USER, (player + L"\\shell\\open\\command").c_str(), nullptr,
                                       RRF_RT_REG_SZ, nullptr, player_command, &bytes) == ERROR_SUCCESS &&
        std::wstring(old_command) == player_command) {
      reg_set(HKEY_CURRENT_USER, player, L"FriendlyAppName", L"Cue");
      reg_set(HKEY_CURRENT_USER, player, L"NoOpenWith", L"");
      reg_set(HKEY_CURRENT_USER, player + L"\\shell\\open\\command", nullptr, cmd);
    }
    reg_set(HKEY_CURRENT_USER, legacy + L"\\Application", L"ApplicationName", L"Cue");
    reg_set(HKEY_CURRENT_USER, legacy, L"NoOpenWith", L"");
    reg_set(HKEY_CURRENT_USER, legacy + L"\\DefaultIcon", nullptr, std::wstring(exe) + L",0");
    reg_set(HKEY_CURRENT_USER, legacy + L"\\shell\\open\\command", nullptr, cmd);
  }
  const std::wstring empty_value;
  auto offer = [&](const wchar_t* e, const wchar_t* mine, const wchar_t* other) {
    std::wstring owp = std::wstring(L"Software\\Classes\\") + e + L"\\OpenWithProgids";
    RegDeleteKeyValueW(HKEY_CURRENT_USER, owp.c_str(), L"tinyoffice.Player.video");
    RegDeleteKeyValueW(HKEY_CURRENT_USER, owp.c_str(), other);  // one Cue in "Open with", never two
    ok = ok && reg_set(HKEY_CURRENT_USER, owp, mine, empty_value);
    ok = ok && reg_set(HKEY_CURRENT_USER, app + L"\\SupportedTypes", e, empty_value);
  };
  for (auto e : kVideoExts) offer(e, L"tinyoffice.Cue.video", L"tinyoffice.Cue.audio");
  for (auto e : kAudioExts) offer(e, L"tinyoffice.Cue.audio", L"tinyoffice.Cue.video");
  const std::wstring caps = kCapabilities;
  ok = ok && reg_set(HKEY_CURRENT_USER, caps, L"ApplicationName", L"Cue") &&
       reg_set(HKEY_CURRENT_USER, caps, L"ApplicationDescription", widen(tr(str::win_progid))) &&
       reg_set(HKEY_CURRENT_USER, caps, L"ApplicationIcon", std::wstring(exe) + L",0");
  for (auto e : kVideoExts) ok = ok && reg_set(HKEY_CURRENT_USER, caps + L"\\FileAssociations", e, L"tinyoffice.Cue.video");
  for (auto e : kAudioExts) ok = ok && reg_set(HKEY_CURRENT_USER, caps + L"\\FileAssociations", e, L"tinyoffice.Cue.audio");
  ok = ok && reg_set(HKEY_CURRENT_USER, L"Software\\RegisteredApplications", L"Cue", caps);
  if (ok) ok = reg_set(HKEY_CURRENT_USER, app, L"CueMediaTypes", kMediaTypes);
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
  if (!ok && err) *err = "register failed";  // log only
  return ok;
#endif
}

void refresh_open_with() {
  if (CUE_STORE_BUILD) return;  // see register_open_with
  // Portable updates can leave Explorer's old Player registration behind.
  // Only the main process calls this; thumbnail helpers must never register.
  const wchar_t* app = L"Software\\Classes\\Applications\\Cue.exe";
  const wchar_t* prog = kVideoProg;
  auto read = [](const std::wstring& key, const wchar_t* name) {
    wchar_t value[MAX_PATH * 4]{};
    DWORD bytes = sizeof value;
    if (RegGetValueW(HKEY_CURRENT_USER, key.c_str(), name, RRF_RT_REG_SZ, nullptr, value, &bytes) != ERROR_SUCCESS)
      return std::wstring{};
    return std::wstring(value);
  };
  auto exists = [](const wchar_t* key) {
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, key, 0, KEY_READ, &k) != ERROR_SUCCESS) return false;
    RegCloseKey(k);
    return true;
  };
  wchar_t exe[MAX_PATH * 4]{};
  DWORD len = GetModuleFileNameW(nullptr, exe, (DWORD)(sizeof exe / sizeof exe[0]));
  if (!len || len >= sizeof exe / sizeof exe[0]) return;
  const std::wstring command = L"\"" + std::wstring(exe) + L"\" \"%1\"";
  if (read(app, L"FriendlyAppName") == L"Cue" &&
      read(prog, nullptr) == widen(tr(str::win_progid)) &&
      read(app, L"CueMediaTypes") == kMediaTypes &&
      read(L"Software\\RegisteredApplications", L"Cue") == kCapabilities &&
      read(kAudioProg, nullptr) == widen(tr(str::win_progid_audio)) &&
      read(std::wstring(kAudioProg) + L"\\shell\\open\\command", nullptr) == command &&
      read(std::wstring(app) + L"\\shell\\open\\command", nullptr) == command &&
      read(std::wstring(prog) + L"\\Application", L"ApplicationName") == L"Cue" &&
      read(std::wstring(prog) + L"\\shell\\open\\command", nullptr) == command &&
      (!exists(L"Software\\Classes\\tinyoffice.Player.video") ||
       (read(L"Software\\Classes\\tinyoffice.Player.video\\Application", L"ApplicationName") == L"Cue" &&
        read(L"Software\\Classes\\tinyoffice.Player.video\\shell\\open\\command", nullptr) == command))) return;
  register_open_with(nullptr);
}


// Pixel art is only crisp at whole multiples (every art pixel the same number of screen
// pixels), so the scale is always an integer and never interpolated:
//  * 100/150/200/250/300%: the exact multiple (2/3/4/5/6 px per art px)
//  * 125/175/225%: halfway between two multiples. The larger one when the whole design
//    canvas still fits with it (maximized, fullscreen, big windows), the smaller otherwise
//  * very large windows at low scaling (a TV, 4K at 100%) step up, read from a distance
//  * never so large that the layout (320 x 180 art px) stops fitting
// A small bias toward the current scale keeps it from flickering while resizing.

bool system_reduce_motion() {
  BOOL anim = TRUE;
  SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &anim, 0);
  return !anim;
}

// Windows' own animation when a window is minimized and restored ("Animation effects" in the
// accessibility settings turns it off with the rest)
bool system_window_animation() {
#ifdef CUE_MEASURE_WINDOW_ANIMATION  // (Wine never animates windows: the measurements ask for Windows' default)
  return true;
#endif
  ANIMATIONINFO ai{sizeof(ai), 1};
  if (!SystemParametersInfoW(SPI_GETANIMATION, sizeof(ai), &ai, 0)) return true;
  return ai.iMinAnimate != 0;
}

bool system_high_contrast() {
  HIGHCONTRASTW hc{sizeof(hc)};
  if (!SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0)) return false;
  return (hc.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

// ── drop target ─────────────────────────────────────────────────────
HRESULT DropTarget::QueryInterface(REFIID riid, void** ppv) {
  if (riid == IID_IUnknown || riid == IID_IDropTarget) { *ppv = static_cast<IDropTarget*>(this); AddRef(); return S_OK; }
  *ppv = nullptr;
  return E_NOINTERFACE;
}
ULONG DropTarget::Release() {
  LONG r = InterlockedDecrement(&ref_);
  if (r == 0) delete this;
  return r;
}
POINT DropTarget::to_client(POINTL pt) const {
  POINT p{pt.x, pt.y};
  ScreenToClient(hwnd_, &p);
  return p;
}
HRESULT DropTarget::DragEnter(IDataObject* data, DWORD, POINTL pt, DWORD* effect) {
  FORMATETC fe{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
  ok_ = data && data->QueryGetData(&fe) == S_OK;
  *effect = ok_ ? DROPEFFECT_COPY : DROPEFFECT_NONE;
  if (on_enter) on_enter(ok_, to_client(pt));
  return S_OK;
}
HRESULT DropTarget::DragOver(DWORD, POINTL pt, DWORD* effect) {
  *effect = ok_ ? DROPEFFECT_COPY : DROPEFFECT_NONE;
  if (ok_ && on_over) on_over(to_client(pt));
  return S_OK;
}
HRESULT DropTarget::DragLeave() {
  if (on_leave) on_leave();
  return S_OK;
}
HRESULT DropTarget::Drop(IDataObject* data, DWORD, POINTL, DWORD* effect) {
  *effect = DROPEFFECT_NONE;
  dropping = true;
  if (on_leave) on_leave();
  dropping = false;
  FORMATETC fe{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
  STGMEDIUM st{};
  if (data && SUCCEEDED(data->GetData(&fe, &st))) {
    auto files = files_from_hdrop((HDROP)st.hGlobal);
    ReleaseStgMedium(&st);
    *effect = DROPEFFECT_COPY;
    if (on_drop) on_drop(files);
  }
  return S_OK;
}

// Check lexical syntax before touching the filesystem (no UNC/ADS/device namespace tricks).
// Reject junctions/symlinks below the explicitly shared root, and compare resolved handles.
bool remote_path_allowed(const std::string& root, const std::string& path) {
  if (!remote::path_within(root, path)) return false;
  std::wstring rw = widen(root), pw = widen(path);
  for (auto& c : rw) if (c == L'/') c = L'\\';
  for (auto& c : pw) if (c == L'/') c = L'\\';
  while (rw.size() > 3 && rw.back() == L'\\') rw.pop_back();
  for (size_t at = rw.size() + (rw.back() != L'\\'); at < pw.size();) {
    size_t end = pw.find(L'\\', at);
    if (end == std::wstring::npos) end = pw.size();
    DWORD attrs = GetFileAttributesW(pw.substr(0, end).c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    at = end + 1;
  }
  auto final_path = [](const std::wstring& v) {
    std::wstring result;
    HANDLE h = CreateFileW(v.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (h == INVALID_HANDLE_VALUE) return result;
    std::vector<wchar_t> b(32768);
    DWORD n = GetFinalPathNameByHandleW(h, b.data(), (DWORD)b.size(), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (n && n < b.size()) result.assign(b.data(), n);
    CloseHandle(h);
    return result;
  };
  auto rp = final_path(rw), pp = final_path(pw);
  if (rp.empty() || pp.size() < rp.size()) return false;
  if (CompareStringOrdinal(rp.data(), (int)rp.size(), pp.data(), (int)rp.size(), TRUE) != CSTR_EQUAL) return false;
  return pp.size() == rp.size() || rp.back() == L'\\' || pp[rp.size()] == L'\\';
}

bool on_external_drive(const std::string& path) {
  const std::wstring w = widen(path);
  if (w.size() < 3 || w[1] != L':' || (w[2] != L'\\' && w[2] != L'/')) return false;
  const wchar_t letter = (wchar_t)towupper(w[0]);
  if (letter < L'A' || letter > L'Z') return false;
  static std::mutex mu;
  static signed char known[26];  // 0 not asked, 1 external, -1 not
  {
    std::lock_guard lock(mu);
    if (known[letter - L'A']) return known[letter - L'A'] > 0;
  }
  const wchar_t root[4] = {letter, L':', L'\\', 0};
  const UINT type = GetDriveTypeW(root);
  bool external = type == DRIVE_REMOVABLE;
  if (type == DRIVE_FIXED) {
    // the volume's device, opened with no access to its data: asks the driver, not the disk
    const wchar_t volume[7] = {L'\\', L'\\', L'.', L'\\', letter, L':', 0};
    HANDLE h = CreateFileW(volume, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
      STORAGE_PROPERTY_QUERY q{};
      q.PropertyId = StorageDeviceProperty;
      q.QueryType = PropertyStandardQuery;
      alignas(8) unsigned char buf[1024]{};
      DWORD got = 0;
      if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &q, sizeof q, buf, sizeof buf, &got, nullptr) &&
          got >= sizeof(STORAGE_DEVICE_DESCRIPTOR)) {
        const auto* d = reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR*>(buf);
        external = d->BusType == BusTypeUsb || d->BusType == BusType1394 || d->BusType == BusTypeSd ||
                   d->BusType == BusTypeMmc || d->RemovableMedia;
      }
      CloseHandle(h);
    }
  }
  std::lock_guard lock(mu);
  known[letter - L'A'] = external ? 1 : -1;
  return external;
}

std::vector<std::wstring> folder_files(const std::wstring& dir, size_t max) {
  std::vector<std::wstring> out;
  if (dir.empty()) return out;
  std::wstring pattern = dir;
  if (pattern.back() != L'\\' && pattern.back() != L'/') pattern += L'\\';
  pattern += L'*';
  WIN32_FIND_DATAW fd{};
  HANDLE h = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
  if (h == INVALID_HANDLE_VALUE) return out;
  do {
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
    out.emplace_back(fd.cFileName);
  } while (out.size() < max && FindNextFileW(h, &fd));
  FindClose(h);
  return out;
}
