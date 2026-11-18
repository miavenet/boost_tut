# Lesson 06 — Inside the hooks and the pool: building it right (~30 min)

Lesson 05 used intrusive lists over `hft::slab`. Today you open both: what a slab is, what each
hook costs here, how the hashed and tree containers want to be fed, and five ways to corrupt the
whole thing, each one broken on purpose and fixed. Start by breaking it (1 minute):

```
./labs/build/l6_traps linked-destroy ; echo "exit=$?"
./labs/build/l6_traps_ndebug linked-destroy ; echo "exit=$?"
```

Same machine and caveats as lesson 05 (M1 Max, Apple clang 21, Boost 1.90, shared Mac, median of
5 runs in `bench/results_intrusive/RESULTS.md`, timer floor about 21 ns mean).

## Pretest — commit your guesses FIRST (no fair reading ahead)

### CARD 06-P1  [type: pretest]  [topic: hooks]  [box: 1]
Q: Cold guess. On this 64-bit machine, how many bytes is `bi::set_member_hook<>` (red-black tree
   hook), and with `optimize_size<true>`?
Key:
  - 32 B (4 pointers) and 24 B (3 pointers): optimize_size packs the colour bit into the parent
    pointer. A list hook is 16 B, an unordered_set hook 8 B, an slist hook 8 B. CONCEPT L6C2.
Concepts: L6C2
Source: labs/l6_hooks.cpp run 2026-11-14

### CARD 06-P2  [type: pretest]  [topic: buckets]  [box: 1]
Q: Cold guess. A Boost.Intrusive `unordered_set` gets a 64-bucket array, then 50,000 orders are
   inserted. What happens to the bucket count, and to `find(id)`?
Key:
  - Nothing grows: "not rehashed to maintain a load factor". bucket_count stays 64 (load factor
    781) and mean find(id) was about 2,710 ns; after `rehash()` onto a 65,536-bucket array you supply,
    1.5–1.6 ns. CONCEPT L6C3.
Concepts: L6C3
Source: labs/l6_buckets.cpp runs 2026-11-14 (_evidence/r3/lab-repeats.txt) ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/unordered_set_unordered_multiset.html

### CARD 06-P3  [type: pretest]  [topic: traps]  [box: 1]
Q: Cold guess. You `slab.destroy()` an order that is still in a price level, then the next
   `create()` reuses its slot. What happens in a debug build, and in a `-DNDEBUG` build?
Key:
  - Debug: the safe_link hook's destructor asserts (`!hook.is_linked()`), exit 134.
  - `-DNDEBUG`: no check; the new order reused the slot ("yes") and the program then crashed
    (exit 139) while the level was walked or cleared. CONCEPT L6C5.
Concepts: L6C5
Source: labs/l6_traps.cpp runs 2026-11-14 (both builds)

---

## Concept cards

### CONCEPT L6C1  The pool: hft::slab<T>
`labs/hft_mem/hft_slab.hpp` (205 lines, added to the library). Construction carves ONE block,
`capacity` slots of `sizeof(T)` bytes then a 4-byte generation per slot, from the current thread's
arena (bulk layer → monotonic → pre-faulted region; no `operator new`, `l6_slab` prints 0 calls).
A **free list is threaded through the free slots** (a free slot's first bytes hold `next`), so
`create()` is a pop and `destroy()` a push: O(1), no search, no lock, single thread. It is
threaded front to back, so the first objects are adjacent (`b - a = 64 B`). **LIFO**: `destroy(a)`
then `create()` returns a's slot. **Exhaustion** never falls back to malloc: `try_create()`
returns nullptr (reject the order), `create()` throws `std::bad_alloc`; a throwing constructor
leaves the slot free. The generation is bumped by `create()` AND `destroy()` (odd = live), so a
checked build aborts on a double `destroy()` and no handle ever matches a free slot. The slab must
die before its arena and aborts (checked builds) if destroyed with live objects. LIFO vs a FIFO
ring on a 64 MB slab half full, 6 interleaved runs: 12.2–12.8 vs 11.7–12.2 ns per
destroy+create+write. Isolated in the same runs: the same churn without create's generation store
took 10.4–10.6 ns (the pre-fix slab 10.3–10.5), so that one store costs about 1.7 ns here, the
price of the double-destroy check. The FIFO ring does none of the slab's bookkeeping, so LIFO vs
FIFO itself is still **not isolated**.
Figure: slot array [A][free][B][free][free]; head → free → free → null threaded through the free slots; generations below each slot.
Elaborate: Why does the slab need no memory of its own for the free list?
ElaborateKey:
  - A free slot holds no object, so its first bytes can hold the "next free" pointer; the list
    costs nothing and freeing needs nothing.
HandsOn: Run `./labs/build/l6_slab`.
HandsOnKey:
  - 0 operator new; slots adjacent (64 B); LIFO yes; full → try_create nullptr, create → bad_alloc;
    then the LIFO/FIFO line.
Source: labs/hft_mem/hft_slab.hpp ; labs/l6_slab.cpp runs 2026-11-14 (_evidence/r3/lab-repeats.txt) ; tests/run.sh modes slab-*

### CONCEPT L6C2  Hooks: base or member, tags, and their bytes here
Four chunks; pause after each.

**1. Two ways to attach a hook.**
- **Base hook**: `struct Order : bi::list_base_hook<bi::tag<by_level>>, bi::list_base_hook<bi::tag<by_client>>`;
  two base hooks of one kind "must" have different tags; the list names the hook with
  `bi::base_hook<...>`.
- **Member hook**: `bi::list_member_hook<> level;` and the list names it with
  `bi::member_hook<Order, Hook, &Order::level>`: no inheritance, no tags. This course uses member hooks.

*Checkpoint: which of the two needs tags, and when?*

**2. The rule both share.** **The type must name every container it can join** (coupling), and
an object cannot be in one container twice through one hook.

**3. Bytes per hook here** (`l6_hooks`), smallest first:
- slist 8, unordered_set 8 (16 with `store_hash` or `optimize_multikey`).
- list 16.
- set 32 (24 with `optimize_size<true>`), avl_set 32 (24), bs_set 24 (splay / scapegoat / treap
  trees default to it).

*Checkpoint: cover the list and say the list, set and unordered_set hook sizes.*

**4. Bytes per container header:** list 24 B (16 with `constant_time_size<false>`), slist 16 (24
with `cache_last<true>`, needed for push_back), set 40. And the trees Boost offers that you would
not want to write: rbtree (set), AVL, splay, scapegoat, treap.
Figure: Order layout with hooks laid out after the payload; a list header pointing into the hook, not into the object start.
Elaborate: Base or member hook: which keeps your Order an aggregate-free plain struct, and why might you still pick base?
ElaborateKey:
  - Member hooks leave the class hierarchy alone and need no tags. Base hooks make the hook part of
    the type's identity (useful when one hook type per class is natural, e.g. one list only).
HandsOn: Run `./labs/build/l6_hooks` and write down the list, set and unordered_set hook sizes.
HandsOnKey:
  - 16, 32 (24 with optimize_size), 8.
Source: labs/l6_hooks.cpp run 2026-11-14 ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/usage.html ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/set_multiset.html ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/presenting_containers.html ; /opt/homebrew/include/boost/intrusive/bstree.hpp line 66

### CONCEPT L6C3  Lookup by key: buckets you own, trees, and the id map next door
An intrusive `unordered_set` takes **your bucket array** (`bucket_traits(ptr, n)`); "the contents
... are not rehashed to maintain a load factor"; rehash happens only when you call
`rehash(new_traits)`, and "the bucket array must be destroyed after the container". With
`power_2_buckets<true>` it masks instead of dividing (you guarantee the power of two). A too-small
array never throws and never allocates: it just gets slow (mean find 2,696–2,743 ns vs 1.5–1.6 ns,
64 vs 65,536 buckets, dense ids, 6 runs). `key_of_value<id_of>` lets you call `find(42)` with a
plain id; for a different key type pass `find(key, hasher, equal)`. A `set` by id is O(log n):
57–62 ns mean here (56.6–61.5 over the same 6 runs).
The **id → Order*** index is often a `boost::unordered_flat_map`. Trap, isolated in
`l6_flat_drift`: reserved for exactly the 50,000 live ids, steady erase+insert at constant size
still allocated a new 1 MiB table 9 times in 200,000 operations (bucket count unchanged), because
`max_load` "may decrease on erasure under high-load conditions". p99.9 stayed 125 ns, **max was
531 µs**. Reserved 2× (100,000): 0 allocations, max 1,750 ns. `hft_book.hpp` first reserved 1×
capacity and hit exactly this: filled to capacity under `seal()`, churn threw `bad_alloc` at op
21,508; it now reserves 2× and the same run makes 200,000 cancel+add with the bucket count
unchanged (`bash tests/run.sh`, mode `book-churn-full`).
Figure: bucket array (yours, fixed) → chains threaded through each order's 8-byte hook.
Elaborate: Why can't an intrusive unordered_set grow on its own?
ElaborateKey:
  - It allocates nothing: the bucket array belongs to you, so only you can supply a bigger one.
HandsOn: Run `./labs/build/l6_flat_drift`.
HandsOnKey:
  - `reserve(50000): ... operator new in churn: 9 (each 1048576 B, first at op 20686)`;
    `reserve(100000): ... 0`.
Source: labs/l6_buckets.cpp, labs/l6_flat_drift.cpp runs 2026-11-14 (_evidence/r3/lab-repeats.txt) ; _evidence/r3/book-churn.txt ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/unordered_set_unordered_multiset.html ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/map_multimap.html ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/advanced_lookups_insertions.html ; https://www.boost.org/doc/libs/1_90_0/libs/unordered/doc/html/unordered/reference/unordered_flat_map.html ; bench/results_intrusive/RESULTS.md

### CONCEPT L6C4  Disposers and who owns what
The container **never owns** anything: `erase` only unlinks. To unlink AND release, pass a
disposer: `ids.erase_and_dispose(it, [&](Order* o){ pool.destroy(o); })`,
`lvl.clear_and_dispose(...)`, `lvl.remove_and_dispose_if(pred, disposer)`; the disposer runs
after the element is removed. Teardown order: the slab (owner) must be declared **before** the
containers that link its objects, so it dies after them; unlink everything, then destroy. A
safe_link (or auto_unlink) list's destructor unlinks every element, "Linear to the number of
elements": 1,426–1,458 µs for 1,000,000 orders here (12 runs, both builds); normal_link: 0.0–1.7
µs, but every hook is left holding links into a dead list. Boost containers are move-only
(`BOOST_MOVABLE_BUT_NOT_COPYABLE`, list.hpp line 1472), and objects need no copy constructor to
be stored; copying a hooked object copies the payload, not the membership (the hook's copy
constructor leaves the copy unlinked; `hr::dlink`/`hr::hlink` now do the same).
Figure: slab (owns) ← orders ← hooks ← lists (link only); arrows for destruction order: lists first, slab last.
Elaborate: Why must the disposer, not the list, call `pool.destroy`?
ElaborateKey:
  - The list does not know where the object came from (slab, stack, vector); only you do, so you
    hand that knowledge in as the disposer.
HandsOn: Run `./labs/build/l6_traps container-dies`.
HandsOnKey:
  - `~list with 1,000,000 orders: safe_link ~1400 us, normal_link ~0 us`, and the safe list left all
    orders unlinked (live 1000000) so the slab could take them back.
Source: https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/erasing_and_disposing.html ; /opt/homebrew/include/boost/intrusive/list.hpp lines 249–257, 1472 ; _evidence/r3/lab-repeats.txt ; _evidence/r3/labs-output-l5-l7.txt ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/intrusive_vs_nontrusive.html ; labs/l6_traps.cpp run 2026-11-14

### CONCEPT L6C5  Five traps, broken and fixed (labs/l6_traps)
| trap | debug build (asserts) | `-DNDEBUG` build | fix |
|---|---|---|---|
| `linked-destroy`: destroy while linked | assert in `~hook`, exit 134 | slot reused, crash later, exit 139 | unlink first (`erase_and_dispose`) |
| `order-dies-first`: object declared after its list | assert, exit 134 | exit 0 (silent; the list's destructor then clears a dead object's hook, list.hpp 251–256) | declare the owner first |
| `autounlink`: auto_unlink surprises | copy is NOT linked; a temporary left the list "by itself" (size 2 → 1) | same | prefer safe_link; never link temporaries |
| `aba`: raw pointer after reuse | old pointer now reads id=8 (another order) | same | keep a `slab::handle` {index, generation}; `get()` → nullptr |
| `iterate`: erase inside range-for | segfault, exit 139 | segfault, exit 139 | `it = l.erase(it)` or `remove_and_dispose_if` |

Note the release column: **safe_link's checks are `BOOST_ASSERT`, which compiles to nothing with
`NDEBUG`**; in release, safe_link still resets hooks on erase (so `is_linked()` works) but no
longer catches misuse. Test with asserts on. Erase-inside-range-for crashes because erase resets
the hook to null links (safe mode, `circular_list_algorithms::init`) and the loop's `++` then
follows a null `next`.
Figure: the table above.
Elaborate: Why does the generation in the handle catch the ABA case when the pointer cannot?
ElaborateKey:
  - The slot (address) is the same for the old and the new order; only the generation changed on
    destroy(), so comparing it tells "same object" from "same address".
HandsOn: Run `./labs/build/l6_traps aba` and `./labs/build/l6_traps iterate-fixed`.
HandsOnKey:
  - aba: `raw pointer to order 7 now reads id=8 ... handle ... get() = nullptr`. iterate-fixed: `level: 1 3 5`.
Source: labs/l6_traps.cpp runs 2026-11-14 (both builds) ; /opt/homebrew/include/boost/intrusive/detail/generic_hook.hpp lines 43–48 ; /opt/homebrew/include/boost/assert.hpp ; /opt/homebrew/include/boost/intrusive/circular_list_algorithms.hpp lines 70–75 ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/safe_hook.html

### CONCEPT L6C6  Index links: 32-bit slot numbers instead of pointers
If every object lives in ONE slab, a link can be a 32-bit **slot number**: `struct ilink {
uint32_t prev, next; }`, 8 B instead of 16 (`labs/hft_mem/hft_handroll.hpp`, `hr::idx_list`).
With three hooks an order shrinks from 80 B to 56 B: 2.29 orders per 128 B line instead of 1.60.
It is **relocatable**: memcpy the slab elsewhere (snapshot, shared memory, file) and the links
still hold (`l6_index_list`); a pointer-linked copy still points into the old array. Boost's own
relocatable option is `void_pointer<offset_ptr<void>>` hooks (also intact after memcpy here, but
16 B). The price: slot × stride arithmetic on every step and explicit nil checks. Generated code:
`idx_erase` 22 instructions, 5 branches vs `hand_erase` 5 / 1 (`bench/results_intrusive/asm.md`);
walking a list of about 49 was 209 vs 167 ns p50 (`b6_handroll`, every run). The 4 B id fits the
slab, not the process: at most 2^32 − 2 slots.
Figure: slab with slots 0..7; links drawn as small integers (1 → 3 → nil) instead of arrows.
Elaborate: Why does relocation break a pointer list but not an index list?
ElaborateKey:
  - Pointers hold absolute addresses of the old copy; indices are offsets from whatever base the
    slab now has.
HandsOn: Run `./labs/build/l6_index_list`.
HandsOnKey:
  - 16 vs 8 B per hook, 80 vs 56 B per order; the COPY walks `100 102 104 106` with indices; the
    pointer copy says "next is in the OLD array"; the offset_ptr copy walks fine at 16 B per hook.
Source: labs/l6_index_list.cpp run 2026-11-14 ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/using_smart_pointers.html ; bench/results_intrusive/asm.md ; bench/results_intrusive/RESULTS.md

---

## Completion cards

### CARD 06-C1  [type: completion]  [topic: traps]  [box: 1]
Q: Fill the gap with one word: "Call `slab.destroy(o)` only after `o` is ______ from every
   container it is in."
Key:
  - unlinked (or erased / removed / detached)
AcceptIf: regex(/^\s*(unlinked|erased|removed|detached|unhooked)\s*\.?\s*$/i)
RejectIf: regex(/linked\s+to|inserted/i)
Concepts: L6C4, L6C5
Source: CONCEPT L6C5

### CARD 06-C2  [type: completion]  [topic: index]  [box: 1]
Q: Complete the index-linked list's `void erase(std::uint32_t i)` (members `T* base_;
   std::uint32_t head_, tail_;`, link of slot i is `base_[i].*M`, `nil` = no neighbour). The
   line for the PREV neighbour is given; write the ONE missing line for the NEXT neighbour:
   ```
   ilink& l = base_[i].*M;
   if (l.prev != nil) (base_[l.prev].*M).next = l.next; else head_ = l.next;
   ______
   l.prev = l.next = nil;
   ```
Key:
  - `if (l.next != nil) (base_[l.next].*M).prev = l.prev; else tail_ = l.prev;`
  - The mirror image of the given line: forget the `else tail_` and erasing the last slot leaves
    the tail pointing at a freed slot.
AcceptIf: check(checks/l6_idx_fill.sh)
Concepts: L6C6
Source: checks/l6_idx_fill.sh ; checks/l6_idx_erase.sh ; labs/hft_mem/hft_handroll.hpp

---

## Question cards

### CARD 06-Q1  [type: recall]  [topic: hooks]  [box: 1]
Q: From memory: bytes per hook here for list, slist, set (default and optimize_size) and
   unordered_set, and the one rule for two BASE hooks of the same kind.
Key:
  - list 16, slist 8, set 32 / 24 with optimize_size, unordered_set 8.
  - Two base hooks of one kind need different `tag<>`s (member hooks need none).
Concepts: L6C2
Source: labs/l6_hooks.cpp run 2026-11-14

### CARD 06-Q6  [type: apply]  [topic: index]  [box: 1]
Q: Code. Write the body of the index-linked list's `void erase(std::uint32_t i)`. Members:
   `T* base_; std::uint32_t head_, tail_;`, slot i's link is `base_[i].*M` with `.prev/.next`,
   `nil` = no neighbour; leave slot i's links nil.
Key:
  - `ilink& l = base_[i].*M;`
    `if (l.prev != nil) (base_[l.prev].*M).next = l.next; else head_ = l.next;`
    `if (l.next != nil) (base_[l.next].*M).prev = l.prev; else tail_ = l.prev;`
    `l.prev = l.next = nil;`
AcceptIf: check(checks/l6_idx_erase.sh)
Concepts: L6C6
Source: checks/l6_idx_erase.sh ; labs/hft_mem/hft_handroll.hpp

### CARD 06-Q2  [type: recall]  [topic: buckets]  [box: 1]
Q: From memory: three rules for the bucket array of an intrusive `unordered_set`, and what a
   too-small array does to latency.
Key:
  - You supply it (bucket_traits); it is never rehashed for you (call `rehash()` with a new array);
    it must outlive the container (and is not shared between containers); power_2_buckets needs a
    power of two.
  - Too small: no error, no allocation, long chains: mean find about 2,710 ns with 64 buckets vs 1.5–1.6 ns with 65,536 here.
Concepts: L6C3
Source: CONCEPT L6C3

### CARD 06-Q5  [type: apply]  [topic: aba]  [box: 1]
Q: Code. A cancel request carries `hft::slab<Order>::handle h`, taken when the order was created.
   Write the statement(s) for `cancel_by_handle(pool, h)` that cancel it only if that same order
   is still alive (its slot may now hold another order).
Key:
  - `if (Order* o = pool.get(h)) pool.destroy(o);`
AcceptIf: check(checks/l6_handle.sh)
Concepts: L6C1, L6C5
Source: checks/l6_handle.sh ; labs/hft_mem/hft_slab.hpp

### CARD 06-Q3  [type: apply]  [topic: dispose]  [box: 1]
Q: Code (one statement). Empty the price level `lvl` AND hand every order back to `pool`
   (`hft::slab<Order>`).
Key:
  - `lvl.clear_and_dispose([&](Order* o) { pool.destroy(o); });`
AcceptIf: check(checks/l6_dispose.sh)
Concepts: L6C4
Source: checks/l6_dispose.sh ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/erasing_and_disposing.html

### CARD 06-Q7  [type: apply]  [topic: index]  [box: 1]
Q: Transfer. Your matching engine must publish a consistent snapshot of the whole book to a risk
   process through shared memory every 100 ms, by copying the order slab. Pointer hooks, offset_ptr
   hooks, or index links? Name the cost of your choice.
Key:
  - Index links (or offset_ptr hooks): a memcpy'd slab keeps working at the new address; raw
    pointer hooks point into the writer's memory.
  - Index links: 8 B per hook and relocatable, but every step is base + i × stride and needs nil
    checks (22 vs 5 instructions for erase here). offset_ptr: Boost does it for you, 16 B per hook.
Concepts: L6C6
Source: CONCEPT L6C6 ; labs/l6_index_list.cpp

### CARD 06-Q4  [type: recall]  [topic: traps]  [box: 1]
Q: From memory: the five traps of intrusive-over-a-pool, and which ones safe_link catches in a
   debug build.
Key:
  - Destroy while linked (caught: assert); object dies before its list (caught: assert);
    auto_unlink surprises (not a bug Boost can see); stale pointer after slot reuse (not caught:
    use a generation handle); erase inside a range-for (not caught: crash).
  - With -DNDEBUG none is caught.
Concepts: L6C5
Source: CONCEPT L6C5

---

## Integrate (day close — one synthesis question)

### CARD 06-INT  [type: synthesize]  [topic: all]
Q: Review: "Orders come from `hft::slab<Order>`; levels are `bi::list` with normal_link for speed;
   cancel does `slab.destroy(o)` then `level.erase(level.iterator_to(*o))`; the id index is a
   `boost::unordered_flat_map` reserved for the expected peak of live orders; clients keep raw
   `Order*` to cancel later." Name every problem and its fix.
Key:
  - Destroy before unlink: in debug (safe_link) an assert; with normal_link no check at all, and
    after slot reuse the level contains a different order. Unlink first, then destroy (or erase_and_dispose).
  - normal_link: no is_linked(), no reset, no debug checks; the measured speed difference vs
    safe_link was below the timer here (one extra store in the code): keep safe_link.
  - Flat map reserved for exactly the peak: under churn it rehashes (1 MiB, max 531 µs here): reserve with headroom (2×).
  - Raw `Order*` held by clients: ABA after slot reuse; hand out `slab::handle` and use `get()`.
Concepts: L6C1, L6C3, L6C4, L6C5
Source: CONCEPT L6C3 ; CONCEPT L6C5 ; bench/results_intrusive/asm.md
