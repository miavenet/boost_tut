// l3_first_touch.cpp — mmap gives you address space, not memory. The first write to each page
// takes a page fault; that is the cost hft::region_resource pays at start-up instead of in the
// hot path. Also tries mlock(2) on the region. Run: ./build/l3_first_touch
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>

#include <chrono>
#include <cstddef>
#include <cstdio>

long minor_faults() {
    rusage u{};
    getrusage(RUSAGE_SELF, &u);
    return u.ru_minflt;
}

int main() {
    const std::size_t bytes = std::size_t{64} << 20;
    const long pg = ::sysconf(_SC_PAGESIZE);
    std::printf("page size here: %ld bytes, region: %zu MiB = %zu pages\n", pg, bytes >> 20, bytes / pg);

    auto* p = static_cast<volatile char*>(
        ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (p == MAP_FAILED) return 1;

    for (int pass = 1; pass <= 2; ++pass) {
        long f0 = minor_faults();
        auto t0 = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < bytes; i += pg) p[i] = 1;   // one write per page
        auto t1 = std::chrono::steady_clock::now();
        long faults = minor_faults() - f0;
        double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        std::printf("pass %d: %6ld page faults, %8.0f ns per page touched\n", pass, faults, ns / (bytes / pg));
    }
    int rc = ::mlock(const_cast<char*>(p), bytes);
    std::printf("mlock(64 MiB) returned %d%s\n", rc, rc == 0 ? " (pages now locked in RAM)" : " (see errno / RLIMIT_MEMLOCK)");
    ::munmap(const_cast<char*>(p), bytes);
}
