// Copyright 2020 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ABSL_NUMERIC_INTERNAL_BITS_H_
#define ABSL_NUMERIC_INTERNAL_BITS_H_

#include <stdint.h>
#include "./optimization.h"

// ABSL_IS_LITTLE_ENDIAN
// ABSL_IS_BIG_ENDIAN
//
// Checks the endianness of the platform.
//
// Notes: uses the built in endian macros provided by GCC (since 4.6) and
// Clang (since 3.2); see
// https://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html.
// Otherwise, if _WIN32, assume little endian. Otherwise, bail with an error.
#if defined(ABSL_IS_BIG_ENDIAN)
#error "ABSL_IS_BIG_ENDIAN cannot be directly set."
#endif
#if defined(ABSL_IS_LITTLE_ENDIAN)
#error "ABSL_IS_LITTLE_ENDIAN cannot be directly set."
#endif

#if (defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
     __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define ABSL_IS_LITTLE_ENDIAN 1
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define ABSL_IS_BIG_ENDIAN 1
#elif defined(_WIN32)
#define ABSL_IS_LITTLE_ENDIAN 1
#else
#error "absl endian detection needs to be set up for your compiler"
#endif

// Clang on Windows has __builtin_clzll; otherwise we need to use the
// windows intrinsic functions.
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

#if ABSL_NUMERIC_INTERNAL_HAVE_BUILTIN_OR_GCC(__builtin_popcountl) && \
    ABSL_NUMERIC_INTERNAL_HAVE_BUILTIN_OR_GCC(__builtin_popcountll)
#define ABSL_INTERNAL_CONSTEXPR_POPCOUNT constexpr
#define ABSL_INTERNAL_HAS_CONSTEXPR_POPCOUNT 1
#else
#define ABSL_INTERNAL_CONSTEXPR_POPCOUNT
#define ABSL_INTERNAL_HAS_CONSTEXPR_POPCOUNT 0
#endif

#if ABSL_NUMERIC_INTERNAL_HAVE_BUILTIN_OR_GCC(__builtin_clz) && \
    ABSL_NUMERIC_INTERNAL_HAVE_BUILTIN_OR_GCC(__builtin_clzll)
#define ABSL_INTERNAL_CONSTEXPR_CLZ constexpr
#define ABSL_INTERNAL_HAS_CONSTEXPR_CLZ 1
#else
#define ABSL_INTERNAL_CONSTEXPR_CLZ
#define ABSL_INTERNAL_HAS_CONSTEXPR_CLZ 0
#endif

#if ABSL_NUMERIC_INTERNAL_HAVE_BUILTIN_OR_GCC(__builtin_ctz) && \
    ABSL_NUMERIC_INTERNAL_HAVE_BUILTIN_OR_GCC(__builtin_ctzll)
#define ABSL_INTERNAL_CONSTEXPR_CTZ constexpr
#define ABSL_INTERNAL_HAS_CONSTEXPR_CTZ 1
#else
#define ABSL_INTERNAL_CONSTEXPR_CTZ
#define ABSL_INTERNAL_HAS_CONSTEXPR_CTZ 0
#endif

static inline uint64_t gbswap_64(uint64_t host_int) {
#if ABSL_HAVE_BUILTIN(__builtin_bswap64)
  return __builtin_bswap64(host_int);
#elif defined(__GNUC__)
  return __builtin_bswap64(host_int);
#elif defined(_MSC_VER)
  return _byteswap_uint64(host_int);
#else
  return (((host_int & uint64_t{0xFF}) << 56) |
          ((host_int & uint64_t{0xFF00}) << 40) |
          ((host_int & uint64_t{0xFF0000}) << 24) |
          ((host_int & uint64_t{0xFF000000}) << 8) |
          ((host_int & uint64_t{0xFF00000000}) >> 8) |
          ((host_int & uint64_t{0xFF0000000000}) >> 24) |
          ((host_int & uint64_t{0xFF000000000000}) >> 40) |
          ((host_int & uint64_t{0xFF00000000000000}) >> 56));
#endif
}

static inline int count_leading_zeroes_unchecked32(uint32_t x) {
#if ABSL_NUMERIC_INTERNAL_HAVE_BUILTIN_OR_GCC(__builtin_clz)
  // Use __builtin_clz, which uses the following instructions:
  //  x86: bsr, lzcnt
  //  ARM64: clz
  //  PPC: cntlzd

  // static_assert(sizeof(unsigned int) == sizeof(x),
  //               "__builtin_clz does not take 32-bit arg");
  // Handle 0 as a special case because __builtin_clz(0) is undefined.
  return x == 0 ? 32 : __builtin_clz(x);
#elif defined(_MSC_VER) && !defined(__clang__)
  unsigned long result = 0;  // NOLINT(runtime/int)
  if (_BitScanReverse(&result, x)) {
    return 31 - result;
  }
  return 32;
#else
  int zeroes = 28;
  if (x >> 16) {
    zeroes -= 16;
    x >>= 16;
  }
  if (x >> 8) {
    zeroes -= 8;
    x >>= 8;
  }
  if (x >> 4) {
    zeroes -= 4;
    x >>= 4;
  }
  return "\4\3\2\2\1\1\1\1\0\0\0\0\0\0\0"[x] + zeroes;
#endif
}

static inline int count_trailing_zeroes_unchecked32(uint32_t x) {
#if ABSL_NUMERIC_INTERNAL_HAVE_BUILTIN_OR_GCC(__builtin_ctz)
  // static_assert(sizeof(unsigned int) == sizeof(x),
  //               "__builtin_ctz does not take 32-bit arg");
  return __builtin_ctz(x);
#elif defined(_MSC_VER) && !defined(__clang__)
  unsigned long result = 0;  // NOLINT(runtime/int)
  _BitScanForward(&result, x);
  return result;
#else
  int c = 31;
  x &= ~x + 1;
  if (x & 0x0000FFFF) c -= 16;
  if (x & 0x00FF00FF) c -= 8;
  if (x & 0x0F0F0F0F) c -= 4;
  if (x & 0x33333333) c -= 2;
  if (x & 0x55555555) c -= 1;
  return c;
#endif
}

static inline int count_trailing_zeroes_unchecked64(uint64_t x) {
#if ABSL_NUMERIC_INTERNAL_HAVE_BUILTIN_OR_GCC(__builtin_ctzll)
  // static_assert(sizeof(unsigned long long) == sizeof(x),  // NOLINT(runtime/int)
  //               "__builtin_ctzll does not take 64-bit arg");
  return __builtin_ctzll(x);
#elif defined(_MSC_VER) && !defined(__clang__) && \
    (defined(_M_X64) || defined(_M_ARM64))
  unsigned long result = 0;  // NOLINT(runtime/int)
  _BitScanForward64(&result, x);
  return result;
#elif defined(_MSC_VER) && !defined(__clang__)
  unsigned long result = 0;  // NOLINT(runtime/int)
  if ((uint32_t) x == 0) {
    _BitScanForward(&result, (unsigned long)(x >> 32));
    return result + 32;
  }
  _BitScanForward(&result, (unsigned long) x);
  return result;
#else
  int c = 63;
  x &= ~x + 1;
  if (x & 0x00000000FFFFFFFF) c -= 32;
  if (x & 0x0000FFFF0000FFFF) c -= 16;
  if (x & 0x00FF00FF00FF00FF) c -= 8;
  if (x & 0x0F0F0F0F0F0F0F0F) c -= 4;
  if (x & 0x3333333333333333) c -= 2;
  if (x & 0x5555555555555555) c -= 1;
  return c;
#endif
}

static inline int count_trailing_zeroes_unchecked16(uint16_t x) {
#if ABSL_HAVE_BUILTIN(__builtin_ctzs)
  // static_assert(sizeof(unsigned short) == sizeof(x),  // NOLINT(runtime/int)
  //               "__builtin_ctzs does not take 16-bit arg");
  return __builtin_ctzs(x);
#else
  return count_trailing_zeroes_unchecked32(x);
#endif
}

static inline int count_trailing_zeroes32(uint32_t x) {
  return (x == 0) ? 32 : count_trailing_zeroes_unchecked32(x);
}

static inline int count_trailing_zeroes64(uint64_t x) {
  return (x == 0) ? 64 : count_trailing_zeroes_unchecked64(x);
}

static inline int count_trailing_zeroes16(uint16_t x) {
  return (x == 0) ? 16 : count_trailing_zeroes_unchecked16(x);
}

#endif  // ABSL_NUMERIC_INTERNAL_BITS_H_
