// Renders the first-run screens to PNG (and frame sequences for GIFs) on any OS.
//   g++ -std=c++20 -O2 -o preview_ob tools/preview_onboarding.cpp src/ui/*.cpp src/core/*.cpp
//   ./preview_ob out 2 [frames]
#include <cstdio>
#include <cstdlib>
#include <string>
#include "../src/core/i18n.h"

#include "../src/ui/onboarding.h"
#include "../src/core/ui_scale.h"

void save_canvas(const Canvas& c, const std::string& name) {
  std::string raw = name + ".rgba";
  FILE* f = std::fopen(raw.c_str(), "wb");
  std::vector<unsigned char> buf((size_t)c.w * c.h * 4);
  for (size_t i = 0; i < c.px.size(); ++i) {
    uint32_t v = c.px[i];
    unsigned a = v >> 24;
    auto un = [&](unsigned x) { return a ? std::min(255u, (x * 255 + a / 2) / a) : 0u; };
    buf[i * 4 + 0] = (unsigned char)un((v >> 16) & 255);
    buf[i * 4 + 1] = (unsigned char)un((v >> 8) & 255);
    buf[i * 4 + 2] = (unsigned char)un(v & 255);
    buf[i * 4 + 3] = (unsigned char)a;
  }
  std::fwrite(buf.data(), 1, buf.size(), f);
  std::fclose(f);
  std::string cmd = "convert -size " + std::to_string(c.w) + "x" + std::to_string(c.h) + " -depth 8 rgba:" + raw + " " + name +
                    " && rm " + raw;
  if (std::system(cmd.c_str()) != 0) std::fprintf(stderr, "convert failed\n");
}

int main(int argc, char** argv) {
  // CUE_LANG=en|es|fr|zh-Hans picks the interface language (Portuguese by default)
  { i18n::Lang l = i18n::Lang::PtBR; if (const char* e = std::getenv("CUE_LANG")) i18n::parse_tag(e, l); i18n::set_language(l); }
  std::string out = argc > 1 ? argv[1] : "out";
  double s = argc > 2 ? std::atof(argv[2]) : 2;
  bool frames = argc > 3 && std::string(argv[3]) == "frames";
  int W = argc > 4 ? std::atoi(argv[4]) : (int)(480 * s), H = argc > 5 ? std::atoi(argv[5]) : (int)(270 * s);
  OnboardingView ob;
  ob.set_size(W, H, s);
  double now = 10, dt = 1 / 60.0;
  ob.start(now);
  Canvas c;
  auto run = [&](double secs, const std::string& seq = "") {
    int n = (int)std::lround(secs / dt);
    for (int i = 0; i < n; ++i) {
      now += dt;
      ob.tick(now, dt);
      if (!seq.empty() && i % 2 == 0) {
        char b[64]; std::snprintf(b, sizeof b, "%s-%04d.png", seq.c_str(), i / 2);
        ob.render(c, now); save_canvas(c, out + "/" + b);
      }
    }
  };
  if (frames) {
    run(4.2, "intro");
    ob.go(1, now); run(1.4, "page1");
    ob.set_ambient(false, now); run(1.2, "toggle-off");
    ob.set_ambient(true, now); run(1.0, "toggle-on");
    ob.go(2, now); run(1.0, "page2");
    ob.set_controls_free(true, now); run(0.8, "background");
    ob.go(3, now); run(1.0, "page3");
    ob.set_scale_choice(2, now); run(0.8, "size");
    return 0;
  }
  run(0.6); ob.render(c, now); save_canvas(c, out + "/ob-intro-early.png");
  run(3.0); ob.render(c, now); save_canvas(c, out + "/ob-intro.png");
  ob.go(1, now); run(0.18); ob.render(c, now); save_canvas(c, out + "/ob-eyes-close.png");
  run(0.2); ob.render(c, now); save_canvas(c, out + "/ob-slide.png");
  run(1.2); ob.render(c, now); save_canvas(c, out + "/ob-ambient.png");
  ob.light_inside = true; run(0.1); ob.render(c, now); save_canvas(c, out + "/ob-ambient-inside.png"); ob.light_inside = false;
  ob.set_ambient(false, now); run(1.2); ob.render(c, now); save_canvas(c, out + "/ob-ambient-off.png");
  ob.set_ambient(true, now);
  ob.go(2, now); run(1.2); ob.render(c, now); save_canvas(c, out + "/ob-background.png");
  ob.set_controls_free(true, now); run(0.8); ob.render(c, now); save_canvas(c, out + "/ob-background-without.png");
  ob.go(3, now); run(1.2); ob.render(c, now); save_canvas(c, out + "/ob-size-without.png");
  ob.go(2, now); run(1.0); ob.set_controls_free(false, now); run(0.8);
  ob.go(3, now); run(1.2); ob.render(c, now); save_canvas(c, out + "/ob-size.png");
  const char* names[4] = {"pequena", "media", "grande", "maior"};
  for (int i = 0; i < 4; ++i) { ob.set_scale_choice(ui_size_choice(i), now); run(0.5); ob.render(c, now); save_canvas(c, out + "/ob-size-" + names[i] + ".png"); }
  ob.set_scale_choice(ui_size_choice(1), now); run(0.5);
  ob.mouse_move(ob.w - 40 * s, ob.h - 20 * s, now); run(0.3); ob.render(c, now); save_canvas(c, out + "/ob-size-hover.png");
  ob.mouse_leave(now);
  ob.finish(now, 0, 0, 2); run(0.1); ob.render(c, now); save_canvas(c, out + "/ob-finish.png");
  std::puts("ok");
}
