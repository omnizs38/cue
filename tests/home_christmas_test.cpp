// The home's surprises for Christmas: snow that lands on the eyes, a Santa hat, a string of
// lights. Each one draws, ends on time and leaves the eyes as they were; the old surprises'
// "seen" marks do not count for the new ones.
#include "../src/ui/home.h"
#include "../src/core/i18n.h"
#include <cassert>
#include <cstdio>

static void run(HomeView& hv, double& now, double secs) {
  for (double t = 0; t < secs; t += 1 / 60.0) { now += 1 / 60.0; hv.tick(now, 1 / 60.0); }
}
// how many pixels of a colour family (red, green, gold or blue light; white snow)
static int count(const Canvas& c, bool (*f)(int, int, int)) {
  int n = 0;
  for (uint32_t v : c.px) if (f((v >> 16) & 255, (v >> 8) & 255, v & 255)) ++n;
  return n;
}

int main() {
  i18n::set_language(i18n::Lang::PtBR);
  HomeView hv;
  hv.set_size(960, 540, 2);
  hv.set_rows({{"Cena", "faltam 41 min", 0.5, "vídeo"}});
  Canvas c;
  double now = 10;
  run(hv, now, 1);
  hv.render(c, now);
  const int white_idle = count(c, [](int r, int g, int b) { return r > 230 && g > 230 && b > 230; });
  // the old (Halloween) marks: every new surprise is still unseen
  hv.eggs_seen = 2 | 4 | 8;
  for (int v = 1; v <= 3; ++v) {
    double t0 = now;
    hv.force_egg(v, now);
    int red = 0, colourful = 0, white = 0;
    for (double t = 0; t < 5.6; t += 0.1) {
      run(hv, now, 0.1);
      hv.render(c, now);
      red = std::max(red, count(c, [](int r, int g, int b) { return r > 180 && g < 90 && b < 90; }));
      colourful = std::max(colourful, count(c, [](int r, int g, int b) { return (g > 160 && r < 120) || (b > 200 && r < 120); }));
      white = std::max(white, count(c, [](int r, int g, int b) { return r > 230 && g > 230 && b > 230; }));
    }
    if (v == 1) assert(white > white_idle + 40);   // snow
    if (v == 2) assert(red > 200);                  // the hat
    if (v == 3) assert(colourful > 50 && red > 20); // the lights
    assert(now - t0 > 5.5);
    // it ended: the eyes are back to themselves
    assert(!hv.face.happy && !hv.face.wide && !hv.face.closed);
    assert(hv.eggs_seen & (1 << (v + 4)));
    run(hv, now, 1);
  }
  std::puts("home christmas: passed");
}
