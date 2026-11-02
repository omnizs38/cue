#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

// Session-only queue. Stable IDs keep delayed UI/phone actions tied to an item,
// even after reordering. Removing the playing item never stops its decoder.
class MediaQueue {
 public:
  static constexpr size_t kLimit = 512;
  struct Item { uint32_t id; std::string path; };
  // Windows paths: ASCII case and slash direction do not distinguish files.
  static bool same_path(const std::string& a,const std::string& b) {
    if(a.size()!=b.size())return false;
    for(size_t i=0;i<a.size();++i) {
      auto fold=[](char c){ if(c=='/')return '\\'; return (c>='A'&&c<='Z')?char(c-'A'+'a'):c; };
      if(fold(a[i])!=fold(b[i]))return false;
    }
    return true;
  }
  // When a queue starts from the file already playing, that file is its first item.
  // The same file inside the added batch is then dropped once, so it is not listed twice.
  static std::vector<std::string> without_first(std::vector<std::string> paths,const std::string& playing) {
    auto it=std::find_if(paths.begin(),paths.end(),[&](const auto& p){return same_path(p,playing);});
    if(it!=paths.end())paths.erase(it);
    return paths;
  }
  const std::vector<Item>& items() const { return items_; }
  uint32_t current() const { return current_; }
  uint64_t revision() const { return revision_; }
  bool active() const { return explicit_; }
  void clear() { items_.clear(); current_=0; cursor_=-1; explicit_=false; ++revision_; }
  size_t append(const std::vector<std::string>& paths) {
    size_t added=0;
    for (const auto& p:paths) {
      if (items_.size()>=kLimit||serial_>=1000000000u) break;
      if (p.empty() || p.size()>4096 || p.find('\0')!=std::string::npos) continue;
      items_.push_back({++serial_,p}); ++added;
    }
    if (added) { explicit_=true; ++revision_; }
    return added;
  }
  bool select(uint32_t id) {
    auto i=index(id); if (i<0) return false;
    current_=id; cursor_=i; ++revision_; return true;
  }
  bool select_path(const std::string& path) {
    // A manual reopen of the current file retains its exact queue occurrence.
    if (auto* i=get(current_); i && i->path==path) return true;
    for (const auto& i:items_) if(i.path==path) return select(i.id);
    return false;
  }
  const Item* get(uint32_t id) const { int i=index(id); return i<0?nullptr:&items_[i]; }
  const Item* next() const { return cursor_+1<(int)items_.size()?&items_[cursor_+1]:nullptr; }
  // The entry before the playing one. If the playing entry was removed, the cursor already
  // rests on the entry that preceded it, which is then the one to go back to.
  const Item* previous() const {
    int i=current_?cursor_-1:cursor_;
    return i>=0&&i<(int)items_.size()?&items_[i]:nullptr;
  }
  bool remove(uint32_t id) {
    int i=index(id); if(i<0)return false;
    if(i<=cursor_)--cursor_;
    if(current_==id)current_=0;
    items_.erase(items_.begin()+i); ++revision_; return true;
  }
  bool move(uint32_t id,int direction) {
    int i=index(id); return move_to(id,i+(direction<0?-1:1));
  }
  bool move_to(uint32_t id,int destination) {
    int from=index(id);
    if(from<0||destination<0||destination>=(int)items_.size()||from==destination)return false;
    Item item=std::move(items_[from]);
    items_.erase(items_.begin()+from);items_.insert(items_.begin()+destination,std::move(item));
    if(current_)cursor_=index(current_);
    else { // Keep the next surviving occurrence after a removed playing item.
      if(from<=cursor_&&destination>cursor_)--cursor_;
      else if(from>cursor_&&destination<=cursor_)++cursor_;
    }
    ++revision_;return true;
  }
 private:
  int index(uint32_t id) const {
    for(size_t i=0;i<items_.size();++i)if(items_[i].id==id)return (int)i;
    return -1;
  }
  std::vector<Item> items_;
  uint32_t current_=0,serial_=0;
  uint64_t revision_=0;
  int cursor_=-1;
  bool explicit_=false;
};
