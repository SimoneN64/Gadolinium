#pragma once
#include <bitset>
#include <cstdint>
#ifdef USE_NEON
#include <sse2neon.h>
#else
#include <emmintrin.h>
#endif  

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;
using u128 = __uint128_t;
using s128 = __int128_t;
using m128i = __m128i;

#define FORCE_INLINE inline __attribute__((always_inline))

constexpr u32 N64_CPU_FREQ = 93750000;
constexpr u16 KAIZEN_VERSION = 0x010;
extern std::string savePath;

static FORCE_INLINE constexpr u32 GetCyclesPerFrame(bool pal) {
  if (pal) {
    return N64_CPU_FREQ / 50;
  } else {
    return N64_CPU_FREQ / 60;
  }
}

static FORCE_INLINE constexpr u32 GetVideoFrequency(bool pal) {
  if (pal) {
    return 49'656'530;
  } else {
    return 48'681'812;
  }
}

#define HALF_ADDRESS(addr) ((addr) ^ 2)
#define BYTE_ADDRESS(addr) ((addr) ^ 3)

#define RD(x) (((x) >> 11) & 0x1F)
#define RT(x) (((x) >> 16) & 0x1F)
#define RS(x) (((x) >> 21) & 0x1F)
#define FD(x) (((x) >> 6) & 0x1F)
#define FT(x) RT(x)
#define FS(x) RD(x)
#define BASE(x) RS(x)
#define VT(x) (((x) >> 16) & 0x1F)
#define VS(x) (((x) >> 11) & 0x1F)
#define VD(x) (((x) >> 6) & 0x1F)
#define E1(x) (((x) >> 7) & 0x0F)
#define E2(x) (((x) >> 21) & 0x0F)
#define ELEMENT_INDEX(i) (7 - (i))
#define BYTE_INDEX(i) (15 - (i))

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#define ABI_WINDOWS
#else
#define ABI_UNIX
#endif
