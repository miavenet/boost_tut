// l3_region.cpp — the ~40-line custom memory_resource that owns a region. This is the teaching
// copy of hft::region_resource (portable part only: mmap + pre-fault; no huge pages / NUMA).
// Run: ./build/l3_region      Break it: delete the alignment line in do_allocate, rebuild, rerun.
#include <sys/mman.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory_resource>
#include <new>

class region final : public std::pmr::memory_resource {
public:
    explicit region(std::size_t bytes) : cap_(bytes) {
        void* p = ::mmap(nullptr, cap_, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (p == MAP_FAILED) throw std::bad_alloc();
        base_ = static_cast<std::byte*>(p);
        long pg = ::sysconf(_SC_PAGESIZE);
        for (std::size_t i = 0; i < cap_; i += pg) base_[i] = std::byte{0};  // pre-fault: touch now
    }
    ~region() override { ::munmap(base_, cap_); }
    region(const region&) = delete;
    region& operator=(const region&) = delete;
    std::size_t used() const { return off_; }

private:
    void* do_allocate(std::size_t bytes, std::size_t align) override {
        std::size_t start = (off_ + align - 1) & ~(align - 1);   // round up: align is a power of 2
        if (start + bytes > cap_) throw std::bad_alloc();       // full: must throw, never return null
        off_ = start + bytes;
        return base_ + start;
    }
    void do_deallocate(void*, std::size_t, std::size_t) override {}  // freed when the region dies
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;  // memory from one region can only be freed by that same region
    }

    std::byte* base_ = nullptr;
    std::size_t cap_;
    std::size_t off_ = 0;
};

int main() {
    region r(1 << 20);
    void* a = r.allocate(1, 1);
    void* b = r.allocate(64, 64);   // must come back 64-byte aligned even after a 1-byte request
    // Observed 2026-11-13: at -O2, a plain `b % 64` printed 0 even with the alignment line deleted
    // (the optimiser folded it; cause not investigated). Reading the pointer back through a
    // volatile keeps the test honest.
    void* volatile bv = b;
    std::uintptr_t bits = reinterpret_cast<std::uintptr_t>(static_cast<void*>(bv));
    std::printf("b %% 64 = %zu  (must be 0)\n", static_cast<std::size_t>(bits % 64));

    region other(1 << 16);
    std::printf("r == r? %d   r == other? %d   (must be 1 then 0)\n", r.is_equal(r), r.is_equal(other));

    std::pmr::map<int, int> m(&r);      // any pmr container can sit on it
    for (int i = 0; i < 1000; ++i) m[i] = i;
    std::printf("map of %zu entries used %zu bytes of the region\n", m.size(), r.used());

    try {
        (void)other.allocate(1 << 20, 8);
        std::puts("no throw?!");
    } catch (const std::bad_alloc&) {
        std::puts("exhausted region threw std::bad_alloc");
    }
    (void)a;
}
