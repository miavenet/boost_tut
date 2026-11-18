// asm_hot_ops.cpp — the hot operations as separate non-inlined functions, so their generated
// code can be read and counted (bench/asm_intrusive.sh compiles this with -S). Not a benchmark.
#include "../labs/hft_mem/hft_handroll.hpp"
#include "../labs/hft_mem/hft_slab.hpp"

#include <boost/intrusive/list.hpp>
#include <boost/pool/object_pool.hpp>
#include <boost/pool/pool.hpp>

namespace bi = boost::intrusive;

template <class Hook> struct Obj { long id; long qty; Hook h; };
using safe_h = bi::list_member_hook<>;
using normal_h = bi::list_member_hook<bi::link_mode<bi::normal_link>>;
using SO = Obj<safe_h>;
using NO = Obj<normal_h>;
using SafeList = bi::list<SO, bi::member_hook<SO, safe_h, &SO::h>>;
using NormalList = bi::list<NO, bi::member_hook<NO, normal_h, &NO::h>>;
using SafeNoSize = bi::list<SO, bi::member_hook<SO, safe_h, &SO::h>, bi::constant_time_size<false>>;
using NormalNoSize = bi::list<NO, bi::member_hook<NO, normal_h, &NO::h>, bi::constant_time_size<false>>;

struct HO { long id; long qty; hr::dlink h; };
using HandList = hr::dlist<HO, &HO::h>;
struct IO { long id; long qty; hr::ilink h; };
using IdxList = hr::idx_list<IO, &IO::h>;
struct PO { long id; long qty; char pad[48]; };

#define HOT extern "C" __attribute__((noinline))

HOT void bi_safe_push_back(SafeList& l, SO& o) { l.push_back(o); }
HOT void bi_safe_erase(SafeList& l, SO& o) { l.erase(SafeList::s_iterator_to(o)); }
HOT void bi_normal_push_back(NormalList& l, NO& o) { l.push_back(o); }
HOT void bi_normal_erase(NormalList& l, NO& o) { l.erase(NormalList::s_iterator_to(o)); }
HOT void bi_safe_nosize_push_back(SafeNoSize& l, SO& o) { l.push_back(o); }
HOT void bi_safe_nosize_erase(SafeNoSize& l, SO& o) { l.erase(SafeNoSize::s_iterator_to(o)); }
HOT void bi_normal_nosize_push_back(NormalNoSize& l, NO& o) { l.push_back(o); }
HOT void bi_normal_nosize_erase(NormalNoSize& l, NO& o) { l.erase(NormalNoSize::s_iterator_to(o)); }
HOT bool bi_safe_is_linked(const SO& o) { return o.h.is_linked(); }

HOT void hand_push_back(HandList& l, HO& o) { l.push_back(o); }
HOT void hand_erase(HO& o) { HandList::erase(o); }
HOT bool hand_is_linked(const HO& o) { return o.h.is_linked(); }
HOT void idx_push_back(IdxList& l, std::uint32_t i) { l.push_back(i); }
HOT void idx_erase(IdxList& l, std::uint32_t i) { l.erase(i); }

HOT PO* slab_create(hft::slab<PO>& s) { return s.try_create(); }
HOT void slab_destroy(hft::slab<PO>& s, PO* p) { s.destroy(p); }
HOT PO* hand_pool_create(hr::pool<PO>& p) { return p.create(); }
HOT void hand_pool_destroy(hr::pool<PO>& p, PO* o) { p.destroy(o); }
HOT void* boost_pool_malloc(boost::pool<>& p) { return p.malloc(); }
HOT void boost_pool_free(boost::pool<>& p, void* o) { p.free(o); }
HOT PO* object_pool_construct(boost::object_pool<PO>& p) { return p.construct(); }
HOT void object_pool_destroy(boost::object_pool<PO>& p, PO* o) { p.destroy(o); }
