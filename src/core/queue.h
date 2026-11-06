#pragma once
#include <unordered_set>
#include <random>
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

// Shuffle (music): a deck of the list's songs in a random order, each once a round; repeat-all
// deals a new round when it runs out. The songs played this round, newest last, are the way back.
// The deck keeps its order for the songs still in the list; songs added later go in at random
// places, songs removed leave it.
class ShuffleDeck {
 public:
  explicit ShuffleDeck(uint32_t seed = std::random_device{}()) : rng_(seed) {}
  void reset() { deck_.clear(); played_.clear(); back_.clear(); rev_ = ~0ull; cur_ = 0; }
  void sync(const MediaQueue& q, bool loop_all) {
    const uint32_t cur = q.current();
    if (q.revision() == rev_ && cur == cur_ && !(deck_.empty() && loop_all)) return;
    if (cur != cur_) {
      if (cur_) played_.insert(cur_);
      if (!back_.empty() && back_.back() == cur) back_.pop_back();  // back to the song before
      else if (cur_ && q.get(cur_)) { back_.push_back(cur_); if (back_.size() > MediaQueue::kLimit) back_.erase(back_.begin()); }
      cur_ = cur;
    }
    rev_ = q.revision();
    std::vector<uint32_t> deck;
    for (uint32_t id : deck_) if (id != cur && q.get(id) && !played_.count(id)) deck.push_back(id);
    for (const auto& i : q.items())
      if (i.id != cur && !played_.count(i.id) && std::find(deck.begin(), deck.end(), i.id) == deck.end()) {
        std::uniform_int_distribution<size_t> at(0, deck.size());
        deck.insert(deck.begin() + (std::ptrdiff_t)at(rng_), i.id);
      }
    if (deck.empty() && loop_all && q.items().size() > 1) {  // a new round
      played_.clear();
      for (const auto& i : q.items()) if (i.id != cur) deck.push_back(i.id);
      std::shuffle(deck.begin(), deck.end(), rng_);
    }
    deck_ = std::move(deck);
  }
  const MediaQueue::Item* next(const MediaQueue& q) const { return deck_.empty() ? nullptr : q.get(deck_.front()); }
  const MediaQueue::Item* previous(const MediaQueue& q) const {
    for (auto it = back_.rbegin(); it != back_.rend(); ++it) if (const auto* i = q.get(*it)) return i;
    return nullptr;
  }
  const std::vector<uint32_t>& deck() const { return deck_; }
 private:
  std::vector<uint32_t> deck_, back_;
  std::unordered_set<uint32_t> played_;
  uint64_t rev_ = ~0ull;
  uint32_t cur_ = 0;
  std::mt19937 rng_;
};
