// asm_alloc_paths.cpp — the two allocate paths of lesson 03 C5, each in its own extern "C"
// function, so their machine code can be read with objdump. Run: bash bench/asm_alloc.sh
#include "../labs/hft_mem/hft_mem.hpp"

extern "C" __attribute__((noinline)) void* stateful_allocate(std::pmr::memory_resource* r) {
    return r->allocate(64, 8);                        // virtual call through the pointer
}
extern "C" __attribute__((noinline)) void* stateless_allocate() {
    return hft::allocator<std::max_align_t>{}.allocate(4);  // TLS read + qualified (direct) call
}
