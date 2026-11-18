// l5_membership.cpp — "Is this order in the pending-send list?" answered from the order itself.
// Run: ./labs/build/l5_membership            (all sections)
//      ./labs/build/l5_membership twice      (insert one order into the same list twice: safe_link catches it)
//
// One hook per list an order can be in. Membership = hook.is_linked(): a load and a compare
// on the order's own cache line, no search, no other structure touched.
#include <boost/intrusive/list.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <vector>

namespace bi = boost::intrusive;
using hook = bi::list_member_hook<>;  // safe_link by default

enum class State : std::uint8_t { New, PendingSend, PendingAck, Active, Done };

struct Order {
    long id;
    long px;
    long qty;
    int client;
    std::uint8_t venue;     // which venue the order is routed to (used by fix (a) below)
    State state;            // the TRADITIONAL answer, kept in sync by hand
    hook level_hook;        // price-level FIFO
    hook client_hook;       // per-client list
    hook active_hook;       // all active orders (resting at the venue)
    hook send_hook;         // the per-venue "to be sent" lists: ONE hook shared by N lists
    hook ack_hook;          // pending-ack (sent, not yet acknowledged)
};
template <hook Order::*H>
using list_of = bi::list<Order, bi::member_hook<Order, hook, H>>;
using SendList = list_of<&Order::send_hook>;

struct Lean {               // same payload, no hooks
    long id, px, qty; int client; std::uint8_t venue; State state;
};

int main(int argc, char** argv) {
    const bool twice = argc > 1 && std::string_view(argv[1]) == "twice";

    std::puts("== 1. what the hooks cost ==");
    std::printf("sizeof(hook)=%zu  sizeof(Lean)=%zu  sizeof(Order with 5 hooks)=%zu\n",
                sizeof(hook), sizeof(Lean), sizeof(Order));
    std::printf("orders per 128 B line (this Mac): lean %.2f, hooked %.2f;  per 64 B line (x86): lean %.2f, hooked %.2f\n",
                128.0 / sizeof(Lean), 128.0 / sizeof(Order), 64.0 / sizeof(Lean), 64.0 / sizeof(Order));

    constexpr int V = 4;
    std::array<SendList, V> to_venue;      // send lists, one per venue, all through send_hook
    list_of<&Order::ack_hook> pending_ack;
    list_of<&Order::active_hook> active;

    std::vector<Order> store(8);           // stand-in for the slab (lesson 06); never resized
    for (int i = 0; i < 8; ++i) {
        Order& o = store[i];
        o.id = 100 + i; o.px = 10; o.qty = 1; o.client = 0; o.state = State::New;
        o.venue = static_cast<std::uint8_t>(i % V);
    }

    std::puts("\n== 2. membership by hook (no search) ==");
    Order& a = store[2];                   // venue 2
    to_venue[a.venue].push_back(a); a.state = State::PendingSend;
    std::printf("order %ld: in a send list? %d  pending-ack? %d  active? %d\n",
                a.id, a.send_hook.is_linked(), a.ack_hook.is_linked(), a.active_hook.is_linked());
    // transition send -> ack: O(1) unlink + O(1) push_back, nothing allocated
    to_venue[a.venue].erase(to_venue[a.venue].iterator_to(a));
    pending_ack.push_back(a); a.state = State::PendingAck;
    std::printf("after send:  in a send list? %d  pending-ack? %d\n", a.send_hook.is_linked(), a.ack_hook.is_linked());

    std::puts("\n== 3. the enum can disagree with the lists; the hook cannot ==");
    Order& b = store[5];
    to_venue[b.venue].push_back(b);        // a code path that forgot `b.state = PendingSend`
    std::printf("order %ld: state says PendingSend? %d   send hook says linked? %d   <- two sources of truth\n",
                b.id, b.state == State::PendingSend, b.send_hook.is_linked());

    std::puts("\n== 4. the Venue X trap: ONE hook shared by N venue lists ==");
    Order& c = store[1];                   // venue 1
    to_venue[c.venue].push_back(c);
    const int X = 3;
    std::printf("asked 'is order %ld queued for venue %d?'\n", c.id, X);
    std::printf("  wrong: send_hook.is_linked()                 -> %d   (it is queued for venue %d!)\n",
                c.send_hook.is_linked(), c.venue);
    std::printf("  fix a: is_linked() && o.venue == X  (1 byte)  -> %d\n",
                c.send_hook.is_linked() && c.venue == X);
    // fix c: walk from the node to the list header and compare with each list's end().
    // Boost list iterators compare node pointers (detail/list_iterator.hpp), so end() of the
    // right list is recognised. Cost: O(position in the list) steps x V compares.
    auto venue_by_walk = [&](Order& o) {
        int steps = 0;
        for (auto it = SendList::s_iterator_to(o);; ++it, ++steps)
            for (int v = 0; v < V; ++v)
                if (it == to_venue[v].end()) return std::pair{v, steps};
    };
    auto [v, steps] = venue_by_walk(c);
    std::printf("  fix c: walk to the header                   -> venue %d after %d step(s)\n", v, steps);

    // How the walk grows: put 100,000 orders in front of the probe in venue 0's list.
    {
        std::vector<Order> many(100'001);
        for (auto& o : many) to_venue[0].push_back(o);
        Order& last = many.front();          // first in, so 100,000 steps from the header
        auto t0 = std::chrono::steady_clock::now();
        auto r = venue_by_walk(last);
        auto t1 = std::chrono::steady_clock::now();
        std::printf("  fix c for the first of 100,001 orders in one list: %d steps, %lld us\n", r.second,
                    (long long)std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());
        to_venue[0].clear();
    }
    std::printf("  fix b: one hook per venue: %d venues x %zu B = %zu B more per order\n", V, sizeof(hook), V * sizeof(hook));

    std::puts("\n== 5. the cheap alternative: a membership bitmask ==");
    std::uint8_t member = 0;               // bit k = "in list k"; 1 byte for 8 lists
    member |= 1u << 3;
    std::printf("bit test in list 3? %d   (O(1), 1 bit per list, but no next/prev and no O(1) unlink:\n"
                "   the list itself must still be found and searched to remove the order)\n", (member >> 3) & 1);

    if (twice) {
        std::puts("\n== 6. the same order into the SAME list twice ==");
        Order& d = store[7];
        active.push_back(d);
        std::puts("pushed once; pushing again (safe_link asserts in builds without NDEBUG)...");
        std::fflush(stdout);
        active.push_back(d);
        std::printf("survived the second push_back: active.size()=%zu\n", active.size());
        std::fflush(stdout);
    }

    for (auto& l : to_venue) l.clear();    // unlink before the orders die (lesson 06)
    pending_ack.clear();
    std::puts("unlinked send + ack lists"); std::fflush(stdout);
    active.clear();
    std::puts("\ndone");
}
