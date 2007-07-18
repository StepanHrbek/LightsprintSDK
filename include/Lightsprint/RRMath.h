#ifndef RRMATH_H
#define RRMATH_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRMath.h
//! \brief LightsprintCore | basic math support
//! \author Copyright (C) Stepan Hrbek, Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#	ifdef RR_STATIC
		// use static library
		#define RR_API
#	else // use dll
#define RR_API __declspec(dllimport)
#pragma warning(disable:4251) // stop MSVC warnings
#	endif
#else
	// use static library
	#define RR_API
#endif

#ifndef RR_MANUAL_LINK
#ifdef _MSC_VER
#	ifdef RR_STATIC
		// use static library
#		ifdef NDEBUG
			#pragma comment(lib,"LightsprintCore_sr.lib")
#		else
			#pragma comment(lib,"LightsprintCore_sd.lib")
#		endif
#	else
#	ifdef RR_DLL_BUILD
		// build dll
#		undef RR_API
#		define RR_API __declspec(dllexport)
#	else // use dll
	#if _MSC_VER<1400
#	ifdef NDEBUG
		#ifdef RR_DEBUG
			#pragma comment(lib,"LightsprintCore.vs2003_dd.lib")
		#else
			#pragma comment(lib,"LightsprintCore.vs2003.lib")
		#endif
#	else
		#pragma comment(lib,"LightsprintCore.vs2003_dd.lib")
#	endif
	#else
#	ifdef NDEBUG
		#ifdef RR_DEBUG
			#pragma comment(lib,"LightsprintCore_dd.lib")
		#else
			#pragma comment(lib,"LightsprintCore.lib")
		#endif
#	else
		#pragma comment(lib,"LightsprintCore_dd.lib")
#	endif
	#endif
#	endif
#	endif
#endif
#endif

//#define RR_DEVELOPMENT

#include <cmath>

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
		RRReal   length()                   const {return sqrtf(x*x+y*y);}
		RRReal   length2()                  const {return x*x+y*y;}
		void     normalize()                      {*this /= length();}
		RRVec2   normalized()               const {return *this/length();}
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
		RRReal   length()                     const {return sqrtf(x*x+y*y+z*z);}
		RRReal   length2()                    const {return x*x+y*y+z*z;}
		void     normalize()                        {*this /= length();}
		RRVec3   normalized()                 const {return *this/length();}
	};

	//! Vector of 3 real numbers plus 4th number as a padding. Operators use only 3 components.
	struct RRVec3p : public RRVec3
	{
		RRReal    w;

		RRVec3p()                                        {}
		explicit RRVec3p(RRReal a)                       {x=y=z=w=a;}
		RRVec3p(const RRVec3& a,RRReal aw)               {x=a.x;y=a.y;z=a.z;w=aw;}
		RRVec3p(RRReal ax,RRReal ay,RRReal az,RRReal aw) {x=ax;y=ay;z=az;w=aw;}
		void   operator =(const RRVec3 a)                {x=a.x;y=a.y;z=a.z;}
	};

	//! Vector of 4 real numbers. Operators use all 4 components.
	struct RRVec4 : public RRVec3p
	{
		RRVec4()                                    {}
		explicit RRVec4(RRReal a)                   {x=y=z=w=a;}
		RRVec4(const RRVec3& a,RRReal aw)           {x=a.x;y=a.y;z=a.z;w=aw;}
		RRVec4(RRReal ax,RRReal ay,RRReal az,RRReal aw) {x=ax;y=ay;z=az;w=aw;}
		void   operator =(const RRVec3 a)           {x=a.x;y=a.y;z=a.z;}

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
		RRReal   length()                     const {return sqrtf(x*x+y*y+z*z+w*w);}
		RRReal   length2()                    const {return x*x+y*y+z*z+w*w;}
		void     normalize()                        {*this /= length();}
		RRVec4   normalized()                 const {return *this/length();}
	};

} // namespace

#endif
