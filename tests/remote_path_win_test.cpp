// Windows: a shared folder's boundary is also checked through resolved handles and reparse
// points, not just the lexical path the portable test covers.
#include "../src/win/platform.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static std::string utf8(const fs::path& p) { return narrow(p.wstring()); }

int main() {
  const fs::path base = fs::temp_directory_path() / "cue-remote-path-test";
  std::error_code ec;
  fs::remove_all(base, ec);
  const fs::path root = base / "shared", outside = base / "outside", link = root / "link";
  fs::create_directories(root / "sub");
  fs::create_directories(outside);
  { std::ofstream f(root / "sub" / "inside.mkv"); f << "x"; }
  { std::ofstream f(outside / "secret.mkv"); f << "x"; }
  assert(remote_path_allowed(utf8(root), utf8(root / "sub" / "inside.mkv")));
  assert(!remote_path_allowed(utf8(root), utf8(root / ".." / "outside" / "secret.mkv")));
  assert(!remote_path_allowed(utf8(root / "sub"), utf8(root / "subway.mkv")));

  // A junction inside the share is a link out, not a folder of it. Junctions need no admin
  // rights, so this runs on a normal developer PC (Wine and locked-down PCs skip it).
  std::wstring cmd = L"cmd.exe /c mklink /J \"" + link.wstring() + L"\" \"" + outside.wstring() + L"\"";
  STARTUPINFOW si{sizeof(si)};
  PROCESS_INFORMATION pi{};
  bool made = false;
  if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    WaitForSingleObject(pi.hProcess, 10000);
    DWORD code = 1;
    made = GetExitCodeProcess(pi.hProcess, &code) && code == 0;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
  }
  // (Wine 9's mklink answers 0 without making the junction: no folder there, nothing to check)
  if (made && std::filesystem::is_directory(link)) {
    assert(!remote_path_allowed(utf8(root), utf8(link / "secret.mkv")));
  } else {
    std::puts("remote path: junction not available, link check skipped");
  }
  if (std::filesystem::exists(link)) RemoveDirectoryW(link.c_str());  // never let remove_all follow a link
  fs::remove_all(base, ec);
  std::puts("Remote path: normal files allowed; outside, sibling and junction paths refused");
  return 0;
}
