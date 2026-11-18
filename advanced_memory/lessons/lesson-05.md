# Lesson 05 — Membership is a pointer test: intrusive lists over a pool (~30 min)

Your order sits in several lists at once: its price level, its client, "to be sent to venue X",
"waiting for an ack". The hot-path questions are "is THIS order in the pending-send list?" and
"take it out of everything, now". Lessons 01–04 gave containers a pooled memory supply. This
lesson flips the design: the **pool owns the orders**, the order **carries the links**, and the
lists allocate nothing. Cancel an order in O(1) before you read anything (1 minute):

```
bash labs/build.sh                 # once (about 35 s here); skip if you already built
./labs/build/l5_cancel
```

Numbers below: Apple M1 Max, macOS, Apple clang 21, Boost 1.90, `-std=c++23 -O2` (benches add
`-DNDEBUG`), measured 2026-11-14 on a **shared** Mac with a Time Machine backup running; median of
5 runs, each row in its own process (`bench/results_intrusive/RESULTS.md`). The timer step is
about 41 ns, and an empty timed region already costs about 21 ns on average, so single-operation
rows near 0–42 ns are at the floor. Re-run on your x86 box (64 B lines): the mechanisms carry
over, the numbers do not.

## Pretest — commit your guesses FIRST (no fair reading ahead)

### CARD 05-P1  [type: pretest]  [topic: membership]  [box: 1]
Q: Cold guess. You hold an `Order&`. Rank these answers to "is it in the pending-send list?" from
   cheapest to dearest per query: (a) `pending_ids.contains(o.id)` on a `std::unordered_set<long>`,
   (b) `std::find` in a `std::list<Order*>` of 50,000, (c) `o.send_hook.is_linked()`,
   (d) `o.state == State::PendingSend`.
Key:
  - (d) ≈ (c) < (a) < (b). Measured per batch of 64 random orders (p50): enum 83 ns, hook 125 ns,
    unordered_set 750 ns; ONE list find 66,083 ns. The hook is not faster than a flag: it reads
    the order's own bytes, like the flag. Why it measured slower (two compares, or its offset 80
    in the 112 B order vs the enum's 29) is not isolated. Its win is elsewhere (L5C2).
Concepts: L5C2
Source: bench/b4_membership.cpp ; bench/results_intrusive/RESULTS.md ; _evidence/r3/b4-offsets.txt

### CARD 05-P2  [type: pretest]  [topic: venue]  [box: 1]
Q: Cold guess. Four per-venue send lists all link orders through the SAME member hook
   `send_hook`. For an order queued for venue 1, what does `o.send_hook.is_linked()` say when you
   ask "is it queued for venue 3?"
Key:
  - `true`, and that is the wrong answer. A hook knows it is linked into SOME list through that
    hook, never which one. `labs/l5_membership` section 4 prints exactly this. CONCEPT L5C4.
Concepts: L5C4
Source: labs/l5_membership.cpp run 2026-11-14

### CARD 05-P3  [type: pretest]  [topic: linkmode]  [box: 1]
Q: Cold guess. With `link_mode<normal_link>` (the fastest mode), what does `hook.is_linked()` do?
Key:
  - It does not compile: a static assertion in `generic_hook.hpp` requires safe_link or
    auto_unlink. With normal_link nothing records "unlinked", so membership needs another source
    (a state field, a bit). CONCEPT L5C3.
Concepts: L5C3
Source: /opt/homebrew/include/boost/intrusive/detail/generic_hook.hpp line 204 ; labs/l5_link_modes.cpp -DBREAK_NORMAL_IS_LINKED

---

## Concept cards

### CONCEPT L5C1  Use it: cancel in O(1) with no allocation
`labs/l5_cancel.cpp` is the whole idea in 60 lines. Storage: `hft::slab<Order> orders(N)`, carved
ONCE from the thread arena (`labs/hft_mem/hft_slab.hpp`, lesson 06 opens it). Each `Order`
carries two `boost::intrusive::list_member_hook<>`s, `level_hook` and `client_hook`, so one order
sits in its price-level FIFO and its client list at the same time. An id index
(`unordered_flat_map<long, Order*>`, reserved) finds it. **Cancel** = id lookup →
`level.erase(level.iterator_to(*o))` → the same for the client list → `ids.erase(id)` →
`orders.destroy(o)`. `iterator_to` builds an iterator FROM the object, in constant time, so no
list is searched. The run prints `in level list? 1 → 0`, `level 18 size=156 → 155`, and
`operator new calls during the cancel: 0`.
Figure: one Order box in the slab, two hook boxes inside it; arrows from the level list and from the client list point INTO the order's hooks; the id map points at the order.
Elaborate: Why can `level.erase(level.iterator_to(*o))` be O(1) when `std::list::remove` is O(n)?
ElaborateKey:
  - The links live inside the order, so the order's address IS the node's address: the iterator is
    computed from it with no search. `std::list::remove` must walk to find the node holding the value.
HandsOn: Run `./labs/build/l5_cancel` and read the four lines.
HandsOnKey:
  - before: in both lists, level size 156. after: in neither, size 155. 0 operator new calls.
    Live orders 9999 of 10000, then 0 after the tidy shutdown.
Source: labs/l5_cancel.cpp run 2026-11-14 ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/obtaining_iterators_from_values.html

### CONCEPT L5C2  Membership is a hook test: one hook per list, one source of truth
Give the order **one hook per list it can join** (level, client, active, pending-send,
pending-ack). Then "is it in pending-send?" is `o.send_hook.is_linked()`: it reads the order's own
cache line and nothing else.

**Speed of the yes/no question** (`bench/b4_membership`, 100,000 orders, half pending, p50 per
batch of 64 random orders):
- enum compare 83 ns, bitmask 84, **hook 125**;
- `multi_index` find(id)→state 584, `unordered_set<id>` 750, `flat_set<id>` 3,541;
- one `std::find` in a 50,000-entry `std::list<Order*>`: 66,083 ns.

So the hook is **not** faster than a flag (why it is slower, instruction count or where in the
object it sits, is **not isolated**). *Checkpoint: if not speed, what could a hook win on?*

**It wins on two other things.**
1. **It cannot disagree with the list.** A hand-kept enum can: `l5_membership` section 3 queues
   an order and forgets the enum, and prints `state says PendingSend? 0  send hook says linked? 1`.
2. **Moving between states is O(1) and allocates nothing**: send → ack (unlink + push_back, batch
   of 16) p50 42 ns, against 459 ns for erase + insert in two `unordered_set`s (a node allocation
   per insert) and 33,833 ns for one find + erase + push in `std::list<Order*>`.
Figure: the order's bytes: [payload 32 B][level 16][client 16][active 16][send 16][ack 16] = 112 B; is_linked reads one 8-byte field inside it.
Elaborate: The enum is as fast as the hook for the yes/no question. Why keep the hook?
ElaborateKey:
  - The hook IS the list membership, so it cannot drift from the list; and the same hook gives O(1)
    unlink and next/prev for the transition, which a flag cannot.
HandsOn: Run `./labs/build/l5_membership` and find the line where the two sources of truth disagree.
HandsOnKey:
  - Section 3: `order 105: state says PendingSend? 0   send hook says linked? 1`.
Source: bench/b4_membership.cpp ; bench/results_intrusive/RESULTS.md ; labs/l5_membership.cpp run 2026-11-14 ; _evidence/r3/b4-offsets.txt

### CONCEPT L5C3  Which link modes make is_linked() trustworthy
Boost.Intrusive has three link modes; the default is **safe_link**. `is_linked()` has the
precondition "link_mode must be safe_link or auto_unlink" and returns "true, if the node belongs to
a container" (list_hook.hpp). **safe_link**: the hook starts unlinked, and erase "puts the erased
object in the well-known default state" (null links), so `is_linked()` goes 1 → 0 (`l5_link_modes`).
In builds without `NDEBUG` it also asserts on misuse (lesson 06). **normal_link**: nothing resets
the hook, `is_linked()` is a compile error (pretest 05-P3); a list destroyed while holding objects
leaves their hooks stale. **auto_unlink**: as safe_link, plus the hook's destructor unlinks itself
and `hook.unlink()` needs no list. It is a compile error with the default
`constant_time_size<true>` ("static assertion", docs and `-DBREAK_AUTO_UNLINK_CTS`), because the
list cannot keep a count while elements leave on their own; `size()` then walks the list.
Figure: three columns: safe_link (reset on erase, is_linked ok, asserts in debug), normal_link (no reset, no is_linked), auto_unlink (reset + self-unlink, no constant-time size).
Elaborate: In one sentence, what does `is_linked() == true` tell you, and what does it NOT?
ElaborateKey:
  - That this hook is currently in some container; not which container, and not which of several
    containers sharing the hook.
HandsOn: Compile `clang++ -std=c++23 -I/opt/homebrew/include -DBREAK_AUTO_UNLINK_CTS labs/l5_link_modes.cpp` and read the error.
HandsOnKey:
  - `list.hpp:125: error: static assertion failed ... !(constant_time_size && ... auto_unlink)`.
Source: /opt/homebrew/include/boost/intrusive/list_hook.hpp lines 144–151 ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/safe_hook.html ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/auto_unlink_hooks.html ; labs/l5_link_modes.cpp run 2026-11-14

### CONCEPT L5C4  The Venue X trap, and the hook as a state machine
One hook shared by N per-venue send lists answers "in SOME venue list". Asked "queued for venue
3?" about an order queued for venue 1, `is_linked()` says 1 (`l5_membership` section 4). Three
fixes, with their costs: **(a)** store the venue in the order (1 byte) and test
`is_linked() && o.venue == X`: O(1), but now two fields must agree; **(b)** one hook per venue:
exact, 16 B × N per order (4 venues = 64 B more); **(c)** walk from the node to the list header
and compare with each list's `end()`: no extra bytes, O(position): 100,001 steps took 205–280 µs
over 5 runs (one more run, on a busy machine, took 1,258 µs).
Sharing a hook is also a **design tool**: send and ack through ONE hook means an order can never
be in both at once (inserting a linked safe_link hook asserts: `l5_membership twice`, exit 134;
the `-DNDEBUG` build `l5_membership_ndebug twice` printed `survived the second push_back:
active.size()=2`, then crashed in `active.clear()`, exit 139). Separate hooks allow both at once. A status enum allows anything. The
**bitmask** (1 bit per list, `member_bits & 1`, 84 ns per 64) is the cheapest yes/no, but gives no
next/prev and no O(1) unlink.
Figure: four venue lists threading through the same send_hook slot of different orders; the order shows "linked" but not which thread.
Elaborate: Why does sharing ONE hook between the send and ack lists make an illegal state unrepresentable?
ElaborateKey:
  - A hook can be in one container at a time, so the order cannot be "pending send" and
    "pending ack" together; to move it you must unlink first, which is the transition.
HandsOn: Run `./labs/build/l5_membership twice; echo $?`.
HandsOnKey:
  - `Assertion failed: (!safemode_or_autounlink || node_algorithms::inited(to_insert)), function push_back`, exit 134.
Source: labs/l5_membership.cpp runs 2026-11-14 (asserts on and -DNDEBUG: _evidence/r3/labs-output-l5-l7.txt, walk: _evidence/r3/lab-repeats.txt) ; /opt/homebrew/include/boost/intrusive/detail/list_iterator.hpp line 118

### CONCEPT L5C5  Three models, counted
Same workload in `labs/l5_models` (10,000 inserts, 1,000 cancels). Columns: `operator new` per
insert / `operator delete` per cancel / bytes per order including setup and storage.
**VALUE** (container owns the Order): `std::map<id,Order>` 1 / 1 / 72 B; `std::list<Order>` per
level 1 / 1 / 48 B; reserved `flat_map<id,Order>` 0 / 0 / 40 B; `multi_index` (hashed id +
ordered px: ONE node with two index headers) 1 / 1 / 82 B. A value container gives one structure;
a second structure must hold a key or pointer. **POINTER** (`list<Order*>` + `unordered_map<id,it>`):
2 / 2 / 109 B, and every "next" is two dependent loads (node → Order). **INTRUSIVE** (slab +
2 hook lists + flat id map): 0 / 0 / 94 B, all allocated once at start; "next" is one load,
because the link sits inside the order. The cost is bytes **in the object**: 16 B per list it can
join. 5 hooks make a 32 B order 112 B: 1.14 orders per 128 B line instead of 4. Scanning 1,024
consecutive orders: 875 ns hooked vs 375 ns lean (p50).
Figure: three rows of boxes: VALUE (node contains order), POINTER (node → separate order), INTRUSIVE (order contains the links, containers are just headers).
Elaborate: Why does the pointer model pay TWO dependent loads per "next" and the intrusive model one?
ElaborateKey:
  - Pointer model: load the list node's next, then load the Order through the node's pointer.
    Intrusive: the next link already points at the next order's hook, inside the order.
HandsOn: Run `./labs/build/l5_models` and read the new/ins and B/total columns.
HandsOnKey:
  - std::map 1.00 / 72.0; flat_map 0.00 / 40.0; multi_index 1.00 / 81.9; POINTER 2.00 / 108.7; INTRUSIVE 0.00 / 94.4.
Source: labs/l5_models.cpp run 2026-11-14 ; bench/b4_membership.cpp scan rows ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/intrusive_vs_nontrusive.html

### CONCEPT L5C6  Exactly what is O(1), and what is not
**O(1) given the object:** `iterator_to` / static `s_iterator_to` (list, slist, set, multiset;
"Constant time" in list.hpp), `erase` from a list, next/prev, `is_linked()`, slab release.

**Not O(1):**
- lookup **by key**: `set::find` O(log n), 84 ns p50 for 50,000 ids in `bench/b6_handroll`;
  `unordered_set::find` average O(1), 41 ns, and only as good as the bucket array you supply
  (lesson 06);
- `unordered_set::erase(it)` too is "Average case O(1), worst case O(this->size())": its buckets
  are singly linked, so erase must find the predecessor in the chain;
- `slist::erase(it)` is "Linear to the elements before i" (91 ns mean vs 19 for `list` in b6);
- `size()` is O(n) with `constant_time_size<false>`; "which list am I in?" with a shared hook is
  O(position) (L5C4); destroying a safe_link list is O(n) (it unlinks every element).

*Checkpoint: cover the list above and name two things that are not O(1).*

**One catch:** erase on a Boost `list` needs the **list object** (to keep its size counter), so
you must know which list the order is in (`o.px` tells you the level). "Intrusive = O(1)
everything" is false: intrusive makes *membership operations given the object* O(1); keyed
lookup still needs an index.
Figure: two columns: given the object (O(1)) | given a key or a position (log n, average 1, or n).
Elaborate: Why can a hand-rolled circular list unlink with only the node, while `bi::list::erase` needs the list?
ElaborateKey:
  - With no size counter, unlinking touches only the neighbours. Boost's default
    `constant_time_size<true>` must decrement the list's count, so it needs the list object.
Source: /opt/homebrew/include/boost/intrusive/list.hpp lines 249–257, 1301–1309 ; /opt/homebrew/include/boost/intrusive/slist.hpp line 987 ; /opt/homebrew/include/boost/intrusive/hashtable.hpp line 2898 ; bench/results_intrusive/RESULTS.md ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/presenting_containers.html

---

## Completion cards

### CARD 05-C1  [type: completion]  [topic: linkmode]  [box: 1]
Q: Fill the gap with one link mode: "`is_linked()` compiles with `auto_unlink` or with ______,
   which is also the default."
Key:
  - safe_link
AcceptIf: regex(/^\s*`?(bi::|boost::intrusive::)?safe_link`?\s*\.?\s*$/i)
RejectIf: regex(/normal/i)
Concepts: L5C3
Source: CONCEPT L5C3

### CARD 05-C2  [type: completion]  [topic: cancel]  [box: 1]
Q: Complete `cancel(Book& b, long id)`. `Book` holds `hft::slab<Order> orders`, `Level levels[64]`,
   `Client clients[8]` (the two intrusive list types) and `ids` (id → `Order*`); the order has
   `px` and `client`. Write the ONE missing statement:
   ```
   Order* o = b.ids.find(id)->second;
   b.levels[o->px].erase(Level::s_iterator_to(*o));
   ______                                    // take it out of its client list, O(1)
   b.ids.erase(id);
   b.orders.destroy(o);
   ```
   (Graded by compiling the completed body with asserts on.)
Key:
  - `b.clients[o->client].erase(Client::s_iterator_to(*o));`
  - Skip it, or unlink from the wrong list, and `destroy` asserts: the order is still linked.
AcceptIf: check(checks/l5_cancel_fill.sh)
Concepts: L5C1, L5C6
Source: checks/l5_cancel_fill.sh ; checks/l5_cancel.sh ; labs/l5_cancel.cpp

---

## Question cards

### CARD 05-Q1  [type: apply]  [topic: membership]  [box: 1]
Q: Code. `struct Order { long id; State state; bi::list_member_hook<> send_hook; bi::list_member_hook<> ack_hook; };`
   Write the body of `bool in_pending_send(const Order& o)`. Warning: some code paths forget
   to update `state`. (Graded by compiling and running it.)
Key:
  - `return o.send_hook.is_linked();`
AcceptIf: check(checks/l5_membership.sh)
Concepts: L5C2, L5C3
Source: checks/l5_membership.sh ; CONCEPT L5C2

### CARD 05-Q2  [type: apply]  [topic: cancel]  [box: 1]
Q: Code. `struct Book { hft::slab<Order> orders; Level levels[64]; Client clients[8];
   boost::unordered_flat_map<long, Order*> ids; };` with `Order { long id; int px; int client;
   long qty; level_hook; client_hook; }`. Write the body of `void cancel(Book& b, long id)`:
   out of every structure, slot back to the slab, nothing allocated, O(1): no loop, no search.
   (Compiled with asserts on; levels hold several orders.)
Key:
  - `Order* o = b.ids.find(id)->second;`
    `b.levels[o->px].erase(Level::s_iterator_to(*o));`
    `b.clients[o->client].erase(Client::s_iterator_to(*o));`
    `b.ids.erase(id); b.orders.destroy(o);`
  - Forget one unlink and `destroy` asserts (safe_link): the order is still in a list.
AcceptIf: check(checks/l5_cancel.sh)
Concepts: L5C1, L5C6
Source: checks/l5_cancel.sh ; labs/l5_cancel.cpp

### CARD 05-Q3  [type: recall]  [topic: complexity]  [box: 1]
Q: From memory: name three operations an intrusive design makes O(1) given the object, and three
   that stay O(log n), average O(1) or O(n).
Key:
  - O(1) given the object: unlink/erase from a list (via iterator_to / s_iterator_to), next/prev,
    is_linked() (safe/auto mode), moving between lists, slab release.
  - Not O(1): find by key in an intrusive set (log n) or unordered_set (average 1, depends on your
    buckets); slist erase (linear in position); size() without constant_time_size; "which list"
    with a shared hook; destroying a safe-mode list (n).
Concepts: L5C6
Source: CONCEPT L5C6

### CARD 05-Q4  [type: apply]  [topic: venue]  [box: 1]
Q: Code. All per-venue send lists share `send_hook`; the order has `std::uint8_t venue`. Write the
   body of `bool queued_for(const Order& o, int v)`.
Key:
  - `return o.send_hook.is_linked() && o.venue == v;`
  - `is_linked()` alone says "some venue", which is the trap.
AcceptIf: check(checks/l5_venue.sh)
Concepts: L5C4
Source: checks/l5_venue.sh ; CONCEPT L5C4

### CARD 05-Q5  [type: recall]  [topic: models]  [box: 1]
Q: From memory: per insert, how many heap allocations do (a) `multi_index` with two indices,
   (b) `std::list<Order*>` + `unordered_map<id, iterator>`, (c) intrusive lists over a slab make,
   and what does (c) pay instead?
Key:
  - (a) 1 (one node holding both index headers); (b) 2 (list node + hash node; 3 if the Order is
    also `new`ed); (c) 0.
  - (c) pays bytes inside the object: 16 B per list it can join (l5_models: 0.00 new/ins, 94 B/order total).
Concepts: L5C5
Source: labs/l5_models.cpp run 2026-11-14

### CARD 05-Q6  [type: apply]  [topic: membership]  [box: 1]
Q: Transfer. A risk engine keeps 200,000 accounts. Two questions per tick: (1) "is account A in
   the breached set?" (yes/no, millions of times), (2) every second, walk all breached accounts
   and clear the ones back under limit. Hook, bitmask, or both? Why?
Key:
  - Both, or a hook alone: (2) needs to walk only the breached accounts and unlink each in O(1),
    which is a hook's job; (1) can be the hook's `is_linked()` (measured 125 ns per 64 here) or a
    bit if you want the cheapest test. A bit alone forces a scan of 200,000 accounts for (2).
Concepts: L5C2, L5C4
Source: CONCEPT L5C2 ; CONCEPT L5C4 ; bench/results_intrusive/RESULTS.md

### CARD 05-Q7  [type: apply]  [topic: venue]  [box: 1]
Q: Transfer. A market-data fan-out has 64 gateways; each symbol may be subscribed by any subset.
   Someone proposes "one intrusive list hook per gateway inside each Symbol". Cost it, and propose
   the cheaper membership answer.
Key:
  - 64 hooks × 16 B = 1,024 B per symbol, eight 128 B lines of links per symbol, most of them idle.
  - A `std::uint64_t` subscription bitmask answers "is gateway g subscribed?" in O(1) with 8 B; keep
    per-gateway iteration in a per-gateway array of symbol ids (or hooks only for the few hot gateways).
Concepts: L5C4, L5C5
Source: CONCEPT L5C4 ; CONCEPT L5C5

### CARD 05-Q8  [type: recall]  [topic: linkmode]  [box: 1]
Q: From memory: which link modes make `is_linked()` usable, what does `true` tell you, and which
   question about the order does it leave open?
Key:
  - safe_link (the default) and auto_unlink; with normal_link it does not compile.
  - `true` = this hook is currently in some container. It does not say which container, nor which
    of several lists sharing the hook (the Venue X trap).
Concepts: L5C3, L5C4
Source: CONCEPT L5C3 ; CONCEPT L5C4

---

## Integrate (day close — one synthesis question)

### CARD 05-INT  [type: synthesize]  [topic: all]
Q: An order can be in: its price level, its client list, the pending-send list of exactly one
   venue (of 4), and the pending-ack list. Design the Order's hooks and fields so that every
   membership question is O(1) and correct, "pending-send and pending-ack at once" is impossible,
   and say what it costs per order in bytes.
Key:
  - level_hook + client_hook (2 × 16 B); ONE state hook shared by the 4 venue send lists AND the
    ack list (16 B), so the order is in at most one of them; a 1-byte venue field.
  - Membership: level/client/state via is_linked() (safe_link); "queued for venue X" =
    state-hook linked && venue == X && not in ack: so also keep a 1-byte "which state list" tag
    (or give ack its own hook and accept that both-at-once is then representable).
  - Cost: 48 B of hooks + 2 B of tags on top of the payload; all transitions O(1), no allocation.
Concepts: L5C2, L5C3, L5C4, L5C5
Source: CONCEPT L5C2 ; CONCEPT L5C4
