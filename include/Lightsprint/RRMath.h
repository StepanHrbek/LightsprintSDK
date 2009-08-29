#ifndef RRMATH_H
#define RRMATH_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRMath.h
//! \brief LightsprintCore | basic math support
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2000-2009
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

// define RR_API
#ifdef _MSC_VER
	#ifdef RR_STATIC
		// build or use static library
		#define RR_API
	#elif defined(RR_BUILD)
		// build dll
		#define RR_API __declspec(dllexport)
		#pragma warning(disable:4251) // stop MSVC warnings
	#else
		// use dll
		#define RR_API __declspec(dllimport)
		#pragma warning(disable:4251) // stop MSVC warnings
	#endif
#else
	// build or use static library
	#define RR_API
#endif

// autolink library when external project includes this header
#ifdef _MSC_VER
	#if !defined(RR_MANUAL_LINK) && !defined(RR_BUILD)
		#ifdef RR_STATIC
			// use static library
			#if _MSC_VER<1400
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintCore.vs2003_sr.lib")
				#else
					#pragma comment(lib,"LightsprintCore.vs2003_sd.lib")
				#endif
			#elif _MSC_VER<1500
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintCore.vs2005_sr.lib")
				#else
					#pragma comment(lib,"LightsprintCore.vs2005_sd.lib")
				#endif
			#else
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintCore.vs2008_sr.lib")
				#else
					#pragma comment(lib,"LightsprintCore.vs2008_sd.lib")
				#endif
			#endif
		#else
			// use dll
			#if _MSC_VER<1400
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintCore.vs2003.lib")
				#else
					#pragma comment(lib,"LightsprintCore.vs2003_dd.lib")
				#endif
			#elif _MSC_VER<1500
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintCore.vs2005.lib")
				#else
					#pragma comment(lib,"LightsprintCore.vs2005_dd.lib")
				#endif
			#else
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintCore.vs2008.lib")
				#else
					#pragma comment(lib,"LightsprintCore.vs2008_dd.lib")
				#endif
			#endif
		#endif
	#endif
#endif

//#define RR_DEVELOPMENT

#include <cmath>

// fix symbols missing in gcc
#ifdef __GNUC__
	#define _strdup strdup
	#define _stricmp strcasecmp
	#define _snprintf snprintf
	#define _vsnprintf vsnprintf
	#define _finite finite
	#define _isnan isnan
#if !(defined(__MINGW32__) || defined(__MINGW64__))
	#define __cdecl
#endif
#endif

// Common math macros.
#define RR_PI           ((float)3.14159265358979323846)
#define RR_DEG2RAD(deg) ((deg)*(RR_PI/180))
#define RR_RAD2DEG(rad) ((rad)*(180/RR_PI))
#define RR_MIN(a,b)     (((a)<(b))?(a):(b))
#define RR_MAX(a,b)     (((a)>(b))?(a):(b))
#define RR_MIN3(a,b,c)  RR_MIN(a,RR_MIN(b,c))
#define RR_MAX3(a,b,c)  RR_MAX(a,RR_MAX(b,c))

namespace rr /// LightsprintCore - platform independent realtime global illumination solver.
{

	//////////////////////////////////////////////////////////////////////////////
	//
	// primitives
	//
	// RRReal - real number
	// RRVec2 - vector in 2d
	// RRVec3 - vector in 3d
	// RRVec3p - vector in 3d with unused 4th element
	// RRVec4 - vector in 4d
	// RRMatrix3x4 - matrix 3x4
	//
	//////////////////////////////////////////////////////////////////////////////

	//! Real number used in most of calculations.
	typedef float RRReal;

	//! Vector of 2 real numbers plus basic support.
	struct RRVec2
	{
		RRReal    x;
		RRReal    y;
		RRReal& operator [](int i)          const {return ((RRReal*)this)[i];}

		RRVec2()                                  {}
		explicit RRVec2(RRReal a)                 {x=y=a;}
		RRVec2(RRReal ax,RRReal ay)               {x=ax;y=ay;}
		RRVec2 operator + (const RRVec2& a) const {return RRVec2(x+a.x,y+a.y);}
		RRVec2 operator - (const RRVec2& a) const {return RRVec2(x-a.x,y-a.y);}
		RRVec2 operator * (RRReal f)        const {return RRVec2(x*f,y*f);}
		RRVec2 operator * (const RRVec2& a) const {return RRVec2(x*a.x,y*a.y);}
		RRVec2 operator / (RRReal f)        const {return RRVec2(x/f,y/f);}
		RRVec2 operator / (const RRVec2& a) const {return RRVec2(x/a.x,y/a.y);}
		RRVec2 operator +=(const RRVec2& a)       {x+=a.x;y+=a.y;return *this;}
		RRVec2 operator -=(const RRVec2& a)       {x-=a.x;y-=a.y;return *this;}
		RRVec2 operator *=(RRReal f)              {x*=f;y*=f;return *this;}
		RRVec2 operator *=(const RRVec2& a)       {x*=a.x;y*=a.y;return *this;}
		RRVec2 operator /=(RRReal f)              {x/=f;y/=f;return *this;}
		RRVec2 operator /=(const RRVec2& a)       {x/=a.x;y/=a.y;return *this;}
		bool   operator ==(const RRVec2& a) const {return a.x==x && a.y==y;}
		bool   operator !=(const RRVec2& a) const {return a.x!=x || a.y!=y;}
		unsigned components()               const {return 2;}
		RRReal   sum()                      const {return x+y;}
		RRReal   avg()                      const {return (x+y)*0.5f;}
		RRVec2   abs()                      const {return RRVec2(fabs(x),fabs(y));}
		RRVec2   neg()                      const {return RRVec2(-x,-y);}
		RRReal   mini()                     const {return RR_MIN(x,y);}
		RRReal   maxi()                     const {return RR_MAX(x,y);}
		RRReal   length()                   const {return sqrtf(x*x+y*y);}
		RRReal   length2()                  const {return x*x+y*y;}
		void     normalize()                      {*this /= length();}
		void     normalizeSafe()                  {RRReal len=length(); if (len) *this/=len; else {x=1;y=0;}}
		RRVec2   normalized()               const {return *this/length();}
		RRReal   dot(const RRVec2& a)       const {return x*a.x+y*a.y;}
	};

	//! Vector of 3 real numbers plus basic support.
	struct RRVec3 : public RRVec2
	{
		RRReal    z;

		RRVec3()                                    {}
		explicit RRVec3(RRReal a)                   {x=y=z=a;}
		RRVec3(RRReal ax,RRReal ay,RRReal az)       {x=ax;y=ay;z=az;}
		RRVec3 operator + (const RRVec3& a)   const {return RRVec3(x+a.x,y+a.y,z+a.z);}
		RRVec3 operator - (const RRVec3& a)   const {return RRVec3(x-a.x,y-a.y,z-a.z);}
		RRVec3 operator * (RRReal f)          const {return RRVec3(x*f,y*f,z*f);}
		RRVec3 operator * (const RRVec3& a)   const {return RRVec3(x*a.x,y*a.y,z*a.z);}
		RRVec3 operator / (RRReal f)          const {return RRVec3(x/f,y/f,z/f);}
		RRVec3 operator / (const RRVec3& a)   const {return RRVec3(x/a.x,y/a.y,z/a.z);}
		RRVec3 operator / (int f)             const {return RRVec3(x/f,y/f,z/f);}
		RRVec3 operator / (unsigned f)        const {return RRVec3(x/f,y/f,z/f);}
		RRVec3 operator +=(const RRVec3& a)         {x+=a.x;y+=a.y;z+=a.z;return *this;}
		RRVec3 operator -=(const RRVec3& a)         {x-=a.x;y-=a.y;z-=a.z;return *this;}
		RRVec3 operator *=(RRReal f)                {x*=f;y*=f;z*=f;return *this;}
		RRVec3 operator *=(const RRVec3& a)         {x*=a.x;y*=a.y;z*=a.z;return *this;}
		RRVec3 operator /=(RRReal f)                {x/=f;y/=f;z/=f;return *this;}
		RRVec3 operator /=(const RRVec3& a)         {x/=a.x;y/=a.y;z/=a.z;return *this;}
		bool   operator ==(const RRVec3& a)   const {return a.x==x && a.y==y && a.z==z;}
		bool   operator !=(const RRVec3& a)   const {return a.x!=x || a.y!=y || a.z!=z;}
		unsigned components()                 const {return 3;}
		RRReal   sum()                        const {return x+y+z;}
		RRReal   avg()                        const {return (x+y+z)*0.33333333333333f;}
		RRVec3   abs()                        const {return RRVec3(fabs(x),fabs(y),fabs(z));}
		RRVec3   neg()                        const {return RRVec3(-x,-y,-z);}
		RRReal   mini()                       const {return RR_MIN3(x,y,z);} // mini/maxi to avoid conflict with evil min/max macros from windows.h
		RRReal   maxi()                       const {return RR_MAX3(x,y,z);}
		RRReal   length()                     const {return sqrtf(x*x+y*y+z*z);}
		RRReal   length2()                    const {return x*x+y*y+z*z;}
		void     normalize()                        {*this /= length();}
		void     normalizeSafe()                    {RRReal len=length(); if (len) *this/=len; else {x=1;y=0;z=0;}}
		RRVec3   normalized()                 const {return *this/length();}
		RRReal   dot(const RRVec3& a)         const {return x*a.x+y*a.y+z*a.z;}
		RRVec3   cross(const RRVec3& a)       const {return RRVec3(y*a.z-z*a.y,-x*a.z+z*a.x,x*a.y-y*a.x);}
	};

	//! Vector of 3 real numbers plus 4th number as a padding. Operators use only 3 components.
	struct RRVec3p : public RRVec3
	{
		RRReal    w;

		RRVec3p()                                        {}
		explicit RRVec3p(RRReal a)                       {x=y=z=w=a;}
		RRVec3p(const RRVec3& a,RRReal aw)               {x=a.x;y=a.y;z=a.z;w=aw;}
		RRVec3p(RRReal ax,RRReal ay,RRReal az,RRReal aw) {x=ax;y=ay;z=az;w=aw;}
		const RRVec3& operator =(const RRVec3 a)         {x=a.x;y=a.y;z=a.z;return *this;}
	};

	//! Vector of 4 real numbers. Operators use all 4 components.
	struct RRVec4 : public RRVec3p
	{
		RRVec4()                                    {}
		explicit RRVec4(RRReal a)                   {x=y=z=w=a;}
		RRVec4(const RRVec3& a,RRReal aw)           {x=a.x;y=a.y;z=a.z;w=aw;}
		RRVec4(RRReal ax,RRReal ay,RRReal az,RRReal aw) {x=ax;y=ay;z=az;w=aw;}
		const RRVec3& operator =(const RRVec3& a)   {x=a.x;y=a.y;z=a.z;return *this;}

		RRVec4 operator + (const RRVec4& a)   const {return RRVec4(x+a.x,y+a.y,z+a.z,w+a.w);}
		RRVec4 operator - (const RRVec4& a)   const {return RRVec4(x-a.x,y-a.y,z-a.z,w-a.w);}
		RRVec4 operator * (RRReal f)          const {return RRVec4(x*f,y*f,z*f,w*f);}
		RRVec4 operator * (const RRVec4& a)   const {return RRVec4(x*a.x,y*a.y,z*a.z,w*a.w);}
		RRVec4 operator / (RRReal f)          const {return RRVec4(x/f,y/f,z/f,w/f);}
		RRVec4 operator / (const RRVec4& a)   const {return RRVec4(x/a.x,y/a.y,z/a.z,w/a.w);}
		RRVec4 operator / (int f)             const {return RRVec4(x/f,y/f,z/f,w/f);}
		RRVec4 operator / (unsigned f)        const {return RRVec4(x/f,y/f,z/f,w/f);}
		RRVec4 operator +=(const RRVec4& a)         {x+=a.x;y+=a.y;z+=a.z;w+=a.w;return *this;}
		RRVec4 operator -=(const RRVec4& a)         {x-=a.x;y-=a.y;z-=a.z;w-=a.w;return *this;}
		RRVec4 operator *=(RRReal f)                {x*=f;y*=f;z*=f;w*=f;return *this;}
		RRVec4 operator *=(const RRVec4& a)         {x*=a.x;y*=a.y;z*=a.z;w*=a.w;return *this;}
		RRVec4 operator /=(RRReal f)                {x/=f;y/=f;z/=f;w/=f;return *this;}
		RRVec4 operator /=(const RRVec4& a)         {x/=a.x;y/=a.y;z/=a.z;w/=a.w;return *this;}
		bool   operator ==(const RRVec4& a)   const {return a.x==x && a.y==y && a.z==z && a.w==w;}
		bool   operator !=(const RRVec4& a)   const {return a.x!=x || a.y!=y || a.z!=z || a.w!=w;}
		unsigned components()                 const {return 4;}
		RRReal   sum()                        const {return x+y+z+w;}
		RRReal   avg()                        const {return (x+y+z+w)*0.25f;}
		RRVec4   abs()                        const {return RRVec4(fabs(x),fabs(y),fabs(z),fabs(w));}
		RRVec4   neg()                        const {return RRVec4(-x,-y,-z,-w);}
		RRReal   mini()                       const {return RR_MIN(RR_MIN(x,y),RR_MIN(z,w));}
		RRReal   maxi()                       const {return RR_MAX(RR_MAX(x,y),RR_MAX(z,w));}
		RRReal   length()                     const {return sqrtf(x*x+y*y+z*z+w*w);}
		RRReal   length2()                    const {return x*x+y*y+z*z+w*w;}
		void     normalize()                        {*this /= length();}
		void     normalizeSafe()                    {RRReal len=length(); if (len) *this/=len; else {x=1;y=0;z=0;w=0;}}
		RRVec4   normalized()                 const {return *this/length();}
		RRReal   dot(const RRVec4& a)         const {return x*a.x+y*a.y+z*a.z+w*a.w;}
		RRReal   planePointDistance(const RRVec3& a)const {return x*a.x+y*a.y+z*a.z+w;}
	};

	//! Matrix of 3x4 real numbers in row-major order.
	//
	//! Translation is stored in m[x][3].
	//! Rotation and scale in the rest.
	//! \n We have chosen this format because it contains only what we need, is smaller than 4x4
	//! and its shape makes no room for row or column major ambiguity.
	struct RR_API RRMatrix3x4
	{
		RRReal m[3][4];

		//! Returns position in 3d space transformed by matrix.
		RRVec3 transformedPosition(const RRVec3& a) const;
		//! Transforms position in 3d space by matrix.
		RRVec3& transformPosition(RRVec3& a) const;
		//! Returns direction in 3d space transformed by matrix.
		RRVec3 transformedDirection(const RRVec3& a) const;
		//! Transforms direction in 3d space by matrix.
		RRVec3& transformDirection(RRVec3& a) const;
		//! Returns determinant of first 3x3 elements.
		RRReal determinant3x3() const;
		//! Sets matrix to identity.
		void setIdentity();
	};

} // namespace

#endif
