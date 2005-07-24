#include <assert.h>
#include <math.h>
#include <memory.h>
#include "geometry.h"

namespace rrIntersect
{

//////////////////////////////////////////////////////////////////////////////
//
// 3d vector

Vec3 operator -(Vec3 a)
{
	return Vec3(-a.x,-a.y,-a.z);
}

real size(Vec3 a)
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

real dot(Vec3 a,Vec3 b)
{
	return a.x*b.x+a.y*b.y+a.z*b.z;
}

// dohoda:
// predpokladejme triangl s vrcholy 0,a,b.
// pri pohledu na jeho vnejsi stranu (tu s normalou) vidime vrcholy
// serazene proti smeru hodinovych rucicek.
// jinak receno: ortogonalTo(doprava,dopredu)==nahoru

Vec3 ortogonalTo(Vec3 a,Vec3 b)
{
	return Vec3(a.y*b.z-a.z*b.y,-a.x*b.z+a.z*b.x,a.x*b.y-a.y*b.x);
}

//////////////////////////////////////////////////////////////////////////////
//
// plane in 3d

real normalValueIn(Plane n,Vec3 a)
{
	return a.x*n.x+a.y*n.y+a.z*n.z+n.d;
}

//////////////////////////////////////////////////////////////////////////////
//
// sphere in 3d

void Sphere::detect(const Vec3 *vertex,unsigned vertices)
{
	Vec3 sum = Vec3(0,0,0);
	for(unsigned i=0;i<vertices;i++) sum += vertex[i];
	center = sum/vertices;
	radius2 = 0;
	for(unsigned i=0;i<vertices;i++) 
	{
		real tmp = size2(vertex[i]-center);
		if(tmp>radius2) radius2=tmp;
	}
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
	min -= Vec3(0.01f,0.01f,0.01f);//!!! nekdo ma problem kdyz je to uplne tesne (napr linear v cub)
	max += Vec3(0.01f,0.01f,0.01f);
}

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

} // namespace
