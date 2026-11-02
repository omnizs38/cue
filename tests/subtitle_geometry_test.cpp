#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>
#define private public
#include "../src/ui/play.h"
#undef private
#include "../src/core/subtitle_geometry.h"
#include "../src/core/sub_style.h"
#include <cassert>
#include <cstdio>
#include <limits>

static double property(const std::vector<PropSet>& props,const std::string& name) {
  for(const auto& p:props)if(p.names[0]==name)return std::stod(p.value);
  assert(false);return 0;
}
int main() {
  assert(subtitle_typography_scale(0,720)==1&&subtitle_typography_scale(1920,0)==1);
  assert(subtitle_typography_scale(1280,720)==1);
  assert(subtitle_typography_scale(540,960)==1);
  assert(subtitle_typography_scale(1024,768)==1);
  assert(subtitle_typography_scale(100000,1)==2);
  for(int width:{960,1280,1920,3840})for(int size:{28,44,60,72}) {
    SubStyle st;st.size=size;st.line_gap=4;st.tracking=2;
    auto base=sub_style_props(st,SubKind::Text);
    const double ref_h=width*9./16.;
    for(double aspect:{16./9.,1.85,2.,2.35,2.39,2.40,32./9.}) {
      int height=(int)std::lround(width/aspect);
      auto scaled=sub_style_props(st,SubKind::Text,subtitle_typography_scale(width,height));
      assert(property(scaled,"sub-line-spacing")==property(base,"sub-line-spacing"));
      double pixels=property(scaled,"sub-font-size")*height/720.;
      assert(std::abs(pixels-size*ref_h/720.)<.05);
      assert(std::abs(property(scaled,"sub-shadow-offset")*height/720.-4*ref_h/720.)<.05);
      for(const auto& track:{SubKind::Ass,SubKind::Bitmap}) {
        auto authored=sub_style_props(st,track,subtitle_typography_scale(width,height));
        assert(property(authored,"sub-font-size")==size);
      }
      st.ass_mode=1;
      assert(property(sub_style_props(st,SubKind::Ass,subtitle_typography_scale(width,height)),"sub-font-size")==property(scaled,"sub-font-size"));
      st.ass_mode=0;
      PlayView p;p.in.style=st;p.in.sid=1;p.in.kind=SubKind::Text;
      p.in.sub_lines=2;p.in.sub_text={"a reading size that stays consistent","second line"};
      p.resize(width,height,1);p.reset(1);
      std::vector<Rect> lines;assert(p.sub_line_rects(st.pos,lines));
      assert(std::abs(lines[0].h-pixels)<=1);
      // Drag bounds use the same font geometry as the engine, including spacing/padding.
      assert(p.sub_rect_at(st.pos).h>=lines.back().b()-lines.front().y);
      assert(st.size==size); // compensation never writes back into the saved setting
    }
    st.bg_op=0;
    auto outline=sub_style_props(st,SubKind::Text,1.35);
    assert(std::abs(property(outline,"sub-outline-size")-st.outline*.9*1.35)<.011);
  }
  // No threshold jump around 16:9, nor drift when repeatedly changing formats.
  for(int h=719;h<=721;++h)assert(std::abs(44*subtitle_typography_scale(1280,h)*h/720.-44)<.07);
  for(int i=0;i<10000;++i){assert(subtitle_typography_scale(1920,1080)==1);assert(subtitle_typography_scale(1920,800)==1.35);}
  assert(property(sub_style_props(SubStyle{},SubKind::Text,std::numeric_limits<double>::quiet_NaN()),"sub-font-size")==44);
  puts("Subtitle scale: same reading size at equal widths across 16:9 to 32:9, 4K, font sizes, drag bounds, outlines, ASS preservation, invalid geometry and repeated changes passed");
}
