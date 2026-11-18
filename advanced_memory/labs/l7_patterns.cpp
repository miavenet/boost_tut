// l7_patterns.cpp — three more HFT patterns, each an intrusive list over a slab:
//   1. timer wheel: one intrusive list per tick bucket; schedule / cancel / expire, all O(1) per timer
//   2. pending-ack queue: FIFO by send time, so the oldest is always at the front (timeout check O(1))
//   3. LRU: touch = unlink + push_back; evict = pop_front. Plus the id lookup it needs.
// Run: ./labs/build/l7_patterns
#include "hft_mem/hft_slab.hpp"

#include <boost/intrusive/list.hpp>
#include <boost/intrusive/unordered_set.hpp>

#include <array>
#include <cstdio>
#include <functional>
#include <vector>

namespace bi = boost::intrusive;

// ---- 1. timer wheel ----------------------------------------------------------------------------
struct Timer {
    long id;
    long due;                                   // absolute tick
    bi::list_member_hook<> wheel_hook;
};
using Bucket = bi::list<Timer, bi::member_hook<Timer, bi::list_member_hook<>, &Timer::wheel_hook>,
                        bi::constant_time_size<false>>;          // 16 B header, size() unused

template <std::size_t Slots>                    // power of two; delays >= Slots wrap (checked on expiry)
class timer_wheel {
public:
    explicit timer_wheel(std::size_t capacity) : pool_(capacity) {}
    ~timer_wheel() { for (auto& b : wheel_) b.clear_and_dispose([&](Timer* t) { pool_.destroy(t); }); }
    Timer* schedule(long id, long delay) {
        Timer* t = pool_.create(Timer{id, now_ + delay, {}});
        wheel_[(now_ + delay) & (Slots - 1)].push_back(*t);
        return t;
    }
    void cancel(Timer* t) {                      // O(1): s_iterator_to needs no bucket search
        Bucket::iterator it = Bucket::s_iterator_to(*t);
        wheel_[t->due & (Slots - 1)].erase(it);
        pool_.destroy(t);
    }
    template <class F>
    void tick(F&& fire) {                        // advance one tick, fire what is due
        ++now_;
        auto& b = wheel_[now_ & (Slots - 1)];
        b.remove_and_dispose_if([&](const Timer& t) { return t.due <= now_; },
                                [&](Timer* t) { fire(*t); pool_.destroy(t); });
    }
    std::uint32_t live() const { return pool_.size(); }
private:
    hft::slab<Timer> pool_;
    std::array<Bucket, Slots> wheel_;
    long now_ = 0;
};

// ---- 2. pending-ack queue ------------------------------------------------------------------------
struct Sent {
    long id;
    long sent_at;
    bi::list_member_hook<> ack_hook;
};
using AckQ = bi::list<Sent, bi::member_hook<Sent, bi::list_member_hook<>, &Sent::ack_hook>>;

// ---- 3. LRU (e.g. a symbol cache) ----------------------------------------------------------------
struct Entry {
    long key;
    long value;
    bi::list_member_hook<> lru_hook;
    bi::unordered_set_member_hook<> map_hook;
};
struct key_of { using type = long; const long& operator()(const Entry& e) const { return e.key; } };
using LruList = bi::list<Entry, bi::member_hook<Entry, bi::list_member_hook<>, &Entry::lru_hook>>;
using LruMap = bi::unordered_set<Entry, bi::member_hook<Entry, bi::unordered_set_member_hook<>, &Entry::map_hook>,
                                 bi::key_of_value<key_of>, bi::hash<std::hash<long>>, bi::power_2_buckets<true>>;

int main() {
    hft::thread_arena arena;

    std::puts("== 1. timer wheel (64 slots) ==");
    {
        timer_wheel<64> w(1024);
        Timer* a = w.schedule(1, 3);
        w.schedule(2, 5);
        Timer* c = w.schedule(3, 70);            // wraps: lands in slot 6, fires at tick 70, not 6
        (void)c;
        w.cancel(a);                             // cancelled before it fires
        for (int t = 0; t < 80; ++t) w.tick([&](Timer& tm) { std::printf("  tick fired timer %ld (due %ld)\n", tm.id, tm.due); });
        std::printf("  live timers after 80 ticks: %u\n", w.live());
    }

    std::puts("== 2. pending-ack queue: oldest first, timeout check looks at front() only ==");
    {
        hft::slab<Sent> pool(64);
        AckQ q;
        for (long i = 0; i < 5; ++i) q.push_back(*pool.create(Sent{100 + i, i * 10, {}}));
        Sent& acked = *std::next(q.begin(), 2);   // venue acks order 102 out of order
        q.erase(AckQ::s_iterator_to(acked)); pool.destroy(&acked);
        long now = 35, timeout = 20;
        while (!q.empty() && now - q.front().sent_at > timeout) {
            std::printf("  order %ld: no ack after %ld ticks -> escalate\n", q.front().id, now - q.front().sent_at);
            q.pop_front_and_dispose([&](Sent* s) { pool.destroy(s); });
        }
        std::printf("  still waiting: %zu\n", q.size());
        q.clear_and_dispose([&](Sent* s) { pool.destroy(s); });
    }

    std::puts("== 3. LRU of capacity 3 ==");
    {
        hft::slab<Entry> pool(4);
        std::vector<LruMap::bucket_type> buckets(16);
        LruList lru;
        LruMap map(LruMap::bucket_traits(buckets.data(), buckets.size()));
        auto get = [&](long k) {
            auto it = map.find(k);
            if (it != map.end()) {               // hit: move to the back, O(1)
                lru.erase(LruList::s_iterator_to(*it));
                lru.push_back(*it);
                return;
            }
            if (map.size() == 3) {               // miss and full: evict the front, O(1)
                Entry& victim = lru.front();
                std::printf("  evict %ld\n", victim.key);
                lru.pop_front();
                map.erase(map.iterator_to(victim));
                pool.destroy(&victim);
            }
            Entry* e = pool.create(Entry{k, k * 10, {}, {}});
            lru.push_back(*e);
            map.insert(*e);
        };
        for (long k : {1, 2, 3, 1, 4, 5}) get(k);
        std::printf("  LRU order now:");
        for (auto& e : lru) std::printf(" %ld", e.key);
        std::puts("");
        map.clear();
        lru.clear_and_dispose([&](Entry* e) { pool.destroy(e); });
    }
}
