#!/bin/bash
# Card 07-C2 (completion before 07-Q2): the learner writes ONE missing statement of mark_sent(o),
# the unlink from `send`. The scaffold is completed with the answer and graded by
# l7_transition.sh (O(1), no allocation, neighbours intact, asserts on).
set -u
D="$(cd "$(dirname "$0")" && pwd)"
ans="$(grep -v '^[[:space:]]*```' || true)"
printf '%s' "$ans" | grep -q '[^[:space:]]' || exit 1
{
printf '%s\n' "$ans"
printf '%s\n' 'ack.push_back(o);'
} | bash "$D/l7_transition.sh"
