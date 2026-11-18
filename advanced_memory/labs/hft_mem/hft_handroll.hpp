// hft_handroll.hpp — the MINIMUM honest hand-rolled versions, to compare with Boost.Intrusive
// (lesson 07). Each is the smallest thing that does the job correctly; none has debug checks
// beyond what is written here. Read them, then count what you would have to own and test.
//
//   hr::dlink / hr::dlist<T, &T::member>   doubly linked, pointer links (16 B per hook), circular
//                                          with a sentinel: unlink needs ONLY the node.
//   hr::ilink / hr::idx_list<T, &T::member> doubly linked by 32-bit slot indices (8 B per hook);
//                                          relocatable (no absolute addresses inside).
//   hr::hlink / hr::hash_index<...>        fixed power-of-two bucket array, chained through the
//                                          object (next + pprev: O(1) erase given the object).
//   hr::pool<T>                            fixed array + free list threaded through free slots.
//
// Copy rules (as Boost's hooks): copying a dlink/hlink gives an UNLINKED hook, and assigning one
// leaves the target's links alone, so copying a linked object copies the payload, never the
// membership. ilink stays trivially copyable ON PURPOSE: memcpy of the whole slab must carry the
// links (that is what "relocatable" means); never copy ONE linked element by value.
// Containers and the pool are non-copyable (idx_list may be moved: its links are indices).
// Like the lesson says, the rest is unchecked: unlink of an unlinked dlink, double insert,
// erase of an object not in this list are undefined. Boost's safe_link catches those in debug.
#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace hr {

// ---------------------------------------------------------------------------------------------
// Pointer-linked circular list. A hook with next == nullptr is "not linked" (we reset it on
// unlink, the equivalent of Boost's safe_link; drop the two stores to get normal_link).
struct dlink {
    dlink* prev = nullptr;
    dlink* next = nullptr;
    dlink() noexcept = default;
    dlink(const dlink&) noexcept {}                              // a copy is not linked
    dlink& operator=(const dlink&) noexcept { return *this; }    // keeps this hook's links
    bool is_linked() const noexcept { return next != nullptr; }
    void unlink() noexcept {                 // O(1); no list needed (no size counter to keep)
        prev->next = next;
        next->prev = prev;
        prev = next = nullptr;
    }
};

template <class T, dlink T::*M>
class dlist {
public:
    dlist() noexcept { head_.prev = head_.next = &head_; }
    dlist(const dlist&) = delete;
    dlist& operator=(const dlist&) = delete;
    ~dlist() { clear(); }

    void push_back(T& v) noexcept {
        dlink* n = &(v.*M);
        n->prev = head_.prev;
        n->next = &head_;
        head_.prev->next = n;
        head_.prev = n;
    }
    static void erase(T& v) noexcept { (v.*M).unlink(); }
    bool empty() const noexcept { return head_.next == &head_; }
    T& front() noexcept { return *from(head_.next); }
    void clear() noexcept {
        while (!empty()) head_.next->unlink();
    }
    // next element given the element (nullptr at the end): O(1)
    T* next(T& v) noexcept { dlink* n = (v.*M).next; return n == &head_ ? nullptr : from(n); }
    template <class F>
    void for_each(F&& f) {
        for (dlink* n = head_.next; n != &head_;) { dlink* nx = n->next; f(*from(n)); n = nx; }
    }

private:
    // container_of: hook address -> enclosing object. The offset trick below is the classic C
    // idiom and formally undefined (member access through a null pointer: clang's constant
    // evaluator rejects it, "cannot access field of null pointer"; UBSan at -O0/-O1 is silent).
    // Boost uses the same expression for GCC/Clang in boost/intrusive/detail/parent_from_member.hpp
    // (offset_from_pointer_to_member). One more thing YOU own if you hand-roll.
    static T* from(dlink* n) noexcept {
        auto off = reinterpret_cast<std::size_t>(&(static_cast<T*>(nullptr)->*M));
        return reinterpret_cast<T*>(reinterpret_cast<char*>(n) - off);
    }
    dlink head_;
};

// ---------------------------------------------------------------------------------------------
// Index-linked list: links are 32-bit slot numbers into ONE array (the slab). Half the bytes of
// pointer hooks, and the whole array can be memcpy'd, mmap'd or shared: nothing absolute inside.
inline constexpr std::uint32_t nil = 0xFFFFFFFFu;
struct ilink {
    std::uint32_t prev = nil;
    std::uint32_t next = nil;
};
static_assert(std::is_trivially_copyable_v<ilink>, "memcpy of the slab must carry index links");

template <class T, ilink T::*M>
class idx_list {
public:
    explicit idx_list(T* base) noexcept : base_(base) {}
    idx_list(const idx_list&) = delete;              // two headers for one chain = corruption
    idx_list& operator=(const idx_list&) = delete;
    idx_list(idx_list&& o) noexcept : base_(o.base_), head_(o.head_), tail_(o.tail_) {
        o.head_ = o.tail_ = nil;                     // safe: no link points at the header
    }
    idx_list& operator=(idx_list&&) = delete;
    void push_back(std::uint32_t i) noexcept {
        ilink& l = base_[i].*M;
        l.prev = tail_;
        l.next = nil;
        if (tail_ != nil) (base_[tail_].*M).next = i; else head_ = i;
        tail_ = i;
    }
    void erase(std::uint32_t i) noexcept {   // O(1), but needs the list (head_/tail_)
        ilink& l = base_[i].*M;
        if (l.prev != nil) (base_[l.prev].*M).next = l.next; else head_ = l.next;
        if (l.next != nil) (base_[l.next].*M).prev = l.prev; else tail_ = l.prev;
        l.prev = l.next = nil;
    }
    std::uint32_t front() const noexcept { return head_; }
    std::uint32_t next(std::uint32_t i) const noexcept { return (base_[i].*M).next; }
    bool empty() const noexcept { return head_ == nil; }

private:
    T* base_;
    std::uint32_t head_ = nil, tail_ = nil;
};

// ---------------------------------------------------------------------------------------------
// Intrusive hash index (Linux hlist style): bucket = pointer to first object; each object holds
// next + pprev (address of whatever points at it), so erase given the object is O(1).
struct hlink {
    void* next = nullptr;
    void** pprev = nullptr;
    hlink() noexcept = default;
    hlink(const hlink&) noexcept {}                              // a copy is not linked
    hlink& operator=(const hlink&) noexcept { return *this; }
    bool is_linked() const noexcept { return pprev != nullptr; }
};

template <class T, class Key, Key T::*K, hlink T::*H, class Hash>
class hash_index {
public:
    explicit hash_index(std::size_t buckets_pow2) : mask_(buckets_pow2 - 1), b_(check(buckets_pow2), nullptr) {}
    hash_index(const hash_index&) = delete;          // nodes' pprev point into THIS bucket array
    hash_index& operator=(const hash_index&) = delete;
    void insert(T& v) noexcept {
        void** head = &b_[Hash{}(v.*K) & mask_];
        hlink& h = v.*H;
        h.next = *head;
        if (*head) (static_cast<T*>(*head)->*H).pprev = &h.next;
        *head = &v;
        h.pprev = head;
    }
    static void erase(T& v) noexcept {
        hlink& h = v.*H;
        *h.pprev = h.next;
        if (h.next) (static_cast<T*>(h.next)->*H).pprev = h.pprev;
        h.next = nullptr;
        h.pprev = nullptr;
    }
    T* find(const Key& k) const noexcept {
        for (void* p = b_[Hash{}(k) & mask_]; p; p = (static_cast<T*>(p)->*H).next)
            if (static_cast<T*>(p)->*K == k) return static_cast<T*>(p);
        return nullptr;
    }

private:
    static std::size_t check(std::size_t n) {
        if (n == 0 || (n & (n - 1)) != 0) throw std::invalid_argument("hr::hash_index: bucket count must be a power of two");
        return n;
    }
    std::size_t mask_;
    std::vector<void*> b_;
};

// ---------------------------------------------------------------------------------------------
// The minimum pool: fixed array of slots, free list threaded through the free slots, LIFO.
template <class T>
class pool {
    union slot { slot* next; alignas(T) unsigned char bytes[sizeof(T)]; };
public:
    explicit pool(std::size_t n) : s_(n) {
        for (std::size_t i = 0; i < n; ++i) s_[i].next = i + 1 < n ? &s_[i + 1] : nullptr;
        head_ = n ? &s_[0] : nullptr;
    }
    pool(const pool&) = delete;                      // free-list links point into THIS array
    pool& operator=(const pool&) = delete;
    // The caller destroys every live object before the pool dies (nothing is tracked here).
    template <class... A>
    T* create(A&&... a) {
        slot* s = head_;
        if (!s) return nullptr;
        slot* next = s->next;
        T* p;
        if constexpr (std::is_nothrow_constructible_v<T, A...>) {
            p = ::new (static_cast<void*>(s->bytes)) T(std::forward<A>(a)...);
        } else {
            try {
                p = ::new (static_cast<void*>(s->bytes)) T(std::forward<A>(a)...);
            } catch (...) {
                s->next = next;                      // slot stays free
                throw;
            }
        }
        head_ = next;
        return p;
    }
    void destroy(T* p) noexcept {
        p->~T();
        slot* s = std::launder(reinterpret_cast<slot*>(p));
        s->next = head_;
        head_ = s;
    }

private:
    std::vector<slot> s_;
    slot* head_ = nullptr;
};

}  // namespace hr
