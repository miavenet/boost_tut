#!/bin/bash
# Card 03-Q1: body of do_allocate for a bump region (members: std::byte* base_; size_t cap_, off_).
# Must honour `align` (a power of two) and throw std::bad_alloc when full.
. "$(dirname "$0")/lib.inc"
read_answer
{
cat <<'CPP'
#include <cstddef>
#include <cstdint>
#include <memory>
#include <memory_resource>
#include <new>
class region final : public std::pmr::memory_resource {
public:
    region(std::byte* b, std::size_t n) : base_(b), cap_(n) {}
private:
    void* do_allocate(std::size_t bytes, std::size_t align) override {
CPP
cat "$WORK/answer.txt"
cat <<'CPP'
    }
    void do_deallocate(void*, std::size_t, std::size_t) override {}
    bool do_is_equal(const std::pmr::memory_resource& o) const noexcept override { return this == &o; }
    std::byte* base_; std::size_t cap_; std::size_t off_ = 0;
};
alignas(256) static std::byte buf[4096];
alignas(256) static std::byte buf2[4096];
int main() {
    region r(buf, sizeof buf);
    auto in = [](void* p) { return p >= static_cast<void*>(buf) && p < static_cast<void*>(buf + sizeof buf); };
    // Read results back through a volatile: without it, clang -O1 folded the alignment test
    // away and passed an answer that ignored `align` (observed 2026-11-13).
    auto opaque = [](void* p) { void* volatile v = p; return static_cast<void*>(v); };
    void* a = opaque(r.allocate(1, 1));
    void* b = opaque(r.allocate(64, 64));
    void* c = opaque(r.allocate(3, 1));
    void* d = opaque(r.allocate(8, 8));
    if (!in(a) || !in(b) || !in(c) || !in(d)) return 1;
    if (reinterpret_cast<std::uintptr_t>(b) % 64 || reinterpret_cast<std::uintptr_t>(d) % 8) return 1;
    if (static_cast<std::byte*>(b) < static_cast<std::byte*>(a) + 1) return 1;          // no overlap
    if (static_cast<std::byte*>(d) < static_cast<std::byte*>(c) + 3) return 1;
    try { (void)r.allocate(8192, 8); return 1; } catch (const std::bad_alloc&) {}
    void* e = opaque(r.allocate(16, 16));                                                     // still usable
    if (!in(e) || reinterpret_cast<std::uintptr_t>(e) % 16 != 0) return 1;
    // Padding must count: off_ = 4033, then 40 bytes at align 64 starts at 4096 = past the end.
    // A body that tests `off_ + bytes > cap_` BEFORE rounding up (4073 <= 4096) wrongly succeeds.
    region r2(buf2, sizeof buf2);
    (void)r2.allocate(1, 1); (void)r2.allocate(4096 - 64, 1);
    try { (void)opaque(r2.allocate(40, 64)); return 1; } catch (const std::bad_alloc&) {}
    return 0;
}
CPP
} > "$WORK/a.cpp"
compile_run "$WORK/a.cpp"
