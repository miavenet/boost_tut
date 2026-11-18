#!/bin/bash
# Self-test for checks/: every check must PASS a right answer and FAIL plausible
# wrong ones (and FAIL empty input). Run from anywhere: bash tests/test_checks.sh
cd "$(dirname "$0")/.."
fails=0
expect() {  # expect <0|1> <check> <answer>
  local want=$1 chk=$2 ans=$3 got
  printf '%s' "$ans" | bash "checks/$chk" >/dev/null 2>&1; got=$?
  [ "$got" -ne 0 ] && got=1
  if [ "$got" = "$want" ]; then echo "ok   $chk want=$want :: ${ans//$'\n'/ }" | cut -c1-110
  else echo "FAIL $chk want=$want got=$got :: ${ans//$'\n'/ }" | cut -c1-110; fails=$((fails+1)); fi
}
for c in checks/*.check.sh; do expect 1 "$(basename "$c")" ""; done

expect 0 l1-bytes.check.sh "160000"
expect 0 l1-bytes.check.sh "about 160,000 bytes (10000 x 16)"
expect 0 l1-bytes.check.sh "160 KB"
expect 1 l1-bytes.check.sh "10000"
expect 1 l1-bytes.check.sh "80000"
expect 1 l1-bytes.check.sh "16 bytes"
expect 0 l1-bytes.check.sh "10,000 * 16 = 160,000 bytes"
expect 0 l1-bytes.check.sh "156.25 KiB"
expect 1 l1-bytes.check.sh "160000 bits"
expect 1 l1-bytes.check.sh "Not 160000, it is 80000"
expect 1 l1-bytes.check.sh "80000 or 160000"
expect 1 l1-bytes.check.sh "160 KiB"

expect 0 l1-ndebug.check.sh "-DNDEBUG"
expect 0 l1-ndebug.check.sh "-O2 -DNDEBUG"
expect 0 l1-ndebug.check.sh "-DBOOST_DISABLE_ASSERTS"
expect 1 l1-ndebug.check.sh "-O3"
expect 1 l1-ndebug.check.sh "-DDEBUG"
expect 1 l1-ndebug.check.sh "-UNDEBUG"
expect 0 l1-ndebug.check.sh "-D NDEBUG"
expect 0 l1-ndebug.check.sh '`-DNDEBUG`'
expect 0 l1-ndebug.check.sh "-O2 -D NDEBUG"
expect 1 l1-ndebug.check.sh "-DBOOST_ASSERT_IS_VOID"

expect 0 l2-dangling.check.sh 'long limit_after_listing(RiskTable& t) { add_listings(t); return t.find(200)->second.max_qty; }'
expect 0 l2-dangling.check.sh 'long limit_after_listing(RiskTable& t) { long v = t.find(200)->second.max_qty; add_listings(t); return v; }'
expect 1 l2-dangling.check.sh 'long limit_after_listing(RiskTable& t) { Limit& l = t.find(200)->second; add_listings(t); return l.max_qty; }'
expect 1 l2-dangling.check.sh 'long limit_after_listing(RiskTable& t) { auto it = t.find(200); add_listings(t); return it->second.max_qty; }'
expect 1 l2-dangling.check.sh 'long limit_after_listing(RiskTable& t) { return 2000; }'
expect 1 l2-dangling.check.sh 'long limit_after_listing(RiskTable& t) { add_listings(t); return 2000; }'

expect 0 l2-std-extract.check.sh 'auto c = std::move(s).extract();
for (auto& v : c.values) v = 0.0;
s.replace(std::move(c.keys), std::move(c.values));'
expect 1 l2-std-extract.check.sh 'auto c = s.extract();
for (auto& v : c.values) v = 0.0;
s.replace(std::move(c.keys), std::move(c.values));'
expect 1 l2-std-extract.check.sh 'auto c = std::move(s).extract();
for (auto& v : c.values) v = 0.0;'
expect 1 l2-std-extract.check.sh 'auto c = std::move(s).extract(); auto k = c.keys; auto v = std::vector<double>(3, 0.0); s.replace(std::move(k), std::move(v));'
expect 1 l2-std-extract.check.sh 'for (auto p : s) p.second = 0.0;'
expect 1 l2-std-extract.check.sh '// extract() then replace()
for (auto p : s) p.second = 0.0;'
expect 1 l2-std-extract.check.sh 'std::puts("extract( replace(");  for (auto p : s) p.second = 0.0;'

expect 0 l3-bulk.check.sh 'FM build(std::vector<std::pair<uint64_t,uint64_t>> v) { std::sort(v.begin(), v.end()); return FM(bc::ordered_unique_range, v.begin(), v.end()); }'
expect 0 l3-bulk.check.sh 'FM build(std::vector<std::pair<uint64_t,uint64_t>> v) { return FM(v.begin(), v.end()); }'
expect 0 l3-bulk.check.sh 'FM build(std::vector<std::pair<uint64_t,uint64_t>> v) { FM::sequence_type s(v.begin(), v.end()); std::sort(s.begin(), s.end()); FM m; m.adopt_sequence(bc::ordered_unique_range, std::move(s)); return m; }'
expect 1 l3-bulk.check.sh 'FM build(std::vector<std::pair<uint64_t,uint64_t>> v) { FM m; for (auto& p : v) m.insert(p); return m; }'
expect 1 l3-bulk.check.sh 'FM build(std::vector<std::pair<uint64_t,uint64_t>> v) { return FM(bc::ordered_unique_range, v.begin(), v.end()); }'

expect 0 l3-overflow.check.sh "boost::container::bad_alloc"
expect 0 l3-overflow.check.sh "std::exception"
expect 0 l3-overflow.check.sh "catch (const boost::container::bad_alloc& e) {"
expect 1 l3-overflow.check.sh "std::bad_alloc"
expect 1 l3-overflow.check.sh "std::length_error"
expect 1 l3-overflow.check.sh "int"
expect 0 l3-overflow.check.sh '`boost::container::bad_alloc`'
expect 0 l3-overflow.check.sh "boost::container::bad_alloc;"
expect 0 l3-overflow.check.sh "const std::exception&"
expect 1 l3-overflow.check.sh "catch (const std::bad_alloc& e) {"

expect 0 l4-direct.check.sh 'const Config* find_cfg(const std::vector<Config>& t, uint32_t id) { return id < t.size() ? &t[id] : nullptr; }'
expect 1 l4-direct.check.sh 'const Config* find_cfg(const std::vector<Config>& t, uint32_t id) { return &t[id]; }'
expect 0 l4-direct.check.sh '// O(1), no find() or for()
const Config* find_cfg(const std::vector<Config>& t, uint32_t id) { return id < t.size() ? &t[id] : nullptr; }'
expect 1 l4-direct.check.sh 'const Config* find_cfg(const std::vector<Config>& t, uint32_t id) { auto it = std::upper_bound(t.begin(), t.end(), id, [](uint32_t k, const Config& c){ return k < c.id; }); return (it != t.begin() && (it-1)->id == id) ? &*(it-1) : nullptr; }'
expect 1 l4-direct.check.sh 'const Config* find_cfg(const std::vector<Config>& t, uint32_t id) { auto r = std::equal_range(t.begin(), t.end(), id, [](const auto& a, const auto& b){ return key(a) < key(b); }); return r.first != r.second ? &*r.first : nullptr; }'
expect 1 l4-direct.check.sh '// uses indexing
const Config* find_cfg(const std::vector<Config>& t, uint32_t id) { return std::bsearch(&id, t.data(), t.size(), sizeof(Config), cmp) ? &t[id] : nullptr; }'
expect 1 l4-direct.check.sh 'const Config* find_cfg(const std::vector<Config>& t, uint32_t id) { for (auto& c : t) if (c.id == id) return &c; return nullptr; }'
expect 1 l4-direct.check.sh 'const Config* find_cfg(const std::vector<Config>& t, uint32_t id) { return id <= t.size() ? &t[id] : nullptr; }'

expect 0 l4-rehash.check.sh 'h.reserve(1000000);'
expect 0 l4-rehash.check.sh 'h.reserve(2000000);'
expect 1 l4-rehash.check.sh 'h.rehash(500000);'
expect 1 l4-rehash.check.sh 'h.reserve(500000);'
expect 0 l4-rehash.check.sh 'h.rehash(2000000);'
expect 1 l4-rehash.check.sh 'h.max_load_factor(0.5f);'

expect 0 l4-capstone.check.sh 'risk.reserve(risk.size() + kMaxNewListings);'
expect 0 l4-capstone.check.sh 'risk.reserve(2 * risk.size());'
expect 1 l4-capstone.check.sh 'risk.reserve(risk.size());'
expect 1 l4-capstone.check.sh 'risk.shrink_to_fit();'
expect 1 l4-capstone.check.sh 'cfg.reserve(cfg.size() + kMaxNewListings);'
expect 1 l4-capstone.check.sh 'risk.clear();'
expect 1 l4-capstone.check.sh 'risk.clear(); risk.reserve(2 * 100000);'

echo "self-test failures: $fails"
exit $((fails > 0))
