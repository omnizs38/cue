// The card's cover drawn by Cue (the spectrum-alone look has no cover in the engine's picture):
// a layer under everything else, the card's square opaque where the cover is, its shadow round
// it, drawn again only for a new cover or a new place, gone when it is not wanted.
#include <cassert>
#include <cstdio>
#include <memory>
#define private public
#include "../src/ui/play.h"

static void settle(PlayView& p, double& t) { for (int i = 0; i < 20; ++i) { p.changed(t); t += 1 / 60.; p.tick(t, 1 / 60.); } }

static std::shared_ptr<const CoverArt> art_of(int side, uint32_t colour) {
  auto a = std::make_shared<CoverArt>();
  a->side = side;
  a->px.assign((size_t)side * side, colour);
  return a;
}

int main() {
  for (auto [W, H, s] : {std::tuple{640, 337, 1.5}, std::tuple{960, 505, 2.5}, std::tuple{480, 253, 1.0}}) {
    PlayView p;
    p.in.mini_player = true; p.in.audio_only = true; p.in.cover_scene = true; p.in.dur = 200;
    p.in.music_title = "DNA"; p.in.music_artist = "Number Teddie"; p.in.music_scene = MusicScene::wide();
    p.set_base_scale(s); p.resize(W, H, s);
    double t = 1; p.reset(t); settle(p, t);
    assert(p.card_ && p.card_cover_.w > 8);
    const Layer& L = p.layers[PlayView::L_ART];
    assert(!L.visible);
    // read and shown: over the card's square, opaque in it, a soft shadow round it
    const Rect c = p.card_cover_;
    p.in.card_art = art_of(c.w, 0xFF8040C0u); p.in.card_art_show = true; settle(p, t);
    assert(L.visible && L.x <= c.x && L.y <= c.y && L.x + L.c.w >= c.r() && L.y + L.c.h >= c.b());
    auto at = [&](int x, int y) { return L.c.px[(size_t)(y - L.y) * L.c.w + (x - L.x)]; };
    assert((at(c.cx(), c.cy()) >> 24) == 255);
    const uint32_t sh = at(c.cx(), std::min(c.b() + 3, L.y + L.c.h - 1));
    assert((sh >> 24) > 0 && (sh >> 24) < 255);
    // nothing new: not drawn again
    const CoverArt* drawn = p.art_drawn_;
    p.layers[PlayView::L_ART].dirty = false;
    settle(p, t);
    assert(!p.layers[PlayView::L_ART].dirty && p.art_drawn_ == drawn);
    // a new cover: drawn again
    p.in.card_art = art_of(c.w, 0xFF20A060u); settle(p, t);
    assert(p.layers[PlayView::L_ART].dirty && p.art_drawn_ == p.in.card_art.get());
    // a turn draws it (or the full player): gone
    p.in.card_art_show = false; settle(p, t);
    assert(!L.visible);
    p.in.card_art_show = true; settle(p, t);
    assert(L.visible);
    p.in.mini_player = false; p.resize(W * 2, H * 2, s); settle(p, t);
    assert(!L.visible);
  }
  std::puts("Card art: the card's cover drawn by Cue over its square, with its shadow, redrawn only when it changes passed");
}
