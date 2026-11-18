// b4_membership.cpp — "is this order in the pending-send list?" given the order, measured.
// Release build: NDEBUG is defined below (production config). Build: bash labs/build.sh
//
// 100,000 orders; a random half are in the pending-send state. One timed sample = a BATCH of
// queries over pre-generated random orders (batch size in the row name), because one query is
// far below the ~41 ns timer step here. Also: moving an order pending-send -> pending-ack
// (batch of 16), and scanning 1,024 consecutive orders (what the hooks' bytes cost a scan).
#ifndef BENCH_ASSERTS
#  define NDEBUG 1
#endif
#include "bench_common.hpp"

#include <boost/container/flat_set.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index_container.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <list>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace bi = boost::intrusive;
namespace bmi = boost::multi_index;
using hook = bi::list_member_hook<>;
using bench::recorder;

enum class State : std::uint8_t { Active, PendingSend, PendingAck };
struct Order {
    long id, px, qty;
    int client;
    std::uint8_t member_bits;      // bit 0 = pending-send, bit 1 = pending-ack
    State state;
    hook level_hook, client_hook, active_hook, send_hook, ack_hook;
};
struct Lean { long id, px, qty; int client; std::uint8_t member_bits; State state; };
using SendList = bi::list<Order, bi::member_hook<Order, hook, &Order::send_hook>>;
using AckList = bi::list<Order, bi::member_hook<Order, hook, &Order::ack_hook>>;

constexpr int N = 100'000, SAMPLES = 100'000, BATCH = 64;

// Usage: b4_membership [row]   rows: hook bits enum uset flatset mi list | t_hook t_uset t_enum t_list
//        | scan_hooked scan_lean. No argument = all rows in one process; bench/run_intrusive.sh
//        runs each row in its own process (the first row measured in a process can pay warm-up).
int main(int argc, char** argv) {
    std::string_view R = argc > 1 ? argv[1] : "";
    auto sel = [&](std::string_view r) { return R.empty() || R == r; };
    std::printf("== MEMBERSHIP (given the order) == sizeof(Order)=%zu sizeof(Lean)=%zu\n", sizeof(Order), sizeof(Lean));
    std::vector<Order> orders(N);
    bench::xorshift rng;
    SendList send; AckList ack;
    std::unordered_set<long> send_ids;  send_ids.reserve(N);
    boost::container::flat_set<long> send_flat;
    std::list<Order*> send_ptrs;
    for (int i = 0; i < N; ++i) {
        Order& o = orders[i];
        o.id = i; o.px = i & 1023; o.qty = 1; o.client = i & 15; o.member_bits = 0; o.state = State::Active;
        if (rng() & 1) {
            send.push_back(o); o.member_bits = 1; o.state = State::PendingSend;
            send_ids.insert(o.id); send_flat.insert(o.id); send_ptrs.push_back(&o);
        }
    }
    using MI = bmi::multi_index_container<Lean, bmi::indexed_by<bmi::hashed_unique<bmi::member<Lean, long, &Lean::id>>>>;
    MI mi; mi.reserve(N);
    for (auto& o : orders) mi.insert(Lean{o.id, o.px, o.qty, o.client, o.member_bits, o.state});

    std::vector<std::uint32_t> q(SAMPLES * BATCH);
    for (auto& x : q) x = static_cast<std::uint32_t>(rng() % N);

    auto run = [&](const char* name, auto&& pred, int samples, int batch) {
        recorder r(samples);
        long hits = 0;
        for (int s = 0; s < samples; ++s) {
            const std::uint32_t* b = &q[static_cast<std::size_t>(s) * batch];
            auto t0 = bench::now_ns();
            for (int k = 0; k < batch; ++k) hits += pred(orders[b[k]]);
            auto t1 = bench::now_ns();
            r.add(t1 - t0);
        }
        bench::keep(hits);
        r.report(name);
    };
    if (sel("hook")) run("hook.is_linked() x64", [](const Order& o) { return o.send_hook.is_linked(); }, SAMPLES, BATCH);
    if (sel("bits")) run("bitmask bit test x64", [](const Order& o) { return o.member_bits & 1; }, SAMPLES, BATCH);
    if (sel("enum")) run("state enum compare x64", [](const Order& o) { return o.state == State::PendingSend; }, SAMPLES, BATCH);
    if (sel("uset")) run("unordered_set<id>.contains x64", [&](const Order& o) { return send_ids.contains(o.id); }, SAMPLES, BATCH);
    if (sel("flatset")) run("flat_set<id>.contains x64", [&](const Order& o) { return send_flat.contains(o.id); }, SAMPLES, BATCH);
    if (sel("mi")) run("multi_index find(id)->state x64", [&](const Order& o) { return mi.find(o.id)->state == State::PendingSend; }, SAMPLES, BATCH);
    if (sel("list")) run("std::list<Order*> find x1 (50k list)", [&](const Order& o) {
        return std::find(send_ptrs.begin(), send_ptrs.end(), &o) != send_ptrs.end(); }, 2'000, 1);

    std::puts("== TRANSITION pending-send -> pending-ack (and back), batch of 16 ==");
    std::vector<Order*> in_send;
    for (auto& o : send) in_send.push_back(&o);
    constexpr int TB = 16, TS = 20'000;
    auto trans = [&](const char* name, auto&& fwd, auto&& back) {
        recorder r(TS);
        for (int s = 0; s < TS; ++s) {
            Order** b = &in_send[(static_cast<std::size_t>(s) * TB) % (in_send.size() - TB)];
            auto t0 = bench::now_ns();
            for (int k = 0; k < TB; ++k) fwd(*b[k]);
            auto t1 = bench::now_ns();
            for (int k = 0; k < TB; ++k) back(*b[k]);   // untimed: restore the state
            r.add(t1 - t0);
        }
        r.report(name);
    };
    if (sel("t_hook")) trans("hooks: unlink + push_back x16",
          [&](Order& o) { send.erase(SendList::s_iterator_to(o)); ack.push_back(o); },
          [&](Order& o) { ack.erase(AckList::s_iterator_to(o)); send.push_back(o); });
    std::unordered_set<long> ack_ids; ack_ids.reserve(N);
    if (sel("t_uset")) trans("unordered_set<id>: erase + insert x16",
          [&](Order& o) { send_ids.erase(o.id); ack_ids.insert(o.id); },
          [&](Order& o) { ack_ids.erase(o.id); send_ids.insert(o.id); });
    if (sel("t_enum")) trans("state enum assign x16", [](Order& o) { o.state = State::PendingAck; }, [](Order& o) { o.state = State::PendingSend; });
    if (sel("t_list")) {
        std::list<Order*> ack_ptrs;
        constexpr int TS2 = 500;
        recorder r(TS2);
        for (int s = 0; s < TS2; ++s) {
            Order* o = in_send[(static_cast<std::size_t>(s) * 97) % in_send.size()];
            auto t0 = bench::now_ns();
            send_ptrs.erase(std::find(send_ptrs.begin(), send_ptrs.end(), o));
            ack_ptrs.push_back(o);
            auto t1 = bench::now_ns();
            ack_ptrs.pop_back(); send_ptrs.push_back(o);
            r.add(t1 - t0);
        }
        r.report("std::list<Order*>: find+erase+push x1");
    }

    std::puts("== SCAN 1,024 consecutive orders, sum qty (bytes per order matter) ==");
    std::vector<Lean> lean(N);
    for (int i = 0; i < N; ++i) lean[i].qty = orders[i].qty;
    auto scan = [&](const char* name, auto& v) {
        recorder r(20'000);
        for (int s = 0; s < 20'000; ++s) {
            std::size_t start = (rng() % (N - 1024));
            long sum = 0;
            auto t0 = bench::now_ns();
            for (std::size_t k = start; k < start + 1024; ++k) sum += v[k].qty;
            auto t1 = bench::now_ns();
            bench::keep(sum);
            r.add(t1 - t0);
        }
        r.report(name);
    };
    if (sel("scan_hooked")) scan("scan 1024 hooked orders (112 B each)", orders);
    if (sel("scan_lean")) scan("scan 1024 lean orders (32 B each)", lean);

    send.clear(); ack.clear();
}
