// Copyright 2018 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// An open-addressing
// hashtable with quadratic probing.
//
// This is a low level hashtable on top of which different interfaces can be
// implemented, like flat_hash_set, node_hash_set, string_hash_set, etc.
//
// The table interface is similar to that of std::unordered_set. Notable
// differences are that most member functions support heterogeneous keys when
// BOTH the hash and eq functions are marked as transparent. They do so by
// providing a typedef called `is_transparent`.
//
// When heterogeneous lookup is enabled, functions that take key_type act as if
// they have an overload set like:
//
//   iterator find(const key_type& key);
//   template <class K>
//   iterator find(const K& key);
//
//   size_type erase(const key_type& key);
//   template <class K>
//   size_type erase(const K& key);
//
//   std::pair<iterator, iterator> equal_range(const key_type& key);
//   template <class K>
//   std::pair<iterator, iterator> equal_range(const K& key);
//
// When heterogeneous lookup is disabled, only the explicit `key_type` overloads
// exist.
//
// find() also supports passing the hash explicitly:
//
//   iterator find(const key_type& key, size_t hash);
//   template <class U>
//   iterator find(const U& key, size_t hash);
//
// In addition the pointer to element and iterator stability guarantees are
// weaker: all iterators and pointers are invalidated after a new element is
// inserted.
//
// IMPLEMENTATION DETAILS
//
// # Table Layout
//
// A raw_hash_set's backing array consists of control bytes followed by slots
// that may or may not contain objects.
//
// The layout of the backing array, for `capacity` slots, is thus, as a
// pseudo-struct:
//
//   struct BackingArray {
//     // Sampling handler. This field isn't present when the sampling is
//     // disabled or this allocation hasn't been selected for sampling.
//     HashtablezInfoHandle infoz_;
//     // The number of elements we can insert before growing the capacity.
//     size_t growth_left;
//     // Control bytes for the "real" slots.
//     ctrl_t ctrl[capacity];
//     // Always `ctrl_t::kSentinel`. This is used by iterators to find when to
//     // stop and serves no other purpose.
//     ctrl_t sentinel;
//     // A copy of the first `kWidth - 1` elements of `ctrl`. This is used so
//     // that if a probe sequence picks a value near the end of `ctrl`,
//     // `Group` will have valid control bytes to look at.
//     ctrl_t clones[kWidth - 1];
//     // The actual slot data.
//     slot_type slots[capacity];
//   };
//
// The length of this array is computed by `RawHashSetLayout::alloc_size` below.
//
// Control bytes (`ctrl_t`) are bytes (collected into groups of a
// platform-specific size) that define the state of the corresponding slot in
// the slot array. Group manipulation is tightly optimized to be as efficient
// as possible: SSE and friends on x86, clever bit operations on other arches.
//
//      Group 1         Group 2        Group 3
// +---------------+---------------+---------------+
// | | | | | | | | | | | | | | | | | | | | | | | | |
// +---------------+---------------+---------------+
//
// Each control byte is either a special value for empty slots, deleted slots
// (sometimes called *tombstones*), and a special end-of-table marker used by
// iterators, or, if occupied, seven bits (H2) from the hash of the value in the
// corresponding slot.
//
// Storing control bytes in a separate array also has beneficial cache effects,
// since more logical slots will fit into a cache line.
//
// # Hashing
//
// We compute two separate hashes, `H1` and `H2`, from the hash of an object.
// `H1(hash(x))` is an index into `slots`, and essentially the starting point
// for the probe sequence. `H2(hash(x))` is a 7-bit value used to filter out
// objects that cannot possibly be the one we are looking for.
//
// # Table operations.
//
// The key operations are `insert`, `find`, and `erase`.
//
// Since `insert` and `erase` are implemented in terms of `find`, we describe
// `find` first. To `find` a value `x`, we compute `hash(x)`. From
// `H1(hash(x))` and the capacity, we construct a `probe_seq` that visits every
// group of slots in some interesting order.
//
// We now walk through these indices. At each index, we select the entire group
// starting with that index and extract potential candidates: occupied slots
// with a control byte equal to `H2(hash(x))`. If we find an empty slot in the
// group, we stop and return an error. Each candidate slot `y` is compared with
// `x`; if `x == y`, we are done and return `&y`; otherwise we continue to the
// next probe index. Tombstones effectively behave like full slots that never
// match the value we're looking for.
//
// The `H2` bits ensure when we compare a slot to an object with `==`, we are
// likely to have actually found the object.  That is, the chance is low that
// `==` is called and returns `false`.  Thus, when we search for an object, we
// are unlikely to call `==` many times.  This likelyhood can be analyzed as
// follows (assuming that H2 is a random enough hash function).
//
// Let's assume that there are `k` "wrong" objects that must be examined in a
// probe sequence.  For example, when doing a `find` on an object that is in the
// table, `k` is the number of objects between the start of the probe sequence
// and the final found object (not including the final found object).  The
// expected number of objects with an H2 match is then `k/128`.  Measurements
// and analysis indicate that even at high load factors, `k` is less than 32,
// meaning that the number of "false positive" comparisons we must perform is
// less than 1/8 per `find`.

// `insert` is implemented in terms of `unchecked_insert`, which inserts a
// value presumed to not be in the table (violating this requirement will cause
// the table to behave erratically). Given `x` and its hash `hash(x)`, to insert
// it, we construct a `probe_seq` once again, and use it to find the first
// group with an unoccupied (empty *or* deleted) slot. We place `x` into the
// first such slot in the group and mark it as full with `x`'s H2.
//
// To `insert`, we compose `unchecked_insert` with `find`. We compute `h(x)` and
// perform a `find` to see if it's already present; if it is, we're done. If
// it's not, we may decide the table is getting overcrowded (i.e. the load
// factor is greater than 7/8 for big tables; `is_small()` tables use a max load
// factor of 1); in this case, we allocate a bigger array, `unchecked_insert`
// each element of the table into the new array (we know that no insertion here
// will insert an already-present value), and discard the old backing array. At
// this point, we may `unchecked_insert` the value `x`.
//
// Below, `unchecked_insert` is partly implemented by `prepare_insert`, which
// presents a viable, initialized slot pointee to the caller.
//
// `erase` is implemented in terms of `erase_at`, which takes an index to a
// slot. Given an offset, we simply create a tombstone and destroy its contents.
// If we can prove that the slot would not appear in a probe sequence, we can
// make the slot as empty, instead. We can prove this by observing that if a
// group has any empty slots, it has never been full (assuming we never create
// an empty slot in a group with no empties, which this heuristic guarantees we
// never do) and find would stop at this group anyways (since it does not probe
// beyond groups with empties).
//
// `erase` is `erase_at` composed with `find`: if we
// have a value `x`, we can perform a `find`, and then `erase_at` the resulting
// slot.
//
// To iterate, we simply traverse the array, skipping empty and deleted slots
// and stopping when we hit a `kSentinel`.

#ifndef MICRODICT_SIMD_H_
#define MICRODICT_SIMD_H_

#include <stdbool.h>
#include <limits.h>
#include "./bits.h"

// ABSL_INTERNAL_HAVE_SSE is used for compile-time detection of SSE support.
// See https://gcc.gnu.org/onlinedocs/gcc/x86-Options.html for an overview of
// which architectures support the various x86 instruction sets.
#ifdef ABSL_INTERNAL_HAVE_SSE
#error ABSL_INTERNAL_HAVE_SSE cannot be directly set
#elif defined(__SSE__)
#define ABSL_INTERNAL_HAVE_SSE 1
#elif (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)) && \
    !defined(_M_ARM64EC)
// MSVC only defines _M_IX86_FP for x86 32-bit code, and _M_IX86_FP >= 1
// indicates that at least SSE was targeted with the /arch:SSE option.
// All x86-64 processors support SSE, so support can be assumed.
// https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros
#define ABSL_INTERNAL_HAVE_SSE 1
#endif

// ABSL_INTERNAL_HAVE_SSE2 is used for compile-time detection of SSE2 support.
// See https://gcc.gnu.org/onlinedocs/gcc/x86-Options.html for an overview of
// which architectures support the various x86 instruction sets.
#ifdef ABSL_INTERNAL_HAVE_SSE2
#error ABSL_INTERNAL_HAVE_SSE2 cannot be directly set
#elif defined(__SSE2__)
#define ABSL_INTERNAL_HAVE_SSE2 1
#elif (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)) && \
    !defined(_M_ARM64EC)
// MSVC only defines _M_IX86_FP for x86 32-bit code, and _M_IX86_FP >= 2
// indicates that at least SSE2 was targeted with the /arch:SSE2 option.
// All x86-64 processors support SSE2, so support can be assumed.
// https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros
#define ABSL_INTERNAL_HAVE_SSE2 1
#endif

// ABSL_INTERNAL_HAVE_SSSE3 is used for compile-time detection of SSSE3 support.
// See https://gcc.gnu.org/onlinedocs/gcc/x86-Options.html for an overview of
// which architectures support the various x86 instruction sets.
//
// MSVC does not have a mode that targets SSSE3 at compile-time. To use SSSE3
// with MSVC requires either assuming that the code will only every run on CPUs
// that support SSSE3, otherwise __cpuid() can be used to detect support at
// runtime and fallback to a non-SSSE3 implementation when SSSE3 is unsupported
// by the CPU.
#ifdef ABSL_INTERNAL_HAVE_SSSE3
#error ABSL_INTERNAL_HAVE_SSSE3 cannot be directly set
#elif defined(__SSSE3__)
#define ABSL_INTERNAL_HAVE_SSSE3 1
#endif

#ifdef ABSL_INTERNAL_HAVE_AVX2
#error ABSL_INTERNAL_HAVE_AVX2 cannot be directly set
#elif defined(__AVX2__)
#define ABSL_INTERNAL_HAVE_AVX2 1
#endif


// ABSL_INTERNAL_HAVE_ARM_NEON is used for compile-time detection of NEON (ARM
// SIMD).
//
// If __CUDA_ARCH__ is defined, then we are compiling CUDA code in device mode.
// In device mode, NEON intrinsics are not available, regardless of host
// platform.
// https://llvm.org/docs/CompileCudaWithLLVM.html#detecting-clang-vs-nvcc-from-code
#ifdef ABSL_INTERNAL_HAVE_ARM_NEON
#error ABSL_INTERNAL_HAVE_ARM_NEON cannot be directly set
#elif defined(__ARM_NEON) && !defined(__CUDA_ARCH__)
#define ABSL_INTERNAL_HAVE_ARM_NEON 1
#endif

#ifdef ABSL_INTERNAL_HAVE_SSE2
#include <emmintrin.h>
#endif

#ifdef ABSL_INTERNAL_HAVE_SSSE3
#include <tmmintrin.h>
#endif

#ifdef ABSL_INTERNAL_HAVE_AVX2
#include <immintrin.h>
#endif

#ifdef _MSC_VER
#include <intrin.h>
#endif

#ifdef ABSL_INTERNAL_HAVE_ARM_NEON
#include <arm_neon.h>
#endif

#if defined(__clang__) || defined(__GNUC__)
#define ABSL_ALIGNED(x) __attribute__((aligned(x)))
#define ABSL_ALIGNED_SUPPORT 1
#elif defined(_MSC_VER)
#define ABSL_ALIGNED(x) __declspec(align(x))
#define ABSL_ALIGNED_SUPPORT 1
#else
#define ABSL_ALIGNED(x)
#endif

const uint8_t FLAGS_EMPTY = 128;     // 0b10000000 
const uint8_t FLAGS_DELETED = 254;   // 0b11111110
const uint8_t FLAGS_SENTINEL = 255;  // 0b11111111
const int8_t FLAGS_EMPTY_SIGNED = -128;
const int8_t FLAGS_DELETED_SIGNED = -2;
const int8_t FLAGS_SENTINEL_SIGNED = -1;
// any full entry will be a 0 | (7-bit h2 value), greater than FLAGS_SENTINEL when interpreted as int8_t

#if defined(ABSL_INTERNAL_HAVE_AVX2)

#define GROUP_WIDTH 32
typedef __m256i g_t;
typedef uint32_t gbits;

static inline int _gbits_next(gbits* mut_bitmask) {
  int result = count_trailing_zeroes32(*mut_bitmask);
  *mut_bitmask &= (*mut_bitmask - 1);
  return result;
}

static inline bool _gbits_has_next(gbits bitmask) {
  return bitmask;
}

static inline g_t _group_load(const uint64_t* pos) {
  return _mm256_loadu_si256((const __m256i*) pos);
}

// Returns a bitmask representing the positions of slots that match hash.
static inline gbits _group_match(g_t ctrl, uint8_t hash) {
  g_t match = _mm256_set1_epi8((char) hash);
  return (gbits) (_mm256_movemask_epi8(_mm256_cmpeq_epi8(match, ctrl)));
}

// Returns a bitmask representing the positions of empty slots.
static inline gbits _group_mask_empty(g_t ctrl) {
  // This only works because ctrl_t::kEmpty is -128.
  return _mm256_movemask_epi8(_mm256_sign_epi8(ctrl, ctrl));
}

// Returns a bitmask representing the positions of full slots.
static inline gbits _group_mask_full(g_t ctrl) {
  return (gbits) (_mm256_movemask_epi8(ctrl)) ^ 0xffffffffU;
}

// Returns a bitmask representing the positions of empty or deleted slots.
static inline gbits _group_mask_empty_or_deleted(g_t ctrl) {
  g_t special = _mm256_set1_epi8((char) FLAGS_SENTINEL);
  return (gbits) _mm256_movemask_epi8(_mm256_cmpgt_epi8(special, ctrl));
}

// Returns the number of leading empty or deleted elements in the group.
static inline uint32_t _group_count_leading_empty_or_deleted(g_t ctrl) {
  g_t special = _mm256_set1_epi8((char) FLAGS_SENTINEL);
  return count_trailing_zeroes32((uint32_t) (
      _mm256_movemask_epi8(_mm256_cmpgt_epi8(special, ctrl)) + 1));
}

ABSL_ALIGNED(32) const uint32_t __sll_permutations[8][8] = {
  { 0, 1, 2, 3, 4, 5, 6, 7 }, { 1, 0, 2, 3, 4, 5, 6, 7 },
  { 2, 1, 0, 3, 4, 5, 6, 7 }, { 3, 1, 2, 0, 4, 5, 6, 7 },
  { 4, 1, 2, 3, 0, 5, 6, 7 }, { 5, 1, 2, 3, 4, 0, 6, 7 },
  { 6, 1, 2, 3, 4, 5, 0, 7 }, { 7, 1, 2, 3, 4, 5, 6, 0 }
};
// sets the octet at `ctrl` bits [offset*8..offset*8+8] to the given hash
static inline void _group_set(g_t ctrl, int8_t* dst, uint32_t offset, uint8_t hash) {
  // only works because ctrl:kEmpty = -128
  g_t ghash = _mm256_setr_epi32(((uint32_t)(0x80 | hash)) << ((offset & 3) << 3), 0, 0, 0, 0, 0, 0, 0);
#ifdef ABSL_ALIGNED_SUPPORT
  g_t perm = _mm256_load_si256((g_t*) &__sll_permutations[offset >> 2]);
#else
  g_t perm = _mm256_loadu_si256((g_t*) &__sll_permutations[offset >> 2]);
#endif
  ghash = _mm256_permutevar8x32_epi32(ghash, perm);
  g_t res = _mm256_xor_si256(ctrl, ghash);
  _mm256_storeu_si256((g_t*) dst, res);
}

static void _group_convert_special_to_empty_and_full_to_deleted(g_t ctrl, int8_t* dst) {
  g_t msbs = _mm256_set1_epi8((char) (-128));
  g_t x126 = _mm256_set1_epi8((char) 126);
  // res[i] = (ctrl[i] sign bit ? 0 : 126) | 128
  g_t res = _mm256_or_si256(_mm256_shuffle_epi8(x126, ctrl), msbs);
  _mm256_storeu_si256((g_t*) dst, res);
}

#elif defined(ABSL_INTERNAL_HAVE_SSE2)
// Quick reference guide for intrinsics used below:
//
// * __m128i: An XMM (128-bit) word.
//
// * _mm_setzero_si128: Returns a zero vector.
// * _mm_set1_epi8:     Returns a vector with the same i8 in each lane.
//
// * _mm_subs_epi8:    Saturating-subtracts two i8 vectors.
// * _mm_and_si128:    Ands two i128s together.
// * _mm_or_si128:     Ors two i128s together.
// * _mm_andnot_si128: And-nots two i128s together.
//
// * _mm_cmpeq_epi8: Component-wise compares two i8 vectors for equality,
//                   filling each lane with 0x00 or 0xff.
// * _mm_cmpgt_epi8: Same as above, but using > rather than ==.
//
// * _mm_loadu_si128:  Performs an unaligned load of an i128.
// * _mm_storeu_si128: Performs an unaligned store of an i128.
//
// * _mm_sign_epi8:     Retains, negates, or zeroes each i8 lane of the first
//                      argument if the corresponding lane of the second
//                      argument is positive, negative, or zero, respectively.
// * _mm_movemask_epi8: Selects the sign bit out of each i8 lane and produces a
//                      bitmask consisting of those bits.
// * _mm_shuffle_epi8:  Selects i8s from the first argument, using the low
//                      four bits of each i8 lane in the second argument as
//                      indices.

// https://github.com/abseil/abseil-cpp/issues/209
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=87853
// _mm_cmpgt_epi8 is broken under GCC with -funsigned-char
// Work around this by using the portable implementation of Group
// when using -funsigned-char under GCC.
static inline __m128i _mm_cmpgt_epi8_fixed(__m128i a, __m128i b) {
#if defined(__GNUC__) && !defined(__clang__) && ( CHAR_MIN == 0 )
  const __m128i mask = _mm_set1_epi8(0x80);
  const __m128i diff = _mm_subs_epi8(b, a);
  return _mm_cmpeq_epi8(_mm_and_si128(diff, mask), mask);
#endif
  return _mm_cmpgt_epi8(a, b);
}

#define GROUP_WIDTH 16
typedef __m128i g_t;
typedef uint16_t gbits;

static inline int _gbits_next(gbits* mut_bitmask) {
  int result = count_trailing_zeroes16(*mut_bitmask);
  *mut_bitmask &= (*mut_bitmask - 1);
  return result;
}

static inline bool _gbits_has_next(gbits bitmask) {
  return bitmask;
}

static inline g_t _group_load(const uint64_t* pos) {
  return _mm_loadu_si128((const __m128i*) pos);
}

// Returns a bitmask representing the positions of slots that match hash.
static inline gbits _group_match(g_t ctrl, uint8_t hash) {
  g_t match = _mm_set1_epi8((char) hash);
  return (gbits) (_mm_movemask_epi8(_mm_cmpeq_epi8(match, ctrl)));
}

// Returns a bitmask representing the positions of empty slots.
static inline gbits _group_mask_empty(g_t ctrl) {
#ifdef ABSL_INTERNAL_HAVE_SSSE3
  // This only works because ctrl_t::kEmpty is -128.
  return _mm_movemask_epi8(_mm_sign_epi8(ctrl, ctrl));
#else
  g_t match = _mm_set1_epi8((char) FLAGS_EMPTY);
  return (gbits) _mm_movemask_epi8(_mm_cmpeq_epi8(match, ctrl));
#endif
}

// Returns a bitmask representing the positions of full slots.
static inline gbits _group_mask_full(g_t ctrl) {
  return (gbits) (_mm_movemask_epi8(ctrl)) ^ 0xffff;
}

// Returns a bitmask representing the positions of empty or deleted slots.
static inline gbits _group_mask_empty_or_deleted(g_t ctrl) {
  g_t special = _mm_set1_epi8((char) FLAGS_SENTINEL);
  return (gbits) _mm_movemask_epi8(_mm_cmpgt_epi8_fixed(special, ctrl));
}

// Returns the number of leading empty or deleted elements in the group.
static inline uint32_t _group_count_leading_empty_or_deleted(g_t ctrl) {
  g_t special = _mm_set1_epi8((char) FLAGS_SENTINEL);
  return count_trailing_zeroes32((uint32_t) (
      _mm_movemask_epi8(_mm_cmpgt_epi8_fixed(special, ctrl)) + 1));
}

// sets the octet at `ctrl` bits [offset*8..offset*8+8] to the given hash
static inline void _group_set(g_t ctrl, int8_t* dst, uint32_t offset, uint8_t hash) {
  // only works because ctrl:kEmpty = -128
  int64_t high = -((int64_t)offset >> 3);
  uint64_t mask = ((uint64_t)hash | 0x80ULL) << ((offset&7) << 3);
  g_t ghash = _mm_set_epi64x(mask & (uint64_t)high, mask & ~((uint64_t) high));
  g_t res = _mm_xor_si128(ctrl, ghash);
  _mm_storeu_si128((g_t*) dst, res);
}

static void _group_convert_special_to_empty_and_full_to_deleted(g_t ctrl, int8_t* dst) {
  g_t msbs = _mm_set1_epi8((char) (-128));
  g_t x126 = _mm_set1_epi8((char) 126);
#ifdef ABSL_INTERNAL_HAVE_SSSE3
  g_t res = _mm_or_si128(_mm_shuffle_epi8(x126, ctrl), msbs);
#else
  g_t zero = _mm_setzero_si128();
  g_t special_mask = _mm_cmpgt_epi8_fixed(zero, ctrl);
  g_t res = _mm_or_si128(msbs, _mm_andnot_si128(special_mask, x126));
#endif
  _mm_storeu_si128((g_t*) dst, res);
}

#elif defined(ABSL_INTERNAL_HAVE_ARM_NEON) && defined(ABSL_IS_LITTLE_ENDIAN)
#define GROUP_WIDTH 8
typedef uint8x8_t g_t;

typedef uint64_t gbits;

static inline int _gbits_next(gbits* mut_bitmask) {
  int result = count_trailing_zeroes64(*mut_bitmask);
  *mut_bitmask &= 0x8080808080808080ULL;
  *mut_bitmask &= (*mut_bitmask - 1);
  return result >> 3;
}

static inline bool _gbits_has_next(gbits bitmask) {
  return bitmask;
}

static inline g_t _group_load(const uint64_t* pos) {
  return vld1_u8((uint8_t*) pos);
}

// Returns a bitmask representing the positions of slots that match hash.
static inline gbits _group_match(g_t ctrl, uint8_t hash) {
  g_t dup = vdup_n_u8(hash);
  g_t mask = vceq_u8(ctrl, dup);
  return vget_lane_u64(vreinterpret_u64_u8(mask), 0);
}

// Returns a bitmask representing the positions of empty slots.
static inline gbits _group_mask_empty(g_t ctrl) {
  return vget_lane_u64(
      vreinterpret_u64_u8(
          vceq_s8(vdup_n_s8(FLAGS_EMPTY_SIGNED), vreinterpret_s8_u8(ctrl))
      ),
      0
  );
}

// Returns a bitmask representing the positions of full slots.
static inline gbits _group_mask_full(g_t ctrl) {
  return vget_lane_u64(
      vreinterpret_u64_u8(
          vcge_s8(vreinterpret_s8_u8(ctrl), vdup_n_s8(0))
      ),
      0
  );
}

// Returns a bitmask representing the positions of empty or deleted slots.
static inline gbits _group_mask_empty_or_deleted(g_t ctrl) {
  return vget_lane_u64(
      vreinterpret_u64_u8(
          vcgt_s8(vdup_n_s8(FLAGS_SENTINEL_SIGNED), vreinterpret_s8_u8(ctrl))
      ),
      0
  );
}

// Returns the number of leading empty or deleted elements in the group.
static inline uint32_t _group_count_leading_empty_or_deleted(g_t ctrl) {
  uint64_t mask = vget_lane_u64(
      vreinterpret_u64_u8(
          vcle_s8(vdup_n_s8(FLAGS_SENTINEL_SIGNED), vreinterpret_s8_u8(ctrl))
      ),
      0
  );
  return count_trailing_zeroes64(mask);
}

static inline void _group_set(g_t ctrl, int8_t* dst, uint32_t offset, uint8_t hash) {
  uint64_t curr = vget_lane_u64(vreinterpret_u64_u8(ctrl), 0);
  uint64_t mask = (((uint64_t)hash) | 0x80ULL) << (offset<<3);
  uint64_t res = curr ^ mask;
#ifdef ABSL_IS_BIG_ENDIAN
  res = gbswap_64(res);
#endif
  memcpy(dst, &res, sizeof res);
}

static void _group_convert_special_to_empty_and_full_to_deleted(g_t ctrl, int8_t* dst) {
  uint64_t mask = vget_lane_u64(vreinterpret_u64_u8(ctrl), 0);
  const uint64_t slsbs = 0x0202020202020202ULL;
  const uint64_t midbs = 0x7e7e7e7e7e7e7e7eULL;
  uint64_t x = slsbs & (mask >> 6);
  uint64_t res = (x + midbs) | 0x8080808080808080ULL;
#ifdef ABSL_IS_BIG_ENDIAN
  res = gbswap_64(res);
#endif
  memcpy(dst, &res, sizeof res);
}
#else
#define GROUP_WIDTH 8
typedef uint64_t g_t;
typedef uint64_t gbits;

static inline int _gbits_next(gbits* mut_bitmask) {
  int result = count_trailing_zeroes64(*mut_bitmask);
  *mut_bitmask &= 0x8080808080808080ULL;
  *mut_bitmask &= (*mut_bitmask - 1);
  return result >> 3;
}

static inline bool _gbits_has_next(gbits bitmask) {
  return bitmask;
}

static inline g_t _group_load(const uint64_t* pos) {
  g_t res = *pos;
#ifdef ABSL_IS_BIG_ENDIAN
  res = gbswap_64(res);
#endif
  return res;  
}

// Returns a bitmask representing the positions of slots that match hash.
static inline gbits _group_match(g_t ctrl, uint8_t hash) {
  // For the technique, see:
  // http://graphics.stanford.edu/~seander/bithacks.html##ValueInWord
  // (Determine if a word has a byte equal to n).
  //
  // Caveat: there are false positives but:
  // - they only occur if there is a real match
  // - they never occur on ctrl_t::kEmpty, ctrl_t::kDeleted, ctrl_t::kSentinel
  // - they will be handled gracefully by subsequent checks in code
  //
  // Example:
  //   v = 0x1716151413121110
  //   hash = 0x12
  //   retval = (v - lsbs) & ~v & msbs = 0x0000000080800000
  const uint64_t lsbs = 0x0101010101010101ULL;
  g_t x = ctrl ^ (lsbs * hash);
  return ((x - lsbs) & ~x & 0x8080808080808080ULL);
}

// Returns a bitmask representing the positions of empty slots.
static inline gbits _group_mask_empty(g_t ctrl) {
  return (ctrl & ~(ctrl << 6)) & 0x8080808080808080ULL;
}

// Returns a bitmask representing the positions of full slots.
static inline gbits _group_mask_full(g_t ctrl) {
  return (ctrl ^ 0x8080808080808080ULL) & 0x8080808080808080ULL;
}

// Returns a bitmask representing the positions of empty or deleted slots.
static inline gbits _group_mask_empty_or_deleted(g_t ctrl) {
  return (ctrl & ~(ctrl << 7)) & 0x8080808080808080ULL;
}

// Returns the number of leading empty or deleted elements in the group.
static inline uint32_t _group_count_leading_empty_or_deleted(g_t ctrl) {
  // ctrl | ~(ctrl >> 7) will have the lowest bit set to zero for kEmpty and
  // kDeleted. We lower all other bits and count number of trailing zeros.
  const uint64_t bits = 0x0101010101010101ULL;
  return count_trailing_zeroes64((ctrl | ~(ctrl >> 7)) & bits) >> 3;
}

static inline void _group_set(g_t ctrl, int8_t* dst, uint32_t offset, uint8_t hash) {
  uint64_t mask = (((uint64_t)hash) | 0x80ULL) << (offset<<3);
  uint64_t res = ctrl ^ mask;
#ifdef ABSL_IS_BIG_ENDIAN
  res = gbswap_64(res);
#endif
  memcpy(dst, &res, sizeof res);
}

static void _group_convert_special_to_empty_and_full_to_deleted(g_t ctrl, int8_t* dst) {
  const uint64_t lsbs = 0x0101010101010101ULL;
  g_t x = ctrl & 0x8080808080808080ULL;
  g_t res = (~x + (x >> 7)) & ~lsbs;
#ifdef ABSL_IS_BIG_ENDIAN
  res = gbswap_64(res);
#endif
  memcpy(dst, &res, sizeof res);
}
#endif  // SIMD availability switch

#endif  // MICRODICT_SIMD_H_
