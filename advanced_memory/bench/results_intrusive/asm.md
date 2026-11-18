# Generated code for the hot operations

machine: Darwin arm64, Apple clang version 21.0.0 (clang-2100.4.1.1). Source: bench/asm_hot_ops.cpp (each op a noinline
extern "C" function). Regenerate: `bash bench/asm_intrusive.sh`. Counts are static instructions in the
function body (ldp/stp count once though they move two registers), not executed counts.

## Lists, release (-O2 -DNDEBUG)
| function | instructions | branches (incl. ret) | loads | stores | calls |
|---|---|---|---|---|---|
| bi_safe_push_back | 9 | 1 | 2 | 4 | - |
| bi_normal_push_back | 9 | 1 | 2 | 4 | - |
| bi_safe_nosize_push_back | 5 | 1 | 1 | 3 | - |
| bi_normal_nosize_push_back | 5 | 1 | 1 | 3 | - |
| hand_push_back | 5 | 1 | 1 | 3 | - |
| idx_push_back | 15 | 3 | 2 | 4 | - |
| bi_safe_erase | 8 | 1 | 2 | 4 | - |
| bi_normal_erase | 7 | 1 | 2 | 3 | - |
| bi_safe_nosize_erase | 5 | 1 | 1 | 3 | - |
| bi_normal_nosize_erase | 4 | 1 | 1 | 2 | - |
| hand_erase | 5 | 1 | 1 | 3 | - |
| idx_erase | 22 | 5 | 2 | 5 | - |
| bi_safe_is_linked | 5 | 1 | 1 | 0 | - |
| hand_is_linked | 4 | 1 | 1 | 0 | - |

## Lists, asserts ON (-O2, no NDEBUG): what Boost safe_link adds in a debug build
| function | instructions | branches (incl. ret) | loads | stores | calls |
|---|---|---|---|---|---|
| bi_safe_push_back | 25 | 4 | 4 | 5 | assert_rtn, clang_call_terminate |
| bi_normal_push_back | 9 | 1 | 2 | 4 | - |
| bi_safe_erase | 24 | 4 | 4 | 5 | assert_rtn, clang_call_terminate |
| bi_normal_erase | 22 | 4 | 4 | 4 | assert_rtn, clang_call_terminate |

## Pools, release (-O2 -DNDEBUG), hft::slab with HFT_MEM_CHECKED=0
| function | instructions | branches (incl. ret) | loads | stores | calls |
|---|---|---|---|---|---|
| slab_create | 12 | 2 | 3 | 4 | - |
| slab_destroy | 13 | 1 | 4 | 4 | - |
| hand_pool_create | 9 | 2 | 2 | 3 | - |
| hand_pool_destroy | 4 | 1 | 1 | 2 | - |
| boost_pool_malloc | 7 | 3 | 2 | 1 | - |
| boost_pool_free | 4 | 1 | 1 | 2 | - |
| object_pool_construct | 16 | 5 | 3 | 4 | ZN5boost4poolINS_33default_user_allocator_new_deleteEE26ordered_malloc_need_resizeEv |
| object_pool_destroy | 10 | 3 | 2 | 2 | - |

## hft::slab with HFT_MEM_CHECKED=1 (the library default)
| function | instructions | branches (incl. ret) | loads | stores | calls |
|---|---|---|---|---|---|
| slab_create | 12 | 2 | 3 | 4 | - |
| slab_destroy | 31 | 7 | 8 | 5 | abort, clang_call_terminate, fputs |

## Excerpts (release)
```
bi_safe_erase:
    ldp	x8, x9, [x1, #16]
    str	x8, [x9]
    str	x9, [x8, #8]
    ldr	x8, [x0]
    sub	x8, x8, #1
    str	x8, [x0]
    stp	xzr, xzr, [x1, #16]
    ret
```
```
bi_normal_erase:
    ldp	x8, x9, [x1, #16]
    str	x8, [x9]
    str	x9, [x8, #8]
    ldr	x8, [x0]
    sub	x8, x8, #1
    str	x8, [x0]
    ret
```
```
bi_normal_nosize_erase:
    ldp	x8, x9, [x1, #16]
    str	x8, [x9]
    str	x9, [x8, #8]
    ret
```
```
hand_erase:
    ldp	x9, x8, [x0, #16]
    str	x8, [x9, #8]
    str	x9, [x8]
    stp	xzr, xzr, [x0, #16]
    ret
```
```
bi_safe_push_back:
    add	x8, x0, #8
    ldr	x9, [x0, #16]
    stp	x8, x9, [x1, #16]!
    str	x1, [x0, #16]
    str	x1, [x9]
    ldr	x8, [x0]
    add	x8, x8, #1
    str	x8, [x0]
    ret
```
```
bi_safe_nosize_push_back:
    ldr	x8, [x0, #8]
    stp	x0, x8, [x1, #16]!
    str	x1, [x0, #8]
    str	x1, [x8]
    ret
```
```
hand_push_back:
    ldr	x8, [x0]
    stp	x8, x0, [x1, #16]!
    str	x1, [x8, #8]
    str	x1, [x0]
    ret
```
```
bi_safe_is_linked:
    ldr	x8, [x0, #16]!
    cmp	x8, #0
    ccmp	x8, x0, #4, ne
    cset	w0, ne
    ret
```
```
idx_erase:
    ldr	x9, [x0]
    mov	w8, #24
    umaddl	x8, w1, w8, x9
    ldp	w10, w11, [x8, #16]!
    cmn	w10, #1
    b.eq	LBB13_3
    mov	w12, #24
    umaddl	x12, w10, w12, x9
    str	w11, [x12, #20]
    cmn	w11, #1
    b.eq	LBB13_4
    mov	w12, #24
    umaddl	x9, w11, w12, x9
    str	w10, [x9, #16]
    b	LBB13_5
    str	w11, [x0, #8]
    cmn	w11, #1
    b.ne	LBB13_2
    str	w10, [x0, #12]
    movi.2d	v0, #0xffffffffffffffff
    str	d0, [x8]
    ret
```
```
slab_destroy:
    ldp	x9, x8, [x0, #16]
    sub	x9, x1, x9
    asr	x9, x9, #4
    ldr	w10, [x8, x9]
    add	w10, w10, #1
    str	w10, [x8, x9]
    ldr	x8, [x0, #32]
    str	x8, [x1]
    str	x1, [x0, #32]
    ldr	w8, [x0, #12]
    sub	w8, w8, #1
    str	w8, [x0, #12]
    ret
```
```
hand_pool_destroy:
    ldr	x8, [x0, #24]
    str	x8, [x1]
    str	x1, [x0, #24]
    ret
```
```
boost_pool_free:
    ldr	x8, [x0]
    str	x8, [x1]
    str	x1, [x0]
    ret
```
```
object_pool_destroy:
    ldr	x8, [x0]
    cmp	x8, #0
    ccmp	x8, x1, #2, ne
    b.hi	LBB21_3
    mov	x0, x8
    ldr	x8, [x8]
    b	LBB21_1
    str	x8, [x1]
    str	x1, [x0]
    ret
```

## Excerpt, asserts ON: bi_safe_push_back
```
bi_safe_push_back:
    stp	x29, x30, [sp, #-16]!
    mov	x29, sp
    mov	x8, x1
    ldr	x9, [x8, #16]!
    cbnz	x9, LBB0_2
    add	x9, x0, #8
    ldr	x10, [x0, #16]
    stp	x9, x10, [x1, #16]
    str	x8, [x0, #16]
    str	x8, [x10]
    ldr	x8, [x0]
    add	x8, x8, #1
    str	x8, [x0]
    ldp	x29, x30, [sp], #16
    ret
    adrp	x0, l___func__._ZN5boost9intrusive9list_implINS0_8mhtraitsI3ObjINS0_16list_member_hookIJEEEES5_XadL_ZNS6_1hEEEEEmLb1EvE9push_backERS6_@PAGE
    add	x0, x0, l___func__._ZN5boost9intrusive9list_implINS0_8mhtraitsI3ObjINS0_16list_member_hookIJEEEES5_XadL_ZNS6_1hEEEEEmLb1EvE9push_backERS6_@PAGEOFF
    adrp	x1, l_.str@PAGE
    add	x1, x1, l_.str@PAGEOFF
    adrp	x3, l_.str.1@PAGE
    add	x3, x3, l_.str.1@PAGEOFF
    mov	w2, #272
    bl	___assert_rtn
    brk	#0x1
    bl	___clang_call_terminate
```

## Excerpt, HFT_MEM_CHECKED=1: slab_destroy
```
slab_destroy:
    stp	x29, x30, [sp, #-16]!
    mov	x29, sp
    ldr	x9, [x0, #16]
    subs	x8, x1, x9
    b.lo	LBB15_4
    ldr	w10, [x0, #8]
    add	x9, x9, x10, lsl #6
    cmp	x9, x1
    b.ls	LBB15_4
    tst	x8, #0x3f
    b.ne	LBB15_4
    asr	x8, x8, #4
    ldp	x9, x10, [x0, #24]
    ldr	w11, [x9, x8]
    add	w11, w11, #1
    str	w11, [x9, x8]
    str	x10, [x1]
    str	x1, [x0, #32]
    ldr	w8, [x0, #12]
    sub	w8, w8, #1
    str	w8, [x0, #12]
    ldp	x29, x30, [sp], #16
    ret
    adrp	x8, ___stderrp@GOTPAGE
    ldr	x8, [x8, ___stderrp@GOTPAGEOFF]
    ldr	x1, [x8]
    adrp	x0, l_.str@PAGE
    add	x0, x0, l_.str@PAGEOFF
    bl	_fputs
    bl	_abort
    bl	___clang_call_terminate
```
