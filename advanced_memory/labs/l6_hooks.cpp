// l6_hooks.cpp — base vs member hooks, tags for several containers, and sizeof every hook HERE.
// Run: ./labs/build/l6_hooks
#include <boost/intrusive/avl_set.hpp>
#include <boost/intrusive/bs_set_hook.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/set.hpp>
#include <boost/intrusive/sg_set.hpp>
#include <boost/intrusive/slist.hpp>
#include <boost/intrusive/splay_set.hpp>
#include <boost/intrusive/treap_set.hpp>
#include <boost/intrusive/unordered_set.hpp>

#include <cstdio>

namespace bi = boost::intrusive;

// BASE hooks: the Order IS-A hook. Two base hooks of the same kind need different tags.
struct by_level {};
struct by_client {};
struct OrderB : bi::list_base_hook<bi::tag<by_level>>, bi::list_base_hook<bi::tag<by_client>> {
    long id = 0;
};
using LevelB = bi::list<OrderB, bi::base_hook<bi::list_base_hook<bi::tag<by_level>>>>;
using ClientB = bi::list<OrderB, bi::base_hook<bi::list_base_hook<bi::tag<by_client>>>>;

// MEMBER hooks: the Order HAS hooks; no tags needed, the member pointer names the container.
struct OrderM {
    long id = 0;
    bi::list_member_hook<> level;
    bi::list_member_hook<> client;
};
using LevelM = bi::list<OrderM, bi::member_hook<OrderM, bi::list_member_hook<>, &OrderM::level>>;
using ClientM = bi::list<OrderM, bi::member_hook<OrderM, bi::list_member_hook<>, &OrderM::client>>;

struct T { long k; bi::set_member_hook<> h; bi::list_member_hook<> l; bi::slist_member_hook<> s;
           friend bool operator<(const T& a, const T& b) { return a.k < b.k; } };

#define SZ(...) std::printf("  %-62s %3zu B  (%zu pointers)\n", #__VA_ARGS__, sizeof(__VA_ARGS__), sizeof(__VA_ARGS__) / sizeof(void*))

int main() {
    std::puts("== one order in two lists: base hooks + tags, member hooks ==");
    {
        OrderB b; LevelB lb; ClientB cb;
        lb.push_back(b); cb.push_back(b);
        std::printf("  base:   level size %zu, client size %zu\n", lb.size(), cb.size());
        lb.clear(); cb.clear();
        OrderM m; LevelM lm; ClientM cm;
        lm.push_back(m); cm.push_back(m);
        std::printf("  member: level size %zu, client size %zu\n", lm.size(), cm.size());
        lm.clear(); cm.clear();
    }
    std::printf("\n== sizeof each hook on this machine (sizeof(void*) = %zu) ==\n", sizeof(void*));
    SZ(bi::slist_member_hook<>);
    SZ(bi::list_member_hook<>);
    SZ(bi::set_member_hook<>);
    SZ(bi::set_member_hook<bi::optimize_size<true>>);
    SZ(bi::avl_set_member_hook<>);
    SZ(bi::avl_set_member_hook<bi::optimize_size<true>>);
    SZ(bi::bs_set_member_hook<>);   // splay / scapegoat / treap use the plain binary-search-tree hook
    SZ(bi::unordered_set_member_hook<>);
    SZ(bi::unordered_set_member_hook<bi::store_hash<true>>);
    SZ(bi::unordered_set_member_hook<bi::optimize_multikey<true>>);
    std::puts("\n== sizeof container headers ==");
    SZ(bi::list<T, bi::member_hook<T, bi::list_member_hook<>, &T::l>>);
    SZ(bi::list<T, bi::member_hook<T, bi::list_member_hook<>, &T::l>, bi::constant_time_size<false>>);
    SZ(bi::slist<T, bi::member_hook<T, bi::slist_member_hook<>, &T::s>>);
    SZ(bi::slist<T, bi::member_hook<T, bi::slist_member_hook<>, &T::s>, bi::cache_last<true>>);
    SZ(bi::set<T, bi::member_hook<T, bi::set_member_hook<>, &T::h>>);
}
