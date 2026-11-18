// clock_probe — measure the timer we use for every other bench: tick size and now() cost.
// Build: clang++ -std=c++23 -O2 -I/opt/homebrew/include bench/clock_probe.cpp -o build/clock_probe
#include "lat.hpp"
int main() {
    long min_tick = 1L << 40;
    auto t0 = Clock::now();
    for (int i = 0; i < 1000000; ++i) {
        auto a = Clock::now(), b = Clock::now();
        long d = long(std::chrono::duration<double, std::nano>(b - a).count());
        if (d > 0 && d < min_tick) min_tick = d;
    }
    std::printf("steady_clock: smallest nonzero step = %ld ns, cost per now() = %.1f ns\n", min_tick,
                ns_since(t0) / 2e6);
}
