#!/bin/bash
# Card 04-Q6: body of the free list's do_deallocate fast path (members: node* head_; with
# struct node { node* next; };). The freed block must become the new head (LIFO reuse).
. "$(dirname "$0")/lib.inc"
read_answer
{
cat <<'CPP'
#include <cstddef>
#include <memory_resource>
#include <new>
class freelist final : public std::pmr::memory_resource {
public:
    struct node { node* next; };
    node* head_ = nullptr;
    alignas(16) std::byte chunk_[64 * 32];
    freelist() { for (int i = 31; i >= 0; --i) { auto* n = reinterpret_cast<node*>(chunk_ + 64 * i); n->next = head_; head_ = n; } }
private:
    void* do_allocate(std::size_t, std::size_t) override {
        if (!head_) throw std::bad_alloc();
        node* n = head_; head_ = n->next; return n;
    }
    void do_deallocate(void* p, std::size_t bytes, std::size_t align) override {
        (void)bytes; (void)align;
CPP
cat "$WORK/answer.txt"
cat <<'CPP'
    }
    bool do_is_equal(const std::pmr::memory_resource& o) const noexcept override { return this == &o; }
};
int main() {
    freelist f;
    void* a = f.allocate(64); void* b = f.allocate(64); void* c = f.allocate(64);
    f.deallocate(b, 64);
    if (f.allocate(64) != b) return 1;                  // LIFO: last freed comes back first
    f.deallocate(a, 64); f.deallocate(c, 64);
    if (f.allocate(64) != c || f.allocate(64) != a) return 1;
    void* all[32]; int n = 0;                            // nothing lost, nothing duplicated
    f.deallocate(b, 64);
    try { for (;;) { void* p = f.allocate(64); all[n++] = p; } } catch (const std::bad_alloc&) {}
    return n == 30 ? 0 : 1;                              // 32 blocks, 2 (a, c) still held
}
CPP
} > "$WORK/a.cpp"
compile_run "$WORK/a.cpp"
