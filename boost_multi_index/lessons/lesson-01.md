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

### CARD M1-P1  [type: pretest]  [topic: allocs]  [box: 1]
Q: Cold guess. The hand-rolled store keeps `unordered_map<id, Order>` plus
   `map<(side,price,time), Order*>` plus `unordered_multimap<client, Order*>`,
   all buckets pre-reserved. How many heap allocations does one `add()` cost?
   And the same store as a single multi_index_container with three indices?
Key:
  - Hand-rolled: 3 per add (one node per container). multi_index_container: 1
    per insert, whatever the number of indices. Measured by `build/l1_handrolled`
    and `build/l1_mi`. If you guessed "the same", CONCEPT M1C2 is why it isn't.
Concepts: M1C2
Source: labs/l1_handrolled.cpp, labs/l1_mi.cpp (run this session)

### CARD M1-P2  [type: pretest]  [topic: consistency]  [box: 1]
Q: Cold guess. `cancel()` erases the order from the id map and the price map
   but forgets the client multimap. Later you sum `qty` over client 0's
   entries. What happens: compile error, crash, exception, or a normal-looking
   number?
Key:
  - Most likely a normal-looking number. Plain build, run three times on the
    authoring machine: `client 0 open qty = 710` every time, which equals the
    live-orders-only answer (71 live orders x 10) even though the client index
    still points at 72 destroyed ones. Whether freed memory reads back as the
    old value, zero or junk depends on the allocator, so on another machine or
    libc this number can differ and the program is allowed to crash.
    AddressSanitizer reported `heap-use-after-free` at the same read. That
    silence is the bug class CONCEPT M1C1 is about.
Concepts: M1C1
Source: labs/l1_handrolled.cpp (run this session: `bug` x3 plain, and the ASan build; raw output in _evidence/l1-fix-runs.txt)

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
HandsOn: Run `./build/l1_handrolled bug` and note the sizes line. Then
`./build/l1_handrolled stale`. Last, build with ASan:
`clang++ -std=c++23 -O1 -g -fsanitize=address -I/opt/homebrew/include labs/l1_handrolled.cpp -o build/l1_asan && ./build/l1_asan bug`.
HandsOnKey:
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
Elaborate: Why does the plain build happily print a number instead of
crashing?
ElaborateKey:
  - Reading freed memory is undefined behaviour, and nothing is required to
    trap it. The allocator still owns those bytes, so the read "works" and
    returns whatever is there. A tool like ASan has to instrument the access
    before the bug becomes visible.
Source: labs/l1_handrolled.cpp (run this session); Boost.MultiIndex tutorial "secondary data structure of pointers ... annoying burden of keeping the two structures in sync" https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/tutorial/basics.html

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
HandsOn: Run `./build/l1_handrolled` and `./build/l1_mi` side by side.
HandsOnKey:
  - `hand-rolled: allocations per add() = 3` vs `multi_index: allocations per
    insert() = 1`. The MI sizes line shows all three indices at 500. They
    cannot disagree, because they are views of one set of nodes.
Source: labs/l1_handrolled.cpp, labs/l1_mi.cpp, labs/l3_node_size.cpp (run this session); Boost.MultiIndex performance page "Nodes of a multi_index_container with N indices hold the value of the element plus N headers ... the manual simulation allocates N nodes per element" https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/performance.html

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
Elaborate: Why does making elements `const` kill the "stale key" bug from M1C1?
ElaborateKey:
  - The only way to change a key is through a container member function
    (`modify` or `replace`). That function re-files the element in every
    index, so no index can keep an old copy of the key.
Source: https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/tutorial/basics.html ; https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/key_extraction.html ; labs/l1_mi.cpp (compiled this session)

### CONCEPT M1C4  Tags and `get<>()`: name your indices, erase once
`s.get<0>()` returns a reference to index 0, which behaves like a std
container. Numbers rot when someone reorders the list, so give each index a
**tag**: `struct ById {};`, then `hashed_unique<bmi::tag<ById>, ...>`, then
`s.get<ById>()`. Tags and numbers both work. **Erasing through any index
removes the element from all of them.** That one fact deletes the
"forgotten index" bug.
HandsOn: In `labs/l1_mi.cpp`, find the erase loop. Which index does it erase
through, and what are the three sizes afterwards?
HandsOnKey:
  - It erases through `s.get<ById>()` (`ids.erase(id)`), and the three sizes
    print 500/500/500.
Source: https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/tutorial/basics.html ; labs/l1_mi.cpp (run this session)

### CONCEPT M1C5  `composite_key` for (side, price, time), and partial lookups
Price-time priority is a three-part key. Write it as
`composite_key<Order, member<..side>, member<..price>, member<..ts>>` inside an
`ordered_non_unique`. Lookups take a `std::tuple`, and a **shorter tuple is a
prefix lookup**: `book.equal_range(std::make_tuple('B'))` gives every bid in
key order. Bids want the *highest* price first, so the lab sorts on a computed
`rank()` (minus price for bids, price for asks). Lesson 02 shows that
extractor.
HandsOn: Run `./build/l2_keys` and read the `best bids:` line.
HandsOnKey:
  - `best bids: #2@101 #4@101 #1@100`. Orders #2 and #4 share price 101, and
    #2 comes first because it arrived first (smaller `ts`).
Source: https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/key_extraction.html (composite_key comparison "takes the minimum length of the operands") ; labs/l2_keys.cpp (run this session)

---

## Completion cards

### CARD M1-C1  [type: completion]  [topic: allocs]  [box: 1]
Q: Fill the blanks. A multi_index_container with 3 indices allocates ____
   node(s) per element. Each node holds the value plus ____ index headers.
Key:
  - 1 node per element
  - 3 headers (one per index)
AcceptIf: regex(/^\s*(1|one)\b.*\b(3|three)\b/i)
RejectIf: regex(/^\s*(3|three)\b|\bper index\b.*\bnode|\b(2|two)\s+headers/i)
Concepts: M1C2
Source: CONCEPT M1C2

### CARD M1-C2  [type: completion]  [topic: declare]  [box: 1]
Q: Complete the declaration: ids unique and hashed, prices repeatable and
   sorted. Write the two index specifiers that go in `indexed_by<____, ____>`,
   using `member<>` or `key<>`.
Key:
  - `hashed_unique<member<Order, std::uint64_t, &Order::id>>`
  - `ordered_non_unique<member<Order, std::int64_t, &Order::price>>`
AcceptIf: regex(/hashed_unique\s*<.*&Order::id.*ordered_non_unique\s*<.*&Order::price/s)
RejectIf: regex(/ordered_unique\s*<[^>]*price|hashed_non_unique\s*<[^>]*&Order::id/)
Concepts: M1C3
Source: CONCEPT M1C3

---

## Question cards

### CARD M1-Q1  [type: recall]  [topic: consistency]  [box: 1]
Q: Name the three ways a hand-rolled "one owner map + two maps of pointers"
   order store goes out of sync, and say which multi_index property removes
   each one.
Key:
  - Forgotten index (dangling pointer left behind): fixed because erasing
    through any index erases from all of them.
  - Stale key (key changed via one path only): fixed because elements are
    const, and keys change only via modify/replace, which re-file the element
    in every index.
  - Partial failure (an insert or exception leaves only some containers
    updated): fixed because the container inserts into all indices or none,
    as a single operation.
Concepts: M1C1, M1C3, M1C4
Source: CONCEPT M1C1, M1C3, M1C4

### CARD M1-Q2  [type: recall]  [topic: declare]  [box: 1]
Q: Write a complete `using Store = ...;` for `struct Order { std::uint64_t id;
   std::int64_t price; std::uint32_t qty; };` where index 0 is hashed and
   unique on `id` and index 1 is ordered and non-unique on `price`. The
   harness already has `using namespace boost::multi_index;`. Paste it on one
   line or inside a ``` block.
Key:
  - `using Store = multi_index_container<Order, indexed_by<hashed_unique<key<&Order::id>>, ordered_non_unique<key<&Order::price>>>>;`
    (`member<Order, std::uint64_t, &Order::id>` is equally right).
  - The check compiles it, then confirms duplicate ids are rejected, duplicate
    prices accepted, and index 1 sorted by price.
AcceptIf: check(checks/l1-decl.sh)
Concepts: M1C3
Source: checks/tpl/l1-decl.cpp (right and wrong answers run this session)

### CARD M1-Q3  [type: apply]  [topic: declare]  [box: 1]
Q: New domain: a market-data **instrument table**.
   `struct Instrument { std::string symbol; int exchange; std::uint32_t
   local_id; double tick; };` must be found by `symbol` (unique, hashed) and by
   `(exchange, local_id)` (unique as a pair, ordered, so that "all instruments
   of exchange 1" is a range). Write `using Instruments = ...;`.
Hint: the second index needs a composite key; a one-element tuple then does
   the prefix lookup.
Key:
  - Index 0: `hashed_unique<key<&Instrument::symbol>>` (or `member<Instrument, std::string, &Instrument::symbol>`).
  - Index 1: `ordered_unique<key<&Instrument::exchange, &Instrument::local_id>>`
    (or `composite_key<Instrument, member<...exchange>, member<...local_id>>`).
  - Must be ordered, not hashed: a hashed composite cannot answer
    `equal_range(std::make_tuple(1))`.
AcceptIf: check(checks/l1-instruments.sh)
Concepts: M1C3, M1C5
Source: checks/tpl/l1-instruments.cpp (right and wrong answers run this session)

### CARD M1-Q4  [type: apply]  [topic: allocs]  [box: 1]
Q: A colleague's position-keeper has 4 access paths built from 4 std node
   containers (one owning, three holding pointers). At 2 million inserts per
   second, how many allocator calls per second does insertion cost, and how
   many with a 4-index multi_index_container? Assume buckets are pre-reserved.
Key:
  - 4 × 2,000,000 = 8,000,000 allocations/s hand-rolled, against 2,000,000/s
    (one node per element) with multi_index.
  - The bigger gain is in tail behaviour: each allocation is another chance to
    hit a slow allocator path (Lesson 03).
AcceptIf: regex(/8\s*,?\s*000\s*,?\s*000|8\s*(m|million|e6)\b/i) AND regex(/2\s*,?\s*000\s*,?\s*000|2\s*(m|million|e6)\b/i)
RejectIf: regex(/\b(16|6|4)\s*(m|million)|\b8\s*(m|million)[^.]*(multi|mi\b)/i)
Concepts: M1C2
Source: CONCEPT M1C2 (counts measured by labs/l1_*.cpp)

### CARD M1-Q5  [type: recall]  [topic: lookup]  [box: 1]
Q: With an ordered composite index on (side, rank, ts), which single call
   returns every bid in price-time priority, and why does a one-element tuple
   work there?
Key:
  - `book.equal_range(std::make_tuple('B'))`, or `lower_bound`/`upper_bound`
    with that tuple.
  - Comparison between a composite key and a tuple uses only the tuple's
    length (the shorter operand), so `('B')` matches every key whose first
    component is 'B'. This works only in an ordered index.
Concepts: M1C5
Source: CONCEPT M1C5

---

## Integrate (day close)

### CARD M1-INT  [type: synthesize]  [topic: all]
Q: In three sentences, explain to a teammate why the three-container order
   store was dangerous and what changes when you switch to one
   multi_index_container. Use the words "node", "const" and "erase".
Key:
  - Three containers mean three nodes per order, and every operation must
    keep them in sync by hand. Forgotten, stale or partial updates compile
    fine and fail silently.
  - One container gives one node per order with one header per index.
    Elements are const, so keys only change through the container.
  - An erase through any index removes the element everywhere.
Concepts: M1C1, M1C2, M1C3, M1C4
Source: CONCEPT M1C1–M1C4

Next session: Lesson 02. You will break `modify()` on purpose and watch it
delete an order.
