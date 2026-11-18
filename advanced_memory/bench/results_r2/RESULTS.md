# Re-measured results (round 2)

machine: Darwin arm64 (Apple M1 Max), Apple clang version 21.0.0 (clang-2100.4.1.1), flags: -std=c++23 -O2, shared machine (not an isolated core).
5 runs per program, sequential. Each cell: median [min–max] over the runs. Columns in ns
unless stated; faults = minor page faults inside the measured loop. Raw files: bench/results_r2/*.txt.
The Mac was shared with other jobs (load average is in each raw file), so p99.9 and max move
between runs; trust only large differences that repeat in every run. Regenerate: bash bench/run_r2.sh

## bench/b1_containers (N per row in column n)

| variant | mean | p50 | p99 | p99.9 | max | faults |
|---|---|---|---|---|---|---|
| (a) std::map default | 396 [392–425] | 375 [375–416] | 583 [583–708] | 1750 [1625–2042] | 17750 [14667–25583] | 339 [339–340] |
| (b) pmr::map monotonic | 383 [369–391] | 375 [334–375] | 625 [542–833] | 2042 [1959–2333] | 21667 [15792–35709] | 1274 [1274–1274] |
| (c) pmr::map pool->monotonic | 332 [329–346] | 333 [333–333] | 458 [417–542] | 500 [500–667] | 14291 [7542–22166] | 0 [0–0] |
| (d) pmr::map full hierarchy+tripwire | 330 [326–360] | 333 [333–334] | 458 [417–625] | 542 [500–750] | 12375 [6250–23792] | 0 [0–0] |
| (e) hft::map | 377 [342–393] | 334 [333–375] | 667 [458–708] | 833 [500–834] | 8666 [6833–16250] | 0 [0–0] |
| (a) std::unordered_map default | 123 [121–158] | 125 [125–166] | 209 [209–334] | 292 [250–417] | 9334 [2458–18000] | 0 [0–0] |
| (e) hft::unordered_map | 81 [80–104] | 83 [83–84] | 167 [167–250] | 250 [250–334] | 8625 [375–15750] | 0 [0–0] |
| (a) boost::unordered_node_map default | 81 [80–86] | 83 [83–83] | 125 [125–167] | 1167 [1125–1375] | 24625 [9083–31250] | 243 [243–243] |
| (e) hft::unordered_node_map | 37 [36–38] | 42 [42–42] | 84 [84–84] | 125 [84–125] | 7292 [2834–18625] | 0 [0–0] |
| (a) multi_index default | 377 [372–379] | 375 [375–375] | 500 [500–542] | 584 [583–709] | 13666 [6709–35709] | 0 [0–0] |
| (e) hft::multi_index | 333 [330–346] | 333 [333–333] | 459 [459–666] | 583 [542–2875] | 20708 [10000–60333] | 0 [0–0] |
| (a) flat_map default, no reserve | 57 [57–57] | 42 [42–42] | 84 [84–84] | 1209 [1167–1458] | 399584 [382000–428584] | 1011 [1011–1012] |
| (a) flat_map default, reserve | 43 [43–44] | 42 [42–42] | 84 [84–84] | 84 [84–125] | 6625 [209–11250] | 0 [0–0] |
| (b) flat_map monotonic, no reserve | 57 [56–59] | 42 [42–42] | 84 [84–84] | 1250 [1167–1417] | 430750 [384708–473084] | 897 [897–897] |
| (c) flat_map pool->monotonic, no reserve | 49 [48–50] | 42 [42–42] | 84 [84–84] | 167 [125–208] | 392125 [382125–438292] | 288 [288–288] |
| (d) flat_map hierarchy+tripwire, reserve | 44 [44–44] | 42 [42–42] | 84 [84–84] | 833 [750–1042] | 5500 [2959–6000] | 0 [0–0] |
| (e) hft::flat_map, reserve | 44 [44–44] | 42 [42–42] | 84 [84–84] | 917 [708–1166] | 6625 [2625–10709] | 0 [0–0] |
| (a) unordered_flat_map default, no reserve | 36 [35–39] | 0 [0–0] | 42 [42–42] | 42 [42–83] | 1024917 [922125–1228459] | 299 [299–299] |
| (a) unordered_flat_map default, reserve | 21 [21–21] | 0 [0–0] | 42 [42–42] | 42 [42–42] | 6167 [167–14375] | 0 [0–0] |
| (e) hft::unordered_flat_map, reserve | 22 [22–23] | 0 [0–0] | 42 [42–42] | 1000 [791–1208] | 3250 [3167–9584] | 0 [0–0] |
| (a) std::vector default, no reserve | 23 [22–24] | 0 [0–0] | 42 [42–42] | 42 [42–83] | 215833 [168833–246250] | 78 [78–79] |
| (a) std::vector default, reserve | 19 [19–20] | 0 [0–0] | 42 [42–42] | 42 [42–42] | 2000 [125–5583] | 0 [0–0] |
| (e) hft::vector, reserve | 20 [20–21] | 0 [0–0] | 42 [42–42] | 42 [42–708] | 2333 [1417–7291] | 0 [0–0] |
| (a) std::string default | 42 [41–42] | 42 [42–42] | 83 [42–83] | 84 [84–84] | 8000 [2459–30208] | 2 [0–2] |
| (e) hft::string | 31 [31–31] | 41 [41–41] | 42 [42–42] | 42 [42–125] | 2375 [209–8208] | 0 [0–0] |
| (a) std::vector + std::string default | 198 [195–198] | 208 [208–208] | 250 [250–250] | 292 [292–333] | 6709 [4500–22250] | 8 [7–8] |
| (e) hft::scratch + reset() per message | 134 [134–135] | 125 [125–125] | 167 [167–167] | 209 [167–250] | 7042 [4875–11625] | 0 [0–0] |

## labs/l1_tail_histogram (200,000 x 256 B, ARENA and MALLOC sections)

| variant | mean | p50 | p99 | p99.9 | max | faults |
|---|---|---|---|---|---|---|
| MALLOC (all 15 runs) | 45 [44–46] | 41 [41–41] | 1000 [958–1042] | 1417 [1292–1583] | 19083 [7000–49625] | 3138 [3138–3138] |
| ARENA, default (warm, cap 256) | 22 [22–22] | 41 [41–41] | 42 [42–42] | 83 [42–84] | 3584 [209–6417] | 0 [0–0] |
| ARENA, nowarm (cap 256) | 30 [29–30] | 41 [41–41] | 42 [42–42] | 2750 [1042–3917] | 8708 [7625–11041] | 0 [0–0] |
| ARENA, nowarm nocap | 32 [30–32] | 41 [41–41] | 42 [42–42] | 83 [42–83] | 365167 [272708–403417] | 0 [0–0] |

## labs/l3_first_touch (64 MiB, 16 KiB pages)

| pass | page faults | ns per page |
|---|---|---|
| pass 1 | 4096 [4096–4096] | 1012 [998–1078] |
| pass 2 | 0 [0–0] | 58 [7–137] |

## labs/l4_hot_cold (1,000,000 orders; ns per order)

| layout | p50 | p99 |
|---|---|---|
| fat struct (hot+cold) | 4.04 [4.03–4.11] | 4.20 [4.13–4.31] |
| hot fields split out | 0.41 [0.40–0.42] | 0.56 [0.49–1.79] |

Ratio fat/split p50 per run: 9.8x, 10.3x, 9.9x, 10.1x, 9.6x

