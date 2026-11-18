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

# Lesson 01 — Three maps, one lie

**Time: about 25 min. Environment for every output in this lesson:** Apple M1 Max,
Apple clang 21, `clang++ -std=c++23 -O2` (the ASan run uses `-O1 -g -fsanitize=address`),
Boost 1.90, libc++. The allocation counts, sizes and byte counts here are
deterministic, so they should repeat on your box. Pointer-and-freed-memory
behaviour is not: see M1-P2. Timing numbers start in Lesson 02.

Your order handler has to find the same order three ways: by **order id** (a
cancel arrives), by **(side, price, time)** (what is best on the book?), and by
**client** (risk wants everything client 7 has open). The usual move is three
std containers glued together with pointers. It works on day one. Then someone
edits `cancel()` on a Friday afternoon.

In this lesson you will break that hand-rolled store on purpose, watch
AddressSanitizer catch it, then rebuild it as one Boost.MultiIndex container
and count what that saves.

Fair warning: you will be asked to guess before you are told, and to answer
from memory before you see the key. That feels slower than reading. The extra
effort is the method working, not a sign you are doing badly.

**Do this now (2 min):** from the tutorial directory run `bash labs/build.sh`.
Everything compiles into `build/`.

## Pretest — commit your guesses FIRST (no fair reading ahead)

### M1-P1 [pretest]
Cold guess. The hand-rolled store keeps `unordered_map<id, Order>` plus
   `map<(side,price,time), Order*>` plus `unordered_multimap<client, Order*>`,
   all buckets pre-reserved. How many heap allocations does one `add()` cost?
   And the same store as a single multi_index_container with three indices?

Confidence (mark before checking the key): sure / unsure / ___%

### M1-P2 [pretest]
Cold guess. `cancel()` erases the order from the id map and the price map
   but forgets the client multimap. Later you sum `qty` over client 0's
   entries. What happens: compile error, crash, exception, or a normal-looking
   number?

Confidence (mark before checking the key): sure / unsure / ___%

---

## Concept cards

### CONCEPT M1C1  One element, many access paths, kept in sync by hand
The hand-rolled store owns each `Order` in one container, and the other two
hold `Order*` pointers into it. (That only works because `unordered_map` never
moves its nodes. With `vector` it would already be broken.) Every mutation now
has to touch all three containers, in the right order, every time. There are
three failure modes:
**forgotten index** (cancel skips one container, which now holds a dangling
pointer), **stale key** (you change `o.price` through the id map, so the price
map still files the order under the old price), and **partial failure** (the
second `emplace` throws, leaving the first insert behind). The compiler
catches none of them.
Figure: one `Order` box owned by `by_id`, with two arrows into it from
`by_price` and `by_client`; erase the box and the arrows still point there.
Source: labs/l1_handrolled.cpp (run this session); Boost.MultiIndex tutorial "secondary data structure of pointers ... annoying burden of keeping the two structures in sync" https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/tutorial/basics.html

**M1C1 Elaborate:** Why does the plain build happily print a number instead of
crashing?

**M1C1 HandsOn:** Run `./build/l1_handrolled bug` and note the sizes line. Then
`./build/l1_handrolled stale`. Last, build with ASan:
`clang++ -std=c++23 -O1 -g -fsanitize=address -I/opt/homebrew/include labs/l1_handrolled.cpp -o build/l1_asan && ./build/l1_asan bug`.

### CONCEPT M1C2  One node per element, whatever the index count
A `multi_index_container` with N indices allocates **one node per element**.
That node holds the value plus N small index headers (tree links, hash links,
...). The hand-rolled store allocates one node **per container per element**,
so it pays three allocations and three frees for every order. The real cost
of an allocation on the hot path is the tail, not the mean (Lesson 03 shows
this). Fewer allocations means fewer chances of a slow path.
Measured here: hand-rolled **3 allocations per add**, multi_index **1 per
insert**, 0 per erase for both. Bytes per element with these three access
paths: hand-rolled 176.5 B, multi_index 127.5 B (`build/l3_node_size`,
N=100000).
Source: labs/l1_handrolled.cpp, labs/l1_mi.cpp, labs/l3_node_size.cpp (run this session); Boost.MultiIndex performance page "Nodes of a multi_index_container with N indices hold the value of the element plus N headers ... the manual simulation allocates N nodes per element" https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/performance.html

**M1C2 HandsOn:** Run `./build/l1_handrolled` and `./build/l1_mi` side by side.

### CONCEPT M1C3  Declaring it: `indexed_by<...>` + an index type + a key extractor
```cpp
using OrderStore = bmi::multi_index_container<
    Order,                                                     // the element
    bmi::indexed_by<
        bmi::hashed_unique<bmi::member<Order, std::uint64_t, &Order::id>>,          // index 0
        bmi::ordered_non_unique<bmi::member<Order, std::int64_t, &Order::price>>>>; // index 1
```
Each index specifier has the form *structure*_*uniqueness*: `hashed_` behaves
like `unordered_set`, `ordered_` like `set`. The `_unique` variants reject
duplicates and the `_non_unique` variants accept them. The argument is a
**key extractor**: `member<Class, Type, &Class::field>` reads one data member.
Elements are `const` through every index, so you can never assign to a key
behind the container's back. (Lesson 02 covers how you change them.)
Source: https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/tutorial/basics.html ; https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/key_extraction.html ; labs/l1_mi.cpp (compiled this session)

**M1C3 Elaborate:** Why does making elements `const` kill the "stale key" bug from M1C1?

### CONCEPT M1C4  Tags and `get<>()`: name your indices, erase once
`s.get<0>()` returns a reference to index 0, which behaves like a std
container. Numbers rot when someone reorders the list, so give each index a
**tag**: `struct ById {};`, then `hashed_unique<bmi::tag<ById>, ...>`, then
`s.get<ById>()`. Tags and numbers both work. **Erasing through any index
removes the element from all of them.** That one fact deletes the
"forgotten index" bug.
Source: https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/tutorial/basics.html ; labs/l1_mi.cpp (run this session)

**M1C4 HandsOn:** In `labs/l1_mi.cpp`, find the erase loop. Which index does it erase
through, and what are the three sizes afterwards?

### CONCEPT M1C5  `composite_key` for (side, price, time), and partial lookups
Price-time priority is a three-part key. Write it as
`composite_key<Order, member<..side>, member<..price>, member<..ts>>` inside an
`ordered_non_unique`. Lookups take a `std::tuple`, and a **shorter tuple is a
prefix lookup**: `book.equal_range(std::make_tuple('B'))` gives every bid in
key order. Bids want the *highest* price first, so the lab sorts on a computed
`rank()` (minus price for bids, price for asks). Lesson 02 shows that
extractor.
Source: https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/key_extraction.html (composite_key comparison "takes the minimum length of the operands") ; labs/l2_keys.cpp (run this session)

**M1C5 HandsOn:** Run `./build/l2_keys` and read the `best bids:` line.

---

## Completion cards

### M1-C1 [completion]
Fill the blanks. A multi_index_container with 3 indices allocates ____
   node(s) per element. Each node holds the value plus ____ index headers.

Confidence (mark before checking the key): sure / unsure / ___%

### M1-C2 [completion]
Complete the declaration: ids unique and hashed, prices repeatable and
   sorted. Write the two index specifiers that go in `indexed_by<____, ____>`,
   using `member<>` or `key<>`.

Confidence (mark before checking the key): sure / unsure / ___%

---

## Question cards

### M1-Q1 [recall]
Name the three ways a hand-rolled "one owner map + two maps of pointers"
   order store goes out of sync, and say which multi_index property removes
   each one.

Confidence (mark before checking the key): sure / unsure / ___%

### M1-Q2 [recall]
Write a complete `using Store = ...;` for `struct Order { std::uint64_t id;
   std::int64_t price; std::uint32_t qty; };` where index 0 is hashed and
   unique on `id` and index 1 is ordered and non-unique on `price`. The
   harness already has `using namespace boost::multi_index;`. Paste it on one
   line or inside a ``` block.

Confidence (mark before checking the key): sure / unsure / ___%

### M1-Q3 [apply]
New domain: a market-data **instrument table**.
   `struct Instrument { std::string symbol; int exchange; std::uint32_t
   local_id; double tick; };` must be found by `symbol` (unique, hashed) and by
   `(exchange, local_id)` (unique as a pair, ordered, so that "all instruments
   of exchange 1" is a range). Write `using Instruments = ...;`.

Hint (optional): the second index needs a composite key; a one-element tuple then does
   the prefix lookup.

Confidence (mark before checking the key): sure / unsure / ___%

### M1-Q4 [apply]
A colleague's position-keeper has 4 access paths built from 4 std node
   containers (one owning, three holding pointers). At 2 million inserts per
   second, how many allocator calls per second does insertion cost, and how
   many with a 4-index multi_index_container? Assume buckets are pre-reserved.

Confidence (mark before checking the key): sure / unsure / ___%

### M1-Q5 [recall]
With an ordered composite index on (side, rank, ts), which single call
   returns every bid in price-time priority, and why does a one-element tuple
   work there?

Confidence (mark before checking the key): sure / unsure / ___%

---

## Integrate (day close)

### M1-INT [synthesize]
In three sentences, explain to a teammate why the three-container order
   store was dangerous and what changes when you switch to one
   multi_index_container. Use the words "node", "const" and "erase".

Confidence (mark before checking the key): sure / unsure / ___%

## lesson-02.md

# Lesson 02 — Keys, projections, and changing an element safely

**Time: about 25 min. Environment for every number in this lesson:** Apple M1 Max,
Apple clang 21, `clang++ -std=c++23 -O2`, Boost 1.90, `steady_clock` (tick 41.67
ns, so tiny values are tick multiples). Each benchmark was run 5 times on a
machine shared with other jobs; tables show the **median of the 5 runs, with
the min..max in brackets**. The spread is large (up to about 3x between runs
at 64 clients), so learn the **ratios and the mechanism, not the nanoseconds**.
Raw runs: `bench/results/`. An isolated x86-64 Linux core will differ again.

A client sends a cancel/replace that reuses a client-order-id it already has
live. Your store has a unique index on (client, cl_ord_id). You apply the new
id to the stored order. The call returns `false`. You log a warning and move
on. An hour later risk reports that order as missing, and nobody sent a
cancel. Where did it go?

The scenario is hypothetical. The behaviour is not: it is documented, and you
will predict it first, then reproduce it in under a minute.

**Do this now (1 min):** `./build/l4_modify`. Don't read the output yet;
answer the pretest first.

## Pretest — commit your guesses FIRST

### M2-P1 [pretest]
Cold guess. Index 1 is `ordered_unique` on (client, cl_ord_id). You call
   `by_id.modify(it, [](Order& o){ o.cl_ord_id = 100; })`, and (7,100) already
   belongs to another order. What happens to the order you modified?

Confidence (mark before checking the key): sure / unsure / ___%

### M2-P2 [pretest]
Cold guess. 100,000 orders, 64 clients, and the store has a
   `hashed_non_unique` index on client. Changing just `qty` (no index uses
   it) with `modify()`: is it about the cost of a `find`, or 100× slower?

Confidence (mark before checking the key): sure / unsure / ___%

---

## Concept cards

### CONCEPT M2C1  Computed keys: `const_mem_fun`, and the C++17 `key<>` shorthand
A key does not have to be a stored field. `const_mem_fun<Order, std::int64_t,
&Order::rank>` calls a const member function. Here `rank()` returns `-price`
for bids and `price` for asks, so "smaller is better" holds for both sides and
one ascending index serves both books.
In C++17 and later, `#include <boost/multi_index/key.hpp>` lets you write
`key<&Order::id>`, which **is** `member<Order, std::uint64_t, &Order::id>`
(the lab static_asserts this). `key<&Order::rank>` is the const_mem_fun, and
`key<&Order::side, &Order::rank, &Order::ts>` is the composite_key.
Source: https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/key_extraction.html ; labs/l2_keys.cpp (compiled and run this session)

**M2C1 HandsOn:** Open `labs/l2_keys.cpp`. Compare the `LongHand` and `ShortHand`
types, then run `./build/l2_keys`.

### CONCEPT M2C2  `project<>`: jump from one index to another, same element
You found an order by id, and now you want its place in the book. Searching
the book again would cost another O(log n). `s.project<ByBook>(it_id)` turns
the hashed iterator into a book iterator **for the same node**, in constant
time. From there you can walk neighbours, for example to count orders ahead
of you at your price level (your queue position).
Source: https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/tutorial/basics.html (projection) ; labs/l2_keys.cpp (run this session)

**M2C2 HandsOn:** In `./build/l2_keys` output, read the `order #4 has ...` line.

### CONCEPT M2C3  Changing an element: `modify(it, f)` vs `replace(it, x)`
Elements are const, so you change them through the container.
`replace(it, x)` assigns a whole new value. If the new value would break any
unique index, it does **nothing** and returns false (strong guarantee).
`modify(it, f)` runs your functor **on the element in place** (no copy), then
re-files it in every index and returns whether that worked. Both keep `it`
valid on success.
Source: https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/ord_indices.html (modify, replace: Effects, Exception safety) ; labs/l4_modify.cpp (run this session)

**M2C3 Elaborate:** Why can't `modify` simply "refuse" the way `replace` does?

### CONCEPT M2C4  The collision rule: modify ERASES unless you give a rollback
If the modified element collides in a unique index, plain `modify` **erases
it** and returns false. The three-argument `modify(it, f, rollback)` calls
`rollback(e)` on failure. If that restores a value that fits all indices, the
element is kept. Capture the old field value **before** modifying.
Source: https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/ord_indices.html ("If the rearrangement fails, back(e) is invoked ... the element is kept, otherwise it is erased.") ; labs/l4_modify.cpp (run this session)

**M2C4 HandsOn:** Run `./build/l4_modify` and read steps 2, 3 and 4.

### CONCEPT M2C5  The hidden cost: modify on a `hashed_non_unique` index walks the key group
After `modify`, a hashed index cannot know whether *its* key changed, because
the old value is gone. So it checks whether the element is still in the right
place (`in_place()` in `boost/multi_index/hashed_index.hpp`, Boost 1.90). For
`hashed_non_unique` that check walks the bucket and, if the element's
equal-key group is long, steps through the group node by node until it
finds the element. It can return early (for example when the element is the
first node of a group of 3 or more), so the cost is **up to O(group size)**.
With 64 clients and 100,000 orders a group is about 1,562 orders, and the
measured cost of `modify` grows with orders-per-client even for a `qty`
change no index uses. (This is consistent with the code, but I did not
profile it to attribute the time.) `replace` compares old and new keys
first and skips the unchanged index entirely.
Measured (`build/bench_modify`, N=100,000 orders, ns per op including the
`find`, **p50 as median of 5 runs [min..max]**):

| orders per client | modify qty | replace qty | modify price | replace price |
|---|---|---|---|---|
| 1 | 96 [78..130] | 83 [76..99] | 406 [276..430] | 398 [271..425] |
| 100 | 1,172 [1,112..1,260] | 81 [68..83] | 1,643 [1,508..1,820] | 406 [273..430] |
| 1,562 | 14,240 [6,794..18,620] | 86 [49..91] | 17,396 [6,651..24,388] | 266 [258..417] |

Ratios that held in **every** run: at 1 order per client modify is about the
same as replace; at 100 per client modify qty is 14x to 17x replace qty; at
1,562 per client modify qty is 137x to 284x replace qty, and modify price is
25x to 95x replace price.

Rule: on a store with a big non-unique hashed index, change elements with
`replace`, or make that index `ordered_non_unique` (its position check only
compares the two neighbours).
Source: bench/bench_modify.cpp (run 5 times this session; bench/results/) ; /opt/homebrew/include/boost/multi_index/hashed_index.hpp `in_place(..., hashed_non_unique_tag)` and `replace_` (read this session) ; /opt/homebrew/include/boost/multi_index/detail/ord_index_impl.hpp `in_place(..., ordered_non_unique_tag)`

**M2C5 HandsOn:** Run `./build/bench_modify` (about 8 s; your numbers will differ, the pattern will not). Find the "64 clients" block.

---

## Completion cards

### M2-C1 [completion]
Fill the blanks. On a unique-index collision, `replace` leaves the element
   ______ and returns false; plain `modify` ______ the element and returns
   false.

Confidence (mark before checking the key): sure / unsure / ___%

### M2-C2 [completion]
Complete the shorthand. With C++17 and `<boost/multi_index/key.hpp>`,
   `member<Order, std::uint64_t, &Order::id>` can be written as `key<____>`.

Confidence (mark before checking the key): sure / unsure / ___%

---

## Question cards

### M2-Q1 [recall]
Write `bool set_cl_ord_id(Store& s, std::uint64_t id, std::uint32_t v)`
   for the store `multi_index_container<Order, indexed_by<hashed_unique<key<&Order::id>>,
   ordered_unique<key<&Order::client, &Order::cl_ord_id>>>>`. It sets the
   order's `cl_ord_id` to `v`, returns false for an unknown id, and **must keep
   the order alive** if `v` collides.

Hint (optional): three-argument modify, with the old value captured first. Or use replace.

Confidence (mark before checking the key): sure / unsure / ___%

### M2-Q2 [apply]
Queue position. Write `int ahead(const Store& s, std::uint64_t id)` that
   returns how many orders on the same side and price arrived before order
   `id`. The store has tags `ById` (hashed_unique on id) and `ByBook`
   (ordered_non_unique on `key<&Order::side, &Order::price, &Order::ts>`).

Hint (optional): project the id iterator into ByBook, then measure the distance from the
   first order at that (side, price).

Confidence (mark before checking the key): sure / unsure / ___%

### M2-Q3 [recall]
Why is `modify()` that only changes `qty` slow in a store with a
   `hashed_non_unique` client index holding ~1,500 orders per client, and
   what are two fixes?

Confidence (mark before checking the key): sure / unsure / ___%

### M2-Q4 [apply]
Different domain: a **risk-limit table**
   `multi_index_container<Limit, indexed_by<hashed_unique<key<&Limit::account>>,
   ordered_non_unique<key<&Limit::expiry>>>>`. A nightly job extends every
   expiry by one day with `modify()`. Can any limit be erased by this? Then:
   the team adds `hashed_unique<key<&Limit::desk_code>>` and the job also
   rewrites desk codes. What changes?

Confidence (mark before checking the key): sure / unsure / ___%

### M2-Q5 [recall]
You want one ordered index to give best bids (highest price first) and best
   asks (lowest price first), both with earliest time first. What key do you
   index on, and which extractor computes the price part?

Confidence (mark before checking the key): sure / unsure / ___%

---

## Integrate (day close)

### M2-INT [synthesize]
Write the one-paragraph code-review comment you would leave on
   `store.get<ById>().modify(it, [&](Order& o){ o.cl_ord_id = new_id; o.qty = q; });`
   in a store with a unique (client, cl_ord_id) index and a hashed_non_unique
   client index.

Confidence (mark before checking the key): sure / unsure / ___%

## lesson-03.md

# Lesson 03 — Bytes, stability and the tail

**Time: about 30 min. Environment for every number in this lesson:** Apple M1 Max
(128-byte cache line, 16 KiB pages), Apple clang 21, `clang++ -std=c++23 -O2`,
Boost 1.90, `steady_clock` with a 41.67 ns tick (values 41/42/84/125 are
tick multiples). Byte sizes are deterministic. Every timing was run 5 times
on a machine shared with other jobs and is shown as the **median of 5 runs,
with min..max in brackets where it matters** (raw runs: `bench/results/`).
Spreads are large for maxima and for the modify benchmark, small for the
lookup and scatter ratios: learn the ratios and mechanisms. Production HFT
runs x86-64 Linux (64-byte line, 4K/2M/1G pages) on isolated cores: the
mechanisms transfer, the numbers do not.

An insert that "costs about 50 nanoseconds" also cost **about 8 milliseconds**,
once, out of a million (5 runs: 8.2 to 8.7 ms). At that moment the market
moved and you were not there. The mean never told you. In this lesson you will make that spike happen, then make
it go away.

**Do this now (1 min):** `./build/l3_node_size`. Before reading the output,
answer the pretest.

## Pretest — commit your guesses FIRST

### M3-P1 [pretest]
Cold guess. One million single inserts into a hashed_unique index with no
   `reserve()`. Mean latency is about 50 ns. What is the **max** latency of a
   single insert: about 100 ns, about 10 µs, or about 10 ms?

Confidence (mark before checking the key): sure / unsure / ___%

### M3-P2 [pretest]
Cold guess. You keep an iterator from a multi_index **hashed** index, then
   insert 100,000 more elements (the bucket count grows from 53 to 196613).
   Is the iterator still valid? Is a `std::unordered_map` iterator valid after
   a rehash?

Confidence (mark before checking the key): sure / unsure / ___%

---

## Concept cards

### CONCEPT M3C1  Node cost per index type: measured, not assumed
Every element lives in one node: the value plus one header per index. A spy
allocator (the container's last template parameter) printed these sizes for a
40-byte `Order`, with 8-byte pointers:

| index mix | node bytes | header bytes | live bytes/elem incl. buckets |
|---|---|---|---|
| ordered_unique | 64 | 24 | 64.0 |
| hashed_unique | 56 | 16 | 71.7 |
| sequenced | 56 | 16 | 56.0 |
| random_access | 48 | 8 (+ 8 B/elem pointer array) | 56.5 |
| ranked_unique | 72 | 32 | 72.0 |
| hashed + ordered | 80 | 40 | 95.7 |
| hashed + ordered + hashed | 96 | 56 | 127.5 |
| std::map<id,Order> | 80 | — | 80.0 |
| hand-rolled 3 containers | 3 nodes | — | 176.5 |

The ordered header is 3 words, not 4. The Boost docs say ordered indices save
"one system word per ordered index", which matches a red-black colour bit
packed into a pointer. `ranked` adds a size word so it can answer "the k-th
element" and "rank of x" in O(log n). Headers just add up: the cost of an
index is its header plus its work, and nothing else.
Source: labs/l3_node_size.cpp (run this session, N=100000) ; https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/performance.html ("Nodes ... hold the value of the element plus N headers"; "one system word per ordered index") ; /opt/homebrew/include/boost/multi_index/detail/ord_index_node.hpp (`ordered_index_node_compressed_base`: colour stored in the low bit of the parent pointer) ; https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/rnk_indices.html (nth, rank: O(log(n)))

**M3C1 HandsOn:** Run `./build/l3_node_size` and find the `hashed + ordered` line.
Compute 40 + 16 + 24 yourself.

### CONCEPT M3C2  Stability: nodes never move, and what random_access gives up
Because elements sit in nodes, **pointers, references and iterators stay
valid in every index type until that element is erased**. That includes a
hashed index growing its buckets and a random_access index growing its
pointer array. `l5_stability` shows a pointer that survives buckets going
53 → 196613 and capacity 15 → 106710. `std::vector` by contrast moves its
elements on growth.
`random_access` looks like a vector, but it is an array of **pointers to
nodes**. You get `operator[]` and stability. You give up **contiguity**: 0 of
999 neighbours were adjacent in memory, and there is no `data()`. You also
pay O(distance-to-end) for inserts in the middle: 50,000 `push_front` took
about 867 ms (5 runs: 865 to 871) against about 1 ms (0.9 to 1.0) for 50,000 `push_back`, because every front insert
shifts the whole pointer array and updates each node's back-pointer.
Source: labs/l5_stability.cpp (run this session) ; /opt/homebrew/include/boost/multi_index/detail/rnd_index_node.hpp (each node keeps an `up()` pointer to its array slot) ; https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/rnd_indices.html ("Random access indices do not provide memory contiguity"; iterator validity "regardless of the capacity status") ; https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/hash_indices.html (rehash postcondition)

**M3C2 HandsOn:** Run `./build/l5_stability`.

### CONCEPT M3C3  The rehash spike, and `reserve()` on EVERY hashed index
A hashed index keeps `size/buckets ≤ max_load_factor` (1.0 by default). When
an insert crosses that limit, **that one insert** rehashes every element
already stored: O(n) work on a single message. Mean about 52 ns, max about 8 ms.
`reserve(n)` sizes the buckets up front. Careful: `container.reserve(n)`
forwards to **index 0 only**. A second hashed index (by client, say) still
rehashes unless you call `s.get<2>().reserve(n)` too (the check for this card
proves it). Measured, N = 1,000,000 inserts, ns, **median of 5 runs** (max
column shows min..max over the runs):

| case | mean | p50 | p99 | p99.9 | max | >100 µs |
|---|---|---|---|---|---|---|
| MI hashed, no reserve | 52 | 42 | 84 | 1,292 | 8,357,750 [8.23M..8.67M] | 6 |
| MI hashed, reserve(N) | 37 | 42 | 84 | 875 | 35,792 [35.5k..42.2k] | 0 |
| MI hashed, reserve + pre-touched pmr arena | 28 | 41 | 42 | 125 | 10,250 [5.1k..29.8k] | 0 |
| std::unordered_map, no reserve | 50 | 41 | 84 | 1,375 | 6,460,750 [5.69M..10.26M] | 5 to 6 |
| boost::unordered_flat_map, no reserve | 53 | 41 | 125 | 1,250 | 13,358,416 [13.16M..13.69M] | 8 |
| boost::unordered_flat_map, reserve(N) | 28 | 41 | 84 | 167 | 14,500 [6.1k..26.6k] | 0 |

(ns. Values of 41/42/84 are timer ticks: anything below 41 ns reads as 0 or
41.) After `reserve`, the remaining p99.9 comes from allocating fresh nodes,
which includes the first touch of new pages. The arena row hands out nodes
from memory that was already touched, and that p99.9 drops to about 125 ns
(in one run of five it was 1,208 ns, so treat it as "much lower", not as a figure). The
"how" of pools and pre-faulting is the subject of `../advanced_memory`.
**Ordered indices: is rebalancing a tail source?** An ordered index is a
red-black tree. In `ord_index_node.hpp` (`rebalance`, read in Boost 1.90) an
insert recolours nodes while climbing toward the root as long as the uncle is
red. The branch that rotates does at most two rotations, then the loop ends
because the parent is black. So rebalancing after an insert is bounded work,
and no single insert re-walks the whole container as a rehash does. (I read
the insert path only, not the erase path.) Measured with `build/bench_ordered`,
1,000,000 ops on one `ordered_unique` index, nodes from a pre-touched arena so
malloc and page faults are out of the loop; ns, median of 5 runs [min..max]:

| op | p50 | p99 | p99.9 | max | samples >100 µs, per run |
|---|---|---|---|---|---|
| insert, ascending keys | 208 [208..209] | 292 [292..333] | 500 [417..500] | 41,375 [30.1k..168.8k] | 0, 0, 1, 0, 0 |
| insert, random keys | 459 [459..500] | 1,250 [1,208..1,375] | 3,709 [1,500..4,666] | 53,459 [36.5k..508.4k] | 0, 0, 0, 0, 1 |
| erase, random keys | 459 [417..500] | 1,083 [1,042..1,166] | 2,709 [1,292..4,291] | 40,667 [34.8k..831.2k] | 0, 0, 0, 1, 0 |

Read it: there is no repeatable millisecond spike. Zero or one sample per
million exceeded 100 µs in each run, against 5 to 8 per run for an unreserved
hash. Random keys cost about 2.2x ascending keys at p50. I did not separate
cache misses from rebalancing work, so do not attribute that gap to either.
The rare 100 µs to 800 µs maxima came on a machine shared with other jobs and
I did not isolate their cause. The tail you can plan for is the hash rehash;
the ordered index's cost is steady and higher per operation.
Source: bench/bench_rehash.cpp, bench/bench_ordered.cpp (run 5 times this session; bench/results/summary.txt) ; /opt/homebrew/include/boost/multi_index/detail/ord_index_node.hpp (`rebalance`, insert path) ; https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/hash_indices.html ("Upon construction, max_load_factor() is 1.0"; reserve = rehash(ceil(n/max_load_factor()))) ; https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/multi_index_container.html ("all public methods and types of the first index specified are inherited by multi_index_container") ; checks/tpl/l3-reserve.cpp (container-level reserve leaves index 2 rehashing: run this session)

**M3C3 HandsOn:** Run `./build/bench_rehash` twice. Compare the max column with and
without reserve. Then run `./build/bench_ordered` and count the samples over
100 µs.

### CONCEPT M3C4  Pointer chasing: count the dependent cache misses per lookup
In a lookup the next address depends on the last load, so the CPU cannot
overlap the misses. An **ordered** index on n elements is a tree of depth
about log2 n, so at 4M orders that is roughly 22 dependent node visits. A
**node-based hash** costs a bucket load plus a node load. **Open addressing**
(`unordered_flat_map`) keeps elements in the bucket array itself. A **dense
array** indexed by your own id costs one load. Measured, dependent lookups,
ns per lookup (p50, median of 5 runs; 4M column also shows min..max):

| structure | 10k orders (cache-resident) | 4M orders (DRAM) |
|---|---|---|
| dense vector<Order>[id] | 7.8 | 92 [90..94] |
| boost::unordered_flat_map | 19.5 | 145 [143..147] |
| MI hashed_unique | 16.9 | 237 [234..246] |
| std::unordered_map | 19.5 | 288 [280..289] |
| MI ordered_unique | 85.9 | 1,025 [939..1,057] |

At 10k everything fits in cache and the differences are small. At 4M each
pointer hop is a DRAM trip, and the ranking follows the miss count. The
ordered index is about 3.6x to 11x slower than the other four here. Use
ordered indices for **range** questions (best-N), not for id lookup.
Source: bench/bench_lookup.cpp (run 5 times this session; each key depends on the previous result; the modulo cost is included in every row) ; https://www.boost.org/doc/libs/1_90_0/libs/unordered/doc/html/unordered/reference/unordered_flat_map.html ("held directly in the bucket array ... open addressing")

**M3C4 HandsOn:** Run `./build/bench_lookup`.

### CONCEPT M3C5  Allocation order vs traversal order, and the allocator parameter
A best-N query walks the book in **price order**. Whether that walk is fast
depends on where the nodes sit, which depends on **allocation order**. When
orders arrive in price order, each next node is right next door: about 6.6 ns per
node. When they arrive at random prices (the normal case), each step is a
cache miss: about 81 ns per node, **about 12× slower**, same data, same
container (median of 5 runs; min..max 6.4..6.6 and 78..82).
The container's last template parameter is the allocator:
`multi_index_container<Order, indexed_by<...>, std::pmr::polymorphic_allocator<Order>>`.
A bump arena (`std::pmr::monotonic_buffer_resource`) removes malloc from the
hot path and pre-touched memory removes page-fault spikes (M3C3). It does **not**
fix the 81 ns, because it lays nodes out in allocation order (also about 81 ns
per node measured, with the arena sized so it asked its upstream for exactly 1 block). Fixing traversal locality needs a different data layout: the price
ladder in Lesson 04.
Source: bench/bench_scatter.cpp (run 5 times this session, N=2,000,000; bench/results/summary.txt) ; https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/multi_index_container.html (Allocator template parameter) ; pmr compiled and run here with Apple clang 21 / libc++

**M3C5 HandsOn:** Run `./build/bench_scatter`.

---

## Completion cards

### M3-C1 [completion]
Fill the blanks with the header sizes measured on this 64-bit build:
   ordered ____ B, hashed ____ B, sequenced ____ B, random_access ____ B (plus
   its pointer array).

Confidence (mark before checking the key): sure / unsure / ___%

### M3-C2 [completion]
Complete: a store with hashed indices 0 and 2 must call `s.get<0>().reserve(n)`
   and ______ before the session starts, because `s.reserve(n)` only reaches
   index ______. Use numeric indices (this store declares no tags).

Confidence (mark before checking the key): sure / unsure / ___%

---

## Question cards

### M3-Q1 [recall]
Predict the node size, in bytes, of a multi_index_container of the 40-byte
   `Order` with `hashed_unique<key<&Order::id>>` and
   `ordered_non_unique<key<&Order::side, &Order::price, &Order::ts>>`. Type a
   number (an expression like `40+16+24` also works). The check measures it
   with a spy allocator.

Confidence (mark before checking the key): sure / unsure / ___%

### M3-Q2 [recall]
Write `void prepare(Store& s, std::size_t n)` for a store with three
   indices: 0 = `hashed_unique` id, 1 = `ordered_non_unique` price,
   2 = `hashed_non_unique` client. After it runs, n inserts must leave
   **every** bucket count unchanged.

Confidence (mark before checking the key): sure / unsure / ___%

### M3-Q3 [apply]
A smart-order-router keeps 3 million live child orders and looks each one
   up by id on every fill. Today it uses an `ordered_unique` id index "because
   ids are sorted". Estimate the dependent node visits per lookup, name a
   better index or structure, and say what you would measure to prove it.

Confidence (mark before checking the key): sure / unsure / ___%

### M3-Q4 [apply]
A strategy wants a "list of my working orders in submission order, plus
   O(1) access to the k-th one", and it stores raw `const Order*` in its own
   structs. Pick the third index type, and say what you gain and give up
   compared with keeping a `std::vector<Order>`.

Confidence (mark before checking the key): sure / unsure / ___%

### M3-Q5 [recall]
Same container, same 2M orders. A best-N walk costs about 6 ns per node in one
   run and about 80 ns per node in another. What differs between the runs? Does
   swapping in a pmr bump arena fix the slow one?

Confidence (mark before checking the key): sure / unsure / ___%

---

## Integrate (day close)

### M3-INT [synthesize]
Your store must survive the open: 500k inserts in the first second. List
   the three things you do before the session starts and the one thing you
   measure afterwards, each tied to a mechanism from today.

Confidence (mark before checking the key): sure / unsure / ___%

## lesson-04.md

# Lesson 04 — When NOT to use it, what to use instead, and the capstone

**Time: about 30 min. Environment for every number in this lesson:** Apple M1 Max,
Apple clang 21, `clang++ -std=c++23 -O2`, Boost 1.90, `steady_clock` with a
41.67 ns tick (41/42/83/84 ns are one or two ticks, so read them as "at or
below a couple of ticks"). The capstone and each bench were run 5 times, on a
machine shared with other jobs. Tables show the **median of 5 runs**, with
min..max where the spread is large (up to 2x for modify_price, wider for p99).
Raw runs: `bench/results/`. Learn the ratios and the mechanism, not the
nanoseconds. Re-run on your production box before you believe any of them.

Same workload, two order stores, same answers (the capstone's `selftest`
checks the checksums match). One store's `modify_price` has a p50 of
**about 10 µs** (9.7 to 17.3 µs over 5 runs). The other's is **one or two
timer ticks, 42 to 83 ns**. Neither is buggy. By the end of this lesson you
will know which data-structure decision made the difference of more than
100×, and you will remove a factor of 26 to 42 of it with one word.

**Do this now (1 min):** `./build/capstone selftest`, then
`./build/capstone mi | cut -c1-80`.

## Pretest — commit your guesses FIRST

### M4-P1 [pretest]
Cold guess. Prices on an exchange are integer ticks within a known band.
   What structure gives O(1) "best price" and "add at price p", with no tree
   at all?

Confidence (mark before checking the key): sure / unsure / ___%

### M4-P2 [pretest]
Cold guess. In the capstone's MIStore, `modify_price` uses `modify()`.
   What one-word change brings its p50 from about 10 µs to well under 1 µs?

Confidence (mark before checking the key): sure / unsure / ___%

---

## Concept cards

### CONCEPT M4C1  Honest pros and cons, and when NOT to use multi_index
| | Boost.MultiIndex |
|---|---|
| + | One node per element (1 allocation vs N), measured smaller than the hand-rolled store (127.5 vs 176.5 B/elem) |
| + | Consistency by construction: one erase, const elements, all-or-nothing insert (strong guarantee) |
| + | Stable iterators and pointers in every index; `project<>` between views |
| + | Allocator template parameter: pools and arenas plug in |
| − | Still a **node container**: every lookup is a pointer chase, and traversal locality depends on allocation order (about 81 vs 6.6 ns per node) |
| − | Ordered lookups cost about log2 n dependent misses (about 1,000 ns at 4M) |
| − | `modify` traps: erase on collision, O(group) on hashed_non_unique |
| − | Heavy templates: long compile times and error messages |

**Do not use it** on the hot path of a book or an id lookup when you control
the key space (ticks, your own ids). Arrays beat nodes there. Do not use it
for a single access path (a plain map does that). **Do use it** for the warm
side: a reference-data table, a risk or position view, a drop-copy store, or
anything with 2+ lookups where correctness matters more than nanoseconds.
Source: lessons 01–03 measurements; labs/capstone.cpp (run this session); https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/ord_indices.html (insert: "Exception safety: Strong")

**M4C1 Elaborate:** Name one table in your own stack that fits "use it" and one that
fits "don't".

### CONCEPT M4C2  The price ladder: the book as an array of levels
`std::array<LevelList, kTicks> level` per side. Index = price tick, and each
level is a FIFO list of orders. "Add at p" is `level[p].push_back(o)`, which
costs O(1) with no comparisons. "Best" is a tracked index. When the best level
empties, walk to the next non-empty one, which is O(gap) but short near the
touch. Best-N walks adjacent array slots instead of tree nodes. Trade-offs: you
must know the price band (out-of-band prices need a fallback path), and memory
is proportional to the band, not to the order count.
Measured in the capstone (best_5, ns, median of 5 runs): MIStore p50 about
167 / p99 250 to 333, FastStore p50 41 / p99 84. FastStore's 41 is one timer
tick, so its true value is at or below that.
Source: labs/capstone.cpp (run this session)

**M4C2 HandsOn:** In `labs/capstone.cpp`, read `FastStore::unlink`. What happens when
the last order at the best level is cancelled?

### CONCEPT M4C3  Intrusive containers + a pre-allocated pool
`boost::intrusive::list` stores **no nodes of its own**: the links live inside
your object, through a `list_base_hook`. "Operating with intrusive containers
doesn't invoke any memory management at all". The same object can sit in
several intrusive containers at once, for example its price level and its
client list. Put the objects in a `std::vector<Node>` sized at startup with a
free-slot stack, and add, cancel and modify become link and unlink operations
with **zero allocations**. The cost: **you** own lifetime. Unlink before you
reuse a slot, and never let the pool vector reallocate, because the hooks
hold raw addresses.
Source: https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/intrusive_vs_nontrusive.html ; labs/capstone.cpp (run this session)

**M4C3 HandsOn:** In `FastStore`, find where a cancelled order's slot goes, and the one
line that would break everything if the pool were a growing `vector`.

### CONCEPT M4C4  Id lookup: `unordered_flat_map` vs a dense slot array
`boost::unordered_flat_map` is open addressing: elements are "held directly
in the bucket array". That means fewer misses (p50 about 145 vs 237 ns per lookup at 4M
orders against the MI hashed index, medians of 5 runs), but "pointer stability is not kept under
rehashing". So store a **slot index** in it, not the `Order` you link into
lists. If **you** issue the ids (internal order ids, or a client-order-id you
assign), make the id the slot: `slots[id]`, one load, about 92 ns at 4M. Trade-off:
dense ids need recycling and a bounded range. Exchange-assigned ids still
need a hash map. Reserve the flat map too: unreserved, its max insert was
about 13 ms (Lesson 03).
Source: https://www.boost.org/doc/libs/1_90_0/libs/unordered/doc/html/unordered/reference/unordered_flat_map.html ; bench/bench_lookup.cpp, bench/bench_rehash.cpp (run this session)

**M4C4 Elaborate:** Why does FastStore keep `unordered_flat_map<id, uint32_t slot>`
rather than `unordered_flat_map<id, Node>`?

### CONCEPT M4C5  Capstone: read the histogram, fix the store, swap the allocator
`./build/capstone` replays the same 1,000,000 deterministic operations (45%
add, 45% cancel, 7% modify price, 2% best-5, 1% client query) against 100k
resting orders. It prints p50/p99/p99.9/max and power-of-two buckets per
operation. Median of 5 runs, p50 / p99 (ns); brackets show the min..max of
the p50 where it moved by more than 15%:

| op | MI modify() | MI replace() | Fast |
|---|---|---|---|
| add | 250 / 583 | 250 / 417 | 41 / 84 |
| cancel | 167 / 2,625 (p99 ranged 750 to 2,667) | 167 / 417 | 42 / 167 |
| modify_price | 10,875 [9,667..17,292] / 48,917 | 375 / 750 | 42 [42..83] / 167 |
| best_5 | 167 / 333 | 166 / 250 | 41 / 84 |
| client_qty | 24,167 [20,500..45,500] / 72,584 | 27,250 / 65,792 | 18,125 [14,833..22,333] / 56,041 |

Read it like this. `modify→replace` is the cheap fix (Lesson 02). Arrays,
intrusive lists and a pool are the structural fix. `client_qty` is slow
**everywhere**, because summing about 1,560 scattered orders is a scan. The
fix there is a running per-client total updated on add and cancel, not a
better index.
`MIStore` takes its allocator as a template parameter, so changing where
nodes live is **one line**:
`MIStore<std::pmr::polymorphic_allocator<Order>> s(cap, true, &arena);`
Source: labs/capstone.cpp (run 5 times this session; bench/results/; `selftest` confirms all four variants produce the same checksum and size)

**M4C5 HandsOn:** Run `./build/capstone mi`, then `./build/capstone mi-replace`, then
`./build/capstone fast`. Write down the modify_price p99 for each.

---

## Completion cards

### M4-C1 [completion]
Complete: in a price ladder, "add an order at price p" is
   `level[____].push_back(o)`, and time priority within a level comes from
   ______ order of the list.

Confidence (mark before checking the key): sure / unsure / ___%

### M4-C2 [completion]
Fill the blank: FastStore's id map is `boost::unordered_flat_map<std::uint64_t, ______>`
   because flat-map elements ______ on rehash.

Confidence (mark before checking the key): sure / unsure / ___%

---

## Question cards

### M4-Q1 [recall]
Write `int next_best_bid(const std::array<std::uint32_t, kTicks>& qty, int from)`.
   It returns the highest tick `<= from` with non-zero quantity, or -1 if
   there is none. This is the walk a bid ladder does when its best level
   empties.

Confidence (mark before checking the key): sure / unsure / ___%

### M4-Q2 [apply]
Allocator swap, new container. Given
   `template <class Alloc = std::allocator<Order>> using Book = multi_index_container<Order, indexed_by<ordered_non_unique<key<&Order::price>>>, Alloc>;`
   and `std::pmr::monotonic_buffer_resource arena(1 << 20);` already declared,
   write the **one statement** that declares a `Book` named `book` whose nodes
   come from `arena`.

Confidence (mark before checking the key): sure / unsure / ___%

### M4-Q3 [recall]
Give two situations where you should avoid Boost.MultiIndex in a
   trading system, and one where it is the right tool. Give a mechanism for
   each.

Confidence (mark before checking the key): sure / unsure / ___%

### M4-Q4 [apply]
Different domain: a **market-data feed handler** for one venue keeps
   per-instrument state, found by the venue's 32-bit instrument id (sparse
   values up to 4 billion, about 20,000 live). Which id → state structure would
   you pick: MI hashed, `unordered_flat_map`, or a dense array? Where does the
   state itself live?

Confidence (mark before checking the key): sure / unsure / ___%

### M4-Q5 [apply]
The capstone shows `client_qty` at tens of µs p50 in every variant (about
   18 to 38 µs across the four variants here), even FastStore. A risk check calls it on every new order. What do you change,
   and why does no index choice fix it?

Confidence (mark before checking the key): sure / unsure / ___%

---

## Integrate (course close)

### M4-INT [synthesize]
Your team lead asks: "Should the new order gateway use Boost.MultiIndex?"
   Give your answer in five bullets: decision, the consistency argument, the
   two latency mechanisms that matter, and the one-line next step for memory.

Confidence (mark before checking the key): sure / unsure / ___%

## Next step — one line to pooled memory
The sibling tutorial `../advanced_memory` builds drop-in allocators that give
containers pooled, pre-touched memory. Because `MIStore` takes its allocator
as a template parameter, adopting one is a one-line change in
`labs/capstone.cpp`:

```cpp
MIStore<std::pmr::polymorphic_allocator<Order>> s(cap, true, &arena);   // today: pmr arena (runs here)
// MIStore<YourPoolAllocator<Order>> s(cap, true);                      // after ../advanced_memory
```
Re-run `./build/capstone` and compare the add and cancel p99.9 and max.

# Answer keys
Compare only after writing an attempt and marking its confidence.
Sure (or >= 70%) and missed? Star it: re-test it first tomorrow.

## M1-P1
- Hand-rolled: 3 per add (one node per container). multi_index_container: 1
    per insert, whatever the number of indices. Measured by `build/l1_handrolled`
    and `build/l1_mi`. If you guessed "the same", CONCEPT M1C2 is why it isn't.

## M1-P2
- Most likely a normal-looking number. Plain build, run three times on the
    authoring machine: `client 0 open qty = 710` every time, which equals the
    live-orders-only answer (71 live orders x 10) even though the client index
    still points at 72 destroyed ones. Whether freed memory reads back as the
    old value, zero or junk depends on the allocator, so on another machine or
    libc this number can differ and the program is allowed to crash.
    AddressSanitizer reported `heap-use-after-free` at the same read. That
    silence is the bug class CONCEPT M1C1 is about.

## M1C1 Elaborate
- Reading freed memory is undefined behaviour, and nothing is required to
    trap it. The allocator still owns those bytes, so the read "works" and
    returns whatever is there. A tool like ASan has to instrument the access
    before the bug becomes visible.

## M1C1 HandsOn
- `bug`: `sizes: by_id=500 by_price=500 by_client=1000`. The client index
    still holds 500 pointers to destroyed orders. The last line of the plain
    run prints `client 0 open qty = 710` (it did on all three runs here; see
    M1-P2 for why you must not rely on that number).
- `stale`: `order 999 still filed under price 149 in the book: yes (stale
    key)`. The book never sees the reprice (its own sizes line reads
    `by_id=500 by_price=500 by_client=500`, because this run does not forget
    the client index).
- ASan: `ERROR: AddressSanitizer: heap-use-after-free`, then `READ of size 4`
    at `l1_handrolled.cpp:91`, in a 64-byte region freed by `cancel()`
    (`l1_handrolled.cpp:62`). Its `SUMMARY:` line names `l1_handrolled.cpp:91 in main`.

## M1C2 HandsOn
- `hand-rolled: allocations per add() = 3` vs `multi_index: allocations per
    insert() = 1`. The MI sizes line shows all three indices at 500. They
    cannot disagree, because they are views of one set of nodes.

## M1C3 Elaborate
- The only way to change a key is through a container member function
    (`modify` or `replace`). That function re-files the element in every
    index, so no index can keep an old copy of the key.

## M1C4 HandsOn
- It erases through `s.get<ById>()` (`ids.erase(id)`), and the three sizes
    print 500/500/500.

## M1C5 HandsOn
- `best bids: #2@101 #4@101 #1@100`. Orders #2 and #4 share price 101, and
    #2 comes first because it arrived first (smaller `ts`).

## M1-C1
- 1 node per element
- 3 headers (one per index)

## M1-C2
- `hashed_unique<member<Order, std::uint64_t, &Order::id>>`
- `ordered_non_unique<member<Order, std::int64_t, &Order::price>>`

## M1-Q1
- Forgotten index (dangling pointer left behind): fixed because erasing
    through any index erases from all of them.
- Stale key (key changed via one path only): fixed because elements are
    const, and keys change only via modify/replace, which re-file the element
    in every index.
- Partial failure (an insert or exception leaves only some containers
    updated): fixed because the container inserts into all indices or none,
    as a single operation.

## M1-Q2
- `using Store = multi_index_container<Order, indexed_by<hashed_unique<key<&Order::id>>, ordered_non_unique<key<&Order::price>>>>;`
    (`member<Order, std::uint64_t, &Order::id>` is equally right).
- The check compiles it, then confirms duplicate ids are rejected, duplicate
    prices accepted, and index 1 sorted by price.

## M1-Q3
- Index 0: `hashed_unique<key<&Instrument::symbol>>` (or `member<Instrument, std::string, &Instrument::symbol>`).
- Index 1: `ordered_unique<key<&Instrument::exchange, &Instrument::local_id>>`
    (or `composite_key<Instrument, member<...exchange>, member<...local_id>>`).
- Must be ordered, not hashed: a hashed composite cannot answer
    `equal_range(std::make_tuple(1))`.

## M1-Q4
- 4 × 2,000,000 = 8,000,000 allocations/s hand-rolled, against 2,000,000/s
    (one node per element) with multi_index.
- The bigger gain is in tail behaviour: each allocation is another chance to
    hit a slow allocator path (Lesson 03).

## M1-Q5
- `book.equal_range(std::make_tuple('B'))`, or `lower_bound`/`upper_bound`
    with that tuple.
- Comparison between a composite key and a tuple uses only the tuple's
    length (the shorter operand), so `('B')` matches every key whose first
    component is 'B'. This works only in an ordered index.

## M1-INT
- Three containers mean three nodes per order, and every operation must
    keep them in sync by hand. Forgotten, stale or partial updates compile
    fine and fail silently.
- One container gives one node per order with one header per index.
    Elements are const, so keys only change through the container.
- An erase through any index removes the element everywhere.

## M2-P1
- It is **erased**. `modify` returns false and the container shrinks from 3
    to 2 (`build/l4_modify`: "after colliding modify (no rollback) size=2",
    "find(2) == end()? YES - order 2 is gone"). If you guessed "nothing,
    returns false", CONCEPT M2C4 is the surprise.

## M2-P2
- Over 100× slower. In each of 5 runs the modify-qty p50 was 137× to 284×
    the replace-qty p50 (modify: several µs to tens of µs; replace: under 100
    ns). The absolute figures moved about 3× between runs, the ratio never
    dropped below 100×. CONCEPT M2C5 explains the mechanism.

## M2C1 HandsOn
- The output is identical for both spellings: `best bids: #2@101 #4@101
    #1@100`. The two `static_assert(std::is_same_v<...>)` lines compile, so the
    shorthand resolves to exactly the long-hand extractors.

## M2C2 HandsOn
- `order #4 has 1 order(s) ahead of it at 101`: order #2 arrived earlier at
    the same price.

## M2C3 Elaborate
- By the time modify checks the indices, your functor has already
    overwritten the element, and modify kept no copy of the old value. It
    cannot put the old value back by itself, so it has nothing to restore.

## M2C4 HandsOn
- Step 2: `replace ... returned false`, size stays 3.
- Step 3: `modify ... returned false`, size drops to 2, order 2 is gone.
- Step 4: `modify+rollback ... returned false`, size is 3, and order 2 is
    back with `c7/101`.

## M2C5 HandsOn
- Modify qty is thousands of ns (several µs up to tens of µs) while replace
    qty stays under about 100 ns. Modify scales with orders-per-client;
    replace does not.

## M2-C1
- unchanged (container untouched)
- erases (removes)
  (Self-graded against this Key: free-text negations such as "modify never
  keeps it" cannot be graded honestly by a regex, so there is no AcceptIf.)

## M2-C2
- `key<&Order::id>`

## M2-Q1
- Find through `s.get<0>()`, and return false if not found.
- Capture `old = it->cl_ord_id`, then `modify(it, [v](Order& o){ o.cl_ord_id = v; },
    [old](Order& o){ o.cl_ord_id = old; })`. (A `replace` with a modified
    copy is also correct.)
- The check compiles your function and confirms that a colliding change
    returns false and keeps the order with its old id.

## M2-Q2
- `auto it = s.project<ByBook>(s.get<ById>().find(id));`
- `auto first = s.get<ByBook>().lower_bound(std::make_tuple(it->side, it->price));`
- `return std::distance(first, it);` (counting from `begin()` of the whole
    book is wrong, because it includes other prices and sides).

## M2-Q3
- After modify, the hashed index cannot tell whether its key changed (the
    old value is gone), so it re-checks the element's position by walking
    its equal-key group: up to O(orders for that client).
- Fix 1: use `replace` (it compares old and new keys and skips unchanged
    indices).
- Fix 2: make the client index `ordered_non_unique`, whose position check
    compares the two neighbours only.

## M2-Q4
- With only `account` unique and `expiry` non-unique, changing expiry can
    never collide, so modify cannot erase anything. It always returns true.
- Once the job writes `desk_code` (a unique index), two limits mapped to
    the same desk code collide, and plain modify **erases** the second one.
    Use a rollback, or `replace`, and check the return value.

## M2-Q5
- A composite key (side, rank, ts), where `rank()` returns `-price` for bids
    and `+price` for asks, so smaller rank means better on both sides.
- The price part is computed by `const_mem_fun<Order, std::int64_t, &Order::rank>`,
    written `key<&Order::rank>` in C++17.

## M2-INT
- A collision on (client, cl_ord_id) makes this modify **erase** the order.
    Add a rollback that restores both fields, or use replace, and handle a
    false return.
- Performance: modify re-checks the hashed_non_unique client index by
    walking that client's equal-key group (up to O(group size)), so cost grows
    with the client's order count. replace avoids the walk when the client is unchanged.

## M3-P1
- About 10 ms (milliseconds, not µs). Measured max was 8.2 to 8.7 ms over 5
    runs (mean about 52 ns), and 6 inserts took over 100 µs in every run
    (`build/bench_rehash`). Those few inserts each triggered a
    rehash of everything already stored. CONCEPT M3C3.

## M3-P2
- Multi_index hashed: yes. Its `rehash` postcondition says "Validity of
    iterators and references to the elements contained is preserved."
- std::unordered_map: iterators are invalidated by a rehash, though
    references and pointers to elements survive. CONCEPT M3C2.

## M3C1 HandsOn
- `hashed + ordered node=80 B`, and 40 + 16 + 24 = 80. Live bytes per
    element (95.7) is higher because the hashed bucket array is extra.

## M3C2 HandsOn
- "pointer to element still valid? yes", "hashed iterator still valid after
    rehash? yes", "std::vector element moved on growth? yes".
- "adjacent elements contiguous in memory: 0 of 999".
- push_front is hundreds of times slower than push_back.

## M3C3 HandsOn
- Without reserve, max is in milliseconds and a handful of samples exceed
    100 µs. With reserve, max is tens of µs and none exceed 100 µs. The mean
    moves by tens of ns while the max moves by orders of magnitude. That is the "mean is fine, tail is ugly" picture.

## M3C4 HandsOn
- At 4M the order is dense < flat_map < MI hashed ≲ std::unordered_map ≪
    MI ordered. The ordered index is several times slower than any hash.

## M3C5 HandsOn
- Sorted arrival: under 10 ns per node with 100% "next node within 64 B".
    Random arrival: about 80 ns per node with 0%. The pmr arena rows match
    their std::allocator rows.

## M3-C1
- ordered 24, hashed 16, sequenced 16, random_access 8

## M3-C2
- `s.get<2>().reserve(n)`
- index 0

## M3-Q1
- 80: value 40 + hashed header 16 + ordered header 24.

## M3-Q2
- `s.get<0>().reserve(n); s.get<2>().reserve(n);`
- `s.reserve(n)` alone fails: it only reserves index 0, so index 2 still
    rehashes. The ordered index has no buckets.

## M3-Q3
- About log2(3M) ≈ 21–22 dependent node visits. At that size most are cache
    misses (the 4M ordered row measured about 1,000 ns per lookup here).
- Use `hashed_unique` (about 2 misses), `boost::unordered_flat_map`, or a
    dense id → slot array if you control the ids (one load).
- Measure dependent-lookup latency p50/p99/p99.9 at the production working
    set, on the production box. Do not use an average from a small,
    cache-resident test.

## M3-Q4
- `random_access<>`: it gives `operator[]` and keeps submission order.
- Gained: pointers, references and iterators stay valid through growth and
    through other inserts and erases. A vector moves elements when it grows,
    which would dangle those `const Order*`.
- Given up: contiguity (elements are separate nodes, no `data()`), plus
    O(n) insert or erase away from the back, because the pointer array shifts
    (50,000 push_front took about 867 ms here).

## M3-Q5
- Allocation order differs. In the fast run orders arrived in price order,
    so consecutive nodes sit next to each other. In the slow run arrival
    prices were random, so every step in price order is a cache miss.
- An arena does not fix it, because it packs nodes in allocation order too
    (measured about 81 ns per node). It removes malloc calls and page-fault
    spikes, not traversal misses. Fixing the walk needs a layout that is
    contiguous in price order, such as a price-level array.

## M3-INT
- `reserve()` on every hashed index, not just index 0, so no insert
    rehashes everything (M3C3).
- Use hashed indices (or flat or dense structures) for id lookup and keep
    ordered indices for range queries, to cut dependent misses (M3C4).
- Supply an allocator over pre-touched memory (the allocator parameter), so
    node allocation is not malloc plus first-touch page faults. Keep in mind
    it does not fix traversal order (M3C3, M3C5).
- Measure per-op p99, p99.9 and max, not the mean, on the production box.

## M4-P1
- An array of price levels indexed by `price - band_low`. Each level is a
    FIFO queue of orders (time priority), and you track the best non-empty
    index. This is the "price ladder". CONCEPT M4C2.

## M4-P2
- `replace` instead of `modify`. The client index is hashed_non_unique with
    about 1,560 orders per client, so modify walks that group every time
    (Lesson 02, M2C5). Measured over 5 runs: modify p50 9.7 to 17.3 µs against replace p50
    375 to 416 ns, a factor of 26x to 42x in every run.

## M4C1 Elaborate
- "Use it": a table with several access paths, moderate update rate and
    correctness-critical consistency (for example instruments by symbol and
    by exchange id, or orders for a risk or drop-copy view).
- "Don't": the hot-path book or the id→order map on the matching or
    trading thread, where the key space is dense and known and arrays or flat
    tables win.

## M4C2 HandsOn
- `s.best` steps away from the touch (−1 for bids, +1 for asks) until it
    finds a non-empty level. If the side is now empty, `best` becomes −1.

## M4C3 HandsOn
- The slot index goes back on `free_` (`free_.push_back(it->second)`).
- The pool is `std::vector<Node> pool_(capacity)`, created once and never
    resized. A `push_back` that reallocates would move every Node and leave
    every intrusive link dangling.

## M4C4 Elaborate
- Flat-map elements move when the table rehashes, so intrusive hooks inside
    them would dangle. The Nodes live in a pool that never moves, and the map
    only stores their slot numbers.

## M4C5 HandsOn
- Modify p99 is in the tens of µs, replace p99 under 1 µs, and FastStore
    p99 a couple of hundred ns. Your box will differ, but the order should not.

## M4-C1
- `level[p]` (or `p - band_low`)
- FIFO / insertion order (push_back at the tail, best at the front)

## M4-C2
- a slot index (`std::uint32_t`), not the Node or Order
- move (no pointer stability)

## M4-Q1
- `for (int px = from; px >= 0; --px) if (qty[px]) return px; return -1;`
- Tick 0 is a valid level: a loop with `px > 0` misses it.

## M4-Q2
- `Book<std::pmr::polymorphic_allocator<Order>> book(&arena);`
- The check confirms `book.get_allocator().resource() == &arena` and inserts
    1,000 orders. `Book<std::pmr::polymorphic_allocator<>> book(&arena);` (the
    default `std::byte` argument) is equally right: the container rebinds the
    allocator to its node type, and the check accepts it.

## M4-Q3
- Not for the hot-path book when prices are bounded ticks: a node tree
    costs dependent misses and allocation-order-dependent traversal, and a
    ladder array is O(1) and contiguous.
- Not for hot id lookup when you control the ids (dense array, one load) or
    can use an open-addressing map with slot indices.
- Not for a single access path (plain map or flat map).
- Right tool: multiple access paths with consistency requirements off the
    nanosecond path, because one node, const elements and one erase remove
    the hand-sync bug class.

## M4-Q4
- Not a dense array keyed by raw venue id (sparse 32-bit range, too big).
    Pick `boost::unordered_flat_map<uint32_t, uint32_t slot>`, reserved at
    startup, or remap venue ids to dense internal slots once at subscribe
    time and index an array from then on.
- The state lives in a pre-sized vector or pool, so pointers to it stay
    valid. The flat map stores the slot, because its elements move on rehash.

## M4-Q5
- Keep a running per-client aggregate (open qty, notional) and update it in
    add, cancel and modify. The query becomes one load.
- Every index still has to visit about 1,560 scattered orders, so the cost
    is the scan and its cache misses, not the lookup.

## M4-INT
- Decision: yes for the gateway's warm order store (id / client /
    session lookups). No for the hot book and id map, which should be a
    ladder plus a dense or flat id map plus an intrusive pool.
- Consistency: one node per order, const elements, erase-once, and an
    all-or-nothing insert remove the hand-sync bug class. Use replace or a
    rollback, never a bare modify, on unique keys.
- Latency 1: reserve every hashed index (rehash spike: ms max behind a
    mean of tens of ns).
- Latency 2: node containers pay dependent cache misses, and traversal
    speed depends on allocation order (about 81 vs 6.6 ns per node).
- Memory next step: the allocator is a template parameter, so swap it in
    one line for the pooled, pre-touched allocators from `../advanced_memory`.
