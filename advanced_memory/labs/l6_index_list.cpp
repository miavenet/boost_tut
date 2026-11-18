// l6_index_list.cpp — 32-bit index links into the slab instead of 64-bit pointer hooks.
// Run: ./labs/build/l6_index_list
// Shows: bytes per hook, orders per cache line, and that an index-linked structure survives
// being copied to another address (relocatable) while a pointer-linked one does not.
#include "hft_mem/hft_handroll.hpp"

#include <boost/interprocess/offset_ptr.hpp>
#include <boost/intrusive/list.hpp>
#include <new>

#include <cstdio>
#include <cstring>
#include <vector>

namespace bi = boost::intrusive;

struct PtrOrder {            // 32 B payload + 3 pointer hooks (level, client, active)
    long id, px, qty, client;
    bi::list_member_hook<> level, by_client, active;
};
struct IdxOrder {            // the same payload + 3 index hooks
    long id, px, qty, client;
    hr::ilink level, by_client, active;
};

int main() {
    std::printf("hook: pointer %zu B, index %zu B\n", sizeof(bi::list_member_hook<>), sizeof(hr::ilink));
    std::printf("order with 3 hooks: pointer %zu B, index %zu B\n", sizeof(PtrOrder), sizeof(IdxOrder));
    std::printf("orders per 128 B line: pointer %.2f, index %.2f;  per 64 B line: %.2f, %.2f\n",
                128.0 / sizeof(PtrOrder), 128.0 / sizeof(IdxOrder), 64.0 / sizeof(PtrOrder), 64.0 / sizeof(IdxOrder));

    // ---- relocation: copy the whole slab to a new address ----------------------------------
    constexpr int N = 8;
    std::vector<IdxOrder> a(N);
    hr::idx_list<IdxOrder, &IdxOrder::level> lvl(a.data());
    for (int i = 0; i < N; ++i) { a[i].id = 100 + i; if (i % 2 == 0) lvl.push_back(static_cast<std::uint32_t>(i)); }

    const std::uint32_t head = lvl.front();
    std::vector<IdxOrder> b(N);
    std::memcpy(static_cast<void*>(b.data()), a.data(), sizeof(IdxOrder) * N);   // e.g. snapshot, shm, mmap'd file
    std::memset(static_cast<void*>(a.data()), 0, sizeof(IdxOrder) * N);           // the original is gone
    std::printf("index list walked in the COPY:");
    for (std::uint32_t i = head; i != hr::nil; i = b[i].level.next) std::printf(" %ld", b[i].id);
    std::puts("   <- intact: links are slot numbers, not addresses");

    // The same with pointer hooks (memcpy of a hooked object is formally UB; this shows why).
    using PL = bi::list<PtrOrder, bi::member_hook<PtrOrder, bi::list_member_hook<>, &PtrOrder::level>>;
    std::vector<PtrOrder> pa(N), pb(N);
    PL plist;
    for (int k = 0; k < N; k += 2) { pa[k].id = 100 + k; plist.push_back(pa[k]); }
    std::memcpy(static_cast<void*>(pb.data()), pa.data(), sizeof(PtrOrder) * N);
    const PtrOrder* nx = &*std::next(PL::s_iterator_to(pb[0]));
    std::printf("pointer list: COPY's slot 0 says 'next' is at %s\n",
                (nx >= pa.data() && nx < pa.data() + N) ? "the OLD array (dangling once it is gone)" : "the copy");
    plist.clear();
    std::memset(static_cast<void*>(pb.data()), 0, sizeof(PtrOrder) * N);   // else ~hook (safe_link) sees "linked"

    // Boost's own answer to relocation: hooks with offset_ptr (self-relative) pointers.
    // https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/using_smart_pointers.html
    using OH = bi::list_member_hook<bi::void_pointer<boost::interprocess::offset_ptr<void>>,
                                    bi::link_mode<bi::normal_link>>;
    struct OffOrder { long id; OH h; };
    using OL = bi::list<OffOrder, bi::member_hook<OffOrder, OH, &OffOrder::h>>;
    struct Region { OL list; OffOrder objs[N]; };          // list header AND nodes move together
    alignas(Region) static unsigned char ra[sizeof(Region)], rb[sizeof(Region)];
    Region* r1 = ::new (static_cast<void*>(ra)) Region;
    for (int k = 0; k < N; k += 2) { r1->objs[k].id = 100 + k; r1->list.push_back(r1->objs[k]); }
    std::memcpy(rb, ra, sizeof(Region));
    std::memset(ra, 0, sizeof(Region));
    std::printf("offset_ptr hook: %zu B; list walked in the COPY:", sizeof(OH));
    for (auto& o : reinterpret_cast<Region*>(rb)->list) std::printf(" %ld", o.id);
    std::puts("   <- also intact, but 16 B per hook, not 8");
}
