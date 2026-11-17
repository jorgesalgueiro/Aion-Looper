// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#if !defined(AION_DRIVE_FORCE_SCALAR) && (defined(_M_X64) || defined(__SSE2__))
#include <emmintrin.h>
#define AION_DRIVE_SSE2 1
#endif
#if defined(_MSC_VER)
#define AION_DRIVE_INLINE __forceinline
#else
#define AION_DRIVE_INLINE inline __attribute__((always_inline))
#endif
namespace aion::drive_detail {
// Interleaved independent channels, no cross-channel reductions. SSE2 is part
// of the x64 baseline; other targets retain the same portable scalar equations.
struct Frame {
#ifdef AION_DRIVE_SSE2
 __m128d v;
 Frame():v(_mm_setzero_pd()){}
 Frame(double x):v(_mm_set1_pd(x)){}
 Frame(double l,double r):v(_mm_set_pd(r,l)){}
 Frame(__m128d x):v(x){}
 double lane(unsigned i)const{return _mm_cvtsd_f64(i?_mm_unpackhi_pd(v,v):v);}
#else
 std::array<double,2> v{};
 Frame()=default;
 Frame(double x):v{x,x}{}
 Frame(double l,double r):v{l,r}{}
 double lane(unsigned i)const{return v[i];}
#endif
};
static_assert(sizeof(Frame)==2*sizeof(double));
#ifdef AION_DRIVE_SSE2
using Mask=__m128d;
AION_DRIVE_INLINE __m128d load(Frame x){return x.v;}
AION_DRIVE_INLINE Frame frame(__m128d x){return Frame(x);}
AION_DRIVE_INLINE Frame operator+(Frame a,Frame b){return frame(_mm_add_pd(load(a),load(b)));}
AION_DRIVE_INLINE Frame operator-(Frame a,Frame b){return frame(_mm_sub_pd(load(a),load(b)));}
AION_DRIVE_INLINE Frame operator*(Frame a,Frame b){return frame(_mm_mul_pd(load(a),load(b)));}
AION_DRIVE_INLINE Frame operator/(Frame a,Frame b){return frame(_mm_div_pd(load(a),load(b)));}
AION_DRIVE_INLINE Frame minimum(Frame a,Frame b){return frame(_mm_min_pd(load(a),load(b)));}
AION_DRIVE_INLINE Frame maximum(Frame a,Frame b){return frame(_mm_max_pd(load(a),load(b)));}
AION_DRIVE_INLINE Frame absolute(Frame a){return frame(_mm_andnot_pd(_mm_set1_pd(-0.0),load(a)));}
AION_DRIVE_INLINE Frame root(Frame a){return frame(_mm_sqrt_pd(load(a)));}
AION_DRIVE_INLINE Mask less(Frame a,Frame b){return _mm_cmplt_pd(load(a),load(b));}
AION_DRIVE_INLINE Mask lessEqual(Frame a,Frame b){return _mm_cmple_pd(load(a),load(b));}
AION_DRIVE_INLINE bool both(Mask m){return _mm_movemask_pd(m)==3;}
AION_DRIVE_INLINE Frame select(Mask m,Frame a,Frame b){return frame(_mm_or_pd(_mm_and_pd(m,load(a)),_mm_andnot_pd(m,load(b))));}
#else
using Mask=std::array<bool,2>;
AION_DRIVE_INLINE Frame operator+(Frame a,Frame b){return {a.v[0]+b.v[0],a.v[1]+b.v[1]};}
AION_DRIVE_INLINE Frame operator-(Frame a,Frame b){return {a.v[0]-b.v[0],a.v[1]-b.v[1]};}
AION_DRIVE_INLINE Frame operator*(Frame a,Frame b){return {a.v[0]*b.v[0],a.v[1]*b.v[1]};}
AION_DRIVE_INLINE Frame operator/(Frame a,Frame b){return {a.v[0]/b.v[0],a.v[1]/b.v[1]};}
AION_DRIVE_INLINE Frame minimum(Frame a,Frame b){return {std::min(a.v[0],b.v[0]),std::min(a.v[1],b.v[1])};}
AION_DRIVE_INLINE Frame maximum(Frame a,Frame b){return {std::max(a.v[0],b.v[0]),std::max(a.v[1],b.v[1])};}
AION_DRIVE_INLINE Frame absolute(Frame a){return {std::abs(a.v[0]),std::abs(a.v[1])};}
AION_DRIVE_INLINE Frame root(Frame a){return {std::sqrt(a.v[0]),std::sqrt(a.v[1])};}
AION_DRIVE_INLINE Mask less(Frame a,Frame b){return {a.v[0]<b.v[0],a.v[1]<b.v[1]};}
AION_DRIVE_INLINE Mask lessEqual(Frame a,Frame b){return {a.v[0]<=b.v[0],a.v[1]<=b.v[1]};}
AION_DRIVE_INLINE bool both(Mask m){return m[0]&&m[1];}
AION_DRIVE_INLINE Frame select(Mask m,Frame a,Frame b){return {m[0]?a.v[0]:b.v[0],m[1]?a.v[1]:b.v[1]};}
#endif
AION_DRIVE_INLINE Frame flush(Frame x){return select(less(absolute(x),Frame(1.e-30)),Frame(0),x);}
}
#undef AION_DRIVE_INLINE
#undef AION_DRIVE_SSE2
