# Assessment — flat-containers-delayed-1

Time: about 8 minutes. Stop and resume after any response.
Prerequisites: L1C3, L1C2, L2C1, L2C3, L3C4, L4C4, L4C5
Write every response before opening the keys. Record the actual date and time.
Reusing this form means its transfer tasks are exposed, not novel.

## A1-R — retention — L1C3
Without notes: at N = 1,048,576 with 8-byte keys and values and -DNDEBUG, order boost::unordered_flat_map, std::unordered_map, boost::container::flat_map and std::map from fastest to slowest successful point lookup, and give the mechanism that separates the two tree/array structures from the two hash tables.

## A1-T — transfer — L1C3
Different setting: a compiler team posts 'flat_map is a myth: our 4,096-entry symbol table with 64-byte entries looked up 83 ns with boost::container::flat_map vs 66 ns with std::map. Built with clang++ -O2 bench.cpp.' Diagnose what they measured, say what to change, and predict the direction of the result after the change.

## A2-R — retention — L1C2
Without notes: give the formula for the bytes one flat_map erase moves (no reallocation), and explain why filling a flat_map with N keys in random order by single inserts is quadratic. Name one bulk remedy.

## A2-T — transfer — L1C2
New domain: a log-ingest service keeps flat_map<uint32_t, uint32_t> (8-byte pairs) holding 200,000 entries with spare capacity. Each second a batch of 1,000 new keys arrives, landing at random positions. Estimate the bytes moved per batch if inserted one by one, then give the cheaper way to apply the batch and why it is cheaper.

## A3-R — retention — L2C1
Without notes: a reference into a Boost flat_map is kept across an insert. Name the two different ways this can go wrong, say which one AddressSanitizer reports, which one reserve() prevents, and give one repair.

## A3-T — transfer — L2C1
New domain: a game engine stores flat_map<EntityId, Transform> world. At the start of each frame the physics system caches Transform* t = &world.find(player)->second; during the frame, spawn() inserts new entities with smaller ids. Predict the two symptoms you may see (depending on capacity) and give a fix that would pass under AddressSanitizer.

## A4-R — retention — L2C3
Without notes: for Boost flat_map vs C++23 std::flat_map, state (a) how each stores its elements, (b) each one's reference type, (c) each one's 'already sorted and unique' tag, and (d) how each hands its storage out and back in.

## A4-T — transfer — L2C3
Porting task: a config loader written for boost::container::flat_map<int, Cfg> does (1) m.reserve(n); (2) fills a sequence_type and calls m.adopt_sequence(ordered_unique_range, std::move(seq)); (3) auto& e = *m.begin(); e.second.x = 1;. You switch the type to std::flat_map<int, Cfg>. For each line, say whether it still compiles and what you write instead.

## A5-R — retention — L3C4
Without notes: a flat_map backed by static_vector<pair<K,T>, N> is full and gets one more insert. With default Boost settings, what is thrown, which catch types see it, what macro changes that, and what happens if overflow checking is turned off?

## A5-T — transfer — L3C4
New domain: a browser keeps, per tab, a flat_map of open WebSocket subscriptions; the spec caps it at 8, and the code is try { subs.emplace(id, s); } catch (const std::bad_alloc&) { reject(); }. Choose the underlying sequence, review the catch, and say what you would choose instead if the cap were only 'usually 8'.

## A6-R — retention — L4C4
Without notes: for flat_map, std::map, boost::unordered_flat_map and a direct array, state the insert/erase cost and whether references stay stable, then name the needs that still make flat_map the right choice.

## A6-T — transfer — L4C4
New domain: a ride-hailing dispatch service keeps ~2M active driver records keyed by random 64-bit ids. Drivers log on and off ~50,000 times per second (inserts and erases at random ids), lookups are by id only, and other services hold pointers to records. A teammate proposes flat_map 'because it is cache-friendly'. Decide and justify with a mechanism and a measured number from the course.

## A7-R — retention — L4C5
Without notes: name the three kinds of tail spike a table write can cause that the course measured, the container where each shows up, and the fix for each.

## A7-T — transfer — L4C5
New domain, hypothetical numbers: an ad-auction server loads a 500,000-entry flat_map of campaign budgets at startup with adopt_sequence followed by shrink_to_fit. New campaigns arrive mid-day at random key positions. Its latency report shows: p50 and mean unchanged; on the first new campaign of the day a ~1 ms max plus 1 heap allocation in the request path; on later new campaigns a smaller but still large max and 0 allocations. Attribute each spike and remove both.

# Keys — open after all responses

## A1-R
- Fastest to slowest: boost::unordered_flat_map (~12 ns), std::unordered_map (~25), flat_map (~163), std::map (~424) in the lesson-1 table; the order matters, not the digits.
- Hash tables compute one bucket (O(1) expected probes); flat_map and std::map both do ~log2 N dependent probes.
- flat_map beats std::map because its probes stay inside one array whose top levels are cache-hot, while std::map chases pointers to scattered heap nodes.

## A1-T
- The build has no -DNDEBUG (or -DBOOST_DISABLE_ASSERTS), so BOOST_ASSERT = assert() runs inside Boost's vector iterator (operator*, ++, +=) in the binary-search loop; they timed the checks, not only the layout.
- Rebuild with -DNDEBUG or -DBOOST_DISABLE_ASSERTS; in the lessons' bench this flipped flat_map from losing to std::map (asserts on, 64-byte values, up to 65,536 entries) to about 3x faster than std::map (e.g. 19.5 vs 61.2 ns at 4,096 with 8-byte values; 80.2 -> 22.0 ns per lookup in labs/l1_ndebug).
- Re-measure on their own machine; trust the direction and mechanism, not the exact numbers.

## A2-R
- Bytes moved = (number of elements after the erased slot) x sizeof(value_type); they shift one slot left (insert shifts them right).
- Each random insert moves about N/2 elements on average, so N inserts cost about N^2/4 element moves.
- Remedy: sort, then construct with ordered_unique_range (linear), or adopt_sequence(ordered_unique_range, std::move(seq)) (O(1)), or the plain range constructor that sorts for you.

## A2-T
- One random insert moves about N/2 = 100,000 elements x 8 bytes = ~800,000 bytes; 1,000 of them move ~800 MB per batch (order of magnitude is what counts).
- Sort the batch and merge it once with insert(ordered_unique_range, first, last), which is linear: each existing element moves about once, plus k log k to sort the batch (or rebuild off the hot path and adopt/swap).
- Reserve the expected maximum so growth does not add a reallocation on top.

## A3-R
- Reallocation: the old block is freed, so the reference is a heap-use-after-free; ASan reports it. reserve() prevents this one.
- Shift without reallocation: the reference still points at the same slot, which now holds a different element; silent wrong data, ASan does not report it, and reserve() does not help.
- Repair: keep the key and re-find after any mutation, or copy the value out before mutating.

## A3-T
- If a spawn reallocates: t dangles into freed memory, a heap-use-after-free that ASan reports (or a crash).
- If capacity was enough: no ASan error, but t now points at a different entity's Transform because the elements shifted, so physics silently moves the wrong entity.
- Fix: keep the EntityId and re-find after spawn() (or copy the Transform before mutating), or keep Transforms in stable storage / a node-based container when pointer stability is needed.

## A4-R
- (a) Boost: one sequence of std::pair<K,T>; std: an adaptor over two containers (keys, mapped values) aligned by index.
- (b) Boost: a real std::pair<K,T>& (key writable); std: a proxy std::pair<const K&, T&> returned by value.
- (c) boost::container::ordered_unique_range vs std::sorted_unique.
- (d) Boost: extract_sequence() / adopt_sequence(...); std: std::move(m).extract() -> containers{keys, values}, then replace(std::move(keys), std::move(values)).

## A4-T
- (1) Does not compile: std::flat_map has no member reserve. Reserve the keys and values vectors before handing them in.
- (2) No adopt_sequence: fill a keys vector and a values vector, then construct with std::sorted_unique or call replace(std::move(keys), std::move(values)).
- (3) auto& does not compile because *it is a prvalue proxy pair<const int&, Cfg&>; use auto e = *m.begin(); e.second.x = 1; which writes through the proxy's T& (the key stays const).

## A5-R
- It throws boost::container::bad_alloc (default throw_on_overflow<true>); it never allocates and the size stays N.
- catch boost::container::bad_alloc, boost::container::exception or std::exception catches it; catch (const std::bad_alloc&) does NOT.
- BOOST_CONTAINER_USE_STD_EXCEPTIONS makes Boost.Container throw the standard exceptions; with throw_on_overflow<false>, overflow is undefined behaviour.

## A5-T
- Hard cap of 8: static_vector<pair<...>, 8> as the 4th template argument, so the table never touches the heap and overflow is a loud error.
- The catch is wrong with default settings: the overflow throws boost::container::bad_alloc, which std::bad_alloc does not catch; catch boost::container::bad_alloc (or boost::container::exception / std::exception), or build with BOOST_CONTAINER_USE_STD_EXCEPTIONS.
- Soft cap: small_vector<pair<...>, 8>, which stays inline in the common case and falls back to one heap allocation instead of failing; the price is a larger sizeof per table.

## A6-R
- flat_map: O(N) moves per insert/erase, references not stable. std::map: O(log N) + one allocation, stable. unordered_flat_map: O(1) average with rehash spikes, not stable (unordered_node_map is). Direct array: O(1), stable at fixed size.
- flat_map fits a read-mostly, small-to-medium table that needs sorted order or range queries and can be built in bulk and reserved.

## A6-T
- Not flat_map: random inserts/erases shift about N/2 elements each, and cost grows linearly with N (already ~2.6 us for a random insert at 10,000 entries vs ~208 ns for std::map insert+erase).
- Held pointers dangle on every insert/erase, so pointer stability rules it out on its own.
- Id-only lookups need no order: a hash table wins point lookups (boost::unordered_flat_map ~12 ns at 1M), reserved up front; because pointers must stay valid, use boost::unordered_node_map or have other services keep ids instead of pointers.

## A7-R
- Reallocation: flat_map (vector) growth when size() == capacity() moves the whole array; fix: reserve the session maximum at startup / build at final size.
- Shift: a front/middle insert moves every later element (O(N)), and reserve cannot help; fix: stage new keys in a small side table and merge off the hot path, rebuild off-thread and swap a pointer, or use a structure with cheap inserts.
- Rehash: boost::unordered_flat_map moves every element to a new bucket array when load exceeds the max; fix: reserve(n) at startup (max_load_factor cannot be set).

## A7-T
- First spike: reallocation, because shrink_to_fit left capacity == size, so the first insert reallocates and copies the whole array (the one heap allocation). Fix: reserve(size + expected new campaigns) at startup.
- Later spikes: the O(N) shift of a middle insert into the 500k array, which reserve cannot remove. Fix: stage new campaigns in a small side table (e.g. static_vector-backed, no heap) checked after the main table and merged off the hot path, or rebuild off-thread and swap a pointer, or a structure with cheaper inserts.
- The mean hides both because they are rare events; judge with max/p99.9, and accept that staging can cost a little on the mean.
