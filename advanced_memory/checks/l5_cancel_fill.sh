#!/bin/bash
# Card 05-C2 (completion before 05-Q2): the learner writes ONE missing statement of cancel(),
# the client-list unlink. The scaffold is completed with the answer and graded by l5_cancel.sh
# (same harness as 05-Q2: asserts on, O(1), nothing allocated).
set -u
D="$(cd "$(dirname "$0")" && pwd)"
ans="$(grep -v '^[[:space:]]*```' || true)"
printf '%s' "$ans" | grep -q '[^[:space:]]' || exit 1
{
printf '%s\n' 'Order* o = b.ids.find(id)->second;'
printf '%s\n' 'b.levels[o->px].erase(Level::s_iterator_to(*o));'
printf '%s\n' "$ans"
printf '%s\n' 'b.ids.erase(id);'
printf '%s\n' 'b.orders.destroy(o);'
} | bash "$D/l5_cancel.sh"
