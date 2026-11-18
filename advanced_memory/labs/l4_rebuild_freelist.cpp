// l4_rebuild_freelist.cpp — rebuild ONE layer yourself: replace the general pool with a
// fixed-size free list for a node container whose nodes are all one size.
// Freed blocks are pushed on a singly linked list threaded THROUGH the free blocks themselves;
// allocate pops the most recently freed block (still warm in cache). Refill carves a chunk.
// Run: ./build/l4_rebuild_freelist
#include <cstddef>
#include <cstdio>
#include <map>
#include <memory_resource>
#include <new>

class freelist_resource final : public std::pmr::memory_resource {
public:
    freelist_resource(std::size_t block, std::size_t per_chunk, std::pmr::memory_resource* up)
        : block_(block < sizeof(node) ? sizeof(node) : block), per_chunk_(per_chunk), up_(up) {}

    std::size_t refills = 0;

private:
    struct node { node* next; };

    void* do_allocate(std::size_t bytes, std::size_t align) override {
        if (bytes > block_ || align > alignof(std::max_align_t)) return up_->allocate(bytes, align);
        if (!head_) refill();
        node* n = head_;          // pop
        head_ = n->next;
        return n;
    }
    void do_deallocate(void* p, std::size_t bytes, std::size_t align) override {
        if (bytes > block_ || align > alignof(std::max_align_t)) return up_->deallocate(p, bytes, align);
        auto* n = static_cast<node*>(p);   // push: the freed block becomes the new head
        n->next = head_;
        head_ = n;
    }
    bool do_is_equal(const std::pmr::memory_resource& o) const noexcept override { return this == &o; }

    void refill() {
        ++refills;
        auto* chunk = static_cast<std::byte*>(up_->allocate(block_ * per_chunk_, alignof(std::max_align_t)));
        for (std::size_t i = 0; i < per_chunk_; ++i) {     // thread the list through the chunk
            auto* n = reinterpret_cast<node*>(chunk + i * block_);
            n->next = head_;
            head_ = n;
        }
    }

    std::size_t block_, per_chunk_;
    std::pmr::memory_resource* up_;
    node* head_ = nullptr;
};

int main() {
    std::pmr::monotonic_buffer_resource mono;              // upstream layer stays the same
    freelist_resource fl(64, 256, &mono);                  // any request up to 64 B (a map<long,long> node is 48 B)
    std::pmr::map<long, long> m(&fl);
    for (long i = 0; i < 10'000; ++i) m[i] = i;
    for (long i = 0; i < 100'000; ++i) {                   // churn: erase oldest, insert newest
        m.erase(i);
        m[10'000 + i] = i;
    }
    std::printf("map size %zu, free-list refills: %zu (all during the first fill)\n", m.size(), fl.refills);

    void* a = fl.allocate(40);
    fl.deallocate(a, 40);
    void* b = fl.allocate(40);
    std::printf("LIFO reuse: freed block handed straight back? %s\n", a == b ? "yes" : "no");
}
