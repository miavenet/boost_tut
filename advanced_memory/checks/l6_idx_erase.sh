#!/bin/bash
# Card 06-Q6: body of the index-linked list's `void erase(std::uint32_t i)`. Members:
#   T* base_; std::uint32_t head_, tail_;   link of slot i: (base_[i].*M) with .prev/.next;
#   nil = 0xFFFFFFFF marks "no neighbour". After erase, slot i's links must be nil.
. "$(dirname "$0")/lib.inc"
read_answer
{
cat <<'CPP'
#include <cstdint>
#include <vector>
inline constexpr std::uint32_t nil = 0xFFFFFFFFu;
struct ilink { std::uint32_t prev = nil, next = nil; };
template <class T, ilink T::*M>
struct idx_list {
    T* base_; std::uint32_t head_ = nil, tail_ = nil;
    void push_back(std::uint32_t i) {
        ilink& l = base_[i].*M; l.prev = tail_; l.next = nil;
        if (tail_ != nil) (base_[tail_].*M).next = i; else head_ = i;
        tail_ = i;
    }
    void erase(std::uint32_t i) {
CPP
cat "$WORK/answer.txt"
cat <<'CPP'

    }
};
struct O { long id; ilink h; };
int main() {
    std::vector<O> v(6);
    idx_list<O, &O::h> l{v.data()};
    for (std::uint32_t i = 0; i < 5; ++i) l.push_back(i);       // 0 1 2 3 4
    l.erase(2); l.erase(0); l.erase(4);                          // middle, head, tail -> 1 3
    if (l.head_ != 1 || l.tail_ != 3) return 1;
    if (v[1].h.prev != nil || v[1].h.next != 3 || v[3].h.prev != 1 || v[3].h.next != nil) return 1;
    for (std::uint32_t i : {0u, 2u, 4u}) if (v[i].h.prev != nil || v[i].h.next != nil) return 1;
    l.erase(1); l.erase(3);
    if (l.head_ != nil || l.tail_ != nil) return 1;
    for (std::uint32_t i : {1u, 3u}) if (v[i].h.prev != nil || v[i].h.next != nil) return 1;
    l.push_back(4); l.push_back(1);                              // reusable after emptying
    if (l.head_ != 4 || l.tail_ != 1 || v[4].h.prev != nil || v[4].h.next != 1 || v[1].h.prev != 4 || v[1].h.next != nil) return 1;
    l.erase(4);                                                  // head of two
    return (l.head_ == 1 && l.tail_ == 1 && v[1].h.prev == nil && v[4].h.prev == nil && v[4].h.next == nil) ? 0 : 1;
}
CPP
} > "$WORK/a.cpp"
compile_run "$WORK/a.cpp"
