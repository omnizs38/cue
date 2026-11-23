// (1.4.1, 22/11) A worker that is started and let go, as std::thread(...).detach() did, with the
// same contract: the work runs once on a new thread; std::system_error (or std::bad_alloc) when
// the thread cannot be made, and then the work never runs; an exception escaping the work ends Cue
// (std::terminate), as it did.
//
// Why: the crash once in 95 minutes of the 17/11 monkey (an access violation reading 0 in
// libstdc++'s execute_native_thread_routine+6, the first instruction of a new std::thread) is the
// MinGW thread library's: a short worker made and let go while others end can start with its work
// missing. A program making such workers the way Cue does crashed so under Wine in 6 of 7 runs with
// std::thread(...).detach(), never through this (docs/VALIDATION.md). On Windows the thread comes
// straight from the system (CreateThread), with nothing in between.
#pragma once
#include <memory>
#include <system_error>
#include <type_traits>
#include <utility>
#ifdef _WIN32
#include <windows.h>
#else
#include <thread>
#endif

namespace detached_detail {
struct Task {
  virtual ~Task() = default;
  virtual void run() = 0;
};
template <class F>
struct TaskOf final : Task {
  F f;
  explicit TaskOf(F&& x) : f(std::move(x)) {}
  void run() override { f(); }
};
#ifdef _WIN32
inline DWORD WINAPI entry(void* p) noexcept {
  std::unique_ptr<Task> task(static_cast<Task*>(p));
  task->run();
  return 0;
}
#endif
}  // namespace detached_detail

template <class F>
void start_detached(F&& work) {
  using Fn = std::decay_t<F>;
#ifdef _WIN32
  auto task = std::make_unique<detached_detail::TaskOf<Fn>>(Fn(std::forward<F>(work)));
  HANDLE h = CreateThread(nullptr, 0, detached_detail::entry, task.get(), 0, nullptr);
  if (!h) throw std::system_error((int)GetLastError(), std::system_category(), "CreateThread");
  task.release();  // the new thread owns it now
  CloseHandle(h);
#else
  std::thread(Fn(std::forward<F>(work))).detach();
#endif
}
