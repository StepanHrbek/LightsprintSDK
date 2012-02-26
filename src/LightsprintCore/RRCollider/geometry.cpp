// --------------------------------------------------------------------------
// Helper AABB code for ray-mesh intersections.
// Copyright (c) 2000-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cmath>
#include "geometry.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// box in 3d

void Box::init(RRVec3 _min, RRVec3 _max)
{
	min = _min;
	max = _max;
	// bsp tree traversal requires small overlap to fight precision problems
	// may be deleted if i manage to solve precision problem inside traversal code
	real d = (max.x-min.x + max.y-min.y + max.z-min.z) * 1e-5f;
	min -= Vec3(d,d,d); 
	max += Vec3(d,d,d);
	RR_ASSERT(IS_VEC3(min));
	RR_ASSERT(IS_VEC3(max));
}

#ifndef USE_SSE

float minf(const float a, const float b) { return a < b ? a : b; }
float maxf(const float a, const float b) { return a > b ? a : b; }

bool Box::intersect(RRRay* ray) const
// inputs: rayOrigin, rayDir[Inv], rayLengthMin, rayLengthMax
// outputs: hitDistanceMin, hitDistanceMax
// source: Thierry Berger-Perrin, http://ompf.org/ray/ray_box.html
{
	float
#ifdef BOX_INPUT_INVDIR
		l1	= (min[0] - ray->rayOrigin[0]) * ray->rayDirInv[0],
		l2	= (max[0] - ray->rayOrigin[0]) * ray->rayDirInv[0],
		lmin	= minf(l1,l2),
		lmax	= maxf(l1,l2);

	l1	= (min[1] - ray->rayOrigin[1]) * ray->rayDirInv[1];
	l2	= (max[1] - ray->rayOrigin[1]) * ray->rayDirInv[1];
	lmin	= maxf(minf(l1,l2), lmin);
	lmax	= minf(maxf(l1,l2), lmax);

	l1	= (min[2] - ray->rayOrigin[2]) * ray->rayDirInv[2];
	l2	= (max[2] - ray->rayOrigin[2]) * ray->rayDirInv[2];
	lmin	= maxf(minf(l1,l2), lmin);
	lmax	= minf(maxf(l1,l2), lmax);
#else
		l1	= (min[0] - ray->rayOrigin[0]) / ray->rayDir[0],
		l2	= (max[0] - ray->rayOrigin[0]) / ray->rayDir[0],
		lmin	= minf(l1,l2),
		lmax	= maxf(l1,l2);

	l1	= (min[1] - ray->rayOrigin[1]) / ray->rayDir[1];
	l2	= (max[1] - ray->rayOrigin[1]) / ray->rayDir[1];
	lmin	= maxf(minf(l1,l2), lmin);
	lmax	= minf(maxf(l1,l2), lmax);

	l1	= (min[2] - ray->rayOrigin[2]) / ray->rayDir[2];
	l2	= (max[2] - ray->rayOrigin[2]) / ray->rayDir[2];
	lmin	= maxf(minf(l1,l2), lmin);
	lmax	= minf(maxf(l1,l2), lmax);
#endif

#ifndef COLLIDER_INPUT_UNLIMITED_DISTANCE
	lmin	= maxf(ray->rayLengthMin, lmin);
	lmax	= minf(ray->rayLengthMax, lmax);
#endif
	ray->hitDistanceMin = lmin;
	ray->hitDistanceMax = lmax;

	//return ((lmax > 0.f) & (lmax >= lmin));
	//return ((lmax > 0.f) & (lmax > lmin));
	return ((lmax >= 0.f) & (lmax >= lmin));
}

#else // USE_SSE

#include <float.h>
#include <math.h>
#include <xmmintrin.h>

// turn those verbose intrinsics into something readable.
#define loadps(mem)		_mm_load_ps((const float * const)(mem))
#define storess(ss,mem)		_mm_store_ss((float * const)(mem),(ss))
#define minss			_mm_min_ss
#define maxss			_mm_max_ss
#define minps			_mm_min_ps
#define maxps			_mm_max_ps
#define mulps			_mm_mul_ps
#define divps			_mm_div_ps
#define subps			_mm_sub_ps
#define rotatelps(ps)		_mm_shuffle_ps((ps),(ps), 0x39)	// a,b,c,d -> b,c,d,a
#define muxhps(low,high)	_mm_movehl_ps((low),(high))	// low{a,b,c,d}|high{e,f,g,h} = {c,d,g,h}

static const float flt_plus_inf = -logf(0);	// let's keep C and C++ compilers happy.
static const float _MM_ALIGN16
ps_cst_plus_inf[4]	= {  flt_plus_inf,  flt_plus_inf,  flt_plus_inf,  flt_plus_inf },
ps_cst_minus_inf[4]	= { -flt_plus_inf, -flt_plus_inf, -flt_plus_inf, -flt_plus_inf };

bool Box::intersect(RRRay* ray) const
// inputs: rayOrigin, rayDirInv, rayLengthMin, rayLengthMax
// outputs: hitDistanceMin, hitDistanceMax
// source: Thierry Berger-Perrin, http://www.flipcode.com/cgi-bin/fcarticles.cgi?show=4&id=65014
{
	// use whatever's apropriate to load.
	const __m128
		box_min	= loadps(&min),
		box_max	= loadps(&max),
		pos	= loadps(&ray->rayOrigin),
#ifdef BOX_INPUT_INVDIR
		// use a mul if inverted directions are available
		inv_dir	= loadps(&ray->rayDirInv);

	const __m128 l1 = mulps(subps(box_min, pos), inv_dir);
	const __m128 l2 = mulps(subps(box_max, pos), inv_dir);
#else
		// use a div if inverted directions aren't available
		dir	= loadps(&ray->rayDir);

	const __m128 l1 = divps(subps(box_min, pos), dir);
	const __m128 l2 = divps(subps(box_max, pos), dir);
#endif

#ifdef USE_FAST_BOX
	// returns bogus result and bogus min/max interval when
	// (ray->rayOrigin==min || ray->rayOrigin==max) && ray->rayDir==0
	// in at least one axis.
	__m128 lmax = maxps(l1, l2);
	__m128 lmin = minps(l1, l2);
#else
	const __m128
		plus_inf	= loadps(ps_cst_plus_inf),
		minus_inf	= loadps(ps_cst_minus_inf);
	// the order we use for those min/max is vital to filter out
	// NaNs that happens when an inv_dir is +/- inf and
	// (box_min - pos) is 0. inf * 0 = NaN
	const __m128 filtered_l1a = minps(l1, plus_inf);
	const __m128 filtered_l2a = minps(l2, plus_inf);

	const __m128 filtered_l1b = maxps(l1, minus_inf);
	const __m128 filtered_l2b = maxps(l2, minus_inf);

	// now that we're back on our feet, test those slabs.
	__m128 lmax = maxps(filtered_l1a, filtered_l2a);
	__m128 lmin = minps(filtered_l1b, filtered_l2b);
#endif

	// unfold back. try to hide the latency of the shufps & co.
	const __m128 lmax0 = rotatelps(lmax);
	const __m128 lmin0 = rotatelps(lmin);
	lmax = minss(lmax, lmax0);
	lmin = maxss(lmin, lmin0);

	const __m128 lmax1 = muxhps(lmax,lmax);
	const __m128 lmin1 = muxhps(lmin,lmin);
	lmax = minss(lmax, lmax1);
	lmin = maxss(lmin, lmin1);

#ifdef COLLIDER_INPUT_UNLIMITED_DISTANCE
	const bool ret = ( _mm_comige_ss(lmax, _mm_setzero_ps()) & _mm_comige_ss(lmax,lmin) ) != 0;
	storess(lmin, &ray->hitDistanceMin);
	storess(lmax, &ray->hitDistanceMax);
	return ret;
#else
	float t_near,t_far;
	storess(lmin, &t_near);
	storess(lmax, &t_far);

	ray->hitDistanceMin = RR_MAX(t_near,ray->rayLengthMin);
	ray->hitDistanceMax = RR_MIN(t_far,ray->rayLengthMax);
	return ray->hitDistanceMin<ray->hitDistanceMax;
#endif
}

// the same for unaligned box
bool Box::intersectUnaligned(RRRay* ray) const
{
	const __m128
		box_min	= _mm_loadu_ps(&min.x),
		box_max	= _mm_loadu_ps(&max.x),
		pos	= loadps(&ray->rayOrigin),
#ifdef BOX_INPUT_INVDIR
		// use a mul if inverted directions are available
		inv_dir	= loadps(&ray->rayDirInv);

	const __m128 l1 = mulps(subps(box_min, pos), inv_dir);
	const __m128 l2 = mulps(subps(box_max, pos), inv_dir);
#else
		// use a div if inverted directions aren't available
		dir	= loadps(&ray->rayDir);

	const __m128 l1 = divps(subps(box_min, pos), dir);
	const __m128 l2 = divps(subps(box_max, pos), dir);
#endif

	__m128 lmax = maxps(l1, l2);
	__m128 lmin = minps(l1, l2);

	const __m128 lmax0 = rotatelps(lmax);
	const __m128 lmin0 = rotatelps(lmin);
	lmax = minss(lmax, lmax0);
	lmin = maxss(lmin, lmin0);

	const __m128 lmax1 = muxhps(lmax,lmax);
	const __m128 lmin1 = muxhps(lmin,lmin);
	lmax = minss(lmax, lmax1);
	lmin = maxss(lmin, lmin1);

	float t_near,t_far;
	storess(lmin, &t_near);
	storess(lmax, &t_far);

	ray->hitDistanceMin = RR_MAX(t_near,ray->rayLengthMin);
	ray->hitDistanceMax = RR_MIN(t_far,ray->rayLengthMax);
	return ray->hitDistanceMin<ray->hitDistanceMax;
}

#endif

} // namespace
