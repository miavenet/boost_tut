#!/bin/bash
# Card 04-Q3: element type X for  std::pmr::vector<X>  so the STRING BYTES also come from the
# vector's resource (uses-allocator construction). Answer (stdin): a type.
. "$(dirname "$0")/lib.inc"
read_answer
{
cat <<'CPP'
#include <cstddef>
#include <cstdlib>
#include <memory_resource>
#include <new>
#include <string>
#include <vector>
static long g_news = 0;
void* operator new(std::size_t n) { ++g_news; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
using Elem =
CPP
tr -d ';' < "$WORK/answer.txt"
cat <<'CPP'
;
int main() {
    alignas(64) static std::byte buf[1 << 16];
    std::pmr::monotonic_buffer_resource mono(buf, sizeof buf, std::pmr::null_memory_resource());
    long before = g_news;
    std::pmr::vector<Elem> v(&mono);
    for (int i = 0; i < 20; ++i) v.emplace_back("a string long enough to need a heap buffer, not the SSO one");
    if (v.size() != 20 || v[19].size() < 40) return 1;
    // The characters themselves must live in buf (uses-allocator construction). A non-owning
    // type such as std::string_view passed the count test with 0 news; its bytes are the literal.
    const void* chars = v[19].data();
    if (chars < static_cast<const void*>(buf) || chars >= static_cast<const void*>(buf + sizeof buf)) return 1;
    return g_news == before ? 0 : 1;
}
CPP
} > "$WORK/a.cpp"
compile_run "$WORK/a.cpp"
