#include "../src/core/store.h"
#include <cassert>
#include <cstdio>
#include <chrono>
#include "../src/core/file_io.h"
#include <fstream>
#include <iterator>

static std::string bytes(const std::filesystem::path& p) { std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}}; }
int main() {
  auto dir=std::filesystem::temp_directory_path()/"cue-store-io-regression";
  std::filesystem::remove_all(dir);std::filesystem::create_directories(dir);
  auto path=dir/"state.json";auto backup=dir/"state.json.bak";
  Store points;points.touch("clip.mp4",100,1);
  assert(points.set_position("clip.mp4",20,100));
  assert(!points.set_position("clip.mp4",20,100));
  assert(points.set_position("clip.mp4",20,101));
  assert(!points.set_position("missing.mp4",20,100));
  Store store;assert(store.save(path));auto original=bytes(path);
  assert(store.save(path) && bytes(backup)==original); // seed the recovery copy once
  auto stamp=std::filesystem::file_time_type::clock::now()-std::chrono::hours(1);
  std::filesystem::last_write_time(path,stamp);std::filesystem::last_write_time(backup,stamp);
  // Read back the stored time: some runtimes (e.g. MinGW's std::filesystem) do not return the
  // exact value that was set, so compare against what the filesystem reports. What matters is
  // that the unchanged saves leave it alone.
  auto stamp_path=std::filesystem::last_write_time(path),stamp_backup=std::filesystem::last_write_time(backup);
  for(int i=0;i<1000;++i)assert(store.save(path));
  assert(std::filesystem::last_write_time(path)==stamp_path && std::filesystem::last_write_time(backup)==stamp_backup);
  store.volume=72;assert(store.save(path));assert(bytes(path)!=original && bytes(backup)==original);
  auto changed=bytes(path);assert(store.save(path) && bytes(backup)==changed);
  std::filesystem::last_write_time(path,stamp);std::filesystem::last_write_time(backup,stamp);
  auto stamp_path2=std::filesystem::last_write_time(path),stamp_backup2=std::filesystem::last_write_time(backup);
  for(int i=0;i<100;++i)assert(store.save(path));
  assert(bytes(path)==changed && bytes(backup)==changed);
  assert(std::filesystem::last_write_time(path)==stamp_path2 && std::filesystem::last_write_time(backup)==stamp_backup2);
  assert(!std::filesystem::exists(dir/"state.json.tmp"));
  assert(file_contents_equal(path,changed) && !file_contents_equal(path,changed+"x"));
  assert(!file_contents_equal(path,std::string(changed.size(),'x')));
  assert(!file_contents_equal(dir/"missing", ""));
  { std::ofstream f(dir/"empty"); } assert(file_contents_equal(dir/"empty",""));
  std::string large(12000,'x');{ std::ofstream f(dir/"large",std::ios::binary);f<<large; }
  assert(file_contents_equal(dir/"large",large));large[8000]='y';assert(!file_contents_equal(dir/"large",large));
  Store loaded;assert(loaded.load(path) && loaded.volume==72);
  // Durable write: the store pushes the temporary file to the storage device
  // before it swaps it over the good state, so a power cut cannot leave a torn
  // file behind a backup that has already been replaced.
  {
    const auto files_before=durable_file_flushes(),dirs_before=durable_directory_flushes();
    store.volume=74;assert(store.save(path));           // a changed value: the write path runs
    assert(durable_file_flushes()>files_before);         // the temporary file was flushed
    assert(durable_directory_flushes()>dirs_before);     // the rename was flushed too
  }
  // the flush helpers on their own
  {
    assert(flush_file_to_disk(path));                    // a real file
    assert(!flush_file_to_disk(dir/"missing"));          // a file that is not there
    assert(!flush_file_to_disk(dir));                   // a directory is not a file
    flush_directory(dir);                               // a real directory
    flush_directory(dir/"missing");                     // a directory that is not there
    flush_directory({});                                // no directory at all
  }
  // a file that must not exist yet: a name that is already there (a link to
  // another file) is never opened, so nothing is written through it
  {
    const auto target = dir/"new.txt";
    assert(write_new_file(target, "hello"));
    assert(!write_new_file(target, "again"));           // it exists: not written to
    std::ifstream f(target, std::ios::binary);
    const std::string s((std::istreambuf_iterator<char>(f)), {});
    assert(s == "hello");
    assert(!write_new_file(dir/"missing"/"file.txt", "x"));  // no such folder
  }
  // A paired phone's credential is encrypted once. DPAPI randomizes its output,
  // so encrypting it again on every save would make an unchanged state look
  // changed: every save would rewrite the file and rotate the backup.
  {
    store.remote_credentials = {std::string(64, 'a'), std::string(64, 'b')};
    assert(store.save(path));   // the credentials reach the file
    assert(store.save(path));   // the recovery copy is refreshed once
    assert(bytes(path).find(std::string(64, 'a')) == std::string::npos);
    std::filesystem::last_write_time(path,stamp);std::filesystem::last_write_time(backup,stamp);
    const auto paired_stamp=std::filesystem::last_write_time(path);
    const auto paired_backup_stamp=std::filesystem::last_write_time(backup);
    for(int i=0;i<20;++i)assert(store.save(path));
    assert(std::filesystem::last_write_time(path)==paired_stamp);
    assert(std::filesystem::last_write_time(backup)==paired_backup_stamp);
    Store paired;assert(paired.load(path));
    assert(paired.remote_credentials.size()==2 && paired.remote_credentials[0]==std::string(64,'a'));
    // The window saves on a copy of the store. The protection is prepared on
    // the live store, so every copy carries the same protected bytes; without
    // that, each save would encrypt again (DPAPI output is random) and rewrite.
    Store live;live.remote_credentials={std::string(64,'c')};
    live.prepare_credential_blobs();
    { Store snapshot=live;assert(snapshot.save(path)); }
    std::filesystem::last_write_time(path,stamp);
    const auto snapshot_stamp=std::filesystem::last_write_time(path);
    { Store snapshot=live;assert(snapshot.save(path)); }
    assert(std::filesystem::last_write_time(path)==snapshot_stamp);
    store.remote_credentials.clear();
    assert(store.save(path));
  }
  // Backup copy failure aborts before renaming over the good file: make the
  // backup path a non-empty directory so remove() + copy_file() both fail.
  store.volume=73;
  std::filesystem::remove_all(backup); std::filesystem::create_directories(backup/"x");
  { std::ofstream f(backup/"x"/"y"); f<<"z"; }
  auto before_backup_fail=bytes(path);
  assert(!store.save(path));  // must not rename without a backup
  assert(bytes(path)==before_backup_fail);  // good file untouched
  assert(!std::filesystem::exists(dir/"state.json.tmp"));  // failed save leaves no stale .tmp
  std::filesystem::remove_all(dir);
  std::puts("Settings I/O: after recovery-copy warm-up, 1100 unchanged saves skip file writes/backup rotation; changed settings remain atomic, recoverable and loadable; the temporary file is flushed to the device before it replaces the good state");
}
