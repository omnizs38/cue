// A phone video's window (portrait): its bar is raised to a share of the width so the controls are
// not lost under a tall picture. The result is about 15% smaller than in the first 1.3 builds (0.72 to
// 0.94 of the width at the four sizes): the bar and its menus took too much of the picture.
#include "../src/core/ui_scale.h"
#include <cassert>
#include <cmath>
#include <cstdio>

int main() {
  const double before[kUiSizes] = {0.72, 0.80, 0.88, 0.94};
  int checked = 0;
  for (int i = 0; i < kUiSizes; ++i) {
    const int choice = ui_size_choice(i);
    for (int cw : {405, 540, 607, 720, 810, 1080, 1440})
      for (double ar : {16.0 / 9, 2.0, 1.5, 1.25, 1.2}) {
        const int ch = (int)std::lround(cw * ar);
        for (double base : {1.0, 2.0, 3.0, 4.0}) {
          const double old_want_b = std::floor(cw * before[i] / kCompactDockW * 2) / 2;
          const double old_ps = std::max(base, std::min(old_want_b, base + 2));
          const double ps = portrait_scale(base, cw, ch, choice);
          assert(ps <= old_ps + 1e-9 && ps >= 1);                       // never bigger than before
          assert(std::fabs(ps * 2 - std::round(ps * 2)) < 1e-9);         // whole or half art px
          // a visible step down from 2 px up, about 15% (half art px allowing), also where the
          // window's own scale set it (a 4:5 or 3:4 window)
          if (old_ps >= 2) {
            assert(ps < old_ps);
            assert(std::fabs(ps / old_ps - 0.85) <= 0.5 / old_ps + 1e-9);  // within a half step
          }
        }
        ++checked;
      }
  }
  // a little taller than wide (5:6): the landscape dock's rule, 15% off; square and wider: unchanged
  assert(portrait_scale(3.0, 1000, 1100, kDefaultUiSize) < portrait_scale(3.0, 1100, 1000, kDefaultUiSize) ||
         portrait_scale(3.0, 1100, 1000, kDefaultUiSize) < 2);
  assert(portrait_scale(2.5, 1675, 1675, kDefaultUiSize) == 3.0 && portrait_scale(2.5, 1675, 942, kDefaultUiSize) == 2.5);
  std::printf("Portrait bar: %d vertical windows (9:16 to 5:6) at four sizes and four window scales, bar about 15%% smaller passed\n", checked);
}
