// The subtitle list scrolls inside a bounded viewport; controls remain fixed.
#include "play.h"
#include <algorithm>

void PlayView::layout_sub_list() {
  cc_drag_=false;
  cc_scale_=std::min({panel_s(),w_/220.,h_/180.});
  double s=cc_scale_;
  int width=210;
  for(const auto& row:rows_)if(row.id!=R_SUB_TRACK) {
    int padding=row.k==RK::Stepper?85:row.k==RK::Toggle?48:38;
    width=std::max(width,text_w(row.label,1)+padding);
  }
  width=std::min({width,300,std::max(1,(int)(w_/s)-16)});pw_=std::lround(width*s);
  cc_first_=1;cc_count_=0;
  for(int i=0;i<(int)rows_.size();++i)if(rows_[i].id==R_SUB_TRACK) {if(!cc_count_)cc_first_=i;++cc_count_;}
  cc_row_h_=std::max(1,(int)std::lround(18*s));
  double footer=4*s;
  for(int i=cc_first_+cc_count_;i<(int)rows_.size();++i)footer+=(rows_[i].k==RK::Separator?5:18)*s;
  double header=24*s,natural=header+cc_count_*cc_row_h_+footer;
  Rect dock=dock_rect();int room=std::max(dock.y,h_-dock.b())-12*s;
  int available=room>=150*s?room:h_-16*s;
  ph_=std::max(1,(int)std::min(natural,(double)available));
  cc_viewport_={2*s,header,pw_-4*s,std::max(0.,ph_-header-footer)};
  if(!rows_.empty()) {rows_[0].r={2*s,4*s,pw_-4*s,20*s};rows_[0].parts.clear();}
  for(int i=0;i<cc_count_;++i) {
    auto& row=rows_[cc_first_+i];row.r={0,i*cc_row_h_,cc_viewport_.w,cc_row_h_};row.parts.clear();
    row.display_label=fit_end(row.label,std::max(0,row.r.w-(int)(38*s)),s);
  }
  double y=cc_viewport_.b();
  for(int i=cc_first_+cc_count_;i<(int)rows_.size();++i) {
    auto& row=rows_[i];int rh=row.k==RK::Separator?5:18;
    row.r={2*s,y,pw_-4*s,rh*s};row.parts.clear();
    if(row.k==RK::Stepper) {
      int plus=row.r.r()-13*s;
      int minus=plus-3*s-text_w("+0.0s",s)-12*s;
      row.parts={{minus,y,9*s,row.r.h},{plus,y,9*s,row.r.h}};
    }
    y+=rh*s;
  }
  cc_scroll_.target=std::clamp(cc_scroll_.target,0.,cc_scroll_max());
  cc_scroll_.x=std::clamp(cc_scroll_.x,0.,cc_scroll_max());
  sub_scrollbar();panel_dirty_=true;
}
double PlayView::cc_scroll_max() const { return std::max(0,cc_count_*cc_row_h_-cc_viewport_.h); }
void PlayView::sub_scrollbar() {
  cc_bar_={cc_viewport_.r()-5*cc_scale_,cc_viewport_.y+2*cc_scale_,3*cc_scale_,std::max(0.,cc_viewport_.h-4*cc_scale_)};
  if(cc_scroll_max()<=0||!cc_count_) {cc_thumb_={};return;}
  double height=std::min((double)cc_bar_.h,std::max(12*cc_scale_,(double)cc_bar_.h*cc_viewport_.h/(cc_count_*cc_row_h_)));
  cc_thumb_={cc_bar_.x,cc_bar_.y+(cc_bar_.h-height)*clamp01(cc_scroll_.x/cc_scroll_max()),cc_bar_.w,height};
}
void PlayView::sub_ensure_visible(int row,bool instant) {
  int i=row-cc_first_;if(i<0||i>=cc_count_)return;
  double top=i*cc_row_h_,bottom=top+cc_row_h_;
  if(top<cc_scroll_.target)cc_scroll_.target=top;
  else if(bottom>cc_scroll_.target+cc_viewport_.h)cc_scroll_.target=bottom-cc_viewport_.h;
  cc_scroll_.target=std::clamp(cc_scroll_.target,0.,cc_scroll_max());
  if(instant)cc_scroll_.snap(cc_scroll_.target);
  sub_scrollbar();panel_dirty_=true;
}
void PlayView::render_sub_list() {
  if(!cc_count_||cc_viewport_.h<=0)return;
  const auto& p=P();double s=cc_scale_;
  cc_list_.resize(cc_viewport_.w,cc_viewport_.h);
  int first=std::max(0,(int)(cc_scroll_.x/cc_row_h_));
  int last=std::min(cc_count_,(int)((cc_scroll_.x+cc_viewport_.h)/cc_row_h_)+1);
  for(int n=first;n<last;++n) {
    int i=cc_first_+n;const auto& row=rows_[i];
    Rect box{0,n*cc_row_h_-cc_scroll_.x,cc_list_.w-(cc_scroll_max()>0?9:1)*s,cc_row_h_};
    double hover=i<(int)phov_.size()?ease_out(phov_[i]):0;
    if(hover>0)cc_list_.cornered(box,s,p.hover,hover);
    if(row.checked)action_icon(cc_list_,"check",{3*s,box.y,12*s,box.h},s,row.enabled?p.fg:p.faint);
    text(cc_list_,row.display_label,23*s,box.y+(box.h-12*s)/2+s,s,
         row.checked?(row.enabled?p.fg:p.dim):mix(row.enabled?p.dim:p.faint,p.fg,hover));
  }
  panel_base_.blit(cc_list_,cc_viewport_.x,cc_viewport_.y);
  if(cc_thumb_.h) {panel_base_.fill(cc_bar_,p.edge);panel_base_.fill(cc_thumb_,cc_drag_?p.fg:p.dim);}
}
