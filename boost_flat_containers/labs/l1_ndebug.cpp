// l1_ndebug.cpp - same source, same -O2, one macro. Predict the ratio first.
// Build A: clang++ -std=c++23 -O2          -I/opt/homebrew/include l1_ndebug.cpp -o l1_assert_on
// Build B: clang++ -std=c++23 -O2 -DNDEBUG -I/opt/homebrew/include l1_ndebug.cpp -o l1_assert_off
// Then run both. Boost's iterators check BOOST_ASSERT(!!m_ptr) on every
// ++, +=, * (boost/container/vector.hpp); BOOST_ASSERT is assert() unless
// NDEBUG or BOOST_DISABLE_ASSERTS is defined (boost/assert.hpp).
#include "../bench/lat.hpp"
#include <boost/container/flat_map.hpp>

struct Config { uint64_t w[8]; };  // 64-byte mapped value

int main() {
    constexpr int N = 4096;
    boost::container::flat_map<uint64_t, Config> m;
    m.reserve(N);
    for (uint64_t i = 0; i < N; ++i) m.emplace_hint(m.end(), 3 * i, Config{{i}});
    lat::Rng rng(1);
    std::vector<uint64_t> probes(1 << 16);
    for (auto& p : probes) p = 3 * (rng.next() % N);

    uint64_t sink = 0, best = ~0ULL;
    for (int rep = 0; rep < 15; ++rep) {          // best of 15 runs: robust to noise
        uint64_t t0 = lat::now_ns();
        for (uint64_t k : probes) sink += m.find(k)->second.w[0];
        best = std::min(best, lat::now_ns() - t0);
    }
    lat::keep(sink);
    std::printf("[%s]\nflat_map<uint64_t, 64B>::find, N=%d: %.1f ns/lookup (best of 15)\n",
                lat::build_flags(), N, double(best) / probes.size());
}
