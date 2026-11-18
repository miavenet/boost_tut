// b2_alloc_flavors.cpp — what one allocate+deallocate of a 64-byte block costs, by route.
// One sample = a batch of 1000 allocate/deallocate pairs (keeps 16 blocks live in a ring);
// the printed numbers are per PAIR (batch time / 1000), so the ~41 ns timer step is not a limit.
//
//   new/delete                         -> malloc/free
//   pmr via memory_resource* (stateful) -> virtual call into unsynchronized_pool_resource
//   hft::allocator (stateless)          -> thread_local lookup + direct call into the same pool
//   boost::pool<> (a plain object)      -> simple segregated storage, no lock
//   boost::fast_pool_allocator          -> singleton pool behind a std::mutex (default Mutex)
#include "../labs/hft_mem/hft_mem.hpp"
#include "bench_common.hpp"

#include <boost/pool/pool.hpp>
#include <boost/pool/pool_alloc.hpp>

#include <memory_resource>

constexpr int SAMPLES = 2000;
constexpr int BATCH = 1000;
constexpr std::size_t SZ = 64;

template <class Alloc, class Free>
void run(const char* name, Alloc alloc, Free free_) {
    void* ring[16] = {};
    for (auto& p : ring) p = alloc();
    bench::recorder r(SAMPLES);
    for (int s = 0; s < SAMPLES; ++s) {
        auto t0 = bench::now_ns();
        for (int i = 0; i < BATCH; ++i) {
            void*& slot = ring[i & 15];
            free_(slot);
            slot = alloc();
            bench::keep(slot);
        }
        auto t1 = bench::now_ns();
        r.add((t1 - t0) / BATCH);
    }
    for (auto p : ring) free_(p);
    r.report(name);
}

int main() {
    std::printf("== one 64-byte allocate+deallocate pair, ns per pair ==\n");
    run("new/delete (malloc)",
        [] { return ::operator new(SZ); }, [](void* p) { ::operator delete(p); });

    {
        hft::thread_arena arena;
        std::pmr::memory_resource* volatile vr = arena.resource();  // volatile: no devirtualisation
        std::pmr::memory_resource* r = vr;
        run("pmr: memory_resource* -> pool (virtual)",
            [r] { return r->allocate(SZ, 8); }, [r](void* p) { r->deallocate(p, SZ, 8); });
        hft::allocator<std::byte> a;
        run("hft::allocator stateless (TLS + direct)",
            [a]() mutable { return static_cast<void*>(a.allocate(SZ)); },
            [a](void* p) mutable { a.deallocate(static_cast<std::byte*>(p), SZ); });
    }

    {
        boost::pool<> p(SZ);
        run("boost::pool<> object (no lock)",
            [&p] { return p.malloc(); }, [&p](void* x) { p.free(x); });
    }
    {
        boost::fast_pool_allocator<std::array<std::byte, SZ>> fa;
        run("boost::fast_pool_allocator (singleton+mutex)",
            [&fa]() { return static_cast<void*>(fa.allocate(1)); },
            [&fa](void* x) { fa.deallocate(static_cast<std::array<std::byte, SZ>*>(x), 1); });
    }

    std::printf("== container object size: the price of a stateful allocator ==\n");
    std::printf("sizeof(std::map<long,long>)               = %zu\n", sizeof(std::map<long, long>));
    std::printf("sizeof(std::pmr::map<long,long>)          = %zu\n", sizeof(std::pmr::map<long, long>));
    std::printf("sizeof(hft::map<long,long>)               = %zu\n", sizeof(hft::map<long, long>));
    std::printf("sizeof(std::vector<long>)                 = %zu\n", sizeof(std::vector<long>));
    std::printf("sizeof(std::pmr::vector<long>)            = %zu\n", sizeof(std::pmr::vector<long>));
    std::printf("sizeof(hft::vector<long>)                 = %zu\n", sizeof(hft::vector<long>));
}
