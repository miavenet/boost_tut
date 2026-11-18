// l3_behind_the_scenes.cpp — how a default-constructed hft::allocator finds "its" arena.
// thread_arena's constructor stores `this` in a thread_local pointer; hft::allocator reads it.
// Two threads, same container TYPE, each lands in its own thread's region.
// Run: ./build/l3_behind_the_scenes
#include "hft_mem/hft_mem.hpp"

#include <cstdio>
#include <thread>

void worker(int id) {
    hft::thread_arena arena;                  // (on Linux: hft::pin_this_thread(cpu) first)
    hft::vector<hft::string> names;           // no resource pointer passed anywhere
    names.emplace_back("a string long enough to need a heap buffer, not the SSO one");
    std::printf("thread %d: arena=%p  vector buffer in MY region? %s  string buffer in MY region? %s\n",
                id, static_cast<void*>(hft::thread_arena::current()),
                arena.region().owns(names.data()) ? "yes" : "no",
                arena.region().owns(names[0].data()) ? "yes" : "no");
}

int main() {
    std::printf("sizeof(hft::allocator<int>) = %zu (empty class: no pointer stored)\n", sizeof(hft::allocator<int>));
    std::printf("sizeof(hft::vector<int>) = %zu, sizeof(std::pmr::vector<int>) = %zu\n",
                sizeof(hft::vector<int>), sizeof(std::pmr::vector<int>));
    std::thread a(worker, 1), b(worker, 2);
    a.join();
    b.join();
    std::printf("main thread has an arena? %s\n", hft::thread_arena::current() ? "yes" : "no");
    try {
        hft::vector<int> v;
        v.push_back(1);                       // no arena on main: refuses instead of guessing
    } catch (const std::logic_error& e) {
        std::printf("main thread push_back: %s\n", e.what());
    }
}
