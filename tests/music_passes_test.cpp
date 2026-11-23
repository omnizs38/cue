// The music's shader passes (src/win/blur_shader.h), read as the engine reads them: which passes run
// in each state of the music player (their //!WHEN, evaluated like libplacebo's RPN), how many of
// them cover the whole window, and that the merged passes keep each step's order (1.4.1: the room's
// light inside the light show; the cover, the edges, the list's veil and black and white in one pass).
#include "../src/win/blur_shader.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <sstream>
#include <string>
#include <vector>

struct Pass { std::string when, desc, body; bool full = true; };

static std::vector<Pass> passes() {
  std::vector<Pass> out;
  std::istringstream in(kBlurCloudShader);
  std::string line;
  while (std::getline(in, line)) {
    if (line.rfind("//!HOOK", 0) == 0) { out.push_back({}); continue; }
    if (out.empty()) continue;
    if (line.rfind("//!WHEN ", 0) == 0) out.back().when = line.substr(8);
    else if (line.rfind("//!DESC ", 0) == 0) out.back().desc = line.substr(8);
    else if (line.rfind("//!WIDTH", 0) == 0) out.back().full = false;
    else out.back().body += line + "\n";
  }
  return out;
}

// libplacebo's //!WHEN: reverse Polish, numbers and parameters, + - * / > < = and ! (not)
static double rpn(const std::string& e, const std::map<std::string, double>& p) {
  std::vector<double> st;
  std::istringstream in(e);
  std::string t;
  while (in >> t) {
    auto pop = [&] { assert(!st.empty()); double v = st.back(); st.pop_back(); return v; };
    if (t == "!") { st.push_back(pop() == 0.0 ? 1.0 : 0.0); continue; }
    if (t == "+" || t == "-" || t == "*" || t == "/" || t == ">" || t == "<" || t == "=") {
      double b = pop(), a = pop();
      st.push_back(t == "+" ? a + b : t == "-" ? a - b : t == "*" ? a * b : t == "/" ? a / b : t == ">" ? a > b : t == "<" ? a < b : a == b);
      continue;
    }
    if ((t[0] >= '0' && t[0] <= '9') || t[0] == '.') { st.push_back(std::stod(t)); continue; }
    auto it = p.find(t);
    st.push_back(it == p.end() ? 0.0 : it->second);  // (every PARAM starts at 0)
  }
  assert(st.size() == 1);
  return st.back();
}

static int full_passes(const std::vector<Pass>& all, const std::map<std::string, double>& p, std::string* names = nullptr) {
  int n = 0;
  for (const auto& x : all)
    if (rpn(x.when, p) != 0.0 && x.full) { ++n; if (names) *names += "  " + x.desc.substr(0, 40) + "\n"; }
  return n;
}

int main() {
  const auto all = passes();
  assert(all.size() >= 10);
  for (const auto& x : all) assert(!x.when.empty());
  // a song with its cover, the centred window: the room's light, the light show, the cover's
  // shadow and the edges (1.4.1 and before: four passes over the whole window)
  std::map<std::string, double> centre{{"bg_on", 1}, {"lit_on", 1}, {"cov_on", 1}, {"edge_on", 1}};
  std::string names;
  assert(full_passes(all, centre, &names) == 2);
  // the cover alone (no light show): the room's light on its own, then the cover's pass
  assert(full_passes(all, {{"bg_on", 1}, {"cov_on", 1}, {"edge_on", 1}}) == 2);
  // the spectrum alone, the list open, in black and white: the light show, then one pass
  assert(full_passes(all, {{"lit_on", 1}, {"edge_on", 1}, {"veil", 0.5}, {"mono", 1}}) == 2);
  // the list's veil or black and white alone still run (they were passes of their own)
  assert(full_passes(all, {{"veil", 0.3}}) == 1 && full_passes(all, {{"mono", 0.2}}) == 1);
  assert(full_passes(all, {}) == 0);
  // the room's light never runs twice: on its own only without the light show
  int bg_alone = 0;
  for (const auto& x : all) if (x.body.find("bg_room(o.rgb, HOOKED_pos") != std::string::npos) { ++bg_alone; assert(rpn(x.when, centre) == 0.0); }
  assert(bg_alone == 1);
  // the light passes read the picture as the room's light leaves it, wherever they read it
  for (const auto& x : all)
    if (x.desc.find("music light") != std::string::npos && x.body.find("HOOKED") != std::string::npos && x.desc.find("throw") == std::string::npos) {
      const auto hook = x.body.substr(x.body.find("vec4 hook()"));
      for (size_t at = hook.find("HOOKED_tex("); at != std::string::npos; at = hook.find("HOOKED_tex(", at + 1))
        assert(hook.compare(at, 26, "HOOKED_tex(HOOKED_pos);\n  ") == 0 || hook.compare(at, 23, "HOOKED_tex(HOOKED_pos);") == 0);
      assert(hook.find("room(") != std::string::npos);
    }
  // the merged pass keeps the order: cover, edges, veil, black and white
  for (const auto& x : all)
    if (x.when.find("cov_on") == 0) {
      const auto hook = x.body.substr(x.body.find("vec4 hook()"));
      const size_t c = hook.find("cov_pass()"), e = hook.find("edge_on > 0.0"), v = hook.find("veil > 0.0"), m = hook.find("mono_pass(o)");
      assert(c != std::string::npos && c < e && e < v && v < m);
    }
  std::printf("music passes: centred song 4 -> 2 full-window passes:\n%s", names.c_str());
}
