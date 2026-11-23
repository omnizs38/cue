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
  // (1.4.1) a part of the picture: the size follows the picture (its reading side B), larger in
  // proportion, smaller at a third of the rate, a line of 64 letters always inside the width the
  // engine breaks at; the window Cue opens in (1447 px, 4K at 175%)
  const double R=1447;
  auto fsz=[&](int size,int w,int h,double ar,double track=0){return size*h/720.0*subtitle_typography_scale(w,h,ar,R,size,track);};
  auto fs=[&](int w,int h,double ar){return fsz(52,w,h,ar);};
  auto near=[](double a,double b){return std::fabs(a-b)<0.6;};
  assert(near(fs(2573,1447,16.0/9),104.5) && near(fs(3840,2160,16.0/9),156));    // the opening window, full screen: in proportion
  assert(near(fs(3840,2088,16.0/9),150.8));                                         // maximized: by the picture, not the window
  assert(near(fs(1920,2088,16.0/9),83.2) && near(fs(1280,2088,16.0/9),55.5));     // half, a third: held by the 64-letter line
  assert(near(fs(960,540,16.0/9),41.6) && near(fs(1021,1814,9.0/16),73.7));       // a small window; portrait: by its own width
  // the author's size (32) in his drawing's windows: a third of the rate (the cube root)
  assert(near(fsz(32,1920,2088,16.0/9),64.3*std::cbrt(1080/1447.0)) && near(fsz(32,1487,1445,16.0/9),64.3*std::cbrt(836.4/1447.0)));
  assert(std::fabs(subtitle_typography_scale(600,1000,4.0/3,R)-0.45)<1e-9);       // 4:3: its own height (450 of 1000), no size given
  assert(subtitle_typography_scale(540,960,0,R)==1);                                // shape not known yet: as before
  assert(subtitle_typography_scale(640,360,16.0/9,0)==1);                           // the mini player: its own rule, as before
  { // every size of the style (24 to 80) and letter spacing, every window and shape
    for(int size:{24,28,32,44,52,60,72,80}) for(int track:{-3,0,4,10}) for(double ar:{16.0/9,2.39,1.85,4.0/3,1.0,9.0/16}) {
      double above=-1, last=-1;
      for(int w=320;w<=3840;w+=97) for(int h=240;h<=2160;h+=83) {
        const double f=fsz(size,w,h,ar,track), prop=size*subtitle_picture_base(w,h,ar)/720.0, wrap=subtitle_wrap_width(w,h,ar);
        assert(std::isfinite(f) && f>0);
        assert(f>=prop*(1-1e-9) || f>=0.05*h*size/720.0-1e-9);              // never smaller than in proportion
        const double em64=22.65+64*track/44.0;
        assert(f*em64<=wrap*(1+1e-9) || std::fabs(f-prop)<1e-6*prop+1e-9);   // 64 letters fit, or the zoom keeps the opening window's breaks
        if(subtitle_picture_base(w,h,ar)>=R) {                                // above the opening window: a zoom (same breaks)
          const double r=f/wrap; if(above<0) above=r; else assert(std::fabs(r/above-1)<1e-9);
        }
      }
      // continuous and growing with the window: no jump at the opening window
      for(int k=600;k<=2200;++k){ const double f=fsz(size,k*16/9,k,16.0/9,track); if(last>0) assert(f>=last-1e-9 && f-last<0.5); last=f; }
    }
    // the engine's margin gives that width (its margin is in px of a 720 px window)
    for(int w:{960,1920,1280,3840,1021}) for(int h:{540,1447,2088,1814}) for(double ar:{16.0/9,9.0/16,2.39}) {
      const double m=subtitle_margin_x(w,h,ar,R);
      assert(m>=0 && std::fabs((w-2*m*h/720.0)-subtitle_wrap_width(w,h,ar))<1e-6 || subtitle_wrap_width(w,h,ar)>w);
    }
    assert(std::fabs(subtitle_margin_x(2573,1447,16.0/9,R)-12)<0.2);   // the opening window: 12, as chosen
    assert(subtitle_margin_x(640,360,16.0/9,0)==25 && subtitle_margin_x(360,640,9.0/16,0)==8);  // the mini player: as before
  }
  { // line spacing: a share of the letters, the same at every size; 0 the face's own; the tightest
    // keeps a face's descenders clear of the next line's accents
    assert(subtitle_line_gap_share(0,1)==0 && std::fabs(subtitle_line_gap_share(24,1)-24/44.0)<1e-12);
    for(int font=0;font<3;++font) {
      assert(subtitle_line_gap_share(-16,font)<0 && subtitle_line_gap_share(-16,font)>=-0.30);
      for(int g=-16;g<24;++g) assert(subtitle_line_gap_share(g,font)<subtitle_line_gap_share(g+1,font));
    }
    SubStyle st; st.line_gap=-16;
    for(double px:{30.0,88.0,156.0}) {
      const double v=property(sub_style_props(st,SubKind::Text,1.0,1.0,px),"sub-line-spacing");
      assert(std::fabs(v/px-subtitle_line_gap_share(-16,1))<0.01);
    }
  }
  { // the size and the letter spacing reach the engine through its sub-scale: spacing from -10 to 10
    // only (10 at a size of 52, or a larger window, was refused and the letters closed up)
    SubStyle st; st.size=72; st.tracking=10;
    auto pr=sub_style_props(st,SubKind::Text,1.8);
    assert(property(pr,"sub-font-size")==44 && property(pr,"sub-spacing")==10);
    assert(std::fabs(property(pr,"sub-scale")-72/44.0*1.8)<1e-3);
    auto ass=sub_style_props(st,SubKind::Ass,1.8);  // ASS in its own style: the file's
    assert(property(ass,"sub-font-size")==72 && property(ass,"sub-scale")==1);
  }
  { // (1.4.1) where the engine puts the block (measured): its foot by the position, whatever the size
    // the last line's baseline, measured in the engine (22/11 19:30): size, position, window, the H's foot
    const double measured[][4]={{64.3,30,1447,405},{92.8,30,2088,585},{64.3,73.4,1447,1013},{92.8,73.4,2088,1463},{64.3,93,1447,1288},
      {92.8,93,2088,1859},{64.3,100,1447,1386},{144.7,30,1447,368},{208.8,30,2088,532},{144.7,73.4,1447,951},{208.8,73.4,2088,1372},
      {144.7,93,1447,1214},{208.8,93,2088,1752},{208.8,100,2088,1887},{104.5,93,1447,1251},{104.5,30,1447,387}};
    for(const auto& m:measured) assert(std::fabs(subtitle_baseline(m[2],m[1],m[0])-m[3])<1.5);
    for(double pos:{60.0,80.0,93.0,100.0}) assert(std::fabs(subtitle_pos_for_bottom(2088,subtitle_block_bottom(2088,pos,80),80)-pos)<1e-9);
    // (1.4.1) the position is the picture's: at the opening window as before, with bands inside the
    // picture at the same place in it, under it when dragged there
    for(double fs:{40.0,80.0,160.0}) for(int rows:{1,2,4}) {
      const double gap=0.1*fs;
      assert(subtitle_window_pos(93,2573,1447,16.0/9,fs,gap,rows)==93);             // no band: as before
      assert(subtitle_window_pos(150,2573,1447,16.0/9,fs,gap,rows)==kSubPosWindowFoot);  // under the picture, no band: the window's foot
      const double pic_top=(2088-1080)/2.0, pic_foot=pic_top+1080;
      const double w93=subtitle_window_pos(93,1920,2088,16.0/9,fs,gap,rows);
      assert(std::fabs(subtitle_block_bottom(2088,w93,fs)-(pic_top+subtitle_block_bottom(1080,93,fs)))<1e-6);  // inside, where it is in the picture
      assert(subtitle_block_bottom(2088,w93,fs)<pic_foot);
      for(double p=12;p<=100;p+=0.5) assert(std::fabs(subtitle_picture_pos(subtitle_window_pos(p,1920,2088,16.0/9,fs,gap,rows),1920,2088,16.0/9,fs,gap,rows)-p)<1e-6);
      for(double p=100.5;p<=200;p+=0.5) {
        const double w=subtitle_window_pos(p,1920,2088,16.0/9,fs,gap,rows), bh=rows*fs+(rows-1)*gap;
        const bool fits=pic_foot+0.25*fs+bh<=subtitle_block_bottom(2088,kSubPosWindowFoot,fs);
        assert(w<=kSubPosWindowFoot+1e-9);
        if(fits) {  // under the picture, never over it (a block taller than the band: at the window's foot)
          assert(subtitle_block_bottom(2088,w,fs)-bh>=pic_foot+0.25*fs-1e-6);
          assert(std::fabs(subtitle_picture_pos(w,1920,2088,16.0/9,fs,gap,rows)-p)<1e-6);
        }
      }
      double last=-1;  // continuous while an edge is dragged: no jump
      for(int h=1100;h<=2088;h+=3){ const double p=subtitle_window_pos(93,1920,h,16.0/9,fs,gap,rows); if(last>=0) assert(std::fabs(p-last)<0.5); last=p; }
    }
    assert(subtitle_window_pos(93,0,0,16.0/9,80,0,2)==93 && subtitle_window_pos(150,1920,2088,0,80,0,2)==kSubPosWindowFoot);  // shape not known
  }
  { // dragged in the player: from inside the picture into the band under it, at the author's size
    // and the largest; then in every window it stays under the picture (or at a bandless window's foot)
    for(int size:{32,52,80}) {
      PlayView v; v.in.dur=100; v.in.sid=1; v.in.sub_shown=true; v.in.sub_visible=true; v.in.kind=SubKind::Text;
      v.in.sub_text={"Eu me viro do avesso Ou faço de joelhos"}; v.in.style.size=size;
      v.in.preview_aspect=16.0/9; v.in.sub_ref_h=R; v.place.anchor=DockAnchor::TopCenter;
      double tt=1; v.resize(1920,2088,1.75); v.reset(tt);
      for(int i=0;i<180;++i){ tt+=1/60.0; v.tick(tt,1/60.0); }
      double f=v.sub_font_px(), bot=subtitle_block_bottom(2088,v.lifted_pos(),f);
      assert(bot<1584);                                                          // at 93: inside the picture
      const int sy=(int)(bot-f*0.6);
      v.mouse_move(960,sy,tt); assert(v.cursor_kind()==2);
      v.mouse_down(960,sy,tt);
      for(int i=1;i<=40;++i) v.mouse_move(960,sy+i*10,tt);
      v.mouse_up(960,sy+400,tt);
      for(int i=0;i<120;++i){ tt+=1/60.0; v.tick(tt,1/60.0); }
      assert(v.in.style.pos>100 && v.in.style.pos<=kSubPosPictureMax);
      struct{int w,h;}ws[]={{2573,1447},{1487,1445},{800,1444},{1920,2088}};
      for(auto& x:ws) {
        v.resize(x.w,x.h,1.75); for(int i=0;i<60;++i){ tt+=1/60.0; v.tick(tt,1/60.0); }
        const double pic=std::min<double>(x.h,x.w*9.0/16), lp=v.lifted_pos(), g=v.sub_font_px();
        std::vector<Rect> rs; assert(v.sub_line_rects(lp,rs));
        if(pic>=x.h-1) assert(lp==kSubPosWindowFoot);
        else assert(rs.front().y>=(x.h+pic)/2);                                  // under the picture's foot
      }
      // with the controls showing at the foot: never under them, half a line of its letters above them
      v.place.anchor=DockAnchor::BottomCenter;
      for(auto& x:ws) {
        v.resize(x.w,x.h,3.5); for(int i=0;i<90;++i){ v.activity(tt); tt+=1/60.0; v.tick(tt,1/60.0); }
        const Rect d={(int)v.px_.target,(int)v.py_.target,v.dw_,v.dh_}, sr=v.sub_rect_at(v.lifted_pos());
        assert(d.y>x.h/2);
        if(!(sr.r()<d.x||d.r()<sr.x)) assert(sr.b()<=d.y-0.5*v.sub_font_px()+1);
      }
      v.place.anchor=DockAnchor::TopCenter;
      v.resize(1920,2088,1.75); for(int i=0;i<300;++i){ tt+=1/60.0; v.tick(tt,1/60.0); }
      // and back into the picture
      v.resize(1920,2088,1.75); for(int i=0;i<60;++i){ tt+=1/60.0; v.tick(tt,1/60.0); }
      f=v.sub_font_px(); const int y2=(int)(subtitle_block_bottom(2088,v.lifted_pos(),f)-f*0.6);
      v.mouse_down(960,y2,tt); for(int i=1;i<=40;++i) v.mouse_move(960,y2-i*12,tt); v.mouse_up(960,y2-480,tt);
      for(int i=0;i<120;++i){ tt+=1/60.0; v.tick(tt,1/60.0); }
      assert(v.in.style.pos<100 && subtitle_block_bottom(2088,v.lifted_pos(),v.sub_font_px())<1584);
    }
  }
  assert(subtitle_typography_scale(640,360,16.0/9,0)==1);                // no designed size (the mini player): as before
  for(int width:{960,1280,1920,3840})for(int size:{28,44,60,72}) {
    SubStyle st;st.size=size;st.line_gap=4;st.tracking=2;
    auto base=sub_style_props(st,SubKind::Text);
    const double ref_h=width*9./16.;
    for(double aspect:{16./9.,1.85,2.,2.35,2.39,2.40,32./9.}) {
      int height=(int)std::lround(width/aspect);
      auto scaled=sub_style_props(st,SubKind::Text,subtitle_typography_scale(width,height));
      assert(property(scaled,"sub-line-spacing")==property(base,"sub-line-spacing"));
      double pixels=property(scaled,"sub-font-size")*property(scaled,"sub-scale")*height/720.;
      assert(std::abs(pixels-size*ref_h/720.)<.05);
      assert(std::abs(property(scaled,"sub-shadow-offset")*height/720.-4*ref_h/720.)<.05);
      for(const auto& track:{SubKind::Ass,SubKind::Bitmap}) {
        auto authored=sub_style_props(st,track,subtitle_typography_scale(width,height));
        assert(property(authored,"sub-font-size")==size && property(authored,"sub-scale")==1);
      }
      st.ass_mode=1;
      assert(property(sub_style_props(st,SubKind::Ass,subtitle_typography_scale(width,height)),"sub-scale")==property(scaled,"sub-scale"));
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
  assert(std::fabs(property(sub_style_props(SubStyle{},SubKind::Text,std::numeric_limits<double>::quiet_NaN()),"sub-scale")-36/44.0)<1e-3);
  puts("Subtitle scale: a part of the picture (a zoom above the opening window, a third of the rate below it, 64 letters always fit, sizes 24 to 80), the margins, line spacing as a share, size and spacing through sub-scale, the engine's block position, the picture's position (inside it, or under it when dragged there, also at the largest size); same reading size at equal widths across 16:9 to 32:9, 4K, font sizes, drag bounds, outlines, ASS preservation, invalid geometry and repeated changes passed");
}
