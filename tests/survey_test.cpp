// The survey's invitation on the home: a card in the bottom-right corner, only where it covers
// nothing, in every language; "answer" and "no thanks" by click and by keyboard; gone as the home
// leaves; its little scene writes twice and rests (no endless redraws); the answer is kept.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include "../src/core/store.h"
#define private public
#include "../src/ui/home.h"
#include "../src/core/i18n.h"

static void run(HomeView& hv, double& now, double secs) {
  for (double t = 0; t < secs; t += 1 / 60.0) { now += 1 / 60.0; hv.tick(now, 1 / 60.0); }
}
static bool overlap(const Rect& a, const Rect& b) { return a.w > 0 && b.w > 0 && a.x < b.r() && b.x < a.r() && a.y < b.b() && b.y < a.b(); }

int main() {
  int shown = 0, hidden = 0;
  for (int lang = 0; lang < i18n::kLangCount; ++lang)
    for (auto [W, H] : {std::pair{480, 270}, std::pair{800, 445}, std::pair{960, 540}, std::pair{1400, 600}, std::pair{640, 600}})
      for (int nrows : {0, 2, 5}) {
        i18n::set_language((i18n::Lang)lang);
        HomeView hv;
        hv.set_size(W * 2, H * 2, 2);
        std::vector<HomeRow> rows;
        for (int i = 0; i < nrows; ++i) rows.push_back({"Cena de exemplo", "faltam 41 min", 0.5, "vídeo"});
        hv.set_rows(rows);
        double now = 10;
        run(hv, now, 0.6);
        assert(!hv.survey_shown());
        hv.survey = true;
        run(hv, now, 0.6);
        if (!hv.survey_fits_) { assert(!hv.survey_shown()); ++hidden; continue; }
        ++shown;
        assert(hv.survey_shown());
        const Rect r = hv.survey_rect();
        // inside the window, clear of everything of the home's own
        assert(r.x >= 0 && r.y >= 0 && r.r() <= W * 2 && r.b() <= H * 2);
        for (const Rect& o : {hv.r_list_, hv.r_open_, hv.face.bounds(), hv.r_title_}) assert(!overlap(r, o));
        // the words fit the card, the answers inside it
        const Rect g = hv.survey_answer_rect(), n = hv.survey_close_rect();
        assert(g.x >= r.x && n.r() <= r.r() && g.b() <= r.b() && n.r() <= r.r() && g.r() <= n.x);
        // by click: answer / no thanks; the card is not the window's background
        assert(!hv.is_background(r.x + 4, r.y + 4));
        assert(hv.mouse_down(g.x + 2, g.y + 2, now).a == HomeAct::Survey);
        assert(hv.mouse_down(n.x + 2, n.y + 2, now).a == HomeAct::SurveyClose);
        // by keyboard: reachable with tab
        bool go = false, no = false;
        for (int i = 0; i < 40 && !(go && no); ++i) {
          hv.key("tab", false, now);
          go |= hv.focus_ == HomeView::kSurveyGo;
          no |= hv.focus_ == HomeView::kSurveyNo;
        }
        assert(go && no);
        hv.focus_ = HomeView::kSurveyNo;
        assert(hv.key("enter", false, now).a == HomeAct::SurveyClose);
        // the scene writes twice, then rests: frames 8 a second, then only a blink now and then
        hv.mouse_leave(now);
        const double t0 = hv.survey_t0_;
        assert(t0 > 0 && hv.survey_step(t0 + 0.2) >= 0 && hv.survey_step(t0 + 9) < 0);
        assert(hv.next_event(t0 + 1.01) <= 0.13);
        {  // resting: the card asks for the next blink only (the face has its own events)
          const double tr = t0 + 9.3, ph = std::fmod(tr, 4.6), blink = ph < 0.12 ? 0.12 - ph : 4.6 - ph;
          assert(std::abs(hv.next_event(tr) - std::max(0.0, std::min(hv.face.next_event(tr), blink))) < 1e-9);
        }
        // the home leaves (a file opens): the card goes with it
        hv.survey = false;
        run(hv, now, 0.6);
        assert(!hv.survey_shown());
      }
  assert(shown > 0 && hidden > 0);
  // kept: "" until answered or declined
  {
    auto file = std::filesystem::temp_directory_path() / "cue-survey.json";
    std::filesystem::remove(file);
    Store a; assert(a.survey.empty() && a.installed == 0);
    a.survey = "declined"; a.installed = 1780000000; a.save(file.string());
    Store b; b.load(file.string()); assert(b.survey == "declined" && b.installed == 1780000000);
    std::filesystem::remove(file); std::filesystem::remove(file.string() + ".bak");
  }
  std::printf("Survey: the home's card in %d windows (left out of %d where it would cover something), answers by click and keyboard, a scene that rests, kept passed\n", shown, hidden);
}
