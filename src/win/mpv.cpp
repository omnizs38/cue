#include "mpv.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include "../core/i18n.h"
#include "../core/json.h"

template <class F>
static bool sym(HMODULE m, const char* name, F& out) {
  out = reinterpret_cast<F>(reinterpret_cast<void*>(GetProcAddress(m, name)));
  return out != nullptr;
}

bool Mpv::load(const std::wstring& dir, std::string* err) {
  if (dll_) return true;  // already loaded: never overwrite (and leak) the module handle
  api_ = MpvApi{};        // no dangling pointers from a previous failed load
  const wchar_t* names[] = {L"libmpv-2.dll", L"mpv-2.dll", L"mpv-1.dll", L"mpv.dll"};
  for (auto n : names) {
    std::wstring full = dir + L"\\" + n;
    dll_ = LoadLibraryExW(full.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (dll_) break;
  }
  if (!dll_) { if (err) *err = i18n::key_name(str::engine_missing_dll); return false; }
  bool ok = sym(dll_, "mpv_client_api_version", api_.client_api_version) && sym(dll_, "mpv_create", api_.create) &&
            sym(dll_, "mpv_initialize", api_.initialize) && sym(dll_, "mpv_terminate_destroy", api_.terminate_destroy) &&
            sym(dll_, "mpv_set_option_string", api_.set_option_string) &&
            sym(dll_, "mpv_set_property_string", api_.set_property_string) &&
            sym(dll_, "mpv_set_property", api_.set_property) &&
            sym(dll_, "mpv_set_property_async", api_.set_property_async) &&
            sym(dll_, "mpv_get_property", api_.get_property) &&
            sym(dll_, "mpv_get_property_async", api_.get_property_async) &&
            sym(dll_, "mpv_get_property_string", api_.get_property_string) && sym(dll_, "mpv_command", api_.command) &&
            sym(dll_, "mpv_command_string", api_.command_string) &&
            sym(dll_, "mpv_command_async", api_.command_async) &&
            sym(dll_, "mpv_observe_property", api_.observe_property) && sym(dll_, "mpv_wait_event", api_.wait_event) &&
            sym(dll_, "mpv_set_wakeup_callback", api_.set_wakeup_callback) && sym(dll_, "mpv_free", api_.free) &&
            sym(dll_, "mpv_free_node_contents", api_.free_node_contents) && sym(dll_, "mpv_hook_add", api_.hook_add) &&
            sym(dll_, "mpv_hook_continue", api_.hook_continue) && sym(dll_, "mpv_error_string", api_.error_string);
  if (!ok) { if (err) *err = i18n::key_name(str::engine_too_old); FreeLibrary(dll_); dll_ = nullptr; api_ = MpvApi{}; return false; }
  sym(dll_, "mpv_command_ret", api_.command_ret);  // optional
  sym(dll_, "mpv_request_log_messages", api_.request_log_messages);  // optional
  sym(dll_, "mpv_unobserve_property", api_.unobserve_property);  // optional
  // every symbol above exists since client API 1.100 (mpv 0.28); newer options fall back by name
  unsigned long v = api_.client_api_version();
  if (v < ((1ul << 16) | 100)) { if (err) *err = i18n::key_name(str::engine_too_old); FreeLibrary(dll_); dll_ = nullptr; api_ = MpvApi{}; return false; }
  return true;
}

void Mpv::wakeup(void* ctx) {
  auto* self = static_cast<Mpv*>(ctx);
  PostMessageW(self->notify_, self->msg_, 0, 0);  // thread-safe; events are drained on the UI thread
}

bool Mpv::create(HWND notify, UINT msg, int64_t wid, std::string* err) {
  if (!dll_) return false;
  h_ = api_.create();
  if (!h_) { if (err) *err = i18n::key_name(str::engine_failed); return false; }
  notify_ = notify;
  msg_ = msg;
  if (wid != 0) option("wid", std::to_string(wid));
  (void)err;
  return true;
}

void Mpv::destroy() {
  if (h_) { api_.terminate_destroy(h_); h_ = nullptr; }
}

void Mpv::option(const char* k, const std::string& v) { if (h_) api_.set_option_string(h_, k, v.c_str()); }

// (19h) how long a call that waits for the engine held the caller (the window's thread): cue.log, with --log
void mpv_waited(const char* what, const char* name, double ms);
static double qms() { static LARGE_INTEGER f = [] { LARGE_INTEGER v; QueryPerformanceFrequency(&v); return v; }(); LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart * 1000.0 / f.QuadPart; }
bool Mpv::set(const char* k, const std::string& v) { double t0 = qms(); bool r = h_ && api_.set_property_string(h_, k, v.c_str()) >= 0; mpv_waited("set", k, qms() - t0); return r; }

void Mpv::set_async(const char* k, const std::string& v) {
  if (!h_) return;
  const char* s = v.c_str();
  api_.set_property_async(h_, 0, k, MPV_FORMAT_STRING, &s);  // mpv copies the value
}
void Mpv::set_double_async(const char* k, double v) { if (h_) api_.set_property_async(h_, 0, k, MPV_FORMAT_DOUBLE, &v); }
void Mpv::set_flag_async(const char* k, bool v) {
  int f = v ? 1 : 0;
  if (h_) api_.set_property_async(h_, 0, k, MPV_FORMAT_FLAG, &f);
}

int Mpv::cmd_vec(const std::vector<std::string>& args, uint64_t reply) {
  if (!h_) return -1;
  std::vector<const char*> a;
  a.reserve(args.size() + 1);
  for (auto& s : args) a.push_back(s.c_str());
  a.push_back(nullptr);
  return api_.command_async(h_, reply, a.data());  // arguments are copied before returning
}
int Mpv::cmd(std::initializer_list<std::string> args, uint64_t reply) {
  if (!h_) return -1;
  // All current commands, including overlay-add, fit without heap allocations.
  std::array<const char*, 16> pointers{};
  if (args.size() >= pointers.size()) return cmd_vec(std::vector<std::string>(args), reply);
  size_t i = 0;
  for (const auto& arg : args) pointers[i++] = arg.c_str();
  return api_.command_async(h_, reply, pointers.data());
}

// A semicolon command list is executed under the engine's core lock. Separate
// async property writes let playback run between graph rebuild and exact seek.
// These internal commands do not wait for decoding or playback-restart.
bool Mpv::cmd_batch(const std::vector<std::vector<std::string>>& commands) {
  if (!h_) return false;
  std::string text;
  for (const auto& command : commands) {
    if (!text.empty()) text += "; ";
    text += "no-osd raw";
    for (const auto& arg : command) text += " " + json_quote(arg);
  }
  double t0 = qms(); bool r = !text.empty() && api_.command_string(h_, text.c_str()) >= 0; mpv_waited("batch", text.substr(0, 60).c_str(), qms() - t0); return r;
}

double Mpv::get_double(const char* k, double def) {
  double v = def;
  double t0 = qms();
  if (!h_ || api_.get_property(h_, k, MPV_FORMAT_DOUBLE, &v) < 0) return def;
  mpv_waited("get", k, qms() - t0);
  return v;
}

std::string Mpv::get_string(const char* k) {
  if (!h_) return "";
  double t0 = qms();
  char* s = api_.get_property_string(h_, k);
  mpv_waited("get", k, qms() - t0);
  if (!s) return "";
  std::string r = s;
  api_.free(s);
  return r;
}

void Mpv::observe(const char* k, mpv_format f, uint64_t id) { if (h_) api_.observe_property(h_, id, k, f); }
bool Mpv::unobserve(uint64_t id) { return h_ && api_.unobserve_property && api_.unobserve_property(h_, id) >= 0; }

static const mpv_node* node_get(const mpv_node* m, const char* key) {
  if (!m || m->format != MPV_FORMAT_NODE_MAP || !m->u.list) return nullptr;
  for (int i = 0; i < m->u.list->num; ++i)
    if (std::strcmp(m->u.list->keys[i], key) == 0) return &m->u.list->values[i];
  return nullptr;
}
static std::string node_str(const mpv_node* n) { return (n && n->format == MPV_FORMAT_STRING && n->u.string) ? n->u.string : ""; }
static bool node_flag(const mpv_node* n) { return n && n->format == MPV_FORMAT_FLAG && n->u.flag; }
static int64_t node_int(const mpv_node* n) {
  if (!n) return 0;
  if (n->format == MPV_FORMAT_INT64) return n->u.int64;
  if (n->format == MPV_FORMAT_DOUBLE) return (int64_t)n->u.double_;
  return 0;
}

std::vector<Chapter> Mpv::parse_chapters(const mpv_node* n) {
  std::vector<Chapter> list;
  if (n && n->format == MPV_FORMAT_NODE_ARRAY && n->u.list)
    for (int i = 0; i < n->u.list->num && list.size() < 1000; ++i) {
      const mpv_node& c = n->u.list->values[i];
      if (c.format != MPV_FORMAT_NODE_MAP || !c.u.list) continue;
      Chapter ch;
      bool timed = false;
      for (int k = 0; k < c.u.list->num; ++k) {
        const char* key = c.u.list->keys[k];
        const mpv_node& v = c.u.list->values[k];
        if (!key) continue;
        if (!std::strcmp(key, "time") && v.format == MPV_FORMAT_DOUBLE && std::isfinite(v.u.double_)) { ch.t = std::max(0.0, v.u.double_); timed = true; }
        if (!std::strcmp(key, "title") && v.format == MPV_FORMAT_STRING && v.u.string) ch.title = v.u.string;
      }
      if (ch.title.size() > 200) {  // (cut between characters, never inside one)
        size_t cut = 200;
        while (cut > 0 && ((unsigned char)ch.title[cut] & 0xC0) == 0x80) --cut;
        ch.title.resize(cut);
      }
      if (timed) list.push_back(std::move(ch));
    }
  std::stable_sort(list.begin(), list.end(), [](const Chapter& a, const Chapter& b) { return a.t < b.t; });
  return list;
}

std::vector<Track> Mpv::parse_tracks(const mpv_node* n) {
  std::vector<Track> out;
  if (!n || n->format != MPV_FORMAT_NODE_ARRAY || !n->u.list) return out;
  for (int i = 0; i < n->u.list->num; ++i) {
    const mpv_node* t = &n->u.list->values[i];
    Track tr;
    tr.id = node_int(node_get(t, "id"));
    tr.type = node_str(node_get(t, "type"));
    tr.lang = node_str(node_get(t, "lang"));
    tr.title = node_str(node_get(t, "title"));
    tr.codec = node_str(node_get(t, "codec"));
    tr.selected = node_flag(node_get(t, "selected"));
    tr.external = node_flag(node_get(t, "external"));
    if (tr.external) tr.external_file = node_str(node_get(t, "external-filename"));
    tr.forced = node_flag(node_get(t, "forced"));
    tr.is_default = node_flag(node_get(t, "default"));
    tr.albumart = node_flag(node_get(t, "albumart"));
    tr.demux_w = (int)node_int(node_get(t, "demux-w"));
    tr.demux_h = (int)node_int(node_get(t, "demux-h"));
    if (const mpv_node* fps = node_get(t, "demux-fps"); fps && fps->format == MPV_FORMAT_DOUBLE) tr.demux_fps = fps->u.double_;
    if (const mpv_node* par = node_get(t, "demux-par"); par && par->format == MPV_FORMAT_DOUBLE) tr.demux_par = par->u.double_;
    if (const mpv_node* rot = node_get(t, "demux-rotation"); rot && rot->format == MPV_FORMAT_INT64) tr.demux_rotation = (int)rot->u.int64;
    tr.hearing_impaired = node_flag(node_get(t, "hearing-impaired"));
    const auto* metadata=node_get(t,"metadata");
    if(tr.title.empty()) {tr.title=node_str(node_get(metadata,"TITLE"));if(tr.title.empty())tr.title=node_str(node_get(metadata,"title"));}
    // (1.4.1) a name kept outside the title: an MP4 track's own name ("name", OBS's track names),
    // else a handler that says something ("Microfone"), never a muxer's stock one ("SoundHandler")
    if(tr.title.empty()) {
      tr.name=node_str(node_get(metadata,"name"));
      if(tr.name.empty())tr.name=node_str(node_get(metadata,"NAME"));
      if(tr.name.empty()) {
        std::string h=node_str(node_get(metadata,"handler_name")),l=h;
        for(auto& c:l)c=(char)std::tolower((unsigned char)c);
        if(l.find("handler")==std::string::npos&&l.find("core media")==std::string::npos&&l.find("media")==std::string::npos&&l.find("gpac")==std::string::npos&&l.find("l-smash")==std::string::npos)tr.name=h;
      }
      // (an encoder's own id, not a name: "adv_record_audio_0")
      if(tr.name.find('_')!=std::string::npos&&tr.name.find(' ')==std::string::npos)tr.name.clear();
    }
    if(tr.lang.empty()) {tr.lang=node_str(node_get(metadata,"LANGUAGE"));if(tr.lang.empty())tr.lang=node_str(node_get(metadata,"language"));}
    out.push_back(tr);
  }
  return out;
}
