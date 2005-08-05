#include <assert.h>
#include <math.h>
#include <memory.h>
#include "geometry.h"

#ifdef USE_SPHERE
	#ifndef __GNUC__ //!!! gcc emits linker error with miniball
		#define MINIBALL
	#endif
	#ifdef MINIBALL
		#include "miniball.h"
	#endif
#endif

namespace rrIntersect
{

//////////////////////////////////////////////////////////////////////////////
//
// 3d vector

Vec3 operator -(const Vec3& a)
{
	return Vec3(-a.x,-a.y,-a.z);
}

real size(const Vec3& a)
{
	return sqrt(a.x*a.x+a.y*a.y+a.z*a.z);
}

real size2(Vec3 a)
{
	return a.x*a.x+a.y*a.y+a.z*a.z;
}

Vec3 normalized(Vec3 a)
{
	return a/size(a);
}

real dot(const Vec3& a, const Vec3& b)
{
	return a.x*b.x+a.y*b.y+a.z*b.z;
}

// dohoda:
// predpokladejme triangl s vrcholy 0,a,b.
// pri pohledu na jeho vnejsi stranu (tu s normalou) vidime vrcholy
// serazene proti smeru hodinovych rucicek.
// jinak receno: ortogonalTo(doprava,dopredu)==nahoru

Vec3 ortogonalTo(const Vec3& a, const Vec3& b)
{
	return Vec3(a.y*b.z-a.z*b.y,-a.x*b.z+a.z*b.x,a.x*b.y-a.y*b.x);
}

//////////////////////////////////////////////////////////////////////////////
//
// plane in 3d

real normalValueIn(Plane& n,Vec3& a)
{
	return a.x*n.x+a.y*n.y+a.z*n.z+n.d;
}

//////////////////////////////////////////////////////////////////////////////
//
// sphere in 3d

#ifdef USE_SPHERE

void Sphere::detect(const Vec3 *vertex,unsigned vertices)
{
#ifdef MINIBALL
	Miniball<3>     mb;
	Miniball<3>::Point p;
	for(unsigned i=0;i<vertices;i++) 
	{
		p[0] = vertex[i][0];
		p[1] = vertex[i][1];
		p[2] = vertex[i][2];
		mb.check_in(p);
	}
	mb.build();
	center.x = (real)mb.center()[0];
	center.y = (real)mb.center()[1];
	center.z = (real)mb.center()[2];
	radius2 = (real)mb.squared_radius();
#else
	Vec3 sum = Vec3(0,0,0);
	for(unsigned i=0;i<vertices;i++) sum += vertex[i];
	center = sum/vertices;
	radius2 = 0;
	for(unsigned i=0;i<vertices;i++) 
	{
		real tmp = size2(vertex[i]-center);
		if(tmp>radius2) radius2=tmp;
	}
#endif
	radius = sqrt(radius2);
}

bool Sphere::intersect(RRRay* ray) const
// Returns if intersection was detected.
{
	Vec3 toCenter = center-*(Vec3*)ray->rayOrigin;
	real distEyeCenter2 = size2(toCenter);            //3*mul
	// eye inside sphere
	if(distEyeCenter2-radius2<0) return true;
	real distEyeCenter=sqrt(distEyeCenter2);             //1*sqrt
	// sphere too far
	if(distEyeCenter-radius>=ray->hitDistanceMax) return false;
	assert(fabs(size2(*(Vec3*)ray->rayDir)-1)<0.001);
	Vec3 nearCenter = *(Vec3*)ray->rayOrigin + *(Vec3*)ray->rayDir * distEyeCenter; //3*mul
	real distNrcntrCenter2=size2(nearCenter-center);//3*mul
	// probably no intersection
	if(distNrcntrCenter2>=radius2) return false;
	return true;
}

#endif

//////////////////////////////////////////////////////////////////////////////
//
// box in 3d

void Box::detect(const Vec3 *vertex,unsigned vertices)
{
	min = Vec3(+1e16f,+1e16f,+1e16f);
	max = Vec3(-1e16f,-1e16f,-1e16f);
	for(unsigned i=0;i<vertices;i++)
	{
		min.x = MIN(min.x,vertex[i].x);
		min.y = MIN(min.y,vertex[i].y);
		min.z = MIN(min.z,vertex[i].z);
		max.x = MAX(max.x,vertex[i].x);
		max.y = MAX(max.y,vertex[i].y);
		max.z = MAX(max.z,vertex[i].z);
	}
	// bsp tree traversal requires small overlap to fight precision problems
	// may be deleted if i manage to solve precision problem inside traversal code
	real d = (max.x-min.x + max.y-min.y + max.z-min.z) * 1e-5f;
	min -= Vec3(d,d,d); 
	max += Vec3(d,d,d);
}

#ifndef USE_SSE

// ray-box intersect for non-sse machines

bool Box::intersect(RRRay* ray) const
// Shrinks hitDistanceMin..hitDistanceMax to be as tight as possible.
// Returns if intersection was detected.
{
	const Vec3 &lineBaseT = *(Vec3*)ray->rayOrigin;
	const Vec3 &lineDirT = *(Vec3*)ray->rayDir;

	float sfNumerator;
	float sfDenominator;  
	float Qx = min.x;
	float Qy = min.y;
	float Qz = min.z;
	float nearI = 1e17f;	// min. vzdalenost pruseciku pro tento BB
	float farI = 0;

	// test na to, jestli je pocatek primky uvnitr boxu
	if(( lineBaseT.x >= min.x ) && ( lineBaseT.x <= max.x )&& 
		( lineBaseT.y >= min.y ) && ( lineBaseT.y <= max.y )&& 
		( lineBaseT.z >= min.z ) && ( lineBaseT.z <= max.z )) 
	{ 
		nearI = 0;
	}

	// rovina kolma na osu Z
	sfNumerator = Qz - lineBaseT.z;
	sfDenominator = lineDirT.z;
	if( sfDenominator ) 
	{
		float t = sfNumerator / sfDenominator;
		if( t >= 0 )
		{ 
			// protnuti jen smerem dopredu dle lineDir
			Vec3 p = lineBaseT + lineDirT * t; //prusecik primky se stranou BB
			if( ( p.x >= min.x ) && ( p.x <= max.x ) && ( p.y >= min.y ) && ( p.y <= max.y ) ) 
			{  //prusecik je v oblasti BB
				nearI = MIN(nearI,t);
				farI = MAX(farI,t);
			}
		}	
	}

	// rovina kolma na osu Y
	sfNumerator = Qy - lineBaseT.y;
	sfDenominator = lineDirT.y;
	if( sfDenominator ) 
	{
		float t = sfNumerator / sfDenominator;
		if( t >= 0 )
		{ 
			// protnuti jen smerem dopredu dle lineDir
			Vec3 p = lineBaseT + lineDirT * t; //prusecik primky se stranou BB
			if( ( p.z >= min.z ) && ( p.z <= max.z ) && ( p.x >= min.x ) && ( p.x <= max.x ) )
			{  //prusecik je v oblasti BB
				nearI = MIN(nearI,t);
				farI = MAX(farI,t);
			}
		}	
	}

	// rovina kolma na osu X
	sfNumerator = Qx - lineBaseT.x;
	sfDenominator = lineDirT.x;
	if( sfDenominator ) 
	{
		float t = sfNumerator / sfDenominator;
		if( t >= 0 )
		{ 
			// protnuti jen smerem dopredu dle lineDir
			Vec3 p = lineBaseT + lineDirT * t; //prusecik primky se stranou BB
			if( ( p.z >= min.z ) && ( p.z <= max.z ) && ( p.y >= min.y ) && ( p.y <= max.y ) )
			{  //prusecik je v oblasti BB
				nearI = MIN(nearI,t);
				farI = MAX(farI,t);
			}
		}	
	}

	//pro druhy vrchol BB
	Qx = max.x;
	Qy = max.y;
	Qz = max.z;

	// rovina kolma na osu x
	sfNumerator = Qx - lineBaseT.x;
	sfDenominator = lineDirT.x;
	if( sfDenominator )
	{
		float t = sfNumerator / sfDenominator;
		if( t >= 0 )
		{ 
			// protnuti jen smerem dopredu dle lineDir
			Vec3 p = lineBaseT + lineDirT * t; //prusecik primky se stranou BB
			if ( ( p.z >= min.z ) && ( p.z <= max.z ) && ( p.y >= min.y ) && ( p.y <= max.y ) )
			{  //prusecik je v oblasti BB
				nearI = MIN(nearI,t);
				farI = MAX(farI,t);
			}
		}	
	}

	// rovina kolma na osu Y
	sfNumerator = Qy - lineBaseT.y;
	sfDenominator = lineDirT.y;
	if( sfDenominator ) 
	{
		float t = sfNumerator / sfDenominator;
		if( t >= 0 )
		{ 
			// protnuti jen smerem dopredu dle lineDir
			Vec3 p = lineBaseT + lineDirT * t; //prusecik primky se stranou BB
			if( ( p.z >= min.z ) && ( p.z <= max.z ) && ( p.x >= min.x ) && ( p.x <= max.x ) )
			{  //prusecik je v oblasti BB
				nearI = MIN(nearI,t);
				farI = MAX(farI,t);
			}
		}	
	}

	// rovina kolma na osu Z
	sfNumerator = Qz - lineBaseT.z;
	sfDenominator = lineDirT.z;
	if( sfDenominator )
	{
		float t = sfNumerator / sfDenominator;
		if( t >= 0 )
		{ 
			// protnuti jen smerem dopredu dle lineDir
			Vec3 p = lineBaseT + lineDirT * t; //prusecik primky se stranou BB
			if( ( p.x >= min.x ) && ( p.x <= max.x ) && ( p.y >= min.y ) && ( p.y <= max.y ) )
			{  //prusecik je v oblasti BB
				nearI = MIN(nearI,t);
				farI = MAX(farI,t);
			}
		}	
	}
	if(farI==0) return false; // test range before minmax
	ray->hitDistanceMin = MAX(nearI,ray->hitDistanceMin);
	ray->hitDistanceMax = MIN(farI,ray->hitDistanceMax);
	return ray->hitDistanceMin < ray->hitDistanceMax; // test range after minmax
}

#else

// sse ray-box intersect by Thierry Berger-Perrin

#include <float.h>
#include <math.h>
#include <xmmintrin.h>
#ifdef __GNUC__
	#define _MM_ALIGN16 __attribute__ ((aligned (16)))
#endif

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
//static bool ray_box_intersect(const aabb_t &box, const ray_t &ray, ray_segment_t &rs) 
{
	// you may already have those values hanging around somewhere
	const __m128
		plus_inf	= loadps(ps_cst_plus_inf),
		minus_inf	= loadps(ps_cst_minus_inf);

	// use whatever's apropriate to load.
	const __m128
		box_min	= loadps(&min),
		box_max	= loadps(&max),
		pos	= loadps(&ray->rayOrigin),
		inv_dir	= loadps(&ray->rayDir);

	// use a div if inverted directions aren't available
	//const __m128 l1 = mulps(subps(box_min, pos), inv_dir);
	//const __m128 l2 = mulps(subps(box_max, pos), inv_dir);
	const __m128 l1 = divps(subps(box_min, pos), inv_dir);
	const __m128 l2 = divps(subps(box_max, pos), inv_dir);

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

	// unfold back. try to hide the latency of the shufps & co.
	const __m128 lmax0 = rotatelps(lmax);
	const __m128 lmin0 = rotatelps(lmin);
	lmax = minss(lmax, lmax0);
	lmin = maxss(lmin, lmin0);

	const __m128 lmax1 = muxhps(lmax,lmax);
	const __m128 lmin1 = muxhps(lmin,lmin);
	lmax = minss(lmax, lmax1);
	lmin = maxss(lmin, lmin1);

	const bool ret = ( _mm_comige_ss(lmax, _mm_setzero_ps()) & _mm_comige_ss(lmax,lmin) ) != 0;

	float t_near,t_far;
	storess(lmin, &t_near);
	storess(lmax, &t_far);
	ray->hitDistanceMin = MAX(t_near,ray->hitDistanceMin);
	ray->hitDistanceMax = MIN(t_far,ray->hitDistanceMax);

	return  ret;
}

#endif

} // namespace
