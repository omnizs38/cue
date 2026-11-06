#include "video_enhance.h"

#include <windows.h>
#include <dxgi.h>

#include <cwchar>

bool on_battery() {
  SYSTEM_POWER_STATUS st{};
  if (!GetSystemPowerStatus(&st)) return false;
  return st.ACLineStatus == 0;  // 1 plugged in, 255 unknown (a desktop): both count as power
}

bool primary_gpu_nvidia() {
  static int known = -1;  // the adapter does not change while Cue runs
  if (known >= 0) return known == 1;
  known = 0;
  HMODULE dxgi = LoadLibraryW(L"dxgi.dll");
  if (!dxgi) return false;
  using Create = HRESULT(WINAPI*)(REFIID, void**);
  auto create = reinterpret_cast<Create>(reinterpret_cast<void*>(GetProcAddress(dxgi, "CreateDXGIFactory1")));
  IDXGIFactory1* f = nullptr;
  if (create && SUCCEEDED(create(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&f))) && f) {
    IDXGIAdapter1* a = nullptr;
    if (SUCCEEDED(f->EnumAdapters1(0, &a)) && a) {
      DXGI_ADAPTER_DESC1 d{};
      if (SUCCEEDED(a->GetDesc1(&d)) && d.VendorId == 0x10DE) known = 1;
      a->Release();
    }
    f->Release();
  }
  FreeLibrary(dxgi);
  return known == 1;
}

int nvidia_vsr_switch() {
  // the display adapters' class key: each adapter has a numbered subkey; NVIDIA's driver keeps
  // the RTX Video switch there (0 off, 1-4 a quality, 5 automatic)
  const wchar_t* cls = L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}";
  HKEY root = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, cls, 0, KEY_READ | KEY_WOW64_64KEY, &root) != ERROR_SUCCESS) return -1;
  int result = -1;
  for (DWORD i = 0; i < 64 && result != 1; ++i) {
    wchar_t name[16];
    DWORD len = 16;
    LONG e = RegEnumKeyExW(root, i, name, &len, nullptr, nullptr, nullptr, nullptr);
    if (e == ERROR_NO_MORE_ITEMS) break;
    if (e != ERROR_SUCCESS || len != 4) continue;  // 0000, 0001...: adapters (not "Configuration", "Properties")
    HKEY k = nullptr;
    if (RegOpenKeyExW(root, name, 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &k) != ERROR_SUCCESS) continue;
    wchar_t provider[64] = {};
    DWORD ps = sizeof provider, type = 0;
    bool nvidia = RegQueryValueExW(k, L"ProviderName", nullptr, &type, reinterpret_cast<BYTE*>(provider), &ps) == ERROR_SUCCESS &&
                  type == REG_SZ && wcsstr(provider, L"NVIDIA");
    DWORD v = 0, vs = sizeof v;
    if (nvidia && RegQueryValueExW(k, L"_User_Global_VAL_SuperResolution", nullptr, &type, reinterpret_cast<BYTE*>(&v), &vs) == ERROR_SUCCESS &&
        type == REG_DWORD)
      result = v > 0 ? 1 : 0;
    RegCloseKey(k);
  }
  RegCloseKey(root);
  return result;
}

void playback_priority(bool playing) {
  static int state = -1;
  if (state == (playing ? 1 : 0)) return;
  state = playing ? 1 : 0;
  using SetInfo = BOOL(WINAPI*)(HANDLE, PROCESS_INFORMATION_CLASS, LPVOID, DWORD);
  static SetInfo set = reinterpret_cast<SetInfo>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetProcessInformation")));
  if (!set) return;  // older Windows: nothing to opt out of
  PROCESS_POWER_THROTTLING_STATE st{};
  st.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
  st.ControlMask = playing ? PROCESS_POWER_THROTTLING_EXECUTION_SPEED : 0;  // 0: Windows decides again
  st.StateMask = 0;                                                          // never throttled while playing
  set(GetCurrentProcess(), ProcessPowerThrottling, &st, sizeof st);
}
