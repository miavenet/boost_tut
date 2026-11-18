#!/bin/bash
# Card 05-Q4: body of `bool queued_for(const Order& o, int v)`: is the order queued for venue v?
# All per-venue send lists share ONE hook (send_hook); the order records its venue in `venue`.
. "$(dirname "$0")/lib.inc"
read_answer
{
cat <<'CPP'
#include <boost/intrusive/list.hpp>
#include <array>
#include <cstdint>
namespace bi = boost::intrusive;
struct Order { long id; std::uint8_t venue; bi::list_member_hook<> send_hook; };
using SendList = bi::list<Order, bi::member_hook<Order, bi::list_member_hook<>, &Order::send_hook>>;
bool queued_for(const Order& o, int v) {
CPP
cat "$WORK/answer.txt"
cat <<'CPP'
}
int main() {
    std::array<SendList, 4> to_venue;
    Order a{1, 2, {}}, b{2, 0, {}};
    to_venue[a.venue].push_back(a);            // a queued for venue 2; b not queued anywhere
    bool ok = queued_for(a, 2) && !queued_for(a, 1) && !queued_for(a, 3) && !queued_for(b, 0);
    to_venue[a.venue].erase(SendList::s_iterator_to(a));
    ok = ok && !queued_for(a, 2);
    for (auto& l : to_venue) l.clear();
    return ok ? 0 : 1;
}
CPP
} > "$WORK/a.cpp"
compile_run "$WORK/a.cpp"
