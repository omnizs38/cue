#include "queue_view.h"
#include "../core/i18n.h"
#include <algorithm>
#include <cctype>

namespace {
constexpr int kAdd=-2,kFolder=-3,kClear=-4,kClose=-5,kScrollbar=-6;
int slot_of(int control) { return -control-2; }  // footer/header controls -> hover slot 0..3
// A pixel-dotted outline: the empty place a lifted row will land in.
void dotted(Canvas& c,const Rect& r,int s,Rgb colour,double alpha) {
  if(r.w<=0||r.h<=0||alpha<=0)return;
  for(int x=r.x;x<r.r();x+=2*s) {c.fill(x,r.y,s,s,colour,alpha);c.fill(x,r.b()-s,s,s,colour,alpha);}
  for(int y=r.y+2*s;y<r.b()-s;y+=2*s) {c.fill(r.x,y,s,s,colour,alpha);c.fill(r.r()-s,y,s,s,colour,alpha);}
}
}

std::string QueueView::display_name(const std::string& name) {
  // The extension is noise in a list of media; the file information panel still shows it.
  size_t dot=name.find_last_of('.');
  if(dot==std::string::npos||dot==0||name.size()-dot<2||name.size()-dot>6)return name;
  for(size_t i=dot+1;i<name.size();++i)if(!std::isalnum((unsigned char)name[i]))return name;
  return name.substr(0,dot);
}
void QueueView::split_artist_title(const std::string& name,std::string& title,std::string& artist) {
  title=name;artist.clear();
  // " - ", " – " or " — " between two non-empty parts; the first one (titles keep their own dashes)
  static const char* marks[]={" - "," \xe2\x80\x93 "," \xe2\x80\x94 "};
  size_t best=std::string::npos,len=0;
  for(const char* m:marks) {size_t at=name.find(m);if(at!=std::string::npos&&at<best) {best=at;len=std::char_traits<char>::length(m);}}
  if(best==std::string::npos||best==0||best+len>=name.size())return;
  auto trim=[](std::string v) {
    while(!v.empty()&&v.front()==' ')v.erase(v.begin());
    while(!v.empty()&&v.back()==' ')v.pop_back();
    return v;
  };
  std::string a=trim(name.substr(0,best)),t=trim(name.substr(best+len));
  if(a.empty()||t.empty())return;
  title=t;artist=a;
}
std::string QueueView::plain_title(std::string title) {
  // the uploads' labels at a title's end, in (), [] or {}: "(audio)", "[Official Video]", "(320 Kbps)"
  static const char* noise[]={"audio","official audio","official video","official music video","music video","video",
                              "lyric video","lyrics","lyric","visualizer","visualiser","hq","hd","4k","mv","clip",
                              "audio oficial","video oficial","clipe oficial","official","explicit","clean"};
  for(int guard=0;guard<4;++guard) {
    while(!title.empty()&&title.back()==' ')title.pop_back();
    if(title.size()<3)break;
    char close=title.back(),open=close==')'?'(':close==']'?'[':close=='}'?'{':0;
    if(!open)break;
    size_t at=title.rfind(open);
    if(at==std::string::npos||at==0)break;
    std::string inner;
    for(size_t i=at+1;i+1<title.size();++i)inner+=(char)std::tolower((unsigned char)title[i]);
    bool drop=std::any_of(std::begin(noise),std::end(noise),[&](const char* n){return inner==n;});
    // "320 kbps", "128kbps"
    if(!drop&&inner.size()>=4&&inner.find("kbps")!=std::string::npos)drop=std::isdigit((unsigned char)inner[0])!=0;
    if(!drop)break;
    std::string rest=title.substr(0,at);
    while(!rest.empty()&&rest.back()==' ')rest.pop_back();
    if(rest.empty())break;  // a title that is only a label stays as it is
    title=rest;
  }
  return title;
}
void QueueView::set_compact(bool compact) {
  if(compact==compact_)return;
  const double before=row_h();
  compact_=compact;frame_s_=0;cancel();ghosts_.clear();
  reshape();
  for(int i=0;i<(int)items_.size();++i)items_[i].y.snap(i*row_h());
  // the same rows stay in view (the scroll is in rows' heights)
  scroll_.snap(std::clamp(scroll_.target*row_h()/std::max(1.,before),0.,max_scroll()));scrollbar();
}
int QueueView::index(uint32_t id) const {
  for(int i=0;i<(int)items_.size();++i)if(items_[i].entry.id==id)return i;
  return -1;
}
int QueueView::current_index() const {
  for(int i=0;i<(int)items_.size();++i)if(items_[i].entry.current)return i;
  return -1;
}
double QueueView::max_scroll() const { return std::max(0,((int)items_.size()+(add_row_on()?1:0))*row_h()-viewport_.h); }
// The list ends the same way wherever it is (beside the cover or in its popover): one more line
// that adds songs, by a click or by dropping files on it. An empty popover has its own words.
bool QueueView::add_row_on() const { return bare_||!items_.empty(); }
Rect QueueView::add_row() const {
  if(!add_row_on())return {};
  int y=viewport_.y+(int)std::lround(items_.size()*row_h()-scroll_.x);
  return {viewport_.x,y,viewport_.w-(int)(7*s_),row_h()-(int)(2*s_)};
}
void QueueView::sync(const std::vector<QueueEntry>& entries,uint64_t revision) {
  if(revision==revision_)return;
  int prior_focus=index(focus_);
  cancel();
  const bool initial=revision_==~uint64_t(0);
  if(!initial&&open_&&entries.size()!=items_.size()) {old_count_=count_suffix(items_.size());count_age_=0;}
  if(!open_) {old_count_.clear();count_age_=empty_age_=1;}
  if(open_&&entries.empty()&&!items_.empty())empty_age_=0;
  // Only retain raster snapshots of visible removed rows. Logical removal is immediate.
  if(open_&&!reduce_motion()) {
    int stagger=0,current=current_index();
    for(auto& item:items_) {
      if(std::any_of(entries.begin(),entries.end(),[&](const auto& e){return e.id==item.entry.id;}))continue;
      int y=std::lround(item.y.x-scroll_.x);
      if(y+row_h()<=0||y>=viewport_.h)continue;
      Ghost ghost;ghost.image.resize(viewport_.w,row_h());ghost.image.clear_transparent();
      item.hover=0;draw_item(ghost.image,item,0,false,last_paused_,current);ghost.y=y;
      // Clearing reads top to bottom; a single removal leaves at once.
      ghost.age=entries.empty()?-std::min(.08,stagger++*.016):0;
      ghost.duration=entries.empty()?.18:.15;
      if(ghosts_.size()==12)ghosts_.pop_front();
      ghosts_.push_back(std::move(ghost));
    }
    // the rows leave first, then the list closes up round what is left (it does not shrink under
    // rows still fading)
    if(!ghosts_.empty()) {height_motion_.snap(height_motion_.x);h_=(int)std::lround(height_motion_.x);reshape();}
  }
  album_=!entries.empty()&&std::all_of(entries.begin(),entries.end(),[](const auto& e){return e.audio;});
  std::vector<Item> updated; updated.reserve(std::min<size_t>(entries.size(),512));
  int first_new=-1,last_new=-1;
  for(const auto& entry:entries) {
    if(updated.size()==512)break;
    int old=index(entry.id);
    Item item;
    if(old>=0)item=std::move(items_[old]);
    else {
      item.y.snap(updated.size()*row_h());
      if(open_&&!initial) {
        if(first_new<0)first_new=(int)updated.size();
        last_new=(int)updated.size();
        if(!reduce_motion()) {item.enter=0;item.delay=std::min(.06,(updated.size()-first_new)*.012);item.fresh=1;}
      }
    }
    item.entry=entry; updated.push_back(std::move(item));
  }
  items_=std::move(updated); revision_=revision;
  if(items_.empty()&&control_==kClear)control_=kAdd;
  if(index(focus_)<0)focus_=items_.empty()?0:items_[std::clamp(prior_focus,0,(int)items_.size()-1)].entry.id;
  std::erase_if(pictures_,[&](const auto& p){return index(p.id)<0;});
  std::erase_if(failed_,[&](uint32_t id){return index(id)<0;});
  scroll_.target=std::clamp(scroll_.target,0.,max_scroll());
  if(ghosts_.empty())scroll_.x=std::clamp(scroll_.x,0.,max_scroll());
  if(keyboard_&&!control_)ensure_visible();
  else if(first_new>=0)reveal(first_new,last_new);
  targets(); scrollbar();
}
void QueueView::reshape() {
  // (beside the cover the list has the popover's footer too: the same ways to add and to clear,
  // on no sheet; the card's short list keeps only its last line)
  if(bare_) viewport_={0,2*s_,w_-4*s_,std::max(0.,h_-4*s_-(compact_?0:kFooter*s_))};
  else viewport_={4*s_,(kHeader+2)*s_,w_-8*s_,std::max(0.,h_-(kHeader+kFooter+2)*s_)};
  scroll_.target=std::clamp(scroll_.target,0.,max_scroll());scrollbar();
}
void QueueView::layout(int width,int height,double scale) {
  bool geometry=w_!=width||s_!=scale;
  desired_height_=height;
  if(geometry||!open_) {
    cancel();ghosts_.clear();w_=width;s_=scale;h_=height;height_motion_.snap(height);
    reshape();scroll_.snap(std::clamp(scroll_.target,0.,max_scroll()));
    for(int i=0;i<(int)items_.size();++i)items_[i].y.snap(i*row_h());
  } else if(ghosts_.empty())height_motion_.to(height,.20);
}
void QueueView::closed() {
  open_=false;cancel();ghosts_.clear();old_count_.clear();count_age_=empty_age_=1;
  height_motion_.snap(desired_height_);h_=desired_height_;reshape();
  for(auto& item:items_) {item.y.snap(item.y.target);item.enter=1;item.fresh=0;item.hover=0;}
  for(double& h:control_hover_)h=0;
}
void QueueView::opened() {
  open_=true;ghosts_.clear();height_motion_.snap(desired_height_);h_=desired_height_;reshape();
  for(auto& item:items_) {item.y.snap(item.y.target);item.enter=1;item.fresh=0;}
  cancel(); keyboard_=false;control_=0;
  for(auto& i:items_)if(i.entry.current)focus_=i.entry.id;
  // the song playing near the top, one row of what came before above it: what is next is in view
  ensure_visible();
  if(int i=index(focus_);i>=0) scroll_.target=std::clamp((i-1.0)*row_h(),0.,max_scroll());  // (a whole row, never a cut one)
  scroll_.snap(scroll_.target);scrollbar();
}
void QueueView::cancel() {
  pressed_=-1;pressed_id_=drag_id_=0;lift_=0;targets();
}
void QueueView::targets() {
  int from=index(drag_id_);
  for(int i=0;i<(int)items_.size();++i) {
    int slot=i;
    if(from>=0&&i!=from) {
      if(from<drop_&&i>from&&i<=drop_)--slot;
      if(from>drop_&&i>=drop_&&i<from)++slot;
    }
    items_[i].y.target=slot*row_h();
  }
  if(from>=0)slot_.target=drop_*row_h();
}
void QueueView::scrollbar() {
  bar_={w_-7*s_,viewport_.y+2*s_,3*s_,std::max(0.,viewport_.h-4*s_)};
  if(max_scroll()<=0) {thumb_={};return;}
  double height=std::min<double>(bar_.h,std::max(14*s_,(double)bar_.h*viewport_.h/(items_.size()*row_h())));
  thumb_={bar_.x,bar_.y+(bar_.h-height)*clamp01(scroll_.x/max_scroll()),bar_.w,height};
}
Rect QueueView::button(int n) const {
  if(bare_&&(compact_||n==kClose))return {};  // beside the cover: no header (the song's words stand above)
  auto px=[&](double v){return (int)std::lround(v*s_);};
  if(n==kClose)return {w_-px(26),px(6),px(20),px(20)};
  // Footer: adding happens at the end of the list, so its actions sit under the last row.
  const int y=h_-px(kFooter-4),height=px(20);
  const int clear_w=text_w(tr(str::queue_clear),s_)+px(10);
  if(n==kClear)return {w_-clear_w-px(6),y,clear_w,height};
  auto natural=[&](int b){return px(22)+text_w(tr(b==kAdd?str::queue_add_files_short:str::queue_add_folder_short),s_);};
  // The clear button's place is always reserved, so the add buttons never move when it fades.
  int room=std::max(0,w_-clear_w-px(20));
  int add=natural(kAdd),folder=natural(kFolder);
  if(add+folder+px(4)>room) {int half=std::max(0,(room-px(4))/2);add=std::min(add,half);folder=std::min(folder,half);}
  return n==kAdd?Rect{px(6),y,add,height}:Rect{px(6)+add+px(4),y,folder,height};
}
int QueueView::hit(int x,int y) const {
  if(x<0||y<0||x>=w_||y>=h_)return -1;
  for(int b:{kAdd,kFolder,kClear,kClose})if((b!=kClear||!items_.empty())&&button(b).contains(x,y))return b;
  if(max_scroll()>0&&Rect{w_-12*s_,viewport_.y,10*s_,viewport_.h}.contains(x,y))return kScrollbar;
  if(viewport_.contains(x,y)&&add_row().contains(x,y))return kAdd;
  if(!viewport_.contains(x,y))return -1;
  for(int i=0;i<(int)items_.size();++i) {
    double top=viewport_.y+items_[i].y.x-scroll_.x;
    if(y>=top&&y<top+row_h()&&items_[i].enter>=.25)return i;
  }
  return -1;
}
void QueueView::ensure_visible() {
  int i=index(focus_); if(i<0)return;
  double top=i*row_h(),bottom=top+row_h();
  if(top<scroll_.target)scroll_.target=top;
  else if(bottom>scroll_.target+viewport_.h)scroll_.target=bottom-viewport_.h;
  scroll_.target=std::clamp(scroll_.target,0.,max_scroll());
}
void QueueView::reveal(int first,int last) {
  // Added rows that land out of sight pull the list to them, one row of context above.
  double top=first*row_h(),bottom=(last+1)*row_h();
  if(bottom>scroll_.target&&top<scroll_.target+viewport_.h)return;
  scroll_.target=std::clamp(top-row_h(),0.,max_scroll());
}
void QueueView::move(int x,int y) {
  mx_=x;my_=y;keyboard_=false;control_=0;
  if(pressed_==kScrollbar) {
    if(bar_.h>thumb_.h)scroll_.snap(std::clamp((y-scroll_grab_-bar_.y)*max_scroll()/(bar_.h-thumb_.h),0.,max_scroll()));
    scrollbar();return;
  }
  int from=index(pressed_id_);
  if(from>=0&&down_x_<int(w_-30*s_)&&!drag_id_&&std::abs(x-down_x_)+std::abs(y-down_y_)>4*s_) {
    drag_id_=pressed_id_; grab_=down_y_-viewport_.y+scroll_.x-items_[from].y.x;
    drop_=from;slot_.snap(from*row_h());lift_=hover_step(0,true,0);
  }
  if(drag_id_) {
    drop_=std::clamp((int)std::floor((y-viewport_.y+scroll_.x-grab_+row_h()*.5)/row_h()),0,(int)items_.size()-1);
    targets();
  }
}
void QueueView::down(int x,int y) {
  cancel();scroll_.snap(scroll_.x);scrollbar();move(x,y);down_x_=x;down_y_=y;pressed_=hit(x,y);
  if(pressed_>=0) {pressed_id_=items_[pressed_].entry.id;focus_=pressed_id_;}
  if(pressed_==kScrollbar) {
    if(thumb_.contains(x,y))scroll_grab_=y-thumb_.y;
    else {scroll_grab_=thumb_.h*.5;move(x,y);}
  }
}
QueueView::Event QueueView::up(int x,int y) {
  Event event;
  if(drag_id_) {
    int from=index(drag_id_);
    if(from>=0&&x>=-30*s_&&x<=w_+30*s_) {
      // Keep the lifted row at the pointer while the model commits its new slot.
      items_[from].y.snap(y-viewport_.y+scroll_.x-grab_);
      event={Action::Move,drag_id_,drop_};
    }
  } else if(pressed_!=-1&&hit(x,y)==pressed_) {
    switch(pressed_) {
      case kAdd:event.action=Action::Add;break;
      case kFolder:event.action=Action::Folder;break;
      case kClear:if(!items_.empty())event.action=Action::Clear;break;
      case kClose:event.action=Action::Close;break;
      default:
        if(pressed_>=0&&index(pressed_id_)==pressed_&&down_x_>=grip_w()*s_&&((x>=w_-30*s_)==(down_x_>=w_-30*s_)))
          event={x>=w_-30*s_?Action::Remove:Action::Play,pressed_id_};
        break;
    }
  }
  cancel();return event;
}
void QueueView::wheel(double direction) {
  if(held())return;
  scroll_.target=std::clamp(scroll_.target-direction*row_h()*2.,0.,max_scroll());
  keyboard_=false;
}
QueueView::Event QueueView::key(const std::string& key,bool shift,bool ctrl) {
  if(key=="esc"&&held()) {cancel();return {};}
  if(key=="esc"||key=="q")return {Action::Close};
  int i=index(focus_);
  if(ctrl&&(key=="up"||key=="down")&&i>=0)return {Action::Move,focus_,std::clamp(i+(key=="up"?-1:1),0,(int)items_.size()-1)};
  if(key=="delete"&&i>=0)return {Action::Remove,focus_};
  if(key=="enter") {
    if(control_==kAdd)return {Action::Add};
    if(control_==kFolder)return {Action::Folder};
    if(control_==kClear)return items_.empty()?Event{}:Event{Action::Clear};
    if(control_==kClose)return {Action::Close};
    if(i>=0)return {Action::Play,focus_};
  }
  if(key=="tab") {
    // Reading order: the list, then the footer, then close.
    const int stops[]={0,kAdd,kFolder,kClear,kClose};
    auto it=std::find(std::begin(stops),std::end(stops),control_);
    int position=keyboard_?(int)(it-std::begin(stops)):(shift?0:-1);
    do {position=(position+(shift?4:1))%5;}while(items_.empty()&&(stops[position]==kClear||stops[position]==0));
    control_=stops[position];keyboard_=true;
    if(!control_)ensure_visible();
    return {};
  }
  if(ctrl&&key=="o")return {shift?Action::Folder:Action::Add};
  if(key=="up"||key=="down"||key=="home"||key=="end"||key=="pageup"||key=="pagedown") {
    if(items_.empty())return {};
    int step=key=="pageup"||key=="pagedown"?std::max(1,viewport_.h/row_h()):1;
    if(key=="home")i=0;
    else if(key=="end")i=(int)items_.size()-1;
    else i=std::clamp(i+((key=="up"||key=="pageup")?-step:step),0,(int)items_.size()-1);
    focus_=items_[i].entry.id;keyboard_=true;control_=0;ensure_visible();
    if(i==(int)items_.size()-1)scroll_.target=max_scroll();  // the last song: the list's own last line too
  }
  return {};
}
bool QueueView::tick(double dt) {
  bool changed=!scroll_.settled();
  if(!open_)return false;
  if(count_age_<1) {count_age_=reduce_motion()?1:std::min(1.,count_age_+dt/.15);changed=true;}
  for(auto& ghost:ghosts_)ghost.age+=dt;
  bool had_ghosts=!ghosts_.empty();
  std::erase_if(ghosts_,[](const auto& g){return reduce_motion()||g.age>=g.duration;});
  // (the empty list's words come once the rows are gone)
  if(empty_age_<1&&ghosts_.empty()) {empty_age_=reduce_motion()?1:std::min(1.,empty_age_+dt/.3);changed=true;}
  if(had_ghosts)changed=true;
  if(ghosts_.empty())height_motion_.to(desired_height_,.20);
  if(!height_motion_.settled()) {height_motion_.step(dt);h_=std::lround(height_motion_.x);reshape();changed=true;}
  {
    double target=dropping_?1:0,next=reduce_motion()?target:target>drop_k_?std::min(1.,drop_k_+dt/.18):std::max(0.,drop_k_-dt/.22);
    changed|=next!=drop_k_;drop_k_=next;
    if(dropping_&&!reduce_motion()) {drop_t_+=dt;changed=true;}
  }
  int hot=hit(mx_,my_);
  for(int b:{kAdd,kFolder,kClear,kClose}) {
    double& v=control_hover_[slot_of(b)];
    bool on=!held()&&(keyboard_?control_==b:hot==b)&&(b!=kClear||!items_.empty());
    double next=hover_step(v,on,dt);changed|=next!=v;v=next;
  }
  if(drag_id_) {
    double next=hover_step(lift_,true,dt);changed|=next!=lift_;lift_=next;
    changed|=slot_.step(dt);
    double edge=std::min(28*s_,viewport_.h*.25),speed=0;
    if(my_<viewport_.y+edge)speed=-clamp01((viewport_.y+edge-my_)/std::max(1.,edge));
    else if(my_>viewport_.b()-edge)speed=clamp01((my_-viewport_.b()+edge)/std::max(1.,edge));
    if(speed) {scroll_.snap(std::clamp(scroll_.x+speed*row_h()*8*std::min(.05,dt),0.,max_scroll()));move(mx_,my_);changed=true;}
  }
  scroll_.step(dt);scrollbar();
  for(auto& item:items_) {
    // Off-screen rows settle without spending frames on invisible motion.
    bool visible=item.y.x+row_h()>=scroll_.x&&item.y.x<=scroll_.x+viewport_.h;
    visible|=item.y.target+row_h()>=scroll_.x&&item.y.target<=scroll_.x+viewport_.h;
    if(!visible) {item.y.snap(item.y.target);item.hover=0;item.enter=1;item.fresh=0;continue;}
    if(item.enter<1) {item.delay-=dt;if(item.delay<=0)item.enter=reduce_motion()?1:std::min(1.,item.enter+dt/.16);changed=true;}
    else if(item.fresh>0) {item.fresh=reduce_motion()?0:eased_fall(item.fresh,dt,.7);changed=true;}
    bool unsettled=!item.y.settled();item.y.step(dt);changed|=unsettled;
    bool on=!drag_id_&&(keyboard_?(!control_&&item.entry.id==focus_):(hot>=0&&items_[hot].entry.id==item.entry.id));
    double hover=hover_step(item.hover,on,dt);changed|=hover!=item.hover;item.hover=hover;
  }
  for(auto& pic:pictures_)if(pic.fade<1) {pic.fade=reduce_motion()?1:std::min(1.,pic.fade+dt/.12);changed=true;}
  return changed;
}
uint32_t QueueView::thumbnail_request() const {
  for(const auto& item:items_) {
    if(item.y.target+row_h()<scroll_.x||item.y.target>scroll_.x+viewport_.h)continue;
    uint32_t id=item.entry.id;
    if(failed_.contains(id))continue;
    if(std::none_of(pictures_.begin(),pictures_.end(),[&](const auto& p){return p.id==id;}))return id;
  }
  return 0;
}
void QueueView::thumbnail(uint32_t id,const std::vector<uint32_t>& pixels) {
  if(index(id)<0)return;
  if(pixels.size()!=84*48) {failed_.insert(id);return;}
  if(std::any_of(pictures_.begin(),pictures_.end(),[&](const auto& p){return p.id==id;}))return;
  if(pictures_.size()>=32) {
    auto oldest=std::min_element(pictures_.begin(),pictures_.end(),[](const auto& a,const auto& b){return a.used<b.used;});
    pictures_.erase(oldest);
  }
  Picture picture;picture.id=id;picture.used=++clock_;picture.image.resize(84,48);picture.image.px=pixels;
  pictures_.push_back(std::move(picture));
}
Rect QueueView::cover_box(const QueueEntry& entry,int y) const {
  const int s=(int)s_;(void)s;
  const double k=s_;
  if(compact_)return {(int)std::lround(9*k),y+(int)std::lround(3*k),(int)std::lround(18*k),(int)std::lround(18*k)};
  return entry.audio?Rect{(int)((album_?20:33)*k),y+(int)(5*k),(int)(34*k),(int)(34*k)}:Rect{(int)(20*k),y+(int)(5*k),(int)(60*k),(int)(34*k)};
}
void QueueView::draw_picture(Canvas& c,const QueueEntry& entry,const Rect& image,bool played) {
  const auto& p=P();double s=s_;
  // (the card's playing row puts its play mark where the tile's note would be)
  const bool pictured=(compact_&&entry.current)||std::any_of(pictures_.begin(),pictures_.end(),[&](const auto& pic){return pic.id==entry.id&&pic.fade>=1;});
  if(entry.audio) {
    // a song without a cover wears Cue's own music tile: black, with its note
    c.cornered(image,s,Rgb{12,12,12});
    if(const BitmapDef* note=pictured?nullptr:bmp("cue-note")) {
      double k=compact_?s:s*2;
      c.bitmap(note,image.x+(int)std::lround((image.w-note->w*k)/2/s)*s,image.y+(int)std::lround((image.h-note->h*k)/2/s)*s,k,played?p.faint:mix(p.dim,p.fg,.35));
    }
  } else {
    c.cornered(image,s,p.band);
    action_icon(c,"queue",image,s,played?p.faint:p.dim);
  }
  for(auto& picture:pictures_)if(picture.id==entry.id) {
    picture.used=++clock_;picture.drawn=true;
    if(picture.scaled.w!=image.w||picture.scaled.h!=image.h) {
      // nearest-neighbour, once per size: the preview keeps its pixels. A cover is the
      // square in the middle of its tile.
      const int sx=entry.audio?18:0,sw=entry.audio?48:84;
      picture.scaled.resize(image.w,image.h);
      for(int py=0;py<image.h;++py) {
        const uint32_t* src=&picture.image.px[(size_t)(py*48/image.h)*84+sx];
        uint32_t* dst=&picture.scaled.px[(size_t)py*image.w];
        for(int px=0;px<image.w;++px)dst[px]=src[px*sw/image.w]|0xff000000u;
      }
    }
    // Thumbnails arrive in three pixel steps, like the rest of the pixel interface.
    c.blit(picture.scaled,image.x,image.y,std::ceil(ease_out(picture.fade)*3-1e-9)/3*(played?.55:1));
    break;
  }
}
// The card's row: [grip] [cover] Title  artist, on one line at the card's type size. The song
// playing is lit like the full list's (its band and the bar on its left) and its cover carries
// the play or pause mark; the remove mark takes the line's end only under the pointer.
void QueueView::draw_compact(Canvas& c,Item& item,int y,bool floating,bool paused,int current) {
  const auto& p=P();const auto& entry=item.entry;double s=s_;
  const int position=index(entry.id);
  const bool played=current>=0&&position>=0&&position<current;
  Rect row{0,y,c.w-(int)(7*s),row_h()-(int)(2*s)};
  if(floating) {
    c.cornered(row,s,p.press);
    c.fill(row.x+s,row.y,row.w-2*s,s,p.faint);c.fill(row.x+s,row.b()-s,row.w-2*s,s,p.faint);
    c.fill(row.x,row.y+s,s,row.h-2*s,p.faint);c.fill(row.r()-s,row.y+s,s,row.h-2*s,p.faint);
  } else if(entry.current)c.cornered(row,s,p.hover);
  if(!floating&&item.hover>0)c.cornered(row,s,p.press,ease_out(item.hover)*.65);
  if(!floating&&item.fresh>0)c.cornered(row,s,p.press,ease_out(item.fresh)*.8);
  if(entry.current)c.fill(0,y+6*s,2*s,row.h-12*s,p.fg);
  // the grip: two columns of three dots, quiet until the row is under the pointer
  for(int gy=0;gy<3;++gy)for(int gx=0;gx<2;++gx)c.fill((3+gx*2)*s,y+(8+gy*3)*s,s,s,item.hover>0||floating?p.dim:p.faint,item.hover>0||floating?1.:.7);
  Rect image=cover_box(entry,y);
  draw_picture(c,entry,image,played);
  if(entry.current) {
    // the song playing: its cover dims a little under the play (or pause) mark
    c.fill(image,Rgb{0,0,0},.45);
    int m=(int)(7*s);
    action_icon(c,paused?"pause":"play",{image.cx()-m/2,image.cy()-m/2,m,m},s,p.fg);
  }
  const bool remove=!floating&&(item.hover>0||keyboard_);
  const int tx=image.r()+(int)(7*s);
  const int end=row.r()-(int)((remove?24:6)*s);
  const int available=std::max(0,end-tx);
  std::string title,artist;
  split_artist_title(display_name(entry.name),title,artist);
  title=plain_title(title);
  Rgb name_ink=entry.current||floating?p.fg:played?p.faint:mix(p.dim,p.fg,.25+ease_out(item.hover)*.55);
  const int ty=y+(int)std::lround(5*s);
  std::string t=fit_end(title,available,s);
  text(c,t,tx,ty,s,name_ink);
  // who, beside the title in the faint ink, when there is room for a few letters of it
  int aw=available-text_w(t,s)-(int)(6*s);
  if(!artist.empty()&&aw>=(int)(24*s))
    text(c,fit_end(artist,aw,s),tx+text_w(t,s)+(int)(6*s),ty,s,entry.current?p.dim:p.faint);
  if(remove) {
    Rect x{row.r()-(int)(22*s),y+(int)(2*s),(int)(20*s),row.h-(int)(2*s)};
    c.bitmap(bmp("x-small"),x.cx()-2*s,x.cy()-2*s,s,p.dim,keyboard_&&item.hover<=0?1.:ease_out(item.hover));
  }
}
void QueueView::draw_item(Canvas& c,Item& item,int y,bool floating,bool paused,int current) {
  if(compact_) {draw_compact(c,item,y,floating,paused,current);return;}
  const auto& p=P();const auto& entry=item.entry;double s=s_;
  const int position=index(entry.id);
  const bool played=current>=0&&position>=0&&position<current;
  const bool next=current>=0&&position==current+1;
  Rect row{0,y,c.w-7*s,row_h()-2*s};
  if(floating) {
    c.cornered(row,s,p.press);
    // a one-pixel rim keeps the lifted row legible over the rows sliding beneath it
    c.fill(row.x+s,row.y,row.w-2*s,s,p.faint);c.fill(row.x+s,row.b()-s,row.w-2*s,s,p.faint);
    c.fill(row.x,row.y+s,s,row.h-2*s,p.faint);c.fill(row.r()-s,row.y+s,s,row.h-2*s,p.faint);
  } else if(entry.current)c.cornered(row,s,p.hover);
  if(!floating&&item.hover>0)c.cornered(row,s,p.press,ease_out(item.hover)*.65);
  if(!floating&&item.fresh>0)c.cornered(row,s,p.press,ease_out(item.fresh)*.8);
  if(entry.current)c.fill(0,y+9*s,2*s,row.h-18*s,p.fg);
  // A quiet grip becomes bright on hover, without adding an always-visible toolbar.
  for(int gy=0;gy<3;++gy)for(int gx=0;gx<2;++gx)c.fill((7+gx*3)*s,y+(17+gy*3)*s,s,s,item.hover>0||floating?p.dim:p.faint);
  // Music shows its cover square, like an album's track list; in a list of only music the
  // titles move in next to it, otherwise they keep the column the films use.
  draw_picture(c,entry,cover_box(entry,y),played);
  int tx=(album_?62:88)*s,available=c.w-tx-34*s;
  Rgb name_ink=entry.current||floating?p.fg:played?p.faint:mix(p.dim,p.fg,ease_out(item.hover)*.6);
  text(c,fit_middle(display_name(entry.name),std::max(0,available),s),tx,y+8*s,s,name_ink);
  std::string caption=entry.current?tr(str::queue_playing):next?tr(str::queue_up_next)
                     :items_.size()>1&&position>=0?std::to_string(position+1)+" / "+std::to_string(items_.size()):"";
  text(c,fit_end(caption,std::max(0,available-12*(int)s),s),tx,y+25*s,s,entry.current||next?p.dim:p.faint);
  if(entry.current)action_icon(c,paused?"pause":"play",{tx+text_w(caption,s)+5*s,y+27*s,7*s,7*s},s,p.dim);
  Rect remove{c.w-29*s,y+10*s,20*s,24*s};
  if(!floating&&(item.hover>0||keyboard_))c.bitmap(bmp("x-small"),remove.cx()-2*s,remove.cy()-2*s,s,p.dim,ease_out(item.hover));
}
// The list's last line: a dotted place with a plus, "add tracks / or drag them here"; while files
// are dragged over the window it brightens, breathes and says "drop to add"
void QueueView::draw_add_row(Canvas& list) {
  const auto& p=P();double s=s_;
  // the last line adds songs: a dotted place with a plus, "add tracks / or drag them here";
  // while files are dragged over the window it brightens, breathes and says "drop to add"
  Rect a=add_row();a.y-=viewport_.y;a.x-=viewport_.x;
  if(a.y+a.h>0&&a.y<list.h) {
    double hv=ease_out(control_hover_[slot_of(kAdd)]),dk=ease_out(drop_k_);
    double breath=dropping_?.5+.5*std::sin(drop_t_*5.2):0;
    Rgb ink=mix(mix(p.faint,p.dim,.6+.4*hv),p.fg,dk);
    if(hv>0||dk>0)list.cornered(a,s,p.press,std::max(hv*.5,dk*(.45+.25*breath)));
    std::string first=tr(dropping_?str::queue_drop_to_add:str::queue_add_tracks);
    if(compact_) {
      // the card: one line too, a dotted place where a cover would be
      Rect box=cover_box({},a.y);
      dotted(list,box,s,ink,1);
      if(const BitmapDef* plus=bmp("plus")) list.bitmap(plus,box.cx()-(int)std::lround(2.5*s),box.cy()-(int)std::lround(2.5*s),s,ink);
      int tx=box.r()+(int)(7*s),room=std::max(0,a.r()-tx-6*(int)s);
      text(list,fit_end(first,room,s),tx,a.y+(int)std::lround(5*s),s,ink);
    } else {
      Rect box{a.x+20*s,a.y+5*s,34*s,34*s};
      dotted(list,box,s,ink,1);
      if(const BitmapDef* plus=bmp("plus")) list.bitmap(plus,box.cx()-5*s,box.cy()-5*s,s*2,ink);
      int tx=box.r()+8*s,room=std::max(0,a.r()-tx-8*(int)s);
      text(list,fit_end(first,room,s),tx,a.y+(int)std::lround(lerp(8.,16.,dk)*s),s,ink);  // alone, it sits in the middle
      if(!dropping_)text(list,fit_end(tr(str::queue_or_drag),room,s),tx,a.y+25*s,s,mix(p.faint,p.dim,.4*hv),1-dk);
    }
  }
}
// The footer: "+ files", "+ folder" and "clear" (the popover's, and the list's beside the cover)
void QueueView::draw_footer(Canvas& c,bool rules) {
  const auto& p=P();double s=s_;
  if(rules)c.fill(8*s,h_-kFooter*s,w_-16*s,s,p.edge);
  else c.fill(2*s,h_-kFooter*s,w_-12*s,s,p.edge,.6);
  for(int b:{kAdd,kFolder,kClear}) {
    Rect box=button(b);double h=ease_out(control_hover_[slot_of(b)]);
    double alpha=b==kClear&&items_.empty()?1-ease_out(empty_age_):1;
    if(b==kClear&&items_.empty()&&empty_age_>=1)continue;
    if(h>0)c.cornered(box,s,p.hover,h*alpha);
    Rgb ink=mix(p.dim,p.fg,h);
    if(b==kClear) {text(c,tr(str::queue_clear),box.x+5*s,box.y+4*s,s,ink,alpha);continue;}
    // "+ files", "+ folder": the plus says add, the word says what
    c.bitmap(bmp("plus"),box.x+5*s,box.y+(box.h-5*s)/2,s,ink);
    text(c,fit_end(tr(b==kAdd?str::queue_add_files_short:str::queue_add_folder_short),std::max(0,box.w-22*(int)s),s),box.x+15*s,box.y+4*s,s,ink);
  }
}
void QueueView::draw(Canvas& c,bool paused) {
  const auto& p=P();double s=s_;last_paused_=paused;
  if(frame_.w!=w_||frame_.h!=h_||frame_s_!=s) {
    frame_.resize(w_,h_);frame_.clear_transparent();frame_s_=s;
    if(!bare_)draw_panel(frame_,0,0,w_/s,h_/s,s,{1},p.sheet,.97,p.edge,1);
  }
  c.w=w_;c.h=h_;c.px=frame_.px;  // one copy; resize would zero it first
  if(bare_) {
    if(list_.w!=viewport_.w||list_.h!=viewport_.h)list_.resize(viewport_.w,viewport_.h);else list_.clear_transparent();
    const int current=current_index();
    for(auto& item:items_) {
      if(item.entry.id==drag_id_)continue;
      int y=std::lround(item.y.x-scroll_.x);
      if(y+row_h()<0||y>=list_.h)continue;
      draw_item(list_,item,y,false,paused,current);
    }
    // a cleared list: the rows leave top to bottom (as in the popover), and its line that adds songs
    // comes in once they are gone, where the list closes up
    for(const auto& ghost:ghosts_) {
      double progress=ease_out(clamp01(ghost.age/ghost.duration));
      list_.blit(ghost.image,(int)std::lround(6*s*progress),ghost.y,1-progress);
    }
    if(items_.empty()&&(empty_age_<1||!ghosts_.empty())) {
      const double a=ghosts_.empty()?ease_out(clamp01((empty_age_-.5)/.5)):0.0;
      if(a>0) {
        row_image_.resize(list_.w,list_.h);row_image_.clear_transparent();
        draw_add_row(row_image_);
        list_.blit(row_image_,0,(int)std::lround(3*s*(1-a)),a);
      }
    } else draw_add_row(list_);
    if(drag_id_) {
      int from=index(drag_id_);
      if(from>=0)draw_item(list_,items_[from],my_-viewport_.y-(int)grab_,true,paused,current);
    }
    // the list fades out at its foot when there is more under it
    if(max_scroll()-scroll_.x>1) {
      int fade=std::min(list_.h,(int)(18*s));
      for(int y=list_.h-fade;y<list_.h;++y) {
        double k=(y-(list_.h-fade))/(double)fade;
        uint32_t* row=&list_.px[(size_t)y*list_.w];
        for(int x=0;x<list_.w;++x) {
          uint32_t v=row[x];double a=1-k*k;
          row[x]=(uint32_t)std::lround(((v>>24)&255)*a)<<24|(uint32_t)std::lround(((v>>16)&255)*a)<<16|(uint32_t)std::lround(((v>>8)&255)*a)<<8|(uint32_t)std::lround((v&255)*a);
        }
      }
    }
    c.blit(list_,viewport_.x,viewport_.y);
    if(!compact_)draw_footer(c,false);
    for(auto& picture:pictures_) {if(!picture.drawn&&picture.scaled.w)picture.scaled=Canvas{};picture.drawn=false;}
    if(thumb_.h>0) {c.fill(bar_,p.edge);c.cornered(thumb_,0,held()?p.fg:p.dim);}
    return;
  }
  // header
  action_icon(c,"queue",{10*s,10*s,12*s,12*s},s,p.dim);
  std::string title=fit_end(tr(str::queue_title),w_-94*s,s);
  text(c,title,30*s,10*s,s,p.fg);
  int count_x=30*s+text_w(title,s);
  double change=ease_out(count_age_);
  if(!old_count_.empty()&&change<1)text(c,old_count_,count_x,(10-2*change)*s,s,p.dim,1-change);
  text(c,count_suffix(items_.size()),count_x,(10+2*(1-change))*s,s,p.dim,change);
  {
    Rect box=button(kClose);double h=ease_out(control_hover_[slot_of(kClose)]);
    if(h>0)c.cornered(box,s,p.hover,h);
    action_icon(c,"x-small",box,s,mix(p.dim,p.fg,h));
  }
  c.fill(8*s,(kHeader-1)*s,w_-16*s,s,p.edge);
  draw_footer(c,true);
  // list
  if(list_.w!=viewport_.w||list_.h!=viewport_.h)list_.resize(viewport_.w,viewport_.h);else list_.clear_transparent();
  if(items_.empty()) {
    // The empty list names the two ways in: the footer, or dropping files right here.
    double a=ease_out(clamp01((empty_age_-.6)/.4));
    std::string first=fit_end(tr(str::queue_empty),list_.w-16*s,s),second=fit_end(tr(str::queue_drop_hint),list_.w-16*s,s);
    int top=std::max(0,(list_.h-30*(int)s)/2);
    text(list_,first,(list_.w-text_w(first,s))/2,top,s,p.dim,a);
    text(list_,second,(list_.w-text_w(second,s))/2,top+16*s,s,mix(p.faint,p.dim,.45),a);
  }
  const int current=current_index();
  for(auto& item:items_) {
    if(item.entry.id==drag_id_)continue;
    int y=std::lround(item.y.x-scroll_.x);
    if(y+row_h()<0||y>=list_.h)continue;
    if(item.enter<1) {
      row_image_.resize(list_.w,row_h());row_image_.clear_transparent();draw_item(row_image_,item,0,false,paused,current);
      double reveal=ease_out(item.enter);list_.blit(row_image_,0,y+(int)std::lround(4*s*(1-reveal)),reveal);
    } else draw_item(list_,item,y,false,paused,current);
  }
  draw_add_row(list_);
  if(drag_id_) {
    int from=index(drag_id_);
    double lift=ease_out(lift_);
    dotted(list_,{0,(int)std::lround(slot_.x-scroll_.x),list_.w-(int)(7*s),row_h()-(int)(2*s)},s,p.faint,lift);
    if(from>=0) {
      int y=my_-viewport_.y-(int)grab_;
      // lifted: a hard pixel shadow that grows by a step or two as the row rises
      int drop=(int)std::lround(2*lift)*s;
      if(drop>0)list_.cornered({drop,y+drop,list_.w-(int)(7*s),row_h()-(int)(2*s)},s,p.screen,.6);
      draw_item(list_,items_[from],y-drop/2,true,paused,current);
    }
  }
  for(const auto& ghost:ghosts_) {
    double progress=ease_out(clamp01(ghost.age/ghost.duration));
    list_.blit(ghost.image,(int)std::lround(6*s*progress),ghost.y,1-progress);
  }
  c.blit(list_,viewport_.x,viewport_.y);
  // scaled copies live only while their rows are visible
  for(auto& picture:pictures_) {if(!picture.drawn&&picture.scaled.w)picture.scaled=Canvas{};picture.drawn=false;}
  if(thumb_.h>0) {c.fill(bar_,p.edge);c.cornered(thumb_,0,held()?p.fg:p.dim);}
}
