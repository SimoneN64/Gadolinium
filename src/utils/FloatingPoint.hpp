#pragma once
#include <cmath>
#include <common.hpp>

namespace Util {
static FORCE_INLINE auto roundCeil(float f) {
#ifdef SIMD_SUPPORT
  __m128 t = _mm_set_ss(f);
  t = _mm_round_ss(t, t, _MM_FROUND_TO_POS_INF);
  return _mm_cvtss_f32(t);
#else
  return ceilf(f);
#endif
}

static FORCE_INLINE auto roundCeil(double f) {
#ifdef SIMD_SUPPORT
  __m128d t = _mm_set_sd(f);
  t = _mm_round_sd(t, t, _MM_FROUND_TO_POS_INF);
  return _mm_cvtsd_f64(t);
#else
  return ceil(f);
#endif
}

static FORCE_INLINE auto roundNearest(float f) {
#ifdef SIMD_SUPPORT
  __m128 t = _mm_set_ss(f);
  t = _mm_round_ss(t, t, _MM_FROUND_TO_NEAREST_INT);
  return _mm_cvtss_f32(t);
#else
  return roundf(f);
#endif
}

static FORCE_INLINE auto roundNearest(double f) {
#ifdef SIMD_SUPPORT
  __m128d t = _mm_set_sd(f);
  t = _mm_round_sd(t, t, _MM_FROUND_TO_NEAREST_INT);
  return _mm_cvtsd_f64(t);
#else
  return round(f);
#endif
}

static FORCE_INLINE auto roundCurrent(float f) {
#ifdef SIMD_SUPPORT
  auto t = _mm_set_ss(f);
  t = _mm_round_ss(t, t, _MM_FROUND_CUR_DIRECTION);
  return _mm_cvtss_f32(t);
#else
  return rint(f);
#endif
}

static FORCE_INLINE auto roundCurrent(double f) {
#ifdef SIMD_SUPPORT
  auto t = _mm_set_sd(f);
  t = _mm_round_sd(t, t, _MM_FROUND_CUR_DIRECTION);
  return _mm_cvtsd_f64(t);
#else
  return rint(f);
#endif
}


static FORCE_INLINE auto roundFloor(float f) {
#ifdef SIMD_SUPPORT
  __m128 t = _mm_set_ss(f);
  t = _mm_round_ss(t, t, _MM_FROUND_TO_NEG_INF);
  return _mm_cvtss_f32(t);
#else
  return floor(f);
#endif
}

static FORCE_INLINE auto roundFloor(double f) {
#ifdef SIMD_SUPPORT
  __m128d t = _mm_set_sd(f);
  t = _mm_round_sd(t, t, _MM_FROUND_TO_NEG_INF);
  return _mm_cvtsd_f64(t);
#else
  return floor(f);
#endif
}

static FORCE_INLINE auto roundTrunc(float f) {
#ifdef SIMD_SUPPORT
  __m128 t = _mm_set_ss(f);
  t = _mm_round_ss(t, t, _MM_FROUND_TO_ZERO);
  return _mm_cvtss_f32(t);
#else
  return trunc(f);
#endif
}

static FORCE_INLINE auto roundTrunc(double f) {
#ifdef SIMD_SUPPORT
  __m128d t = _mm_set_sd(f);
  t = _mm_round_sd(t, t, _MM_FROUND_TO_ZERO);
  return _mm_cvtsd_f64(t);
#else
  return trunc(f);
#endif
}
} // namespace Util
