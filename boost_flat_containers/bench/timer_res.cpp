// timer_res.cpp - how fine is steady_clock on this box? (smallest nonzero step)
#include "lat.hpp"
#include <cstdio>
int main() {
    uint64_t best = ~0ULL;
    for (int i = 0; i < 1000000; ++i) {
        uint64_t a = lat::now_ns(), b;
        do { b = lat::now_ns(); } while (b == a);
        if (b - a < best) best = b - a;
    }
    std::printf("smallest steady_clock step: %llu ns\n", (unsigned long long)best);
}
