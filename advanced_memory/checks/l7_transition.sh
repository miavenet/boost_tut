#!/bin/bash
# Card 07-Q2: body of `void mark_sent(Order& o)`: the order leaves the pending-send queue `send`
# and joins the pending-ack queue `ack`, in O(1), allocating nothing.
. "$(dirname "$0")/lib.inc"
read_answer
# Guard: no loop and no linear search (O(1) = iterator_to on the node itself, never a scan or a position).
python3 - "$WORK/answer.txt" <<'GUARD' || exit 1
import re, sys
s = open(sys.argv[1]).read()
s = re.sub(r'/\*.*?\*/', '', re.sub(r'//[^\n]*', '', s), flags=re.S)
sys.exit(1 if re.search(r'\b(for|while|do|goto)\b|remove|find_if|any_of|std::find\b|\bbegin\(|\bnext\(|\bprev\(|\bend\(', s) else 0)
GUARD
{
cat <<'CPP'
#include <boost/intrusive/list.hpp>
#include <cstdlib>
#include <new>
static long g_news = 0;
void* operator new(std::size_t n) { ++g_news; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
namespace bi = boost::intrusive;
struct Order { long id; bi::list_member_hook<> send_hook, ack_hook; };
using SendQueue = bi::list<Order, bi::member_hook<Order, bi::list_member_hook<>, &Order::send_hook>>;
using AckQueue = bi::list<Order, bi::member_hook<Order, bi::list_member_hook<>, &Order::ack_hook>>;
SendQueue send; AckQueue ack;
void mark_sent(Order& o) {
CPP
cat "$WORK/answer.txt"
cat <<'CPP'

}
int main() {
    constexpr int N = 12;
    Order o[N];
    for (int i = 0; i < N; ++i) { o[i].id = i; send.push_back(o[i]); }
    long n0 = g_news;
    const int order[] = {5, 0, 11, 7, 3};   // middle, head, tail, middle, middle: no fixed position works
    bool ok = true; std::size_t sent = 0;
    for (int k : order) {
        mark_sent(o[k]); ++sent;
        ok = ok && !o[k].send_hook.is_linked() && o[k].ack_hook.is_linked() && &ack.back() == &o[k]
             && send.size() == N - sent && ack.size() == sent;
    }
    ok = ok && g_news == n0;
    const int left[] = {1, 2, 4, 6, 8, 9, 10};
    std::size_t k = 0;
    for (auto& x : send) { if (k >= 7 || x.id != left[k]) ok = false; ++k; }
    ok = ok && k == 7;
    send.clear(); ack.clear();
    return ok ? 0 : 1;
}
CPP
} > "$WORK/a.cpp"
compile_run "$WORK/a.cpp"
