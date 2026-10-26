/* SPDX-License-Identifier: MIT. PXA event-loop continuation, no threads/Asyncify. */
#pragma once
#include <coroutine>
#include <stdint.h>
#include <stddef.h>
#include <utility>
#include "backend.h"

namespace port {
// Nested original functions own their child frames. Destroying the root on NEW
// recursively destroys every pending animation or button wait.
struct alignas(16) Frame { uint8_t bytes[2048]; };
inline Frame pool[16];
inline bool used[16];
inline unsigned active, high_water, largest_frame, transitions, max_transitions;
inline uint16_t wait_line;
inline std::coroutine_handle<> leaf;
inline void fail() { arduboy_pxa_state.fault=6; }
inline void transition() { if (++transitions>128) fail(); }

struct Task {
 struct promise_type;
 using Handle=std::coroutine_handle<promise_type>;
 Handle handle{};
 explicit Task(Handle value={}):handle(value){}
 Task(Task&& other) noexcept:handle(std::exchange(other.handle,{})){}
 Task& operator=(Task&& other) noexcept {
  if(this!=&other){if(handle)handle.destroy();handle=std::exchange(other.handle,{});}return *this;
 }
 Task(const Task&)=delete;
 ~Task(){if(handle)handle.destroy();}
 bool await_ready() const noexcept { return !handle; }
 std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) noexcept;
 void await_resume() const noexcept {}
 struct promise_type {
  std::coroutine_handle<> caller{};
  static void* operator new(size_t n) noexcept {
   if(n>sizeof(Frame)){fail();return nullptr;}
   for(unsigned i=0;i<16;i++)if(!used[i]){
    used[i]=true;++active;if(active>high_water)high_water=active;
    if(n>largest_frame)largest_frame=(unsigned)n;return &pool[i];
   }fail();return nullptr;
  }
  static void operator delete(void* p,size_t) noexcept {
   for(unsigned i=0;i<16;i++)if(p==&pool[i]){used[i]=false;--active;return;}
   fail();
  }
  static Task get_return_object_on_allocation_failure() noexcept { return Task{}; }
  Task get_return_object() noexcept { return Task{Handle::from_promise(*this)}; }
  std::suspend_always initial_suspend() const noexcept {return {};}
  struct Final {
   bool await_ready() const noexcept {return false;}
   std::coroutine_handle<> await_suspend(Handle done) const noexcept {
    transition();leaf=done.promise().caller;
    return leaf?leaf:std::noop_coroutine();
   }
   void await_resume() const noexcept {}
  };
  Final final_suspend() const noexcept {return {};}
  void return_void() const noexcept {}
  void unhandled_exception() const noexcept {fail();__builtin_trap();}
 };
};
inline std::coroutine_handle<> Task::await_suspend(std::coroutine_handle<> caller) noexcept {
 transition();handle.promise().caller=caller;leaf=handle;return handle;
}
struct Delay {
 uint32_t milliseconds;
 uint16_t line;
 bool await_ready() const noexcept {return false;}
 void await_suspend(std::coroutine_handle<> suspended) const noexcept {
  transition();leaf=suspended;wait_line=line;arduboy_pxa_wait(milliseconds);
 }
 void await_resume() const noexcept {}
};
}
