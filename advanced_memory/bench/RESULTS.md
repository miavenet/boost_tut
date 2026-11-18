# Measured bench results

machine: Darwin arm64, Apple clang version 21.0.0 (clang-2100.4.1.1), flags: -std=c++23 -O2. Benches: bench/b1_containers.cpp, b2_alloc_flavors.cpp, b3_locality.cpp; N per row in column n.
Not an isolated core: other jobs shared this Mac, so p99.9/max move between runs; trust only
differences that are large and repeat. Re-run on your own box: bash bench/run.sh

Median of 3 runs. Columns in ns; faults = minor page faults inside the measured loop.

| variant | n | mean | p50 | p99 | p99.9 | max | faults |
|---|---|---|---|---|---|---|---|
| (a) std::map default | 200000 | 395 | 375 | 542 | 1792 | 19792 | 340 |
| (b) pmr::map monotonic | 200000 | 380 | 375 | 584 | 1917 | 22000 | 1274 |
| (c) pmr::map pool->monotonic | 200000 | 354 | 333 | 583 | 667 | 23417 | 0 |
| (d) pmr::map full hierarchy+tripwire | 200000 | 346 | 333 | 625 | 750 | 8417 | 0 |
| (e) hft::map | 200000 | 363 | 334 | 583 | 750 | 9541 | 0 |
| (a) std::unordered_map default | 200000 | 121 | 125 | 209 | 250 | 5584 | 0 |
| (e) hft::unordered_map | 200000 | 83 | 83 | 208 | 292 | 8209 | 0 |
| (a) boost::unordered_node_map default | 200000 | 81 | 83 | 125 | 1250 | 26083 | 243 |
| (e) hft::unordered_node_map | 200000 | 37 | 42 | 84 | 125 | 334 | 0 |
| (a) multi_index default | 200000 | 380 | 375 | 542 | 708 | 19334 | 0 |
| (e) hft::multi_index | 200000 | 328 | 333 | 459 | 542 | 10916 | 0 |
| (a) flat_map default, no reserve | 100000 | 57 | 42 | 84 | 1250 | 392708 | 1012 |
| (a) flat_map default, reserve | 100000 | 43 | 42 | 84 | 84 | 250 | 0 |
| (b) flat_map monotonic, no reserve | 100000 | 57 | 42 | 84 | 1292 | 411208 | 897 |
| (c) flat_map pool->monotonic, no reserve | 100000 | 49 | 42 | 84 | 125 | 422250 | 288 |
| (d) flat_map hierarchy+tripwire, reserve | 100000 | 44 | 42 | 84 | 916 | 6416 | 0 |
| (e) hft::flat_map, reserve | 100000 | 44 | 42 | 84 | 916 | 3291 | 0 |
| (a) unordered_flat_map default, no reserve | 100000 | 36 | 0 | 42 | 83 | 1033000 | 299 |
| (a) unordered_flat_map default, reserve | 100000 | 21 | 0 | 42 | 42 | 250 | 0 |
| (e) hft::unordered_flat_map, reserve | 100000 | 22 | 0 | 42 | 1042 | 2958 | 0 |
| (a) std::vector default, no reserve | 100000 | 22 | 0 | 42 | 42 | 166292 | 78 |
| (a) std::vector default, reserve | 100000 | 19 | 0 | 42 | 42 | 3292 | 0 |
| (e) hft::vector, reserve | 100000 | 20 | 0 | 42 | 83 | 1584 | 0 |
| (a) std::vector + std::string default | 200000 | 195 | 208 | 250 | 333 | 8000 | 8 |
| (e) hft::scratch + reset() per message | 200000 | 134 | 125 | 167 | 209 | 11542 | 0 |
| new/delete (malloc) | 2000 | 20 | 20 | 22 | 28 | 31 | 2 |
| pmr: memory_resource* -> pool (virtual) | 2000 | 7 | 7 | 7 | 7 | 10 | 0 |
| hft::allocator stateless (TLS + direct) | 2000 | 8 | 8 | 8 | 8 | 16 | 0 |
| boost::pool<> object (no lock) | 2000 | 2 | 2 | 2 | 2 | 4 | 0 |
| boost::fast_pool_allocator (singleton+mutex) | 2000 | 25 | 25 | 25 | 30 | 39 | 0 |
| (1) default malloc [ascending] | 200 | 4 | 4 | 4 | 4 | 4 | 0 |
| (2) one shared pool [ascending] | 200 | 5 | 5 | 6 | 6 | 6 | 0 |
| (3) pool per container [ascending] | 200 | 4 | 4 | 4 | 4 | 4 | 0 |
| (1) default malloc [random] | 200 | 18 | 17 | 32 | 33 | 33 | 0 |
| (2) one shared pool [random] | 200 | 27 | 26 | 39 | 40 | 40 | 0 |
| (3) pool per container [random] | 200 | 14 | 13 | 27 | 28 | 30 | 0 |
| packed (same line) | 2000 | 106860 | 95125 | 220500 | 456958 | 526708 | 5 |
| alignas(destructive_interference_size) | 2000 | 24568 | 24583 | 25417 | 31333 | 37000 | 5 |
