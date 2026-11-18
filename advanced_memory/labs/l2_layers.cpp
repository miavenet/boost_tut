// l2_layers.cpp — open the box: each std::pmr layer on its own, with a tracer that prints every
// request a layer passes to its upstream. Run: ./build/l2_layers   (predict each line first!)
#include <cstddef>
#include <cstdio>
#include <memory_resource>
#include <thread>
#include <vector>

// A pass-through resource that logs what reaches it. ~15 lines: the shape of every custom resource.
struct tracer final : std::pmr::memory_resource {
    const char* name;
    std::pmr::memory_resource* up;
    tracer(const char* n, std::pmr::memory_resource* u) : name(n), up(u) {}
    void* do_allocate(std::size_t b, std::size_t a) override {
        void* p = up->allocate(b, a);
        std::printf("    [%s] upstream allocate %zu B\n", name, b);
        return p;
    }
    void do_deallocate(void* p, std::size_t b, std::size_t a) override {
        std::printf("    [%s] upstream deallocate %zu B\n", name, b);
        up->deallocate(p, b, a);
    }
    bool do_is_equal(const std::pmr::memory_resource& o) const noexcept override { return this == &o; }
};

int main() {
    std::puts("1) monotonic_buffer_resource: deallocate is a no-op; release() frees everything");
    {
        tracer t("malloc", std::pmr::new_delete_resource());
        std::pmr::monotonic_buffer_resource mono(256, &t);
        void* a = mono.allocate(64);
        mono.deallocate(a, 64);
        void* b = mono.allocate(64);
        std::printf("  a=%p b=%p  same address? %s\n", a, b, a == b ? "yes" : "no");
        for (int i = 0; i < 8; ++i) (void)mono.allocate(64);   // outgrow the first buffer
        std::puts("  calling release():");
        mono.release();
    }

    std::puts("2) unsynchronized_pool_resource: a freed block is reused for the next same-size request");
    {
        std::pmr::unsynchronized_pool_resource pool;
        void* a = pool.allocate(48);
        pool.deallocate(a, 48);
        void* b = pool.allocate(48);
        std::printf("  a=%p b=%p  same address? %s\n", a, b, a == b ? "yes" : "no");
    }

    std::puts("3) null_memory_resource: every allocate throws");
    try {
        (void)std::pmr::null_memory_resource()->allocate(8);
        std::puts("  no throw?!");
    } catch (const std::bad_alloc&) {
        std::puts("  caught std::bad_alloc");
    }

    std::puts("4) set_default_resource is PROCESS-WIDE: another thread sees it too");
    {
        tracer t("global-default", std::pmr::new_delete_resource());
        std::pmr::memory_resource* old = std::pmr::set_default_resource(&t);
        std::thread([] {
            std::pmr::vector<int> v;              // no resource given -> get_default_resource()
            v.push_back(1);
        }).join();
        std::pmr::set_default_resource(old);
    }

    std::puts("5) the chain: stack buffer -> monotonic (null upstream) -> pool -> pmr::vector");
    {
        alignas(64) std::byte buf[8192];
        std::pmr::monotonic_buffer_resource mono(buf, sizeof buf, std::pmr::null_memory_resource());
        tracer t("pool->mono", &mono);
        std::pmr::unsynchronized_pool_resource pool(&t);
        std::pmr::vector<long> v(&pool);
        try {
            for (long i = 0; i < 4000; ++i) v.push_back(i);   // 32 KB > 8 KB buffer
            std::puts("  no throw?!");
        } catch (const std::bad_alloc&) {
            std::printf("  caught std::bad_alloc at v.size()=%zu: the buffer ran out, null upstream said no\n", v.size());
        }
    }
}
