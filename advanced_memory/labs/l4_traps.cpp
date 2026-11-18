// l4_traps.cpp — what "behind the scenes" costs you. One trap per mode; predict, then run.
//   ./build/l4_traps outlive      container outlives its arena            (crashes: SIGSEGV)
//   ./build/l4_traps thread       container destroyed on another thread   (checked mode aborts)
//   ./build/l4_traps static       a static container built before the arena exists
//   ./build/l4_traps pmr-copy     copy-constructing a pmr container: which resource does the copy use?
//   ./build/l4_traps pmr-move     move-assigning between two pmr resources: moved or copied?
//   ./build/l4_traps pmr-nested   pmr::vector<std::string> vs pmr::vector<pmr::string>
#include "hft_mem/hft_mem.hpp"

#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>

#include <cstdio>
#include <cstdlib>
#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace bmi = boost::multi_index;
struct Order { long id; long px; };
using Store = hft::multi_index<Order, bmi::indexed_by<bmi::ordered_unique<bmi::member<Order, long, &Order::id>>>>;

static long g_news = 0;  // counts global operator new (single-threaded modes only)
void* operator new(std::size_t n) { ++g_news; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

Store& global_store() {          // a function-local static: built on first use
    static Store s;              // multi_index allocates its header node in the constructor
    return s;
}

int main(int argc, char** argv) {
    std::string_view mode = argc > 1 ? argv[1] : "";

    if (mode == "outlive") {
        std::optional<hft::map<long, long>> m;
        {
            hft::thread_arena arena;
            m.emplace();
            (*m)[1] = 1;
        }                                     // arena destroyed here; the map is still alive
        std::puts("arena gone (its region is unmapped), destroying the map now...");
        std::fflush(stdout);
        m.reset();                            // the map destructor reads its nodes in the unmapped
                                              // region: SIGSEGV before deallocate is ever reached
        std::puts("survived?!");
    } else if (mode == "thread") {
        hft::thread_arena arena;
        auto* m = new hft::map<long, long>;
        (*m)[1] = 1;
        std::thread([m] {
            hft::thread_arena other;          // the worker has ITS OWN arena
            delete m;                         // frees a node into the wrong pool -> abort
        }).join();
        std::puts("survived?!");
    } else if (mode == "static") {
        try {
            global_store().insert({1, 100});  // first use happens before any arena exists
        } catch (const std::logic_error& e) {
            std::printf("static container: %s\n", e.what());
        }
        hft::thread_arena arena;
        std::puts("(fix: create the arena first, then the container, in the thread that owns both)");
    } else if (mode == "pmr-copy") {
        std::pmr::monotonic_buffer_resource mono;
        std::pmr::vector<long> a({1, 2, 3}, &mono);
        std::pmr::vector<long> b(a);          // copy constructor
        std::printf("copy uses a's resource? %s   uses get_default_resource()? %s\n",
                    b.get_allocator().resource() == &mono ? "yes" : "no",
                    b.get_allocator().resource() == std::pmr::get_default_resource() ? "yes" : "no");
    } else if (mode == "pmr-move") {
        std::pmr::monotonic_buffer_resource r1, r2;
        std::pmr::vector<long> a(1000, 7, &r1), b(&r2), c(&r1);
        const long* before = a.data();
        b = std::move(a);                     // different resources: allocator does NOT propagate
        std::printf("move-assign across resources: buffer stolen? %s  (b still on r2? %s)\n",
                    b.data() == before ? "yes" : "no (elements copied into a new r2 buffer)",
                    b.get_allocator().resource() == &r2 ? "yes" : "no");
        std::pmr::vector<long> d(1000, 7, &r1);
        before = d.data();
        c = std::move(d);                     // same resource: cheap pointer steal
        std::printf("move-assign, same resource:   buffer stolen? %s\n", c.data() == before ? "yes" : "no");
    } else if (mode == "pmr-nested") {
        alignas(64) std::byte buf[4096];      // stack buffer + null upstream: mono itself never mallocs
        std::pmr::monotonic_buffer_resource mono(buf, sizeof buf, std::pmr::null_memory_resource());
        const char* text = "a string long enough to need a heap buffer, not the SSO one";
        long n0 = g_news;
        std::pmr::vector<std::string> bad(&mono);
        bad.emplace_back(text);               // vector storage in mono, string bytes from malloc
        long n1 = g_news;
        std::pmr::vector<std::pmr::string> good(&mono);
        good.emplace_back(text);              // uses-allocator construction: string gets &mono too
        long n2 = g_news;
        std::printf("pmr::vector<std::string>      operator new calls: %ld\n", n1 - n0);
        std::printf("pmr::vector<std::pmr::string> operator new calls: %ld\n", n2 - n1);
    } else {
        std::puts("usage: l4_traps outlive|thread|static|pmr-copy|pmr-move|pmr-nested");
        return 2;
    }
    return 0;
}
