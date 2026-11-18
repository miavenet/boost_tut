// hft_slab.hpp — a fixed-capacity object pool for intrusive containers (lessons 05–07).
//
//   USE IT (three lines):
//     hft::thread_arena arena;               // once per thread (hft_mem.hpp), declared FIRST
//     hft::slab<Order> orders(100'000);      // carved ONCE from this thread's arena
//     Order* o = orders.create(id, px, qty); ... orders.destroy(o);   // never calls malloc
//
//   WHAT IT IS
//     * one contiguous array of `capacity` slots of sizeof(T) bytes, taken from the arena's bulk
//       layer (gate -> monotonic -> pre-faulted region) at construction, never again;
//     * a free list threaded THROUGH the free slots (the first bytes of a free slot hold the
//       next pointer), so create/destroy are a pop/push: O(1), no search, no lock;
//     * LIFO reuse: destroy() then create() hands back the slot just freed (probably in cache);
//     * a 32-bit generation per slot, bumped on every create() AND destroy() (odd = live,
//       even = free), so a `handle` {index, gen} detects "that order is gone and its slot now
//       holds another one" (ABA), and checked builds catch a double destroy().
//       Wraparound: one slot reused 2^31 times brings a generation back to an old value (about
//       43 s of create+destroy on ONE slot at 50 M pairs/s); a handle older than that can lie.
//
//   EXHAUSTION POLICY (never falls back to malloc)
//     create()      throws std::bad_alloc when all slots are live
//     try_create()  returns nullptr instead (the caller rejects the order)
//     If T's constructor throws, the slot stays free and size() is unchanged (strong guarantee).
//
//   OWNERSHIP RULES
//     * the slab owns the STORAGE; intrusive containers only link objects that live in it.
//     * destroy() runs ~T(). With Boost.Intrusive safe_link hooks (the default) and asserts on
//       (no -DNDEBUG), ~T() asserts if the object is still linked in a container.
//     * destroying the slab while objects are live: aborts in checked builds (HFT_MEM_CHECKED=1,
//       the default) with a message; unlink and destroy() them first (clear_and_dispose).
//     * checked builds also abort on destroy() of a pointer that is not a LIVE object of this
//       slab (foreign, misaligned, or already destroyed) and on handle_of()/index_of()/at()
//       outside the slab. With HFT_MEM_CHECKED=0 those are undefined behaviour.
//     * the slab must die before its thread_arena, like every hft container.
#pragma once

#include "hft_mem.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory_resource>
#include <new>
#include <type_traits>
#include <utility>

namespace hft {

namespace detail {
// Out of line and cold, so the checks do not stop destroy() from inlining (they did: with the
// message code inline, clang called destroy() and kept the slab in memory: l6_slab LIFO +8 ns).
[[noreturn, gnu::cold, gnu::noinline]] inline void slab_die(const char* why) noexcept {
    std::fprintf(stderr, "hft::slab: %s\n", why);
    std::abort();
}
}  // namespace detail

template <class T>
class slab {
    static_assert(sizeof(T) >= sizeof(void*), "a free slot stores the next pointer in place");

public:
    struct handle {  // 8 bytes: survives the object, detects slot reuse
        std::uint32_t index = std::numeric_limits<std::uint32_t>::max();
        std::uint32_t gen = 0;
        friend bool operator==(handle, handle) = default;
    };

    // Carves the slab from the CURRENT thread's arena (bulk layer). Throws std::logic_error if
    // this thread has no hft::thread_arena, std::bad_alloc if the arena cannot supply it.
    explicit slab(std::size_t capacity) : slab(capacity, detail::arena_or_throw().bulk()) {}

    // Or from any memory resource you choose (it must outlive the slab).
    slab(std::size_t capacity, std::pmr::memory_resource* upstream)
        : up_(upstream), cap_(static_cast<std::uint32_t>(capacity)) {
        if (capacity == 0 || capacity > std::numeric_limits<std::uint32_t>::max() - 1)
            throw std::length_error("hft::slab: capacity must be in [1, 2^32-2]");
        // ONE block: the slots, then the generations. A single allocation cannot half-fail (a
        // second one that threw used to leak the first), and needs no try/catch here: a handler
        // in the constructor made the slab's address escape, and clang then kept head_ in
        // memory instead of a register in callers' loops (l6_slab LIFO 10.4 -> 15.4 ns).
        slots_ = static_cast<slot*>(up_->allocate(bytes(), alignof(slot)));
        gens_ = reinterpret_cast<std::uint32_t*>(reinterpret_cast<unsigned char*>(slots_) + sizeof(slot) * cap_);
        // Thread the free list front to back, so the first creates return slots 0, 1, 2 ...
        // (adjacent objects). This also touches every slot once, here, not on the hot path.
        for (std::uint32_t i = 0; i < cap_; ++i) {
            slots_[i].next = (i + 1 < cap_) ? &slots_[i + 1] : nullptr;
            gens_[i] = 0;                    // even = free
        }
        head_ = &slots_[0];
    }

    ~slab() {
#if HFT_MEM_CHECKED
        if (live_ != 0) {
            std::fprintf(stderr, "hft::slab: destroyed with %u live object(s); unlink and destroy() them first\n", live_);
            std::abort();
        }
#endif
        up_->deallocate(slots_, bytes(), alignof(slot));
    }
    slab(const slab&) = delete;
    slab& operator=(const slab&) = delete;

    template <class... A>
    T* try_create(A&&... a) noexcept(std::is_nothrow_constructible_v<T, A...>) {
        slot* s = head_;
        if (!s) return nullptr;              // exhausted: the caller decides (reject the order)
        slot* next = s->next;                // read it before T's bytes overwrite it
        T* p;
        if constexpr (std::is_nothrow_constructible_v<T, A...>) {
            p = ::new (static_cast<void*>(s->bytes)) T(std::forward<A>(a)...);
        } else {
            try {
                p = ::new (static_cast<void*>(s->bytes)) T(std::forward<A>(a)...);
            } catch (...) {
                s->next = next;              // the constructor may have scribbled on it
                throw;                       // slot still free, live_ unchanged
            }
        }
        head_ = next;                        // pop only once the object exists
        ++gens_[s - slots_];                 // odd: live
        ++live_;
        return p;
    }
    template <class... A>
    T* create(A&&... a) {
        T* p = try_create(std::forward<A>(a)...);
        if (!p) throw std::bad_alloc();
        return p;
    }

    void destroy(T* p) noexcept {
#if HFT_MEM_CHECKED
        if (!owns(p)) detail::slab_die("destroy() of a pointer this slab does not own");
        if (!(gens_[index_of(p)] & 1u)) detail::slab_die("destroy() of a slot that is not live (double destroy?)");
#endif
        p->~T();                             // safe_link hooks assert here if still linked
        slot* s = slot_of(p);
        ++gens_[s - slots_];                 // even: free; every handle to the old object is now stale
        s->next = head_;                     // push: LIFO
        head_ = s;
        --live_;
    }

    // O(1) handle <-> pointer. get() returns nullptr if the object was destroyed (even if the
    // slot has since been reused for another object).
    handle handle_of(const T* p) const noexcept {
        auto i = index_of(p);
        return {i, gens_[i]};
    }
    T* get(handle h) const noexcept {
        // Generations are odd only while a slot is live, and every create()/destroy() bumps
        // them, so "same, odd generation" means "the same object is still there". A default or
        // forged handle {i, even} never matches.
        if (h.index >= cap_ || gens_[h.index] != h.gen || !(h.gen & 1u)) return nullptr;
        return std::launder(reinterpret_cast<T*>(slots_[h.index].bytes));
    }
    std::uint32_t index_of(const T* p) const noexcept {
#if HFT_MEM_CHECKED
        if (!owns(p)) detail::slab_die("handle_of()/index_of() of a pointer this slab does not own");
#endif
        return static_cast<std::uint32_t>(slot_of(p) - slots_);
    }
    // The object in slot i, live or not (use get(handle) to know it is still the same object).
    T* at(std::uint32_t i) const noexcept {
#if HFT_MEM_CHECKED
        if (i >= cap_) detail::slab_die("at() index out of range");
#endif
        return std::launder(reinterpret_cast<T*>(slots_[i].bytes));
    }

    bool owns(const T* p) const noexcept {
        auto a = reinterpret_cast<std::uintptr_t>(p), b = reinterpret_cast<std::uintptr_t>(slots_);
        return a >= b && a < b + sizeof(slot) * cap_ && (a - b) % sizeof(slot) == 0;
    }
    std::uint32_t size() const noexcept { return live_; }
    std::uint32_t capacity() const noexcept { return cap_; }
    bool full() const noexcept { return head_ == nullptr; }

private:
    union slot {
        slot* next;                          // while free
        alignas(T) unsigned char bytes[sizeof(T)];  // while live
    };
    std::size_t bytes() const noexcept { return (sizeof(slot) + sizeof(std::uint32_t)) * std::size_t{cap_}; }
    static_assert(sizeof(slot) % alignof(std::uint32_t) == 0, "generations follow the slots");
    // T* -> its slot. The slot union sits at the same address as the T built in its bytes;
    // launder tells the compiler we mean the union object that lives there.
    static slot* slot_of(const T* p) noexcept {
        return std::launder(reinterpret_cast<slot*>(const_cast<T*>(p)));
    }
    // Slot stride = sizeof(slot) = sizeof(T) for any T whose size is a multiple of 8 bytes:
    // the objects sit densely packed, one after another.

    std::pmr::memory_resource* up_;
    std::uint32_t cap_;
    std::uint32_t live_ = 0;
    slot* slots_ = nullptr;
    std::uint32_t* gens_ = nullptr;
    slot* head_ = nullptr;
};

}  // namespace hft
