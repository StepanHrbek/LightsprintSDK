#ifndef RRMATH_H
#define RRMATH_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRMath.h
//! \brief RRMath - basic math used by Lightsprint libraries
//! \version 2006.8
//! \author Copyright (C) Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#	ifdef RR_STATIC
		// use static library
		#define RR_API
#	else // use dll
#define RR_API __declspec(dllimport)
#pragma warning(disable:4251) // stop false MS warnings
#	endif
#else
	// use static library
	#define RR_API
#endif
//#define RR_DEVELOPMENT

#include <cmath>

namespace rr /// Encapsulates whole Lightsprint SDK.
{

	//////////////////////////////////////////////////////////////////////////////
	//
	// primitives
	//
	// RRReal - real number
	// RRVec2 - vector in 2d
	// RRVec3 - vector in 3d
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
		RRReal   size2()                    const {return x*x+y*y;}
		RRReal   size()                     const {return sqrtf(x*x+y*y);}
		RRVec2   normalized()               const {return *this/size();}
		void     normalize()                      {*this /= size();}
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
		RRReal   size2()                      const {return x*x+y*y+z*z;}
		RRReal   size()                       const {return sqrtf(x*x+y*y+z*z);}
		RRVec3   normalized()                 const {return *this/size();}
		void     normalize()                        {*this /= size();}
	};

	//! Vector of 4 real numbers plus basic support.
	struct RRVec4 : public RRVec3
	{
		RRReal    w;

		RRVec4()                                    {}
		explicit RRVec4(RRReal a)                   {x=y=z=w=a;}
		RRVec4(RRReal ax,RRReal ay,RRReal az,RRReal aw) {x=ax;y=ay;z=az;w=aw;}
		void   operator =(const RRVec3 a)           {x=a.x;y=a.y;z=a.z;}
		/*
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
		RRReal   size2()                      const {return x*x+y*y+z*z+w*w;}
		RRReal   size()                       const {return sqrtf(x*x+y*y+z*z+w*w);}
		RRVec4   normalized()                 const {return *this/size();}
		void     normalize()                        {*this /= size();}
		*/
	};

} // namespace

#endif
