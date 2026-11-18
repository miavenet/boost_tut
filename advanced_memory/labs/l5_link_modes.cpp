// l5_link_modes.cpp — which link modes make is_linked() trustworthy, and what each one costs.
// Run: ./labs/build/l5_link_modes
// Break it on purpose (each is a COMPILE error, shown by the lesson):
//   clang++ -std=c++23 -I/opt/homebrew/include -DBREAK_NORMAL_IS_LINKED labs/l5_link_modes.cpp
//   clang++ -std=c++23 -I/opt/homebrew/include -DBREAK_AUTO_UNLINK_CTS  labs/l5_link_modes.cpp
#include <boost/intrusive/list.hpp>

#include <cstdio>

namespace bi = boost::intrusive;

using safe_hook   = bi::list_member_hook<>;                                      // default: safe_link
using normal_hook = bi::list_member_hook<bi::link_mode<bi::normal_link>>;
using auto_hook   = bi::list_member_hook<bi::link_mode<bi::auto_unlink>>;

struct S { long id; safe_hook h; };
struct N { long id; normal_hook h; };
struct A { long id; auto_hook h; ~A() {} };

using SList = bi::list<S, bi::member_hook<S, safe_hook, &S::h>>;
using NList = bi::list<N, bi::member_hook<N, normal_hook, &N::h>>;
// auto_unlink REQUIRES a list without a stored size: the hook can leave without telling the list.
using AList = bi::list<A, bi::member_hook<A, auto_hook, &A::h>, bi::constant_time_size<false>>;
#ifdef BREAK_AUTO_UNLINK_CTS
using Bad = bi::list<A, bi::member_hook<A, auto_hook, &A::h>>;    // constant_time_size<true> default
Bad bad_list;
#endif

int main() {
    std::puts("== safe_link (the default) ==");
    {
        S a{1, {}};          // declared BEFORE the list, so it is destroyed AFTER it
        SList l;             // (swap these two lines and ~S asserts: lesson 06, l6_traps)
        std::printf("fresh:        is_linked=%d\n", a.h.is_linked());
        l.push_back(a);
        std::printf("pushed:       is_linked=%d\n", a.h.is_linked());
        l.erase(l.iterator_to(a));
        std::printf("erased:       is_linked=%d   (erase puts the hook back in the default state)\n", a.h.is_linked());
        l.push_back(a);
    }   // ~list: safe/auto modes unlink every element (O(n)), so `a` dies unlinked: no assert
    std::puts("container destroyed while holding `a`: no assert (the list destructor cleared it first)");

    std::puts("\n== normal_link ==");
    {
        NList l;
        N a{1, {}};
        l.push_back(a);
        l.erase(l.iterator_to(a));
#ifdef BREAK_NORMAL_IS_LINKED
        std::printf("%d\n", a.h.is_linked());   // static assertion: needs safe_link or auto_unlink
#endif
        std::puts("is_linked() does not compile; after erase the hook still holds stale pointers,");
        std::puts("so membership must be tracked some other way (a state field, a bit).");
    }

    std::puts("\n== auto_unlink ==");
    {
        AList l;
        {
            A a{1, {}}, b{2, {}};
            l.push_back(a);
            l.push_back(b);
            std::printf("two pushed:   size=%zu (size() walks the list: O(n))\n", l.size());
            a.h.unlink();                       // member unlink(): no list needed
            std::printf("a.unlink():   size=%zu  a.is_linked=%d\n", l.size(), a.h.is_linked());
        }   // b's destructor unlinks b from l: the list changed without anyone touching it
        std::printf("b went out of scope: size=%zu  <- the container shrank 'by itself'\n", l.size());
    }

    std::puts("\n== sizes ==");
    std::printf("sizeof hook: safe %zu, normal %zu, auto %zu; sizeof list (header): safe %zu, auto/no-size %zu\n",
                sizeof(safe_hook), sizeof(normal_hook), sizeof(auto_hook), sizeof(SList), sizeof(AList));
}
