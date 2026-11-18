#!/bin/bash
# Card 03-Q4: order these five start-up steps for a NUMA-local hot-path region.
#   P = pin the thread to its CPU      M = mmap the region      B = mbind the region to the node
#   T = touch (pre-fault) every page   H = start the hot path
# Answer (stdin): the five letters in order, e.g. "P M B T H" (spaces/commas/arrows optional).
# Rules (all must hold): M before B; B before T; P before T; T before H.
. "$(dirname "$0")/lib.inc"
read_answer
seq="$(tr -cd 'A-Za-z' < "$WORK/answer.txt" | tr 'a-z' 'A-Z')"
[ "${#seq}" -eq 5 ] || exit 1
for c in P M B T H; do
    [ "$(printf '%s' "$seq" | tr -cd "$c" | wc -c | tr -d ' ')" -eq 1 ] || exit 1
done
pos() { local s="${seq%%$1*}"; echo "${#s}"; }
[ "$(pos M)" -lt "$(pos B)" ] && [ "$(pos B)" -lt "$(pos T)" ] && \
[ "$(pos P)" -lt "$(pos T)" ] && [ "$(pos T)" -lt "$(pos H)" ]
