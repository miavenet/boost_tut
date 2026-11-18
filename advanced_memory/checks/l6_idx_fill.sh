#!/bin/bash
# Card 06-C2 (completion before 06-Q6): the learner writes ONE missing statement of the index
# list's erase(i), the one that fixes the NEXT neighbour (or the tail). The scaffold is completed
# with the answer and graded by l6_idx_erase.sh (middle, head, tail, last-element cases).
set -u
D="$(cd "$(dirname "$0")" && pwd)"
ans="$(grep -v '^[[:space:]]*```' || true)"
printf '%s' "$ans" | grep -q '[^[:space:]]' || exit 1
{
printf '%s\n' 'ilink& l = base_[i].*M;'
printf '%s\n' 'if (l.prev != nil) (base_[l.prev].*M).next = l.next; else head_ = l.next;'
printf '%s\n' "$ans"
printf '%s\n' 'l.prev = l.next = nil;'
} | bash "$D/l6_idx_erase.sh"
