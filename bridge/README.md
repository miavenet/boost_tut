# bridge: the one-line allocator swap, executed
**Proves:** the swap promised at the end of the multi_index and flat tutorials compiles unchanged with `advanced_memory/labs/hft_mem` and gives identical results, with fewer heap allocations.
- Run: `bash bridge/build.sh` (clang++ -std=c++23 -O2 -DNDEBUG, no extra link flags needed; Boost headers only).
- Both capstones have a `main()`, so the store/table definitions are copied (source lines named in each .cpp). Nothing in the tutorials was edited.
- Counts are global `operator new` calls (replaced operator new/delete) during the setup / workload.
- multi_index, one line: `MIStore<>` -> `MIStore<hft::allocator<Order>>`, after `hft::thread_arena arena;`
- flat, one line: `ConfigTableT<>` -> `ConfigTableT<hft::allocator<InstrumentConfig>>`; `RiskTableT<>` -> `RiskTableT<hft::allocator<std::pair<Sym8, RiskLimit>>>`
- Compile problems: none. Smallest tutorial change needed: none.
Observed output (one run, exit code 0):
```
std::allocator : checksum 2004158949128658989 size 4333 | operator new: construct 5, workload 8000
hft::allocator : checksum 2004158949128658989 size 4333 | operator new: construct 0, workload 0
default allocators: checksum 1109950000 risk size 20020 listed 20 | operator new: setup 25, hot loop 1
hft::allocator    : checksum 1109950000 risk size 20020 listed 20 | operator new: setup 0, hot loop 0
```
- Results (checksums, sizes) are identical per pair; the programs assert it and exit 1 otherwise.
- Caveat: the arena is a pre-faulted region, so its memory is not counted by `operator new`. No timing is claimed.
