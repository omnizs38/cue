// A maximized window stays maximized when the next file, song or the home asks for another shape
// (issue #9): the new shape only becomes the place it restores to. Checks the placement math and
// that the window code no longer restores a maximized window to adapt it.
#include "../src/core/video_window.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

int main(int argc, char** argv) {
  std::filesystem::path exe = argc > 0 ? argv[0] : "";
  const WinRect work{0, 0, 1920, 1040};
  // grown from the old place's centre, the shape of the new file
  auto size = video_window_size(16.0 / 9, 1920, 1040, 96, 1);
  auto r = video_window_rect({400, 200, 1400, 800}, work, size, 0, 0);
  assert(r.right - r.left == size.w && r.bottom - r.top == size.h);
  assert(std::abs((r.left + r.right) / 2 - 900) <= 1 && std::abs((r.top + r.bottom) / 2 - 500) <= 1);
  // never outside the work area
  r = video_window_rect({1700, 900, 1900, 1030}, work, size, 0, 0);
  assert(r.left >= 0 && r.top >= 0 && r.right <= 1920 && r.bottom <= 1040);
  r = video_window_rect({-300, -200, 100, 50}, work, size, 0, 0);
  assert(r.left >= 0 && r.top >= 0);
  // a taller-than-wide film: still inside, portrait shape
  auto tall = video_window_size(9.0 / 16, 1920, 1040, 96, 1);
  r = video_window_rect({0, 0, 1920, 1040}, work, tall, 0, 0);
  assert(r.bottom - r.top > r.right - r.left && r.top >= 0 && r.bottom <= 1040);
  // a work area that does not start at 0,0 (second monitor, taskbar on top)
  const WinRect second{1920, 48, 3840, 1080};
  r = video_window_rect({2200, 300, 3000, 800}, second, size, 0, 0);
  assert(r.left >= 1920 && r.top >= 48 && r.right <= 3840 && r.bottom <= 1080);
  // the invisible borders may hang over the edges, as before
  r = video_window_rect({0, 0, 400, 300}, work, size, 16, 8);
  assert(r.left == -8 && r.top == 0);

  // the window code: adapting never restores a maximized window any more
  // Resolve the source tree from the executable's place (build/tests/…).
  std::filesystem::path root = std::filesystem::absolute(exe).parent_path().parent_path().parent_path();
  std::ifstream in(root / "src/win/app.cpp");
  std::stringstream ss; ss << in.rdbuf();
  std::string s = ss.str();
  auto a = s.find("void App::adapt_window(double ar)");
  auto b = s.find("void App::animate_window_to", a);
  assert(a != std::string::npos && b != std::string::npos);
  std::string body = s.substr(a, b - a);
  assert(body.find("SW_RESTORE") == std::string::npos);
  assert(body.find("SW_SHOWMAXIMIZED") != std::string::npos && body.find("rcNormalPosition") != std::string::npos);
  std::puts("maximized window: passed");
}
