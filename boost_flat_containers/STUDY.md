# Study guide

Time: use short sessions; stop after any step and resume at the next unchecked item.
For each pretest, write a guess, check its key at the end, then return to the next step.
For other questions, write an answer before opening the answer section.
Before checking ANY key, mark your confidence on the Confidence line: sure, unsure or a %.
A miss you marked sure (or >= 70%) is a high-confidence miss: star it (*) in your
tracking sheet, re-test it tomorrow BEFORE any other due card, and keep the star until
you get it right on a later day (confident errors are corrected but fade unless re-tested).
After a core answer and its key, try each optional stretch before viewing its key;
a skipped or attempted stretch does not change the four-box schedule.
Manual review: four boxes; after a hit review in 1, 2, 7, then 21 days.
A miss returns to box 1 and is due tomorrow. Record the date and box on paper.
To redo everything, first date and copy your tracking sheet and attempt log,
then start a blank sheet and return to the first pretest below. To redo one
lesson, keep that dated copy and all old attempts, clear only that lesson's
pretest marks, checked teaching and practice steps, boxes, due dates and
unfinished answer, then restart at that lesson's first pretest. Leave every
other lesson's marks and due dates as they are.

## lesson-01.md

# Lesson 01 — "Flat" means one sorted array (and what that costs)

Your order gateway keeps a table of 64 instruments. Somebody swapped
`std::map` for `boost::container::flat_map` "because flat is faster" and the
lookup got faster. Then someone with 64-byte config records did the same swap
in a debug-ish build and it got *slower* than `std::map`. Both people are
right. By the end of these 25 minutes you will know why, and you will have
measured it yourself.

How this works: you guess before you are told, and you answer from memory
before you see the key. That feels harder than reading. The extra effort is
the method working, not a sign you are doing badly. Every number here was
measured on an Apple M1 Max (Apple clang 21, `-std=c++23 -O2`, Boost 1.90,
`steady_clock` tick ≈ 41.7 ns). **Run protocol:** each bench was run 5 times
on a shared machine with other jobs running; raw runs are in
`bench/results/run1..run5/`, the per-cell median is in `bench/results/median/`,
and the spread is in `bench/results/median/SPREAD.txt`. Lookup p50s moved about
4–5% run to run (median cell; 90% of cells within ~13%), single cells up to 42%; tail maxima moved
by factors of 2–10, so tails are taught as mechanisms, not as exact figures.
**Numbers moved 20–50% between two sessions on this same machine depending on background load** (the 1,048,576-entry `std::map` lookup read 623.7 ns in one session and 423.8 ns in the next), **so trust ratios and shapes, and re-run on your own box.** A tuned, isolated x86-64 Linux core will give
different numbers. **Re-run the benches on your own box**; trust mechanisms,
not my numbers.

First thing, right now (≈1 min): `bash labs/build.sh` from the tutorial directory.

## Pretest — commit your guesses FIRST (no fair reading ahead)

### 01-P1 [pretest]
Cold guess. 64 entries, `uint64_t` key → `uint64_t` value, random successful
   lookups, `-O2 -DNDEBUG`. Rank these from fastest to slowest per lookup:
   `std::map`, `boost::container::flat_map`, `std::unordered_map`,
   `boost::unordered_flat_map`, and a linear scan over an unsorted array.

Confidence (mark before checking the key): sure / unsure / ___%

### 01-P2 [pretest]
Cold guess. A `flat_map<uint64_t, uint64_t>` holds 10,000 entries and has
   spare capacity. You insert a key smaller than every existing key. Roughly how
   long does that one insert take: ~40 ns, ~200 ns, ~5 µs or ~1 ms?

Confidence (mark before checking the key): sure / unsure / ___%

### 01-P3 [pretest]
Cold guess. Same source file, same `-O2`. Build A has no extra flags. Build B
   adds `-DNDEBUG`. With 4,096 entries and 64-byte values, how much faster is
   `flat_map::find` in build B: about the same, 1.2×, or about 4×?

Confidence (mark before checking the key): sure / unsure / ___%

---

## Concept cards

### CONCEPT L1C1  A flat_map is one sorted, contiguous sequence of pairs
`boost::container::flat_map<K, T>` stores a single vector-like sequence of
`std::pair<K, T>`, kept sorted by key. There is no tree and there are no nodes.
`find` is a binary search over that array, and iteration walks plain memory.
Because the storage is one array you also get random access: `nth(i)` and
`index_of(it)` run in O(1). Those are Boost extensions; `std::map` has no
equivalent. The Boost docs call these "ordered, vector-like container based,
associative containers" and list the trade: faster lookup and much faster
iteration, but slower insertion and erasure than node-based containers.
Figure: [10|1.0][20|2.0][30|3.0][40|4.0] laid end to end, 16 bytes per slot, versus std::map with four separately allocated nodes linked by pointers.
Source: https://www.boost.org/doc/libs/1_90_0/doc/html/container/non_standard_containers.html#container.non_standard_containers.flat_xxx ; labs/l1_layout.cpp (run)

**L1C1 Elaborate:** In your own words: why does one sorted array make iteration and
`nth()` cheap, and what has to happen to that array when a new smallest key
arrives?

**L1C1 HandsOn:** Before running, predict (a) the byte gap between consecutive keys,
(b) the order keys print in, (c) whether key 10's address changes after
`emplace(5, …)`. Then run `labs/bin/l1_layout`.

### CONCEPT L1C2  Writes are O(N) moves: cost = elements after the slot × sizeof(value_type)
An insert does a binary search for the slot and then shifts every element
after it one position right. An erase shifts them one position left. If
`size() == capacity()`, the insert first reallocates and moves the whole
array. So **position decides the price**: the bytes moved are (elements after
the slot) × `sizeof(value_type)`. Boost's reference gives the same complexity:
"logarithmic search time plus linear insertion to the elements with bigger
keys". Measured at 10,000 `uint64_t→uint64_t` entries (p50, one op per sample):
insert at the front 5,459 ns, at a random position 2,584 ns, at the back 42 ns.
Erase at the front took 2,708 ns and at the back 42 ns. A `std::map`
insert+erase pair took 208 ns, because it allocates one node and moves nothing.
Figure: insert at index 0 → memmove of [0, N) to [1, N+1); insert at the back → no move.
Source: https://www.boost.org/doc/libs/1_90_0/doc/html/doxygen/boost_container_header_reference/classboost_1_1container_1_1flat__map.html ; bench/results/median/insert_tail.ndebug.txt

**L1C2 Elaborate:** Why is "append in ascending key order" the cheapest way to fill a
flat_map one element at a time, and why does a random insert cost about half of a
front insert?

### CONCEPT L1C3  Lookup mechanism and the crossover you measured
A `std::map` lookup makes about log2(N) dependent pointer chases. Each hop
loads a node somewhere on the heap, and the next address is unknown until
that load lands. A flat_map does the same number of comparisons, but inside
one array: the first few probe points are reused by every lookup and stay in
cache, and the array has no pointers or allocator headers. A hash table
computes one bucket and looks there. Measured p50 ns per successful lookup,
8-byte key and value, `-DNDEBUG`:

| N | flat_map | std::flat_map | std::map | std::unordered_map | boost::unordered_flat_map | linear scan |
|---|---|---|---|---|---|---|
| 64 | 8.5 | 8.5 | 24.8 | 6.5 | 2.6 | 22.1 |
| 4,096 | 19.5 | 19.5 | 61.2 | 7.8 | 3.2 | – |
| 1,048,576 | 162.8 | 128.2 | 423.8 | 24.7 | 11.7 | – |

What the table says: with 8-byte keys, flat_map beat `std::map` at every N
measured (8 to 1M), by 2.6–3.1× at the three sizes shown (24.8/8.5, 61.2/19.5, 423.8/162.8). With 32-byte keys
they tied at N=8. `boost::unordered_flat_map` beat both at every N.
`std::unordered_map` was faster than flat_map at every size except N=32
with 8-byte keys (both value sizes), where flat_map was ahead by 0.7 ns
(7.8 vs 8.5), less than one timer tick per lookup, so treat that as a tie.
The naive linear scan beat binary search only with 32-byte keys and
N ≤ 16 (15.0 vs 18.9 ns at N=8). Choose flat_map for what a hash cannot give:
**sorted order, range queries (`lower_bound`), cheap ordered iteration,
compactness**, and a lookup that is ~3× better than `std::map` (2.6–3.1× here).
Figure: std::map: root→node→node→… (each arrow a cache miss at large N); flat_map: probes at N/2, N/4, … within one array; hash: one bucket.
Source: bench/lookup_crossover.cpp; bench/results/median/lookup_crossover.ndebug.txt

**L1C3 Elaborate:** Explain why the gap between std::map and flat_map grows as N grows
from 4,096 to 1M, while both remain worse than a hash table.

**L1C3 HandsOn:** Run `bench/run_all.sh` (about 15 minutes: 5 runs, normal priority, quiet machine)
or just `clang++ -std=c++23 -O2 -DNDEBUG -I/opt/homebrew/include bench/lookup_crossover.cpp -o /tmp/lc && /tmp/lc quick`.
Find the smallest N at which flat_map's p50 is at least twice std::map's on YOUR machine.

### CONCEPT L1C4  Boost.Container ships with its asserts ON unless you say otherwise
Boost's `vector` iterator (which flat_map uses inside) runs `BOOST_ASSERT(!!m_ptr)` in
`operator*`, `++` and `+=` (boost/container/vector.hpp). `BOOST_ASSERT` expands
to plain `assert()` unless `NDEBUG` or `BOOST_DISABLE_ASSERTS` is defined
(boost/assert.hpp, lines 36–72). The plain `-O2` build therefore keeps
the checks inside the binary-search loop. With 64-byte values (asserts ON),
flat_map lost to `std::map` at every N up to 65,536 (82.7 vs 65.8 ns at
N=4,096; 145.8 vs 127.0 at N=65,536) and only won at N=1,048,576 (354.8 vs 583.3). With `-DNDEBUG` it was 9.1 vs 24.8 ns at N=64. `-DBOOST_DISABLE_ASSERTS`
alone gave the same 22.0 ns as `-DNDEBUG` in labs/l1_ndebug.cpp (5 runs of each
build, all 22.0; asserts ON: 80.1–80.4; bench/results/probes/probes-and-l1-disable-asserts.txt). The lesson
for benchmarking: before you believe a layout story, check what you compiled.
Source: /opt/homebrew/include/boost/assert.hpp (lines 36–72); /opt/homebrew/include/boost/container/vector.hpp (line 158 onward); labs/l1_ndebug.cpp (run)

**L1C4 Elaborate:** Why is a benchmark of a header-only library without `-DNDEBUG`
not a fair test of its data structure?

**L1C4 HandsOn:** Run `labs/bin/l1_assert_on` then `labs/bin/l1_assert_off` and write
down both ns/lookup values and their ratio.

---

## Completion cards (fill the gap)

### 01-C1 [completion]
Complete: a `boost::container::flat_map<K,T>` stores one ______ sequence of
   ______, kept ______ by key, and `find` does a ______ over it.

Confidence (mark before checking the key): sure / unsure / ___%

### 01-C2 [completion]
Complete the cost formula for one flat_map insert with spare capacity:
   bytes moved = (number of elements ______ the insertion slot) × ______.

Confidence (mark before checking the key): sure / unsure / ___%

### 01-C3 [completion]
Complete: `BOOST_ASSERT` expands to ______ unless the macro ______ or
   ______ is defined.

Confidence (mark before checking the key): sure / unsure / ___%

---

## Question cards (free recall + transfer)

### 01-Q1 [recall]
From memory: describe the memory layout of `boost::container::flat_map`, and
   name two operations it makes cheap that `std::map` cannot do as cheaply.

Hint (optional): one block of pairs; think "random access".

Confidence (mark before checking the key): sure / unsure / ___%

### 01-Q2 [recall]
From memory: why is a flat_map lookup faster than a std::map lookup with the
   same number of comparisons, and why does neither beat a hash table on point
   lookups in the bench?

Confidence (mark before checking the key): sure / unsure / ___%

### 01-Q3 [apply]
`flat_map<uint64_t, uint64_t>` with 10,000 entries and spare capacity. You
   insert a key smaller than all of them. How many bytes does the insert move?
   Give a number. (Your answer is checked by a script.)

Hint (optional): sizeof(std::pair<uint64_t,uint64_t>) × how many elements?

Confidence (mark before checking the key): sure / unsure / ___%

### 01-Q4 [apply]
Different domain: a market-data handler keeps a `flat_map<Price, Level>` order
   book side (bids sorted ascending by price, best bid at the BACK). Most updates
   touch the best few levels. Is a flat_map a reasonable fit for this access
   pattern, and what would change if the best bid were stored at the FRONT?

Confidence (mark before checking the key): sure / unsure / ___%

### 01-Q5 [apply]
Your teammate's flat_map benchmark used `clang++ -std=c++23 -O2`. Give the
   extra flag(s) you add so that Boost.Container's internal assertions are
   compiled out. (Your flags are passed to clang++ and tested.)

Hint (optional): one macro name from the C standard library.

Confidence (mark before checking the key): sure / unsure / ___%

### 01-Q6 [apply]
Transfer: a risk service needs "all limits for symbols between `AAPL` and `AMZN`"
   every 100 ms, plus point lookups per order. Using the measured table, argue for
   flat_map over `boost::unordered_flat_map`, and say what you give up.

Confidence (mark before checking the key): sure / unsure / ___%

---

## Integrate (day close)

### 01-INT [synthesize]
In three sentences: what one property of flat_map makes lookups and
   iteration cheap, how does that same property make writes expensive, and what
   must you check about the build before trusting any flat_map benchmark?

Confidence (mark before checking the key): sure / unsure / ___%

## lesson-02.md

# Lesson 02 — References that dangle, keys that lie, and the C++23 cousin

Imagine a risk check that caches `Limit& lim = table.find(sym)->second;` once
per session "for speed". With `std::map` that is legal forever; node-based
containers never move an element. Swap in a flat_map and the first intraday
listing turns `lim` into a pointer to freed memory, or worse, into a pointer to
*somebody else's* limit with no crash at all. Today you will make both happen
under AddressSanitizer, and then meet `std::flat_map`, which is built
differently on purpose.

Start now (≈1 min): `bash labs/build.sh` if you have not, then keep a terminal
open in `labs/`.

## Pretest — commit your guesses FIRST

### 02-P1 [pretest]
Cold guess. `Limit& lim = t.find(200)->second;` then 40 inserts of smaller keys,
   then you read `lim.max_qty`. Run A: `t` had no reserve. Run B: `t.reserve(64)`
   was called first, so it never reallocates. Under AddressSanitizer, which run
   reports an error, and what does the other one print?

Confidence (mark before checking the key): sure / unsure / ___%

### 02-P2 [pretest]
Cold guess. Does `m.find(10)->first = 99;` compile for
   `boost::container::flat_map<int,int>`? For `std::map<int,int>`? For
   `std::flat_map<int,int>`?

Confidence (mark before checking the key): sure / unsure / ___%

### 02-P3 [pretest]
Cold guess. C++23 `std::flat_map<K,T>`: how many underlying containers does it
   hold, and does `auto& p = *m.begin();` compile?

Confidence (mark before checking the key): sure / unsure / ___%

---

## Concept cards

### CONCEPT L2C1  Every insert/erase may move elements: pointers, references and iterators dangle
Node containers allocate each element once and never move it. A flat_map keeps
its elements in an array, so it moves them on every write. Boost's docs state
it plainly: "iterators are invalidated when inserting and erasing elements".
The class reference adds that an insert may invalidate existing elements, that
erase invalidates elements with keys not less than the erased one, and that
reserve invalidates them when it grows capacity. There are two failure modes. **Reallocation**
frees the old block, so a kept reference is a use-after-free that ASan can see.
**Shifting** without reallocation leaves your reference pointing at the same
slot, which now holds a *different element*, and no tool will flag it. `reserve`
cures only the first. The repairs: keep the **key** and re-`find` after any
mutation, or copy the value out before mutating.
Figure: before: [100][200][300], lim → slot 1 (200). After 40 smaller inserts without realloc: [1][2]…, lim → slot 1 (key 2).
Source: https://www.boost.org/doc/libs/1_90_0/doc/html/container/non_standard_containers.html#container.non_standard_containers.flat_xxx ; https://www.boost.org/doc/libs/1_90_0/doc/html/doxygen/boost_container_header_reference/classboost_1_1container_1_1flat__map.html ; labs/l2_dangling.cpp (run)

**L2C1 Elaborate:** Why does `reserve()` make the ASan error disappear but not the bug?

**L2C1 HandsOn:** Run `labs/bin/l2_dangling grow`, then `shift`, then `fixed`. Write down
what each one prints (look for `ERROR: AddressSanitizer` and the `limit for 200` line).

### CONCEPT L2C2  Boost's value_type is pair<Key,T>, the key is writable, and moves are mandatory
`boost::container::flat_map<K,T>::value_type` is `std::pair<K,T>`. The Boost
reference contrasts it with `std::map`'s `pair<const K,T>`. So
`it->first = 99` compiles. It silently breaks the sort invariant, and after
that binary search gives wrong answers even for keys you never touched: in
labs/l2_mutable_key, `find(20)` fails even though 20 is still in the array.
The key is non-const so the container can *move-assign* elements when it
shifts them. That is also why element types must be movable: a non-movable
mapped type fails to compile (`call to deleted constructor`), while
`std::map<int, NoMove>` compiles and runs. There are no node handles either:
`flat_map` has no `extract(key)`, which `std::map` has. Boost lists "weaker
exception safety than standard associative containers" among the trade-offs.
Prefer nothrow-movable mapped types, and you can guard that with
`static_assert(std::is_nothrow_move_constructible_v<T>)`.
Source: https://www.boost.org/doc/libs/1_90_0/doc/html/doxygen/boost_container_header_reference/classboost_1_1container_1_1flat__map.html ; https://www.boost.org/doc/libs/1_90_0/doc/html/container/non_standard_containers.html#container.non_standard_containers.flat_xxx ; labs/l2_mutable_key.cpp (run); tests/compile_claims.sh (run; exact diagnostics recorded in bench/results/probes/compile_claims.txt)

**L2C2 Elaborate:** Why does changing one key through an iterator make lookups of *other*
keys fail?

**L2C2 HandsOn:** Predict, then run `labs/bin/l2_mutable_key`: what order do the keys print
in, and are 99 and 20 found?

### CONCEPT L2C3  std::flat_map is an adaptor over TWO containers
C++23 `std::flat_map<Key, T, Compare, KeyContainer = vector<Key>, MappedContainer = vector<T>>`
in `<flat_map>` is a **container adaptor**. Per cppreference, "the first
container is sorted, and for each key its corresponding value is in the second
container at the same index". `value_type` is `pair<Key,T>`, but `reference` is
`pair<const Key&, T&>`, a proxy returned by value. So the key cannot be assigned
and `auto& p = *it` does not compile. Boost's flat_map is a **container** that
owns one sequence of pairs; its fourth template parameter is an allocator *or*
the sequence type itself. The layout matters for search: std's binary search
reads only the keys array. Measured with 8-byte keys at N=1M (`-DNDEBUG`,
per-cell median of 5 runs): with 8-byte values Boost was 162.8 ns and std 128.2 ns;
with 64-byte values Boost was 199.2 ns and std 151.7 ns. Growing the value from 8 to 64
bytes slowed BOTH (Boost +22%, std +18%), so the keys-only layout did not shield std
from the bigger value in this bench, and std's lead stayed about the same size (Boost/std
1.27× → 1.31×; std won in all 5 runs, 127–133 vs 158–171 ns and 138–153 vs 197–211 ns). I did not isolate why
the bigger value hurts std too, so I will not offer a mechanism. With 32-byte keys at 1M the order flipped:
std was *slower* (464.8 vs 334.0 ns; 458–490 vs 314–388 over 5 runs), also not isolated.
**Measure your own key/value shapes.** This
toolchain compiles `std::flat_map` (`__cpp_lib_flat_map` = 202511 under Apple clang 21; probe _probes/cpp_lib_flat_map.cpp, output in bench/results/probes/probes-and-l1-disable-asserts.txt).
Figure: Boost: [k0 v0][k1 v1][k2 v2]… ; std: keys [k0][k1][k2]… and values [v0][v1][v2]… aligned by index.
Source: https://en.cppreference.com/w/cpp/container/flat_map ; bench/results/median/lookup_crossover.ndebug.txt; labs/l2_std_vs_boost.cpp (run)

**L2C3 Elaborate:** Why can't `std::flat_map` hand you a `T&` inside a real `std::pair`?

### CONCEPT L2C4  The API map: tags, bulk hand-off, and what is missing
| Need | Boost flat_map | std::flat_map (C++23) |
|---|---|---|
| "input already sorted+unique" tag | `boost::container::ordered_unique_range` | `std::sorted_unique` |
| take storage out | `extract_sequence()` → `sequence_type` | `std::move(m).extract()` → `containers{keys, values}` |
| put storage in | `adopt_sequence(ordered_unique_range, std::move(seq))` (O(1)) or `adopt_sequence(std::move(seq))` (sorts, removes duplicates) | `replace(std::move(keys), std::move(values))` |
| pre-size | `reserve()`, `capacity()` | none on the map (`reserve` → "no member named 'reserve'") |
| random access | `nth()`, `index_of()` | iterators are random access; `keys()[i]`, `values()[i]` |

`extract()` is rvalue-qualified: `s.extract()` on an lvalue fails with "function
has rvalue ref-qualifier". To pre-size a std::flat_map, reserve the two vectors
before you `replace` them in.
Source: https://www.boost.org/doc/libs/1_90_0/doc/html/doxygen/boost_container_header_reference/classboost_1_1container_1_1flat__map.html ; https://en.cppreference.com/w/cpp/container/flat_map/extract ; labs/l2_std_vs_boost.cpp (run)

**L2C4 Elaborate:** You must rebuild a 50k-entry table every morning from a sorted file.
Which calls do you use in each library to avoid per-element inserts?

**L2C4 HandsOn:** Run `labs/bin/l2_std_vs_boost` and note the `capacity=` value Boost prints after `reserve(100)` and the `s[0]` values std prints.

---

## Completion cards

### 02-C1 [completion]
Complete: a reallocation turns a kept reference into a ______ (which ASan
   reports). A shift without reallocation makes it point at a ______ (which
   ASan does not report). `reserve` prevents only the ______.

Confidence (mark before checking the key): sure / unsure / ___%

### 02-C2 [completion]
Complete: `std::flat_map<int,double>::reference` is ______, and to take the
   containers out of an lvalue `s` you write ______.

Confidence (mark before checking the key): sure / unsure / ___%

### 02-C3 [completion]
Complete: Boost flat_map's `value_type` is ______ (key constness?), which lets
   the container ______ elements when it shifts them, and which means element
   types must be ______.

Confidence (mark before checking the key): sure / unsure / ___%

---

## Question cards

### 02-Q1 [recall]
From memory: which operations on a Boost flat_map can invalidate pointers,
   references and iterators to its elements, and why does std::map not have this
   problem?

Confidence (mark before checking the key): sure / unsure / ___%

### 02-Q2 [recall]
From memory: what goes wrong after `it->first = 99` on a Boost flat_map, and
   name one other thing std::map can do that a flat_map cannot.

Confidence (mark before checking the key): sure / unsure / ___%

### 02-Q3 [recall]
From memory: state two structural differences between Boost flat_map and
   C++23 std::flat_map, and one consequence of each.

Confidence (mark before checking the key): sure / unsure / ___%

### 02-Q4 [apply]
Repair this function so it returns the limit for id 200 *after* the listings
   are added. `add_listings(t)` inserts 40 smaller keys; `Limit` has `long max_qty`.
   Write the whole function. It is compiled and run under AddressSanitizer.
   ```cpp
   long limit_after_listing(RiskTable& t) {
     Limit& l = t.find(200)->second;
     add_listings(t);
     return l.max_qty;
   }
   ```

Hint (optional): keep the key, not the reference.

Confidence (mark before checking the key): sure / unsure / ___%

### 02-Q5 [apply]
Transfer: an order book stores `flat_map<OrderId, Order>` and a separate
   `std::vector<Order*>` "queue" pointing into it for time priority. What breaks,
   when, and give two designs that fix it.

Confidence (mark before checking the key): sure / unsure / ___%

### 02-Q6 [apply]
`std::flat_map<int,double> s` is already filled. Write statements that take its
   two containers out, set every mapped value to 0.0, and put them back, with no
   copying of the vectors. (Compiled and run.)

Confidence (mark before checking the key): sure / unsure / ___%

### 02-Q7 [apply]
Transfer: a symbol table maps a 16-byte symbol to a 256-byte static-data record,
   and the hot path only asks "is this symbol known?" (`contains`). Which flat
   layout is the better bet for that query, and what should you do before
   committing to it?

Confidence (mark before checking the key): sure / unsure / ___%

---

## Integrate

### 02-INT [synthesize]
One paragraph: explain why both "references dangle" and "the key is writable"
   follow from the same design decision in Boost flat_map, and how std::flat_map's
   two-container design changes each of them.

Confidence (mark before checking the key): sure / unsure / ___%

## lesson-03.md

# Lesson 03 — The write path: bulk builds, growth spikes, and zero-heap tables

The mean lied. Appending 1,048,576 keys to a flat_map averaged **26 ns per
insert**, which sounds great. The slowest single insert took **about 1.0 ms**
(0.95 to 1.09 ms over 5 runs), which
on a trading host is a missed market. Same container, same run (bench/insert_tail.cpp,
`-DNDEBUG`). This lesson is about the write path: how to build a table
without a quadratic blow-up, where the tail spikes come from, and how to
remove them. One of the fixes needs no heap at all.

Start now (≈1 min): `labs/bin/l3_sequences` is built by `bash labs/build.sh`.
Do not run it yet; there is a prediction card first.

## Pretest — commit your guesses FIRST

### 03-P1 [pretest]
Cold guess. Building a flat_map of 100,000 random keys by calling `insert` once
   per key took X ms. Sorting the keys first and using the `ordered_unique_range`
   constructor took Y ms. Guess X and Y.

Confidence (mark before checking the key): sure / unsure / ___%

### 03-P2 [pretest]
Cold guess. Appending 1,048,576 ascending keys to an empty Boost flat_map
   (no reserve): how many times does it reallocate? About 20, 31, or 1,000?

Confidence (mark before checking the key): sure / unsure / ___%

### 03-P3 [pretest]
Cold guess. A flat_map whose underlying sequence is
   `static_vector<pair<int,int>, 16>` holds 16 entries. What happens on the 17th
   insert, and which `catch` clause sees it: `std::bad_alloc` or something else?

Confidence (mark before checking the key): sure / unsure / ___%

---

## Concept cards

### CONCEPT L3C1  Bulk build: never insert one by one
Each insert into a sorted array shifts on average half of it, so N random
inserts cost O(N²) element moves. Build in bulk instead. (1) Put the pairs in a vector,
sort them, and construct with `flat_map(boost::container::ordered_unique_range, first, last)`.
That constructor is linear and *trusts* you that the input is sorted and
unique. (2) Fill a `FM::sequence_type`, sort it, and
`adopt_sequence(ordered_unique_range, std::move(seq))`, which is O(1) and copies
nothing. (3) Use the plain range constructor `FM(first, last)`, which sorts
for you. Measured, uint64→uint64, p50 (`-DNDEBUG`):

| N | one-by-one insert | sort + ordered_unique_range ctor | sort + adopt_sequence | range ctor (Boost sorts) |
|---|---|---|---|---|
| 10,000 | 6.80 ms | 0.48 ms | 0.47 ms | 0.42 ms |
| 100,000 | 865.1 ms | 6.57 ms | 6.52 ms | 5.63 ms |

If you lie to `ordered_unique_range` with unsorted data you get a broken
table (exactly what the check for 03-Q2 rejects).
Source: https://www.boost.org/doc/libs/1_90_0/doc/html/doxygen/boost_container_header_reference/classboost_1_1container_1_1flat__map.html ; bench/results/median/bulk_build.ndebug.txt

**L3C1 Elaborate:** Why is one-by-one insertion quadratic for a flat_map but only
O(N log N) for std::map?

### CONCEPT L3C2  Growth is a latency spike; reserve removes it (the mean hides it)
When `size() == capacity()`, the next insert allocates a block 60% larger and
moves every element across. Boost's default is growth_factor_60
(boost/container/vector.hpp line 768, options.hpp). Most inserts are cheap, so
the mean barely moves, but each reallocation is one very slow insert. Measured
per single append, 1,048,576 entries, ns (Apple M1 Max, Apple clang 21, `-std=c++23 -O2 -DNDEBUG`, 5 runs, per-cell median; max cells vary a lot: no-reserve 0.95–1.09 ms, reserve(N) 4.5–12.9 µs, so quote the mechanism, not the digit):

| case | p50 | p99 | p99.9 | max | mean |
|---|---|---|---|---|---|
| no reserve (31 reallocations) | 41 | 42 | 208 | 998,875 | 25.9 |
| `reserve(N)` first | 41 | 42 | 42 | 6,125 | 22.5 |

(p50 = 41 is one timer tick, so most inserts are below the clock's resolution.)
The max that remains with reserve, about 6 µs (4.5–12.9 µs over 5 runs), is the OS and the machine, not
the container. Here the Mac was shared with other jobs. On a tuned,
isolated Linux core you would expect less noise. Rule: **size the table at
startup for the session's maximum**, off the hot path.
Source: /opt/homebrew/include/boost/container/vector.hpp (line 768); bench/insert_tail.cpp; bench/results/median/insert_tail.ndebug.txt

**L3C2 Elaborate:** Why did the mean change by only ~3.4 ns while the max changed by ~1 ms?

### CONCEPT L3C3  Write position and batching
The bytes an insert moves depend on where it lands (lesson 1, CONCEPT L1C2). Three practical moves:
**update in place**: `it->second = x` touches one element and moves nothing,
so never erase+reinsert just to change a value. **Batch**: collect a burst of
new keys, sort them, and merge once with
`m.insert(boost::container::ordered_unique_range, first, last)` (Boost: "Complexity: Linear"). Measured,
1,000 new keys into a 100,000-entry table: one by one 17.5 ms, sorted batch
0.66 ms (copy of the table, 0.036 ms, included in both). **Rebuild off the hot
path**: build the next table on another thread or during a quiet period, then
swap a pointer on the hot path, so readers never see the shift.
Source: https://www.boost.org/doc/libs/1_90_0/doc/html/doxygen/boost_container_header_reference/classboost_1_1container_1_1flat__map.html ; bench/results/median/bulk_build.ndebug.txt

**L3C3 Elaborate:** Why is a sorted batch merge so much cheaper than 1,000 single inserts?

### CONCEPT L3C4  Choose the sequence: small_vector and static_vector for zero heap
The fourth template parameter of `boost::container::flat_map` can be a
sequence container instead of an allocator:
`flat_map<K, T, std::less<K>, boost::container::small_vector<std::pair<K,T>, 16>>`.
**small_vector<P, N>** keeps up to N elements inside the object. Beyond N it
moves to the heap and keeps working ("capacity can grow beyond the initial
preallocated capacity"). **static_vector<P, N>** stores elements "within the
object itself" and never allocates. Going past N throws when the default
`throw_on_overflow<true>` option is set. The type thrown is
`boost::container::bad_alloc`, not `std::bad_alloc`, unless you build with
`BOOST_CONTAINER_USE_STD_EXCEPTIONS`. With `throw_on_overflow<false>`,
overflowing is undefined behaviour. Measured in labs/l3_sequences: default
(heap) table: 8 allocations for 16 inserts, sizeof 24. small_vector<16>: 0
allocations for 16, 1 at the 17th, sizeof 152. static_vector<16>: 0
allocations, and the 17th insert throws while size stays 16, sizeof 136. The
inline storage counts toward sizeof, so do not nest big ones carelessly.
Source: https://www.boost.org/doc/libs/1_90_0/doc/html/container/non_standard_containers.html#container.non_standard_containers.small_vector ; https://www.boost.org/doc/libs/1_90_0/doc/html/doxygen/boost_container_header_reference/classboost_1_1container_1_1static__vector.html ; /opt/homebrew/include/boost/container/throw_exception.hpp; labs/l3_sequences.cpp (run)

**L3C4 Elaborate:** For a per-connection table of "orders I sent in the last 100 µs"
(usually ≤ 8, never more than 32 by protocol), which sequence would you choose
and why?

**L3C4 HandsOn:** Predict the allocation counts for Heap/Small/Static after 16 and 17
inserts, then run `labs/bin/l3_sequences`.

---

## Completion cards

### 03-C1 [completion]
Complete the O(1) hand-off: fill and sort `FM::sequence_type seq;` then call
   `m.______(boost::container::______, std::move(seq));`

Confidence (mark before checking the key): sure / unsure / ___%

### 03-C2 [completion]
Complete: a reallocation spike happens when ______ equals ______; Boost's
   default vector grows capacity by ______ %; the fix is ______ at startup.

Confidence (mark before checking the key): sure / unsure / ___%

### 03-C3 [completion]
Complete: past its inline capacity, `small_vector` ______, while
   `static_vector` (default options) ______.

Confidence (mark before checking the key): sure / unsure / ___%

---

## Question cards

### 03-Q1 [recall]
From memory: name three ways to build a Boost flat_map from N unsorted pairs
   without per-element insert, and say what `ordered_unique_range` promises.

Confidence (mark before checking the key): sure / unsure / ___%

### 03-Q2 [apply]
Write `FM build(std::vector<std::pair<uint64_t,uint64_t>> v)` (FM is
   `boost::container::flat_map<uint64_t,uint64_t>`, `bc` is `boost::container`).
   `v` is in random order with unique keys. It must build 100,000 pairs at least 4x
   faster than one-by-one insert timed in the same run. (Compiled, run, and timed.)

Confidence (mark before checking the key): sure / unsure / ___%

### 03-Q3 [recall]
From memory: explain how a flat_map can have a ~26 ns mean insert time and a
   ~1 ms max in the same run, and give the fix.

Confidence (mark before checking the key): sure / unsure / ___%

### 03-Q4 [apply]
Transfer: an end-of-day job applies 50,000 corporate-action updates. 40,000 of
   them change values of existing symbols and 10,000 add new symbols to a 2M-entry
   flat_map. Sketch the fastest correct approach.

Confidence (mark before checking the key): sure / unsure / ___%

### 03-Q5 [apply]
Name a type `T` such that `catch (const T&)` catches the failure of the 17th
   insert into a flat_map backed by `static_vector<std::pair<int,int>, 16>`.
   (Compiled and run against the real throw.)

Hint (optional): it is not in namespace std::... or is it a base?

Confidence (mark before checking the key): sure / unsure / ___%

### 03-Q6 [apply]
Transfer: a feed handler creates one small "pending replace" map per order
   (usually 0–2 entries) and creates/destroys ~1M of them per second. Which
   flat_map sequence do you pick, and what is the trade-off you accept?

Confidence (mark before checking the key): sure / unsure / ___%

### 03-Q7 [recall]
From memory: how do you make a Boost flat_map use `small_vector` or
   `static_vector` storage, and what does each do when it is full?

Confidence (mark before checking the key): sure / unsure / ___%

---

## Integrate

### 03-INT [synthesize]
You own a flat_map on a hot path. List the four write-path rules from this
   lesson in priority order and the failure each prevents.

Confidence (mark before checking the key): sure / unsure / ___%

## lesson-04.md

# Lesson 04 — Pick the right table, then kill a tail spike (capstone)

You now know what a flat_map costs. The professional question is a different
one: *should this table be a flat_map at all?* Sometimes the best data
structure is no data structure. Instrument ids handed out densely by your own
reference-data service are just array indices. Sometimes it is a hash table
with a 9.6 ms rehash hiding in it. Sometimes it is a sorted array laid out in a
different order. Then you will take a realistic order-entry loop, watch one
intraday listing produce a quarter-millisecond spike, and remove it.

Start now (≈1 min): `labs/bin/capstone`. Look at the `max` and the last
histogram rows. Keep that output; you will beat it.

## Pretest — commit your guesses FIRST

### 04-P1 [pretest]
Cold guess. Instrument ids are dense, 0..999,999. Per lookup (p50 of batches),
   how do `table[id]` on a `std::vector`, `flat_map::find` and
   `boost::unordered_flat_map::find` compare?

Confidence (mark before checking the key): sure / unsure / ___%

### 04-P2 [pretest]
Cold guess. Inserting 1,048,576 random keys into an un-reserved
   `boost::unordered_flat_map`: mean insert ~40 ns. What is the max single insert:
   ~1 µs, ~100 µs, or ~10 ms?

Confidence (mark before checking the key): sure / unsure / ___%

### 04-P3 [pretest]
Cold guess. 16M sorted `uint64_t` keys (128 MiB). Binary search on the sorted
   array takes ~500 ns per lookup. Same keys, same comparisons, stored in
   breadth-first (Eytzinger) order with a prefetch: ~450 ns, ~250 ns, or ~100 ns?

Confidence (mark before checking the key): sure / unsure / ___%

---

## Concept cards

### CONCEPT L4C1  The cheapest lookups: a direct index, or a scan of a tiny array
If keys are dense small integers you control (instrument ids, session ids, venue
ids), make them array indices: `table[id]` is one address computation and one
load. A bounds check `id < table.size()` is the only logic. Measured (dense ids,
24-byte records, p50 ns): N=2,000: 0.7 (direct) vs 16.3 (flat_map) vs 2.6
(unordered_flat_map). N=1M: 3.3 vs 115.2 vs 13.7 (per-cell median of 5 runs; flat_map 113–124 over the 5 runs). These are batch averages of
*independent* lookups, so the core overlaps them and the result is throughput.
One isolated lookup is never below a load latency. If ids are sparse, a remap
step (exchange symbol → dense internal id, done once at subscription time)
gets you back to direct indexing. For **tiny** tables, a linear scan is the
other "no structure" option. In this bench a naive scan beat binary search only
with 32-byte keys at N ≤ 16 (lesson 1). A SIMD key-only scan was not measured here.
Source: bench/direct_index.cpp; bench/results/median/direct_index.ndebug.txt

**L4C1 Elaborate:** Why is a dense-id remap at subscription time worth its complexity on a hot path?

### CONCEPT L4C2  Open addressing: boost::unordered_flat_map / unordered_node_map
`boost::unordered_flat_map` (`<boost/unordered/unordered_flat_map.hpp>`) uses
open addressing: "the elements … are held directly in the bucket array". It was
the fastest point lookup at every N in lesson 1's bench. Its costs: **no
order** (no range queries), and "pointer stability is not kept under
rehashing". The bucket array grows on insert once the load would exceed
`max_load_factor()`, and that factor is "managed internally and can't be set by
the user" (the setter "does nothing"). That growth is a rehash spike. Measured,
1M inserts: 17 rehashes, mean 38.5 ns, **max 9.64 ms** (9.35–9.84 over 5 runs). With `reserve(N)` first
the max was about 5 µs (4.0–6.9 µs over 5 runs, noise). `reserve(n)` is defined as `rehash(ceil(n / max_load_factor()))`.
If you need references that survive rehashing, `boost::unordered_node_map`
(`<boost/unordered/unordered_node_map.hpp>`) gives pointer stability "at the
expense of being slower" (Boost's words; not benched here).
Source: https://www.boost.org/doc/libs/1_90_0/libs/unordered/doc/html/unordered/reference/unordered_flat_map.html ; https://www.boost.org/doc/libs/1_90_0/libs/unordered/doc/html/unordered/intro.html ; bench/results/median/insert_tail.ndebug.txt

**L4C2 Elaborate:** Lesson 3's flat_map reallocation and this rehash are the same kind
of tail spike. Name what they share and the shared fix.

### CONCEPT L4C3  Large sorted sets: change the layout (Eytzinger, B-tree)
At millions of keys, a sorted array's binary search jumps across the whole
array, and each of the deeper probes is a likely cache miss whose address
depends on the previous comparison. The **Eytzinger** layout stores the same
keys in breadth-first (heap) order: the children of slot k are 2k and 2k+1. The
next probes are therefore at predictable addresses that can be prefetched
several levels ahead, and the comparison becomes `k = 2k + (b[k] < x)` with no
branch. Khuong & Morin ("Array Layouts for Comparison-Based Searching",
arXiv:1509.05053) found sorted order with a good binary search best for small
arrays and Eytzinger "usually the fastest" for larger ones. B-tree layouts
(many keys per node, one cache line or more) are the other cache-friendly
option and were not benched here. Measured p50 ns, uint64 keys (Apple M1 Max, Apple clang 21, `-O2 -DNDEBUG`, per-cell median of 5 runs after adding a warm-up loop to bench/eytzinger.cpp; N=1M `lower_bound` ranged 86–100 over the 5 runs and `flat_set::find` 92–106, so the first two columns are a tie and the Eytzinger gap is the claim):

| N | std::lower_bound | boost flat_set::find | Eytzinger + prefetch |
|---|---|---|---|
| 4,096 | 16.9 | 18.2 | 11.7 |
| 1,048,576 | 93.8 | 100.9 | 39.7 |
| 16,777,216 | 496.8 | 485.0 | 93.8 |

The cost: the layout is static. An insert means a rebuild, so it suits sets
built once per session.
Source: https://arxiv.org/abs/1509.05053 ; bench/eytzinger.cpp; bench/results/median/eytzinger.ndebug.txt

**L4C3 Elaborate:** Why does Eytzinger help most at 16M keys and least at 4,096?

### CONCEPT L4C4  Honest pros/cons, and when NOT to use a flat_map
| | flat_map (Boost) | std::map | boost::unordered_flat_map | direct array |
|---|---|---|---|---|
| point lookup (N=4,096, measured) | 19.5 ns | 61.2 ns | 3.2 ns | – (dense ids only) |
| ordered iteration / range query | yes, contiguous | yes, pointer chasing | no | by index |
| insert/erase | O(N) moves | O(log N) + 1 allocation | O(1) avg, rehash spike | O(1) |
| stable references | no | yes | no (node_map: yes) | yes (fixed size) |
| heap allocations | per growth (0 with static_vector) | per element | per growth | none after startup |

**Do not use flat_map when:** the table is write-heavy on the hot path
(random-position inserts cost µs at 10k entries); N is large *and* writes are
frequent (shift cost grows linearly with N); you need **stable references or
iterators** across mutations (use a node container, or store keys); or you
only need point lookups (a good hash table won every row). **Use it when** the table is
read-mostly, needs sorted order or ranges, is small-to-medium, and can be built
in bulk and reserved.
Source: CONCEPTS L1C2–L1C3, L2C1, L4C1–L4C3; bench/results/*.ndebug.txt

**L4C4 Elaborate:** A colleague proposes flat_map for a live order book keyed by order id
with ~50k resting orders, inserts and cancels at random ids, plus pointers from a
FIFO queue. List every reason this is a poor fit.

### CONCEPT L4C5  Capstone: an order-entry table pair with a deliberate tail spike
`labs/capstone.cpp` builds the two tables an order-entry path needs: an
instrument-id → `InstrumentConfig` table (dense ids → direct index,
`ConfigTableT`) and a symbol → `RiskLimit` table (sparse 8-byte symbols →
`RiskTableT`, a Boost flat_map). The start-of-day risk table is built with
`adopt_sequence` and then `shrink_to_fit`, so capacity == size. Every 4,000th
order, a "new listing" inserts into it **on the hot path**. Shipped run (100,000
symbols, 400,000 orders; Apple M1 Max, Apple clang 21, `-O2 -DNDEBUG`, 5 runs, median run; the spread is
in bench/results/median/SPREAD.txt): p50 83 ns (identical in all 5 runs), mean 81.2 ns (80.9–81.6),
**max 232 µs** (204–262 µs), 1 hot-path heap
allocation. That is the first listing reallocating 2.4 MB. Your fix goes in
`prepare()` (startup, off the hot path). After `reserve(size + max listings)`
the per-cell median showed max 55.6 µs (54.9–56.1 in 5 runs), 0 allocations, and the same mean (80.1; 79.8–81.7). The
remaining ~55 µs is the **shift**: a middle insert into a 100k array moves
~1.2 MB. Stage 2, `-DSTAGE_LISTINGS`, sends new listings to a small side
table, `RiskTableT<static_vector<…, 100>>`, which needs no heap; the hot path
checks the main table and then the side table, and you merge them off the hot
path. Measured (median of 5, without your reserve): max 9.5 µs (4.7–13.3 µs), at the price of mean 105.0 ns (+~24 ns, about +29%; 103.7–109.7 over 5 runs); p50 stayed 84 ns (one run showed 125).
The cause of that extra mean was not isolated: in this bench every lookup hits the main table (`misses=0`), so the side-table search ran zero times.
In production an order for an intraday listing would also pay a second search when the main table misses.
That is a real trade, so decide it with numbers.
**The memory policy is one template argument.** `ConfigTableT<Alloc>` and
`RiskTableT<AllocOrSeq>` default to Boost's/the standard's allocators. Swapping
in pooled or arena memory, such as the drop-in allocators from
`../advanced_memory`, is a one-line change to the aliases, verified here with the
standard's polymorphic allocator (`labs/bin/capstone_pmr`):
`using RiskTable = RiskTableT<std::pmr::polymorphic_allocator<std::pair<Sym8, RiskLimit>>>;`
Source: labs/capstone.cpp; bench/results/run1..run5/capstone*.txt; bench/results/median/capstone*.txt

**L4C5 Elaborate:** The reserve fix left the mean unchanged and cut the max by ~4×. Why
did the mean not move, and what is the remaining max made of?

**L4C5 HandsOn:** Run `labs/bin/capstone` and then `labs/bin/capstone_staged` (which
already has staging but NOT your reserve). Record max and "hot-path heap
allocations" for both.

---

## Completion cards

### 04-C1 [completion]
Complete: `boost::unordered_flat_map` stores elements ______ in the bucket
   array, so references are invalidated on ______; its max load factor ______
   be set by the user; the fix for the rehash spike is ______.

Confidence (mark before checking the key): sure / unsure / ___%

### 04-C2 [completion]
Complete the Eytzinger descent: children of slot `k` are ______ and ______;
   the branch-free step is `k = ______;`

Confidence (mark before checking the key): sure / unsure / ___%

### 04-C3 [completion]
Complete: in the capstone, `reserve` removed the ______ (and the hot-path
   heap allocation), but not the ______, which staging new listings in a ______
   table removed.

Confidence (mark before checking the key): sure / unsure / ___%

---

## Question cards

### 04-Q1 [recall]
From memory: name the four alternatives to flat_map from this lesson and the
   one condition under which each wins.

Confidence (mark before checking the key): sure / unsure / ___%

### 04-Q2 [apply]
Instrument ids are dense, 0..N-1. Write
   `const Config* find_cfg(const std::vector<Config>& t, uint32_t id)` that returns
   the config or `nullptr` for an out-of-range id, with no search. (Compiled and
   run under ASan.)

Confidence (mark before checking the key): sure / unsure / ___%

### 04-Q3 [recall]
From memory: give four situations in which you should NOT use a flat_map.

Confidence (mark before checking the key): sure / unsure / ___%

### 04-Q4 [apply]
Write the statement(s) to run at startup so that inserting 1,000,000 distinct
   keys into `boost::unordered_flat_map<uint64_t,uint64_t> h` never changes
   `h.bucket_count()`. (Compiled and run.)

Confidence (mark before checking the key): sure / unsure / ___%

### 04-Q5 [apply]
Transfer: a pre-trade check asks "is this account id on the 20M-entry
   restricted list?" per order. The list is published once a day. Choose a layout
   and justify it with mechanism, then name the cost you accept.

Confidence (mark before checking the key): sure / unsure / ___%

### 04-Q6 [apply]
Capstone. Write the body of `prepare(ConfigTable& cfg, RiskTable& risk)` in
   `labs/capstone.cpp` so the hot loop performs **zero** heap allocations.
   Constants available: `kMaxNewListings` (new listings per session). (Your body is
   compiled into the capstone and run; it must print
   `hot-path heap allocations: 0` and `lookups ok`.)

Hint (optional): the risk table is built tight (capacity == size), and up to kMaxNewListings inserts arrive intraday.

Confidence (mark before checking the key): sure / unsure / ___%

### 04-Q7 [apply]
Transfer: your capstone fix made "hot-path allocations: 0" but the max is still
   ~55 µs. A colleague says "reserve more". Explain why that will not help and
   propose two changes that would, with the trade-off of each.

Confidence (mark before checking the key): sure / unsure / ___%

---

## Integrate

### 04-INT [synthesize]
A new hot-path table lands on your desk. Write your decision procedure as
   ordered questions (≤6), each with the structure it leads to.

Confidence (mark before checking the key): sure / unsure / ___%

# Answer keys
Compare only after writing an attempt and marking its confidence.
Sure (or >= 70%) and missed? Star it: re-test it first tomorrow.

## 01-P1
- Measured here (bench/results/median/lookup_crossover.ndebug.txt, N=64, p50),
    fastest to slowest: boost::unordered_flat_map (about 3 ns), std::unordered_map (about 6),
    flat_map (about 8), then linear scan and std::map in a near-tie (about 22 and 25,
    within run-to-run spread; accept either order for those two). Key idea, not the
    digits: hash < sorted array < tree. If you put flat_map first, you are in
    good company. boost::unordered_flat_map won point lookups at every N
    measured. CONCEPT L1C3 shows where flat_map does earn its place.

## 01-P2
- About 5 µs (p50 5,459 ns measured). Every one of the 10,000 pairs (16 bytes
    each) shifts one slot right. A `std::map` insert+erase pair at the same
    size took ~208 ns. CONCEPT L1C2.

## 01-P3
- About 4× (80.2 → 22.0 ns per lookup in labs/l1_ndebug.cpp). Boost.Container
    runs `BOOST_ASSERT` checks inside its iterators, and those are plain
    `assert()` calls unless `NDEBUG` is defined. CONCEPT L1C4.

## L1C1 Elaborate
- Elements sit at fixed offsets in one block, so the next element is the next address and element i is at base + i·sizeof(value_type).
- A new smallest key must go at index 0, so every existing element moves one slot right (and the block may be reallocated).

## L1C1 HandsOn
- Gap is 16 bytes, which is sizeof(std::pair<int,double>). Keys print in sorted order 10, 20, 30, 40 although they were inserted as 40, 10, 30, 20.
- After emplace(5) key 10 sits at offset +16 and its address CHANGED. The std::map nodes show scattered addresses.

## L1C2 Elaborate
- A key larger than all others lands at the end, so zero elements move (amortised constant, plus occasional reallocation).
- A random slot has on average N/2 elements after it, so it moves half as many bytes as a front insert, which moves all N.

## L1C3 Elaborate
- At large N the tree's nodes are scattered across many cache lines and pages, so each of ~20 hops is a likely cache miss with a dependent address. The flat array keeps its top probe levels hot and has no per-node overhead.
- Both still do ~log2(N) dependent probes, while a hash lookup does O(1) expected probes, so a good hash table wins point lookups.

## L1C3 HandsOn
- On the authoring machine that happens by N=16 (7.8 vs 18.2 ns; at N=64 it is 8.5 vs 24.8). Your N may differ. What should carry over is the shape: the gap widens as N grows, and hash tables lead throughout.

## L1C4 Elaborate
- Header-only code is compiled with YOUR flags, so its debug checks run in your build and get measured along with the data structure.
- Without NDEBUG (or BOOST_DISABLE_ASSERTS), BOOST_ASSERT checks execute inside the hot search loop, so the timing includes checks that a production build would remove.

## L1C4 HandsOn
- Authoring run: 80.2 vs 22.0 ns per lookup (≈3.6×). Your ratio may differ, but the assert-on build should be clearly slower.

## 01-C1
- contiguous (vector-like); std::pair<K,T>; sorted; binary search.

## 01-C2
- after (with larger keys than) the slot; sizeof(value_type), i.e. sizeof(std::pair<K,T>).

## 01-C3
- assert(expr); NDEBUG; BOOST_DISABLE_ASSERTS.

## 01-Q1
- One contiguous, sorted sequence of std::pair<K,T>; no nodes or per-element allocations.
- Cheap: ordered iteration over contiguous memory, O(1) nth()/index_of() random access (lookup by binary search is also faster than std::map's tree walk).

## 01-Q2
- std::map does ~log2 N dependent pointer chases to scattered heap nodes (cache misses at large N). flat_map searches one array whose top probe levels stay cache-hot, with no node overhead.
- A hash table does O(1) expected probes rather than ~log2 N; boost::unordered_flat_map won every point-lookup row measured.

## 01-Q3
- 160,000 bytes: all 10,000 elements × 16 bytes shift one slot right.

## 01-Q4
- Yes, plausibly: updates near the back move only the few elements after the slot, so inserts and erases near the best price are cheap.
- With the best price at the front, every new best level shifts the whole side (O(N) bytes per update), which hurts the write path and the tail.

## 01-Q5
- -DNDEBUG (or -DBOOST_DISABLE_ASSERTS, which turns off BOOST_ASSERT only).

## 01-Q6
- flat_map keeps keys sorted, so the range query is lower_bound + a contiguous walk. A hash table has no order and would need a full scan or a second index.
- You give up point-lookup speed: the hash table was ~1.5–14× faster per lookup in the bench (e.g. 3.2 vs 19.5 ns, 6×, at N=4,096; 11.7 vs 162.8 ns, 14×, at N=1M).

## 01-INT
- Property: one contiguous, sorted array of pairs, which gives cache-friendly binary search and linear iteration.
- Same property: inserts and erases shift every element after the slot, and growth reallocates and moves everything (O(N) bytes).
- Build check: compile with -DNDEBUG (or BOOST_DISABLE_ASSERTS), otherwise Boost's BOOST_ASSERT checks run inside the search loop.

## 02-P1
- A reports heap-use-after-free: the array was reallocated and lim points into the freed block.
- B reports nothing and prints limit 2 instead of 2000. lim still points at slot 1, which now holds key 2 after the shift. Silent wrong data is the worse bug. CONCEPT L2C1.

## 02-P2
- Boost: yes. value_type is std::pair<int,int> and the key is not const. std::map: no (pair<const int,int>). std::flat_map: no (the reference is pair<const int&, int&>). CONCEPT L2C2.

## 02-P3
- Two: a keys container and a mapped-values container, kept index-aligned. `auto& p = *it` does NOT compile, because `*it` returns a prvalue proxy `pair<const K&, T&>`. CONCEPT L2C3.

## L2C1 Elaborate
- reserve prevents reallocation, so the old block is not freed and ASan sees a valid address.
- Inserts still shift elements, so the same address now holds a different element: silent wrong data.

## L2C1 HandsOn
- grow: `ERROR: AddressSanitizer: heap-use-after-free … READ of size 8`.
- shift: no ASan error; prints `limit for 200 = 2 (expect 2000)`.
- fixed: prints `limit for 200 = 2000 (expect 2000)`.

## L2C2 Elaborate
- Binary search assumes the whole array is sorted. One out-of-order key sends the comparisons the wrong way, so the search lands in the wrong half and misses keys that are present.

## L2C2 HandsOn
- Prints `99 20 30`, then `find(99) NOT found, find(20) NOT found`.

## L2C3 Elaborate
- No pair<Key,T> object exists in memory: keys and values live in separate containers. So the iterator can only build a temporary pair of references (a proxy) on each dereference.

## L2C4 Elaborate
- Boost: fill a `sequence_type` (reserve it), then `adopt_sequence(ordered_unique_range, std::move(seq))`, or use the `ordered_unique_range` constructor.
- std: fill a keys vector and a values vector, then construct with `std::sorted_unique` or call `replace(std::move(keys), std::move(values))`.

## L2C4 HandsOn
- `boost: size=5 capacity=100`; `extracted 5 pairs, map now size=0`; std prints `after replace size=5, s[0]=99` then `wrote through proxy, s[0]=7`.

## 02-C1
- use-after-free (dangling into a freed block); different element; reallocation (first case).

## 02-C2
- std::pair<const int&, double&>; std::move(s).extract().

## 02-C3
- std::pair<Key,T> with a non-const key; move-assign; movable (preferably nothrow-movable).

## 02-Q1
- Insert/emplace (shift, and reallocation when full), erase (shift of elements at/after the erased key), and reserve/growth (reallocation).
- std::map allocates each element as its own node and never moves it, so references stay valid until that element is erased.

## 02-Q2
- The array is no longer sorted, so binary search returns wrong results, even for keys that were not changed.
- std::map has node handles (extract(key)/insert(node)) and stable references. A flat_map has neither (and needs movable element types).

## 02-Q3
- Boost owns one sequence of pair<K,T>; std adapts two containers (keys, values). Consequence: std's search reads only the keys array, which should mean fewer cache lines per probe. In the bench std::flat_map was ~20–25% faster at 1M with 8-byte keys, but larger values slowed it about as much as Boost (+18% vs +22%), and with 32-byte keys at 1M it was slower, so measure.
- Boost value_type/reference is a real pair (writable key); std's reference is a proxy pair<const K&, T&>. Consequence: no `auto&` to an element, and the key cannot be modified. Also: Boost has reserve/capacity, std's map does not.

## 02-Q4
- Re-find after the mutation: `add_listings(t); return t.find(200)->second.max_qty;` (or copy the value before mutating).
- Holding an iterator instead of a reference is the same bug.

## 02-Q5
- Any insert/erase in the flat_map moves Orders, so the queue's pointers dangle (realloc) or silently point at other orders (shift).
- Fixes: store keys (OrderId) in the queue and re-find; or keep Orders in stable storage (node container / pool / std::deque with no middle erase) and index them; or use a node-based map when pointer stability is required.

## 02-Q6
- `auto c = std::move(s).extract(); for (auto& v : c.values) v = 0.0; s.replace(std::move(c.keys), std::move(c.values));`

## 02-Q7
- std::flat_map (separate keys array): contains() reads only the 16-byte keys, so the search touches fewer cache lines than striding over 272-byte pairs.
- Before committing, measure your actual shapes (the bench showed std::flat_map ~24% ahead at 8B keys / 64B values at 1M, but also ~21% ahead at 8B values, so the bigger value added no extra gain, and a loss at 32B keys / 1M: 464.8 vs 334.0 ns). You could also use a key-only flat_set for membership.

## 02-INT
- Same decision: elements live by value in one array that the container shifts with move-assignment. That move-assignment needs a non-const key, and it moves elements under your references.
- std::flat_map also shifts elements (so references and iterators still dangle), but its proxy reference exposes the key as const, so you cannot break the ordering through an iterator.

## 03-P1
- X ≈ 865 ms, Y ≈ 6.6 ms (bench/results/median/bulk_build.ndebug.txt; one-by-one varied only 863–867 ms over 5 runs). One-by-one is quadratic: going from 10k to 100k keys (10×) made it ~127× slower (6.8 → 865 ms). CONCEPT L3C1.

## 03-P2
- 31 (measured). Boost's vector grows by 60% by default (growth_factor_60), and 1.6^30 ≈ 1.3M. The last of those 31 moves a ~1M-entry array: that is the ~1 ms max. CONCEPT L3C2.

## 03-P3
- It throws, and NOT std::bad_alloc: it is `boost::container::bad_alloc`, which derives from `boost::container::exception` → `std::exception`. `catch (const std::bad_alloc&)` misses it. CONCEPT L3C4.

## L3C1 Elaborate
- flat_map: each insert moves ~N/2 elements on average, so the total is ~N²/4 moves.
- std::map: each insert allocates one node and relinks O(log N) pointers. Nothing else moves.

## L3C2 Elaborate
- Only 31 of ~1M inserts reallocated. Their cost (one ~1 ms copy, many smaller ones) is spread over 1M samples in the mean, but the max reports the single worst insert.

## L3C3 Elaborate
- Each single insert shifts the tail of a 100k array (~50k elements on average), so k inserts cost about k·N/2 moves. The sorted-range insert is documented as "Linear": each existing element moves about once, plus the k log k sort of the batch.

## L3C4 Elaborate
- static_vector<…, 32> if 32 really is a hard protocol bound, because it never touches the heap and overflow is a loud error. Otherwise small_vector<…, 8>, which avoids the heap in the common case and degrades to a heap allocation instead of failing.

## L3C4 HandsOn
- `Heap sizeof=24 allocs after 16: 8 after 17th: 8`; `Small sizeof=152 allocs after 16: 0 after 17th: 1`; `Static sizeof=136 allocs after 16: 0 17th insert threw: "boost::container::bad_alloc thrown" (size still 16)`.

## 03-C1
- adopt_sequence; ordered_unique_range.

## 03-C2
- size(); capacity(); 60 (growth_factor_60); reserve(expected maximum).

## 03-C3
- moves to heap storage and keeps working (one allocation); throws boost::container::bad_alloc and never allocates.

## 03-Q1
- Sort + `flat_map(ordered_unique_range, first, last)`; sort a `sequence_type` + `adopt_sequence(ordered_unique_range, std::move(seq))`; plain range constructor `flat_map(first, last)` (sorts for you). Also `adopt_sequence(std::move(seq))`, which sorts and removes duplicates.
- ordered_unique_range promises the input is already sorted by the comparator and has unique keys. The library does not check it.

## 03-Q2
- e.g. `std::sort(v.begin(), v.end()); return FM(bc::ordered_unique_range, v.begin(), v.end());` (or adopt_sequence, or `FM(v.begin(), v.end())`).
- Passing unsorted data with ordered_unique_range, or inserting one by one (~0.9 s here, no faster than itself), fails.

## 03-Q3
- Growth reallocates and moves the whole array when size()==capacity(). That is rare (31 times for 1M appends), so the mean hides it, but the worst insert copies ~MBs.
- Fix: reserve() the session's maximum size at startup (or build in bulk at the final size).

## 03-Q4
- Update existing entries in place (find + assign it->second; nothing moves).
- Collect the 10,000 new pairs, sort them, and merge once with `insert(ordered_unique_range, first, last)` (or rebuild the sequence and adopt it), never 10,000 single inserts into a 2M array.

## 03-Q5
- `boost::container::bad_alloc` (or its bases `boost::container::exception` / `std::exception`). `std::bad_alloc` does NOT catch it with default Boost settings.

## 03-Q6
- small_vector with a small inline N (e.g. 2–4). The common case makes zero heap allocations, and rare bigger cases still work.
- Trade-off: a larger sizeof per map (inline storage), and the first overflow costs a heap allocation (a latency blip).

## 03-Q7
- Pass the sequence as the 4th template argument: `flat_map<K, T, std::less<K>, small_vector<std::pair<K,T>, N>>` (or static_vector<…, N>).
- small_vector: past N it allocates on the heap and keeps working. static_vector: never allocates; past N it throws boost::container::bad_alloc (default throw_on_overflow<true>; UB if overflow checking is turned off).

## 03-INT
- Build in bulk (sorted + ordered_unique_range / adopt_sequence), which prevents O(N²) startup.
- reserve the session maximum at startup, which prevents reallocation spikes (1 ms max vs 26 ns mean).
- Update in place, append or batch-merge, and avoid front/middle inserts on the hot path, which prevents O(N) shifts per event.
- Use small_vector/static_vector when the size is small and bounded, which prevents heap allocation entirely (catch boost::container::bad_alloc for static_vector overflow).

## 04-P1
- Measured at N=1M: direct index about 3.3 ns, unordered_flat_map about 14 ns, flat_map about 115 ns, i.e. direct is ~4x faster than the hash and ~35x faster than flat_map (bench/results/median/direct_index.ndebug.txt; flat_map ranged 113–124 over 5 runs). Dense keys are an array index. Do not search for what you can compute. CONCEPT L4C1.

## 04-P2
- ~10 ms (about 9.6 ms measured, 9.35–9.84 over 5 runs; 17 rehashes). Each rehash moves every element into a new bucket array. With `reserve(N)` first, the max was only ~5 µs (4.0–6.9 µs over 5 runs; noise, no rehash). CONCEPT L4C2.

## 04-P3
- ~100 ns (about 94 vs 497 ns measured: ~5.3x; 5-run ranges 94–97 and 491–511). The layout turns scattered probes into predictable, prefetchable ones. CONCEPT L4C3.

## L4C1 Elaborate
- It moves the search (hash or binary) off the hot path, where it runs once per symbol, and leaves one bounds-checked array load per order on the hot path.

## L4C2 Elaborate
- Both are rare whole-container moves triggered by growth (size reaching capacity or the max load), which the mean hides and the max exposes.
- Shared fix: size for the session's maximum at startup with reserve(), so growth never happens on the hot path.

## L4C3 Elaborate
- At 4,096 keys (32 KiB) everything fits in L1/L2, so there are few misses to hide. At 16M keys (128 MiB) nearly every deep probe misses cache, and Eytzinger's prefetchable, branch-free path overlaps those misses.

## L4C4 Elaborate
- Random-position inserts and erases on ~50k entries shift ~25k elements per event (µs each, worse than std::map's ~200 ns at 10k).
- The FIFO's pointers dangle on every insert/erase (no reference stability).
- Order-id lookups need no ordering, so a hash table (or a dense-id slot array) fits better.

## L4C5 Elaborate
- Only one order in 400,000 hit the reallocation, so it barely affects the mean. The max is exactly that one event.
- The remaining ~55 µs is the O(N) shift of a middle insert into the 100k-entry array (plus OS noise), which reserve cannot remove.

## L4C5 HandsOn
- capstone: max in the hundreds of µs and 1 hot-path allocation (median 232 µs). capstone_staged without reserve: a much smaller max (about 5–13 µs) and 0 allocations, because the main table is never inserted into. Your exact numbers will differ.

## 04-C1
- directly; rehash (growth); cannot; reserve(expected max) before the hot path.

## 04-C2
- 2k; 2k+1; `2*k + (b[k] < x)`.

## 04-C3
- reallocation spike; O(N) shift of the middle insert; small side (static_vector-backed) table.

## 04-Q1
- Direct-indexed array: keys are dense small integers. Linear scan over a tiny array: very small N (here only 32-byte keys, N ≤ 16).
- boost::unordered_flat_map: point lookups only, no order needed (fastest at every N measured; reserve to avoid rehash spikes); unordered_node_map if references must survive rehashing.
- Eytzinger (or B-tree) layout: large, mostly static sorted sets (≈5× faster than binary search at 16M keys here).

## 04-Q2
- `return id < t.size() ? &t[id] : nullptr;`

## 04-Q3
- Write-heavy hot path, especially with random-position inserts and erases (O(N) shifts).
- Large N combined with frequent writes.
- Stable references, pointers or iterators are needed across mutations.
- Only point lookups are needed, with no order or ranges (a hash table or direct index is faster).

## 04-Q4
- `h.reserve(1000000);` — reserve takes an element count. Calling `max_load_factor(…)` does nothing for this container.

## 04-Q5
- A static Eytzinger (or B-tree) layout, or a hash set: built once per day off the hot path. Eytzinger turns ~24 dependent probes (most of them cache misses at this size) into prefetchable, branch-free probes (≈5× faster than binary search at 16M here). A hash set would avoid the log N probes entirely if no order is needed.
- Cost: rebuild on every update (the layout is static), plus extra code to maintain.

## 04-Q6
- `risk.reserve(risk.size() + kMaxNewListings);` — any reserve with room for all intraday listings. `risk.reserve(risk.size())` or touching only `cfg` still allocates on the hot path.

## 04-Q7
- The remaining spike is the O(N) shift of a middle insert (~1.2 MB moved), not allocation. More capacity does not change how many elements move.
- Fixes: stage new listings in a small side table and merge off the hot path (measured max ~9.5 µs, 5–13 over 5 runs; mean +~24 ns), or rebuild the table off-thread and swap a pointer. Or choose a structure with O(1)/O(log N) insert for this table (hash map, node map), giving up order or cache density.

## 04-INT
- Are keys dense ints I control (or can I remap them once)? → direct array.
- Do I only need point lookups? → boost::unordered_flat_map, reserved (unordered_node_map if references must be stable).
- Do I need order or ranges, with read-mostly use and small/medium N? → flat_map (bulk-built, reserved, -DNDEBUG; small_vector/static_vector if tiny and bounded).
- Is the set huge and static? → Eytzinger/B-tree layout.
- Are writes frequent at random positions, or are stable references needed? → not flat_map (node container or hash).
- Always: measure p50/p99/p99.9/max on the target box, and put the allocator in a template parameter so memory policy is a one-line swap.
