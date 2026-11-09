// Windows-only diagnostic for the real Cue helper. No installation/state changes.
// make preview-benchmark
// cue-preview-bench.exe "C:\path\Cue.exe" "C:\path\video.mp4" > preview.csv
#include <windows.h>
#include "../src/core/preview_protocol.h"
#include "../src/core/preview_timeline.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

struct Handle {
  HANDLE h = nullptr;
  Handle() = default;
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  ~Handle() { if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h); }
};

struct Helper {
  Handle process, input, output;
  ~Helper() {
    if (process.h) { TerminateProcess(process.h, 0); WaitForSingleObject(process.h, 300); }
  }
  bool start(const wchar_t* exe, bool gpu) {
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    Handle child_in, child_out;
    if (!CreatePipe(&child_in.h, &input.h, &sa, 65536) ||
        !CreatePipe(&output.h, &child_out.h, &sa, 65536) ||
        !SetHandleInformation(input.h, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(output.h, HANDLE_FLAG_INHERIT, 0)) return false;
    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_FORCEOFFFEEDBACK;
    si.StartupInfo.hStdInput = child_in.h;
    si.StartupInfo.hStdOutput = si.StartupInfo.hStdError = child_out.h;
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    std::vector<unsigned char> attrs(bytes);
    si.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrs.data());
    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytes)) return false;
    HANDLE inherited[] = {child_in.h, child_out.h};
    const bool attributes = UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
        inherited, sizeof inherited, nullptr, nullptr) != FALSE;
    std::wstring cmd = L"\"" + std::wstring(exe) + L"\" --thumb-host";
    if (gpu) cmd += L" --thumb-hwdec=auto-copy";
    PROCESS_INFORMATION pi{};
    const bool ok = attributes && CreateProcessW(exe, cmd.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &si.StartupInfo, &pi);
    DeleteProcThreadAttributeList(si.lpAttributeList);
    if (!ok) return false;
    process.h = pi.hProcess; CloseHandle(pi.hThread);
    return true;
  }
  bool send(const std::string& command) {
    const std::string line = command + '\n';
    DWORD done = 0;
    return WriteFile(input.h, line.data(), (DWORD)line.size(), &done, nullptr) && done == line.size();
  }
  bool read(void* ptr, size_t size) {
    char* dst = static_cast<char*>(ptr);
    const ULONGLONG start = GetTickCount64();
    while (size) {
      DWORD available = 0, done = 0;
      if (!PeekNamedPipe(output.h, nullptr, 0, nullptr, &available, nullptr)) return false;
      if (!available) {
        if (GetTickCount64() - start > 30000 || WaitForSingleObject(process.h, 0) != WAIT_TIMEOUT) return false;
        Sleep(1); continue;
      }
      if (!ReadFile(output.h, dst, (DWORD)std::min<size_t>(size, available), &done, nullptr) || !done) return false;
      dst += done; size -= done;
    }
    return true;
  }
  bool receive(PreviewHeader& h, uint64_t& hash) {
    if (!read(&h.magic, sizeof h.magic) || h.magic != 0x33565054 ||
        !read(reinterpret_cast<char*>(&h) + sizeof h.magic, sizeof h - sizeof h.magic) || !std::isfinite(h.t)) return false;
    if (h.type != 3) return true;
    if (h.w <= 0 || h.h <= 0 || h.w > 2048 || h.h > 2048) return false;
    std::vector<uint32_t> pixels((size_t)h.w * h.h);
    if (!read(pixels.data(), pixels.size() * 4)) return false;
    hash = 14695981039346656037ull;
    for (uint32_t p : pixels) { hash ^= p; hash *= 1099511628211ull; }
    return true;
  }
};

double clock_ms() {
  static const double hz = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return (double)f.QuadPart; }();
  LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart * 1000.0 / hz;
}

std::string utf8(const wchar_t* wide) {
  int size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
  if (size <= 1) return {};
  std::string text(size, '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide, -1, text.data(), size, nullptr, nullptr);
  text.pop_back(); return text;
}

int wmain(int argc, wchar_t** argv) {
  if (argc != 3) {
    std::fputs("Usage: cue-preview-bench.exe <full path to Cue.exe> <full path to local video> > preview.csv\n", stderr);
    return 2;
  }
  const std::string path = utf8(argv[2]);
  if (path.empty() || path.size() > 16000 || path.find_first_of("\r\n") != std::string::npos) return 2;
  std::puts("mode,sample,requested_s,actual_s,roundtrip_ms,decode_ms,prepare_ms,hardware_used,pixel_hash,status");
  bool failed = false;
  for (int mode = 0; mode < 3; ++mode) {
    const char* name = mode == 0 ? "cpu-fast" : mode == 1 ? "cpu-exact" : "gpu-fast-requested";
    Helper helper;
    const double boot = clock_ms();
    PreviewHeader h; uint64_t hash = 0;
    if (!helper.start(argv[1], mode == 2) || !helper.send("o 1 80 " + path) ||
        !helper.receive(h, hash) || h.type != 1 || h.t <= 0) {
      std::printf("%s,load,0,0,0,0,0,0,0,error\n", name); failed = true; continue;
    }
    const double duration = h.t;
    std::printf("%s,load,0,0,%.3f,0,0,0,0,ok\n", name, clock_ms() - boot);
    std::vector<double> elapsed;
    for (int i = 0; i < 24; ++i) {
      // A deterministic mix of distant points, then revisits; shared across all modes.
      const double t = PreviewTimeline::time(PreviewTimeline::key(
          std::min(duration - .001, duration * (((i % 20) * 7 % 20) + .5) / 20)));
      char line[128];
      std::snprintf(line, sizeof line, "s %u %.3f 80 %d", i + 1, t, mode == 1 ? 1 : 0);
      const double start = clock_ms();
      if (!helper.send(line) || !helper.receive(h, hash) || h.type != 3 || h.id != (uint32_t)i + 1 || h.session != 1) {
        std::printf("%s,%d,%.3f,0,0,0,0,0,0,error\n", name, i, t); failed = true; break;
      }
      const double total = clock_ms() - start; elapsed.push_back(total);
      std::printf("%s,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%u,%llu,ok\n", name, i,
          t, h.t, total, h.decode_ms, h.prepare_ms, h.flags & 1u, (unsigned long long)hash);
    }
    if (!elapsed.empty()) {
      std::sort(elapsed.begin(), elapsed.end());
      std::fprintf(stderr, "%s: %zu samples, p50 %.2f ms, p95 %.2f ms\n", name, elapsed.size(),
          elapsed[(elapsed.size() - 1) / 2], elapsed[(size_t)std::ceil(elapsed.size() * .95) - 1]);
    }
  }
  return failed ? 1 : 0;
}
