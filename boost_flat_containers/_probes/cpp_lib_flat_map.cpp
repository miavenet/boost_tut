// Expected: compiles with -std=c++23 and prints __cpp_lib_flat_map (lesson-02.md:111 claims 202511).
#include <cstdio>
#include <flat_map>

int main() {
#ifdef __cpp_lib_flat_map
    std::printf("__cpp_lib_flat_map=%ld\n", (long)__cpp_lib_flat_map);
    return __cpp_lib_flat_map == 202511L ? 0 : 1;
#else
    std::puts("__cpp_lib_flat_map NOT defined");
    return 2;
#endif
}
