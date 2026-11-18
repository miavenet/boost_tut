// hft_book.hpp — an intrusive order book over hft::slab (lesson 07; used by labs/l7_order_book
// and bench/b5_book). Example code, not part of the allocator library.
//
//   Order storage   hft::slab<Order>   (carved once from the thread arena)
//   price level     intrusive FIFO      (level_hook)        time priority within a price
//   per client      intrusive list      (client_hook)       mass cancel
//   state lists     pending_send / pending_ack (one hook each): membership = is_linked()
//   id -> Order*    IdIndex::flat  hft::unordered_flat_map<long, Order*>, reserved from the arena
//                                  (pre-faulted; exchange-assigned ids)
//                   IdIndex::handle the id IS the slab handle {index, generation} (ids you assign)
//
//   cancel(id) = id lookup + unlink from every list it is in + slab.destroy: all O(1).
//   add() returns nullptr (reject) when the slab is full, px/client is out of range, or (flat)
//   the id is already live. The flat id map is reserved for 2 x capacity: reserved for exactly
//   the live count, unordered_flat_map reallocates under steady churn (lesson 06 C3,
//   labs/l6_flat_drift); at <= 50% load it does not (tests/run.sh mode book-churn-full).
#pragma once

#include "hft_slab.hpp"

#include <boost/intrusive/list.hpp>
#include <boost/unordered/unordered_flat_map.hpp>

#include <cstdint>
#include <vector>

namespace hft::book {

static_assert(sizeof(long) >= 8, "IdIndex::handle packs {index, generation} into one long");

namespace bi = boost::intrusive;
using hook = bi::list_member_hook<>;   // safe_link: is_linked() works, debug builds assert misuse

struct Order {
    long id;
    long qty;
    std::int32_t px;          // price in ticks = level index
    std::int32_t client;
    hook level_hook;
    hook client_hook;
    hook send_hook;           // queued to be sent to the venue
    hook ack_hook;            // sent, waiting for the venue's ack
};
template <hook Order::*H>
using list_of = bi::list<Order, bi::member_hook<Order, hook, H>>;
using Level = list_of<&Order::level_hook>;
using ClientList = list_of<&Order::client_hook>;
using SendQueue = list_of<&Order::send_hook>;
using AckQueue = list_of<&Order::ack_hook>;

enum class IdIndex { flat, handle };

template <IdIndex Ix = IdIndex::flat>
class book {
public:
    book(std::size_t capacity, int levels, int clients)
        : orders_(capacity), levels_(levels), clients_(clients) {
        if constexpr (Ix == IdIndex::flat) ids_.reserve(2 * capacity);   // headroom: see above
    }
    ~book() { clear(); }
    book(const book&) = delete;
    book& operator=(const book&) = delete;

    // Returns the order, or nullptr if the slab is full (reject). For IdIndex::handle the id
    // argument is ignored and the order's id becomes its handle (index << 32 | generation).
    Order* add(long id, int px, int client, long qty) {
        if (px < 0 || static_cast<std::size_t>(px) >= levels_.size() ||
            client < 0 || static_cast<std::size_t>(client) >= clients_.size()) return nullptr;
        Order* o = orders_.try_create();
        if (!o) return nullptr;
        o->qty = qty; o->px = px; o->client = client;
        if constexpr (Ix == IdIndex::flat) {
            o->id = id;
            bool fresh;
            try { fresh = ids_.try_emplace(id, o).second; }
            catch (...) { orders_.destroy(o); throw; }      // e.g. bad_alloc under seal(): no orphan
            if (!fresh) { orders_.destroy(o); return nullptr; }   // duplicate id
        } else {
            auto h = orders_.handle_of(o);
            o->id = static_cast<long>((std::uint64_t{h.index} << 32) | h.gen);
        }
        levels_[px].push_back(*o);
        clients_[client].push_back(*o);
        send_.push_back(*o);               // new orders wait to be sent
        return o;
    }
    Order* find(long id) const {
        if constexpr (Ix == IdIndex::flat) {
            auto it = ids_.find(id);
            return it == ids_.end() ? nullptr : it->second;
        } else {
            auto u = static_cast<std::uint64_t>(id);
            return orders_.get({static_cast<std::uint32_t>(u >> 32), static_cast<std::uint32_t>(u)});
        }
    }
    bool cancel(long id) {
        Order* o = find(id);
        if (!o) return false;
        unlink_all(*o);
        if constexpr (Ix == IdIndex::flat) ids_.erase(id);
        orders_.destroy(o);
        return true;
    }
    // qty down keeps time priority (in place); anything else would be cancel + add.
    bool reduce(long id, long new_qty) {
        Order* o = find(id);
        if (!o || new_qty >= o->qty) return false;
        o->qty = new_qty;
        return true;
    }
    // state transitions: O(1) unlink + push_back, nothing allocated. false (and no change) if
    // the order is not in the state the transition starts from.
    bool mark_sent(Order& o) {
        if (!o.send_hook.is_linked()) return false;
        send_.erase(SendQueue::s_iterator_to(o)); ack_.push_back(o); return true;
    }
    bool mark_acked(Order& o) {
        if (!o.ack_hook.is_linked()) return false;
        ack_.erase(AckQueue::s_iterator_to(o)); return true;
    }
    static bool is_pending_send(const Order& o) { return o.send_hook.is_linked(); }
    static bool is_pending_ack(const Order& o) { return o.ack_hook.is_linked(); }

    template <class F>
    void walk_level(int px, F&& f) const { for (const Order& o : levels_[px]) f(o); }
    long level_qty(int px) const { long s = 0; for (const Order& o : levels_[px]) s += o.qty; return s; }

    std::size_t cancel_client(int client) {   // mass cancel: walk ONE list, unlink everywhere
        std::size_t n = 0;
        auto& cl = clients_[client];
        while (!cl.empty()) { cancel_ptr(cl.front()); ++n; }
        return n;
    }
    SendQueue& send_queue() { return send_; }
    AckQueue& ack_queue() { return ack_; }
    std::uint32_t size() const { return orders_.size(); }
    std::size_t id_bucket_count() const {           // flat: to see that churn never rehashes
        if constexpr (Ix == IdIndex::flat) return ids_.bucket_count(); else return 0;
    }
    const Level& level(int px) const { return levels_[px]; }

    void clear() {
        for (auto& l : levels_)
            while (!l.empty()) cancel_ptr(l.front());
    }

private:
    void unlink_all(Order& o) {
        levels_[o.px].erase(Level::s_iterator_to(o));
        clients_[o.client].erase(ClientList::s_iterator_to(o));
        if (o.send_hook.is_linked()) send_.erase(SendQueue::s_iterator_to(o));
        if (o.ack_hook.is_linked()) ack_.erase(AckQueue::s_iterator_to(o));
    }
    void cancel_ptr(Order& o) {
        unlink_all(o);
        if constexpr (Ix == IdIndex::flat) ids_.erase(o.id);
        orders_.destroy(&o);
    }

    slab<Order> orders_;                  // declared FIRST: storage outlives every list below
    std::vector<Level> levels_;
    std::vector<ClientList> clients_;
    SendQueue send_;
    AckQueue ack_;
    struct none {};
    [[no_unique_address]] std::conditional_t<Ix == IdIndex::flat, hft::unordered_flat_map<long, Order*>, none> ids_;
};

}  // namespace hft::book
