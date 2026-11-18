// l7_ordered_free.cpp — why boost::object_pool::destroy (= ordered_free) is fast in one run and
// 300x slower in the next, with the same binary and the same input.
// Run it several times: for i in 1 2 3 4 5 6; do ./labs/build/l7_ordered_free; done
//
// ordered_free keeps the free list sorted by address: "O(N) with respect to the size of the free
// list" (boost/pool/pool.hpp). It walks from the head (lowest free address) until it finds the
// freed block's place. If the pool's newest chunk (where the free blocks are) happens to sit
// BELOW the older chunks in memory, every free of an object in an older chunk walks past all of
// them. Where malloc places the chunk varies from process to process.
#include <boost/pool/pool.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

int main() {
    constexpr int N = 50'000, OPS = 400'000;
    boost::pool<> p(64);
    std::vector<void*> live(N);
    for (auto& x : live) x = p.malloc();
    void* f = p.malloc();                      // the head of the free list: the lowest free address
    p.ordered_free(f);
    long above = 0;
    for (void* x : live) above += reinterpret_cast<std::uintptr_t>(x) > reinterpret_cast<std::uintptr_t>(f);
    std::uint64_t s = 88172645463325252ull;
    auto rnd = [&] { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; };
    auto t0 = std::chrono::steady_clock::now();
    for (int k = 0; k < OPS; ++k) {
        int j = static_cast<int>(rnd() % N);
        p.ordered_free(live[j]);
        live[j] = p.malloc();
    }
    double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / OPS;
    std::printf("live objects ABOVE the lowest free block: %5.1f%%   mean ordered_free + malloc: %7.1f ns\n",
                100.0 * above / N, ns);
}
