// ct_boost.cpp — compile-time probe: the same three structures as ct_hand.cpp, with Boost.Intrusive.
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/unordered_set.hpp>
#include <functional>
#include <vector>
namespace bi = boost::intrusive;
struct O { long id; long qty; bi::list_member_hook<> a; bi::list_member_hook<> b; bi::unordered_set_member_hook<> h; };
struct key { using type = long; const long& operator()(const O& o) const { return o.id; } };
using LA = bi::list<O, bi::member_hook<O, bi::list_member_hook<>, &O::a>>;
using LB = bi::list<O, bi::member_hook<O, bi::list_member_hook<>, &O::b>>;
using H = bi::unordered_set<O, bi::member_hook<O, bi::unordered_set_member_hook<>, &O::h>, bi::key_of_value<key>,
                            bi::hash<std::hash<long>>, bi::power_2_buckets<true>>;
int main() {
    std::vector<O> v(64); std::vector<H::bucket_type> bk(64);
    LA la; LB lb; H h(H::bucket_traits(bk.data(), bk.size()));
    for (long i = 0; i < 64; ++i) { v[i].id = i; la.push_back(v[i]); lb.push_back(v[i]); h.insert(v[i]); }
    long s = 0; for (auto& o : la) s += o.qty;
    la.erase(LA::s_iterator_to(v[3])); lb.erase(LB::s_iterator_to(v[3])); h.erase(h.iterator_to(v[3]));
    s += h.find(5) != h.end();
    la.clear(); lb.clear(); h.clear();
    return static_cast<int>(s & 1);
}
