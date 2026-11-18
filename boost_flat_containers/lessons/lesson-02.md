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

### CARD 02-P1  [type: pretest]  [topic: invalidation]  [box: 1]
Q: Cold guess. `Limit& lim = t.find(200)->second;` then 40 inserts of smaller keys,
   then you read `lim.max_qty`. Run A: `t` had no reserve. Run B: `t.reserve(64)`
   was called first, so it never reallocates. Under AddressSanitizer, which run
   reports an error, and what does the other one print?
Key:
  - A reports heap-use-after-free: the array was reallocated and lim points into the freed block.
  - B reports nothing and prints limit 2 instead of 2000. lim still points at slot 1, which now holds key 2 after the shift. Silent wrong data is the worse bug. CONCEPT L2C1.
Concepts: L2C1
Source: labs/l2_dangling.cpp (run with ASan)

### CARD 02-P2  [type: pretest]  [topic: mutable-key]  [box: 1]
Q: Cold guess. Does `m.find(10)->first = 99;` compile for
   `boost::container::flat_map<int,int>`? For `std::map<int,int>`? For
   `std::flat_map<int,int>`?
Key:
  - Boost: yes. value_type is std::pair<int,int> and the key is not const. std::map: no (pair<const int,int>). std::flat_map: no (the reference is pair<const int&, int&>). CONCEPT L2C2.
Concepts: L2C2
Source: labs/l2_mutable_key.cpp; compile errors observed with Apple clang 21 / libc++

### CARD 02-P3  [type: pretest]  [topic: std-flat]  [box: 1]
Q: Cold guess. C++23 `std::flat_map<K,T>`: how many underlying containers does it
   hold, and does `auto& p = *m.begin();` compile?
Key:
  - Two: a keys container and a mapped-values container, kept index-aligned. `auto& p = *it` does NOT compile, because `*it` returns a prvalue proxy `pair<const K&, T&>`. CONCEPT L2C3.
Concepts: L2C3
Source: https://en.cppreference.com/w/cpp/container/flat_map ; labs/l2_std_vs_boost.cpp

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
Elaborate: Why does `reserve()` make the ASan error disappear but not the bug?
ElaborateKey:
  - reserve prevents reallocation, so the old block is not freed and ASan sees a valid address.
  - Inserts still shift elements, so the same address now holds a different element: silent wrong data.
HandsOn: Run `labs/bin/l2_dangling grow`, then `shift`, then `fixed`. Write down
what each one prints (look for `ERROR: AddressSanitizer` and the `limit for 200` line).
HandsOnKey:
  - grow: `ERROR: AddressSanitizer: heap-use-after-free … READ of size 8`.
  - shift: no ASan error; prints `limit for 200 = 2 (expect 2000)`.
  - fixed: prints `limit for 200 = 2000 (expect 2000)`.
Source: https://www.boost.org/doc/libs/1_90_0/doc/html/container/non_standard_containers.html#container.non_standard_containers.flat_xxx ; https://www.boost.org/doc/libs/1_90_0/doc/html/doxygen/boost_container_header_reference/classboost_1_1container_1_1flat__map.html ; labs/l2_dangling.cpp (run)

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
Elaborate: Why does changing one key through an iterator make lookups of *other*
keys fail?
ElaborateKey:
  - Binary search assumes the whole array is sorted. One out-of-order key sends the comparisons the wrong way, so the search lands in the wrong half and misses keys that are present.
HandsOn: Predict, then run `labs/bin/l2_mutable_key`: what order do the keys print
in, and are 99 and 20 found?
HandsOnKey:
  - Prints `99 20 30`, then `find(99) NOT found, find(20) NOT found`.
Source: https://www.boost.org/doc/libs/1_90_0/doc/html/doxygen/boost_container_header_reference/classboost_1_1container_1_1flat__map.html ; https://www.boost.org/doc/libs/1_90_0/doc/html/container/non_standard_containers.html#container.non_standard_containers.flat_xxx ; labs/l2_mutable_key.cpp (run); tests/compile_claims.sh (run; exact diagnostics recorded in bench/results/probes/compile_claims.txt)

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
Elaborate: Why can't `std::flat_map` hand you a `T&` inside a real `std::pair`?
ElaborateKey:
  - No pair<Key,T> object exists in memory: keys and values live in separate containers. So the iterator can only build a temporary pair of references (a proxy) on each dereference.
Source: https://en.cppreference.com/w/cpp/container/flat_map ; bench/results/median/lookup_crossover.ndebug.txt; labs/l2_std_vs_boost.cpp (run)

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
Elaborate: You must rebuild a 50k-entry table every morning from a sorted file.
Which calls do you use in each library to avoid per-element inserts?
ElaborateKey:
  - Boost: fill a `sequence_type` (reserve it), then `adopt_sequence(ordered_unique_range, std::move(seq))`, or use the `ordered_unique_range` constructor.
  - std: fill a keys vector and a values vector, then construct with `std::sorted_unique` or call `replace(std::move(keys), std::move(values))`.
HandsOn: Run `labs/bin/l2_std_vs_boost` and note the `capacity=` value Boost prints after `reserve(100)` and the `s[0]` values std prints.
HandsOnKey:
  - `boost: size=5 capacity=100`; `extracted 5 pairs, map now size=0`; std prints `after replace size=5, s[0]=99` then `wrote through proxy, s[0]=7`.
Source: https://www.boost.org/doc/libs/1_90_0/doc/html/doxygen/boost_container_header_reference/classboost_1_1container_1_1flat__map.html ; https://en.cppreference.com/w/cpp/container/flat_map/extract ; labs/l2_std_vs_boost.cpp (run)

---

## Completion cards

### CARD 02-C1  [type: completion]  [topic: invalidation]  [box: 1]
Q: Complete: a reallocation turns a kept reference into a ______ (which ASan
   reports). A shift without reallocation makes it point at a ______ (which
   ASan does not report). `reserve` prevents only the ______.
Key:
  - use-after-free (dangling into a freed block); different element; reallocation (first case).
Concepts: L2C1
Source: CONCEPT L2C1

### CARD 02-C2  [type: completion]  [topic: std-flat]  [box: 1]
Q: Complete: `std::flat_map<int,double>::reference` is ______, and to take the
   containers out of an lvalue `s` you write ______.
Key:
  - std::pair<const int&, double&>; std::move(s).extract().
Concepts: L2C3, L2C4
Source: CONCEPTS L2C3–L2C4

### CARD 02-C3  [type: completion]  [topic: mutable-key]  [box: 1]
Q: Complete: Boost flat_map's `value_type` is ______ (key constness?), which lets
   the container ______ elements when it shifts them, and which means element
   types must be ______.
Key:
  - std::pair<Key,T> with a non-const key; move-assign; movable (preferably nothrow-movable).
Concepts: L2C2
Source: CONCEPT L2C2

---

## Question cards

### CARD 02-Q1  [type: recall]  [topic: invalidation]  [box: 1]
Q: From memory: which operations on a Boost flat_map can invalidate pointers,
   references and iterators to its elements, and why does std::map not have this
   problem?
Key:
  - Insert/emplace (shift, and reallocation when full), erase (shift of elements at/after the erased key), and reserve/growth (reallocation).
  - std::map allocates each element as its own node and never moves it, so references stay valid until that element is erased.
Concepts: L2C1
Source: CONCEPT L2C1

### CARD 02-Q2  [type: recall]  [topic: mutable-key]  [box: 1]
Q: From memory: what goes wrong after `it->first = 99` on a Boost flat_map, and
   name one other thing std::map can do that a flat_map cannot.
Key:
  - The array is no longer sorted, so binary search returns wrong results, even for keys that were not changed.
  - std::map has node handles (extract(key)/insert(node)) and stable references. A flat_map has neither (and needs movable element types).
Concepts: L2C2
Source: CONCEPT L2C2

### CARD 02-Q3  [type: recall]  [topic: std-flat]  [box: 1]
Q: From memory: state two structural differences between Boost flat_map and
   C++23 std::flat_map, and one consequence of each.
Key:
  - Boost owns one sequence of pair<K,T>; std adapts two containers (keys, values). Consequence: std's search reads only the keys array, which should mean fewer cache lines per probe. In the bench std::flat_map was ~20–25% faster at 1M with 8-byte keys, but larger values slowed it about as much as Boost (+18% vs +22%), and with 32-byte keys at 1M it was slower, so measure.
  - Boost value_type/reference is a real pair (writable key); std's reference is a proxy pair<const K&, T&>. Consequence: no `auto&` to an element, and the key cannot be modified. Also: Boost has reserve/capacity, std's map does not.
Concepts: L2C3
Source: CONCEPT L2C3

### CARD 02-Q4  [type: apply]  [topic: invalidation]  [box: 1]
Q: Repair this function so it returns the limit for id 200 *after* the listings
   are added. `add_listings(t)` inserts 40 smaller keys; `Limit` has `long max_qty`.
   Write the whole function. It is compiled and run under AddressSanitizer.
   ```cpp
   long limit_after_listing(RiskTable& t) {
     Limit& l = t.find(200)->second;
     add_listings(t);
     return l.max_qty;
   }
   ```
Hint: keep the key, not the reference.
Key:
  - Re-find after the mutation: `add_listings(t); return t.find(200)->second.max_qty;` (or copy the value before mutating).
  - Holding an iterator instead of a reference is the same bug.
AcceptIf: check(checks/l2-dangling.check.sh)
Concepts: L2C1
Source: CONCEPT L2C1

### CARD 02-Q5  [type: apply]  [topic: invalidation]  [box: 1]
Q: Transfer: an order book stores `flat_map<OrderId, Order>` and a separate
   `std::vector<Order*>` "queue" pointing into it for time priority. What breaks,
   when, and give two designs that fix it.
Key:
  - Any insert/erase in the flat_map moves Orders, so the queue's pointers dangle (realloc) or silently point at other orders (shift).
  - Fixes: store keys (OrderId) in the queue and re-find; or keep Orders in stable storage (node container / pool / std::deque with no middle erase) and index them; or use a node-based map when pointer stability is required.
Concepts: L2C1
Source: CONCEPT L2C1

### CARD 02-Q6  [type: apply]  [topic: std-flat]  [box: 1]
Q: `std::flat_map<int,double> s` is already filled. Write statements that take its
   two containers out, set every mapped value to 0.0, and put them back, with no
   copying of the vectors. (Compiled and run.)
Key:
  - `auto c = std::move(s).extract(); for (auto& v : c.values) v = 0.0; s.replace(std::move(c.keys), std::move(c.values));`
AcceptIf: check(checks/l2-std-extract.check.sh)
Concepts: L2C4
Source: CONCEPT L2C4

### CARD 02-Q7  [type: apply]  [topic: std-flat]  [box: 1]
Q: Transfer: a symbol table maps a 16-byte symbol to a 256-byte static-data record,
   and the hot path only asks "is this symbol known?" (`contains`). Which flat
   layout is the better bet for that query, and what should you do before
   committing to it?
Key:
  - std::flat_map (separate keys array): contains() reads only the 16-byte keys, so the search touches fewer cache lines than striding over 272-byte pairs.
  - Before committing, measure your actual shapes (the bench showed std::flat_map ~24% ahead at 8B keys / 64B values at 1M, but also ~21% ahead at 8B values, so the bigger value added no extra gain, and a loss at 32B keys / 1M: 464.8 vs 334.0 ns). You could also use a key-only flat_set for membership.
Concepts: L2C3
Source: CONCEPT L2C3

---

## Integrate

### CARD 02-INT  [type: synthesize]  [topic: all]  [box: 1]
Q: One paragraph: explain why both "references dangle" and "the key is writable"
   follow from the same design decision in Boost flat_map, and how std::flat_map's
   two-container design changes each of them.
Key:
  - Same decision: elements live by value in one array that the container shifts with move-assignment. That move-assignment needs a non-const key, and it moves elements under your references.
  - std::flat_map also shifts elements (so references and iterators still dangle), but its proxy reference exposes the key as const, so you cannot break the ordering through an iterator.
Concepts: L2C1, L2C2, L2C3
Source: CONCEPTS L2C1–L2C3
