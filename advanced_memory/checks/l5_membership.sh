#!/bin/bash
# Card 05-Q1: body of `bool in_pending_send(const Order& o)`: is the order in a pending-send
# list? The program deliberately leaves the hand-kept `state` enum stale for some orders.
. "$(dirname "$0")/lib.inc"
read_answer
{
cat <<'CPP'
#include <boost/intrusive/list.hpp>
#include <cstdint>
namespace bi = boost::intrusive;
enum class State : std::uint8_t { New, PendingSend, PendingAck };
struct Order { long id; State state; bi::list_member_hook<> send_hook; bi::list_member_hook<> ack_hook; };
using SendList = bi::list<Order, bi::member_hook<Order, bi::list_member_hook<>, &Order::send_hook>>;
using AckList = bi::list<Order, bi::member_hook<Order, bi::list_member_hook<>, &Order::ack_hook>>;
bool in_pending_send(const Order& o) {
CPP
cat "$WORK/answer.txt"
cat <<'CPP'
}
int main() {
    Order a{1, State::New, {}, {}}, b{2, State::PendingSend, {}, {}}, c{3, State::New, {}, {}};
    SendList send; AckList ack;
    send.push_back(a);            // queued, but nobody updated a.state
    ack.push_back(b);             // moved on to pending-ack, b.state left stale
    send.push_back(c); c.state = State::PendingSend;
    bool ok = in_pending_send(a) && !in_pending_send(b) && in_pending_send(c);
    send.erase(SendList::s_iterator_to(c));
    ok = ok && !in_pending_send(c);
    send.clear(); ack.clear();
    return ok ? 0 : 1;
}
CPP
} > "$WORK/a.cpp"
compile_run "$WORK/a.cpp"
