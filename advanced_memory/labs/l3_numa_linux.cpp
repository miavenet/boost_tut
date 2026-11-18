// l3_numa_linux.cpp — NUMA-local, huge-page, pre-faulted region, and how to VERIFY placement.
// LINUX ONLY. On the authoring machine (macOS arm64, no NUMA) this file compiles, prints a notice
// and exits 0: the Linux branch was NOT executed there. Every call is grounded at man7.org:
//   mmap(2)   https://man7.org/linux/man-pages/man2/mmap.2.html        MAP_HUGETLB, MAP_POPULATE
//   madvise(2) https://man7.org/linux/man-pages/man2/madvise.2.html    MADV_HUGEPAGE
//   mbind(2)  https://man7.org/linux/man-pages/man2/mbind.2.html       MPOL_BIND, -lnuma, <numaif.h>
//   numa(3)   https://man7.org/linux/man-pages/man3/numa.3.html        numa_available, numa_node_of_cpu
//   move_pages(2) https://man7.org/linux/man-pages/man2/move_pages.2.html  nodes=NULL -> query only
//   pthread_setaffinity_np(3) https://man7.org/linux/man-pages/man3/pthread_setaffinity_np.3.html
//   numa(7)   https://man7.org/linux/man-pages/man7/numa.7.html        /proc/<pid>/numa_maps N<node>=
// Build on Linux:  clang++ -std=c++23 -O2 l3_numa_linux.cpp -lnuma   (needs libnuma-dev)
#include <cstddef>
#include <cstdio>
#include <cstdlib>

#if defined(__linux__) && __has_include(<numa.h>) && __has_include(<numaif.h>)
#include <numa.h>
#include <numaif.h>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <unistd.h>

#include <fstream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (numa_available() < 0) { std::puts("numa_available() < 0: no NUMA support"); return 0; }
    const int cpu = argc > 1 ? std::atoi(argv[1]) : 0;

    // 1. PIN FIRST. The default policy is "local allocation": the node of the CPU that touches.
    // CPU_SET(3): CPU_SETSIZE is "one greater than the maximum CPU number".
    if (cpu < 0 || cpu >= CPU_SETSIZE) { std::printf("cpu %d out of range\n", cpu); return 1; }
    cpu_set_t set; CPU_ZERO(&set); CPU_SET(cpu, &set);
    if (pthread_setaffinity_np(pthread_self(), sizeof set, &set) != 0) { std::perror("affinity"); return 1; }
    const int node = numa_node_of_cpu(cpu);
    std::printf("pinned to cpu %d (node %d)\n", cpu, node);

    // 2. MAP. Try explicit huge pages (needs a reserved pool: /proc/sys/vm/nr_hugepages),
    //    else fall back to normal pages + transparent huge pages via madvise.
    const std::size_t bytes = std::size_t{64} << 20;
    void* p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    bool hugetlb = p != MAP_FAILED;
    if (!hugetlb) {
        p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (p == MAP_FAILED) { std::perror("mmap"); return 1; }
        madvise(p, bytes, MADV_HUGEPAGE);
    }
    std::printf("mapping: %s\n", hugetlb ? "MAP_HUGETLB" : "4K pages + MADV_HUGEPAGE");

    // 3. BIND before any touch: the policy "has an effect only for new allocations".
    //    mbind(2): nodemask holds "up to maxnode bits". A 128-bit mask with maxnode = 128 and
    //    node <= 126 keeps the bit inside the mask under either reading of "up to", and never
    //    shifts 1UL by >= its width (undefined behaviour). Same code as hft_mem.hpp.
    constexpr int kBits = static_cast<int>(sizeof(unsigned long) * 8);
    if (node > 2 * kBits - 2) {
        std::printf("node %d out of range for this lab's 128-bit mask; skipping mbind\n", node);
    } else if (node >= 0) {
        unsigned long mask[2] = {0, 0};
        mask[node / kBits] = 1UL << (node % kBits);
        if (mbind(p, bytes, MPOL_BIND, mask, sizeof mask * 8, 0) != 0) std::perror("mbind");
    } else {
        std::puts("numa_node_of_cpu() < 0: kernel exposes no node for this CPU; skipping mbind");
    }

    // 4. TOUCH (pre-fault) from the pinned thread, then LOCK.
    const long pg = sysconf(_SC_PAGESIZE);
    for (std::size_t i = 0; i < bytes; i += pg) static_cast<volatile char*>(p)[i] = 0;
    if (mlock(p, bytes) != 0) std::perror("mlock (check RLIMIT_MEMLOCK)");

    // 5. VERIFY: ask the kernel which node each page is on (nodes == NULL means query, not move).
    std::vector<void*> pages;
    for (std::size_t i = 0; i < bytes; i += std::size_t{2} << 20) pages.push_back(static_cast<char*>(p) + i);
    std::vector<int> status(pages.size(), -1);
    move_pages(0, pages.size(), pages.data(), nullptr, status.data(), 0);
    if (node >= 0) {
        int on_node = 0;
        for (int s : status) on_node += (s == node);
        std::printf("move_pages: %d of %zu sampled pages on node %d\n", on_node, pages.size(), node);
    } else {
        std::printf("move_pages: status[0]=%d (negative = error code: no node information)\n", status[0]);
    }

    // ... and the same answer from /proc/self/numa_maps (look for N<node>=<pages>).
    char addr[32]; std::snprintf(addr, sizeof addr, "%lx", reinterpret_cast<unsigned long>(p));
    std::ifstream maps("/proc/self/numa_maps");
    if (!maps) std::puts("no /proc/self/numa_maps on this kernel");
    for (std::string line; std::getline(maps, line);)
        if (line.rfind(addr, 0) == 0) std::printf("numa_maps: %s\n", line.c_str());
    std::puts("also try, from another shell:  numastat -p <pid>");
    munlock(p, bytes);
    munmap(p, bytes);
}
#else
int main() {
    std::puts("l3_numa_linux: Linux + libnuma only. NOT executed on this machine (no NUMA here).");
    std::puts("Read the code and the man7.org pages listed at the top; run it on your Linux box.");
    return 0;
}
#endif
