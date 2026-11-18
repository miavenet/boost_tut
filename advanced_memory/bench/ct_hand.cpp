// ct_hand.cpp — compile-time probe: the same three structures as ct_boost.cpp, hand-rolled.
#include "../labs/hft_mem/hft_handroll.hpp"
#include <functional>
#include <vector>
struct O { long id; long qty; hr::dlink a; hr::dlink b; hr::hlink h; };
using LA = hr::dlist<O, &O::a>;
using LB = hr::dlist<O, &O::b>;
using H = hr::hash_index<O, long, &O::id, &O::h, std::hash<long>>;
int main() {
    std::vector<O> v(64);
    LA la; LB lb; H h(64);
    for (long i = 0; i < 64; ++i) { v[i].id = i; la.push_back(v[i]); lb.push_back(v[i]); h.insert(v[i]); }
    long s = 0; la.for_each([&](O& o) { s += o.qty; });
    LA::erase(v[3]); LB::erase(v[3]); H::erase(v[3]);
    s += h.find(5) != nullptr;
    for (auto& o : v) if (o.h.is_linked()) H::erase(o);
    return static_cast<int>(s & 1);
}
