// Shared prelude for check harnesses (included by tpl/*.cpp).
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>
#include <array>
#include <cstdint>
#include <cstdio>
#include <memory_resource>
#include <string>
#include <tuple>
#include <type_traits>
namespace bmi = boost::multi_index;
using namespace boost::multi_index;
#define EXPECT(c) do { if (!(c)) { std::printf("MISS: failed %s\n", #c); return 1; } } while (0)
