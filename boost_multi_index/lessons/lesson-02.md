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

### CARD M2-P1  [type: pretest]  [topic: modify]  [box: 1]
Q: Cold guess. Index 1 is `ordered_unique` on (client, cl_ord_id). You call
   `by_id.modify(it, [](Order& o){ o.cl_ord_id = 100; })`, and (7,100) already
   belongs to another order. What happens to the order you modified?
Key:
  - It is **erased**. `modify` returns false and the container shrinks from 3
    to 2 (`build/l4_modify`: "after colliding modify (no rollback) size=2",
    "find(2) == end()? YES - order 2 is gone"). If you guessed "nothing,
    returns false", CONCEPT M2C4 is the surprise.
Concepts: M2C4
Source: labs/l4_modify.cpp (run this session); https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/ord_indices.html ("If the rearrangement fails, the element is erased.")

### CARD M2-P2  [type: pretest]  [topic: modify-cost]  [box: 1]
Q: Cold guess. 100,000 orders, 64 clients, and the store has a
   `hashed_non_unique` index on client. Changing just `qty` (no index uses
   it) with `modify()`: is it about the cost of a `find`, or 100× slower?
Key:
  - Over 100× slower. In each of 5 runs the modify-qty p50 was 137× to 284×
    the replace-qty p50 (modify: several µs to tens of µs; replace: under 100
    ns). The absolute figures moved about 3× between runs, the ratio never
    dropped below 100×. CONCEPT M2C5 explains the mechanism.
Concepts: M2C5
Source: bench/bench_modify.cpp (run 5 times this session, N=100000, 64 clients; bench/results/summary.txt)

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
HandsOn: Open `labs/l2_keys.cpp`. Compare the `LongHand` and `ShortHand`
types, then run `./build/l2_keys`.
HandsOnKey:
  - The output is identical for both spellings: `best bids: #2@101 #4@101
    #1@100`. The two `static_assert(std::is_same_v<...>)` lines compile, so the
    shorthand resolves to exactly the long-hand extractors.
Source: https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/key_extraction.html ; labs/l2_keys.cpp (compiled and run this session)

### CONCEPT M2C2  `project<>`: jump from one index to another, same element
You found an order by id, and now you want its place in the book. Searching
the book again would cost another O(log n). `s.project<ByBook>(it_id)` turns
the hashed iterator into a book iterator **for the same node**, in constant
time. From there you can walk neighbours, for example to count orders ahead
of you at your price level (your queue position).
HandsOn: In `./build/l2_keys` output, read the `order #4 has ...` line.
HandsOnKey:
  - `order #4 has 1 order(s) ahead of it at 101`: order #2 arrived earlier at
    the same price.
Source: https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/tutorial/basics.html (projection) ; labs/l2_keys.cpp (run this session)

### CONCEPT M2C3  Changing an element: `modify(it, f)` vs `replace(it, x)`
Elements are const, so you change them through the container.
`replace(it, x)` assigns a whole new value. If the new value would break any
unique index, it does **nothing** and returns false (strong guarantee).
`modify(it, f)` runs your functor **on the element in place** (no copy), then
re-files it in every index and returns whether that worked. Both keep `it`
valid on success.
Elaborate: Why can't `modify` simply "refuse" the way `replace` does?
ElaborateKey:
  - By the time modify checks the indices, your functor has already
    overwritten the element, and modify kept no copy of the old value. It
    cannot put the old value back by itself, so it has nothing to restore.
Source: https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/ord_indices.html (modify, replace: Effects, Exception safety) ; labs/l4_modify.cpp (run this session)

### CONCEPT M2C4  The collision rule: modify ERASES unless you give a rollback
If the modified element collides in a unique index, plain `modify` **erases
it** and returns false. The three-argument `modify(it, f, rollback)` calls
`rollback(e)` on failure. If that restores a value that fits all indices, the
element is kept. Capture the old field value **before** modifying.
HandsOn: Run `./build/l4_modify` and read steps 2, 3 and 4.
HandsOnKey:
  - Step 2: `replace ... returned false`, size stays 3.
  - Step 3: `modify ... returned false`, size drops to 2, order 2 is gone.
  - Step 4: `modify+rollback ... returned false`, size is 3, and order 2 is
    back with `c7/101`.
Source: https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/ord_indices.html ("If the rearrangement fails, back(e) is invoked ... the element is kept, otherwise it is erased.") ; labs/l4_modify.cpp (run this session)

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
HandsOn: Run `./build/bench_modify` (about 8 s; your numbers will differ, the pattern will not). Find the "64 clients" block.
HandsOnKey:
  - Modify qty is thousands of ns (several µs up to tens of µs) while replace
    qty stays under about 100 ns. Modify scales with orders-per-client;
    replace does not.
Source: bench/bench_modify.cpp (run 5 times this session; bench/results/) ; /opt/homebrew/include/boost/multi_index/hashed_index.hpp `in_place(..., hashed_non_unique_tag)` and `replace_` (read this session) ; /opt/homebrew/include/boost/multi_index/detail/ord_index_impl.hpp `in_place(..., ordered_non_unique_tag)`

---

## Completion cards

### CARD M2-C1  [type: completion]  [topic: modify]  [box: 1]
Q: Fill the blanks. On a unique-index collision, `replace` leaves the element
   ______ and returns false; plain `modify` ______ the element and returns
   false.
Key:
  - unchanged (container untouched)
  - erases (removes)
  (Self-graded against this Key: free-text negations such as "modify never
  keeps it" cannot be graded honestly by a regex, so there is no AcceptIf.)
Concepts: M2C3, M2C4
Source: CONCEPT M2C3, M2C4

### CARD M2-C2  [type: completion]  [topic: keys]  [box: 1]
Q: Complete the shorthand. With C++17 and `<boost/multi_index/key.hpp>`,
   `member<Order, std::uint64_t, &Order::id>` can be written as `key<____>`.
Key:
  - `key<&Order::id>`
AcceptIf: regex(/^\s*(?:(?:(?:boost::multi_index|bmi)::)?key\s*<\s*&\s*Order::id\s*>|&\s*Order::id)\s*$/)
RejectIf: regex(/Order\.id|\*Order|\bmember\s*</)
Concepts: M2C1
Source: CONCEPT M2C1

---

## Question cards

### CARD M2-Q1  [type: recall]  [topic: modify]  [box: 1]
Q: Write `bool set_cl_ord_id(Store& s, std::uint64_t id, std::uint32_t v)`
   for the store `multi_index_container<Order, indexed_by<hashed_unique<key<&Order::id>>,
   ordered_unique<key<&Order::client, &Order::cl_ord_id>>>>`. It sets the
   order's `cl_ord_id` to `v`, returns false for an unknown id, and **must keep
   the order alive** if `v` collides.
Hint: three-argument modify, with the old value captured first. Or use replace.
Key:
  - Find through `s.get<0>()`, and return false if not found.
  - Capture `old = it->cl_ord_id`, then `modify(it, [v](Order& o){ o.cl_ord_id = v; },
    [old](Order& o){ o.cl_ord_id = old; })`. (A `replace` with a modified
    copy is also correct.)
  - The check compiles your function and confirms that a colliding change
    returns false and keeps the order with its old id.
AcceptIf: check(checks/l2-rollback.sh)
Concepts: M2C3, M2C4
Source: checks/tpl/l2-rollback.cpp (right answer and the plain-modify wrong answer run this session)

### CARD M2-Q2  [type: apply]  [topic: project]  [box: 1]
Q: Queue position. Write `int ahead(const Store& s, std::uint64_t id)` that
   returns how many orders on the same side and price arrived before order
   `id`. The store has tags `ById` (hashed_unique on id) and `ByBook`
   (ordered_non_unique on `key<&Order::side, &Order::price, &Order::ts>`).
Hint: project the id iterator into ByBook, then measure the distance from the
   first order at that (side, price).
Key:
  - `auto it = s.project<ByBook>(s.get<ById>().find(id));`
  - `auto first = s.get<ByBook>().lower_bound(std::make_tuple(it->side, it->price));`
  - `return std::distance(first, it);` (counting from `begin()` of the whole
    book is wrong, because it includes other prices and sides).
AcceptIf: check(checks/l2-ahead.sh)
Concepts: M2C2
Source: checks/tpl/l2-ahead.cpp (compared against a brute-force count, right and wrong answers run this session)

### CARD M2-Q3  [type: recall]  [topic: modify-cost]  [box: 1]
Q: Why is `modify()` that only changes `qty` slow in a store with a
   `hashed_non_unique` client index holding ~1,500 orders per client, and
   what are two fixes?
Key:
  - After modify, the hashed index cannot tell whether its key changed (the
    old value is gone), so it re-checks the element's position by walking
    its equal-key group: up to O(orders for that client).
  - Fix 1: use `replace` (it compares old and new keys and skips unchanged
    indices).
  - Fix 2: make the client index `ordered_non_unique`, whose position check
    compares the two neighbours only.
Concepts: M2C5
Source: CONCEPT M2C5

### CARD M2-Q4  [type: apply]  [topic: modify]  [box: 1]
Q: Different domain: a **risk-limit table**
   `multi_index_container<Limit, indexed_by<hashed_unique<key<&Limit::account>>,
   ordered_non_unique<key<&Limit::expiry>>>>`. A nightly job extends every
   expiry by one day with `modify()`. Can any limit be erased by this? Then:
   the team adds `hashed_unique<key<&Limit::desk_code>>` and the job also
   rewrites desk codes. What changes?
Key:
  - With only `account` unique and `expiry` non-unique, changing expiry can
    never collide, so modify cannot erase anything. It always returns true.
  - Once the job writes `desk_code` (a unique index), two limits mapped to
    the same desk code collide, and plain modify **erases** the second one.
    Use a rollback, or `replace`, and check the return value.
Concepts: M2C4
Source: CONCEPT M2C4 (ord_indices.html modify Effects)

### CARD M2-Q5  [type: recall]  [topic: keys]  [box: 1]
Q: You want one ordered index to give best bids (highest price first) and best
   asks (lowest price first), both with earliest time first. What key do you
   index on, and which extractor computes the price part?
Key:
  - A composite key (side, rank, ts), where `rank()` returns `-price` for bids
    and `+price` for asks, so smaller rank means better on both sides.
  - The price part is computed by `const_mem_fun<Order, std::int64_t, &Order::rank>`,
    written `key<&Order::rank>` in C++17.
Concepts: M2C1
Source: CONCEPT M2C1; labs/l2_keys.cpp

---

## Integrate (day close)

### CARD M2-INT  [type: synthesize]  [topic: all]
Q: Write the one-paragraph code-review comment you would leave on
   `store.get<ById>().modify(it, [&](Order& o){ o.cl_ord_id = new_id; o.qty = q; });`
   in a store with a unique (client, cl_ord_id) index and a hashed_non_unique
   client index.
Key:
  - A collision on (client, cl_ord_id) makes this modify **erase** the order.
    Add a rollback that restores both fields, or use replace, and handle a
    false return.
  - Performance: modify re-checks the hashed_non_unique client index by
    walking that client's equal-key group (up to O(group size)), so cost grows
    with the client's order count. replace avoids the walk when the client is unchanged.
Concepts: M2C3, M2C4, M2C5
Source: CONCEPT M2C3–M2C5

Next session: Lesson 03. You will measure node sizes and watch a
multi-millisecond insert hide behind a mean of tens of nanoseconds.
