#!/usr/bin/env bash
# Run every bench and the capstone 5 times; raw output in bench/results/<name>.runK.txt
cd "$(dirname "$0")/.."
mkdir -p bench/results
for name in bench_lookup bench_modify bench_rehash bench_scatter bench_ordered capstone; do
  for k in 1 2 3 4 5; do ./build/$name > bench/results/$name.run$k.txt 2>&1; echo "$name run$k rc=$?"; done
done
