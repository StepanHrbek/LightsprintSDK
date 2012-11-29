#ifndef RRMATH_H
#define RRMATH_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRMath.h
//! \brief LightsprintCore | basic math support
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2000-2012
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
			#elif _MSC_VER<1600
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintCore.vs2008_sr.lib")
				#else
					#pragma comment(lib,"LightsprintCore.vs2008_sd.lib")
				#endif
			#elif _MSC_VER<1700
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintCore.vs2010_sr.lib")
				#else
					#pragma comment(lib,"LightsprintCore.vs2010_sd.lib")
				#endif
			#else
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintCore.vs2012_sr.lib")
				#else
					#pragma comment(lib,"LightsprintCore.vs2012_sd.lib")
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
			#elif _MSC_VER<1600
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintCore.vs2008.lib")
				#else
					#pragma comment(lib,"LightsprintCore.vs2008_dd.lib")
				#endif
			#elif _MSC_VER<1700
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintCore.vs2010.lib")
				#else
					#pragma comment(lib,"LightsprintCore.vs2010_dd.lib")
				#endif
			#else
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintCore.vs2012.lib")
				#else
					#pragma comment(lib,"LightsprintCore.vs2012_dd.lib")
				#endif
			#endif
		#endif
	#endif
#endif

//#define RR_DEVELOPMENT

#include <cfloat>
#include <cmath>

#ifdef __GNUC__
	// fix symbols missing in gcc
	#define _strdup strdup
	#define _stricmp strcasecmp
	#define _snprintf snprintf
	#define _vsnprintf vsnprintf
	#define _vsnwprintf vswprintf
	#define _finite finite
	#define _isnan isnan
#if !(defined(__MINGW32__) || defined(__MINGW64__))
	#define __cdecl
#endif
#endif

// Common math macros.
#define RR_PI                 ((float)3.14159265358979323846)
#define RR_DEG2RAD(deg)       ((deg)*(RR_PI/180))
#define RR_RAD2DEG(rad)       ((rad)*(180/RR_PI))
#define RR_MIN(a,b)           (((a)<(b))?(a):(b))
#define RR_MAX(a,b)           (((a)>(b))?(a):(b))
#define RR_MIN3(a,b,c)        RR_MIN(a,RR_MIN(b,c))
#define RR_MAX3(a,b,c)        RR_MAX(a,RR_MAX(b,c))
#define RR_CLAMPED(a,min,max) (((a)<(min))?min:(((a)>(max)?(max):(a))))
#define RR_CLAMP(a,min,max)   (a)=RR_CLAMPED(a,min,max)
#define RR_FLOAT2BYTE(f)      RR_CLAMPED(int((f)*256),0,255)
#define RR_BYTE2FLOAT(b)      ((b)*0.003921568627450980392156862745098f)

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
		RRReal& operator [](int i)            const {return ((RRReal*)this)[i];}

		RRVec2()                                    {}
		explicit RRVec2(RRReal a)                   {x=y=a;}
		RRVec2(RRReal ax,RRReal ay)                 {x=ax;y=ay;}
		RRVec2 operator + (const RRVec2& a)   const {return RRVec2(x+a.x,y+a.y);}
		RRVec2 operator - (const RRVec2& a)   const {return RRVec2(x-a.x,y-a.y);}
		RRVec2 operator - ()                  const {return RRVec2(-x,-y);}
		RRVec2 operator * (RRReal f)          const {return RRVec2(x*f,y*f);}
		RRVec2 operator * (const RRVec2& a)   const {return RRVec2(x*a.x,y*a.y);}
		RRVec2 operator / (RRReal f)          const {return RRVec2(x/f,y/f);}
		RRVec2 operator / (const RRVec2& a)   const {return RRVec2(x/a.x,y/a.y);}
		RRVec2 operator +=(const RRVec2& a)         {x+=a.x;y+=a.y;return *this;}
		RRVec2 operator -=(const RRVec2& a)         {x-=a.x;y-=a.y;return *this;}
		RRVec2 operator *=(RRReal f)                {x*=f;y*=f;return *this;}
		RRVec2 operator *=(const RRVec2& a)         {x*=a.x;y*=a.y;return *this;}
		RRVec2 operator /=(RRReal f)                {x/=f;y/=f;return *this;}
		RRVec2 operator /=(const RRVec2& a)         {x/=a.x;y/=a.y;return *this;}
		bool   operator ==(const RRVec2& a)   const {return a.x==x && a.y==y;}
		bool   operator !=(const RRVec2& a)   const {return a.x!=x || a.y!=y;}
		unsigned components()                 const {return 2;}
		RRReal   sum()                        const {return x+y;}
		RRReal   avg()                        const {return (x+y)*0.5f;}
		RRVec2   abs()                        const {return RRVec2(fabs(x),fabs(y));}
		RRVec2   neg()                        const {return RRVec2(-x,-y);}
		RRReal   mini()                       const {return RR_MIN(x,y);}
		RRReal   maxi()                       const {return RR_MAX(x,y);}
		RRReal   length()                     const {return sqrtf(x*x+y*y);}
		RRReal   length2()                    const {return x*x+y*y;}
		void     normalize()                        {*this /= length();}
		void     normalizeSafe()                    {RRReal len=length(); if (len) *this/=len; else {x=1;y=0;}}
		RRVec2   normalized()                 const {return *this/length();}
		RRVec2   normalizedSafe()             const {RRReal len=length(); return len?*this/len:RRVec2(1,0);}
		bool     finite()                     const {return ::_finite(x) && ::_finite(y);}
		RRReal   dot(const RRVec2& a)         const {return x*a.x+y*a.y;}
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
		RRVec3 operator - ()                  const {return RRVec3(-x,-y,-z);}
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
		RRVec3   normalizedSafe()             const {RRReal len=length(); return len?*this/len:RRVec3(1,0,0);}
		bool     finite()                     const {return ::_finite(x) && ::_finite(y) && ::_finite(z);}
		RRReal   dot(const RRVec3& a)         const {return x*a.x+y*a.y+z*a.z;}
		RRVec3   cross(const RRVec3& a)       const {return RRVec3(y*a.z-z*a.y,-x*a.z+z*a.x,x*a.y-y*a.x);}
		RR_API RRVec3 getHsvFromRgb()         const;
		RR_API RRVec3 getRgbFromHsv()         const;
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
		RRVec4 operator - ()                  const {return RRVec4(-x,-y,-z,-w);}
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
		RRVec4   normalizedSafe()             const {RRReal len=length(); return len?*this/len:RRVec4(1,0,0,0);}
		bool     finite()                     const {return ::_finite(x) && ::_finite(y) && ::_finite(z) && ::_finite(w);}
		RRReal   dot(const RRVec4& a)         const {return x*a.x+y*a.y+z*a.z+w*a.w;}

		// planes
		static RRVec4 plane(const RRVec3& normal,const RRVec3& point) {return RRVec4(normal,-normal.dot(point));}
		RRReal   planePointDistance(const RRVec3& a)const {return (x*a.x+y*a.y+z*a.z+w)/sqrt(x*x+y*y+z*z);}
		RRVec3   pointInPlane()               const {return RRVec3(x,y,z)*(-w/(x*x+y*y+z*z));} // returns the most close point to 0
	};

	//! Matrix of 3x4 real numbers in row-major order.
	//
	//! Translation is stored in m[x][3].
	//! Rotation and scale in the rest.
	//!
	//! We have chosen this format because it contains only what we need, is smaller than 4x4
	//! and its shape makes no room for row or column major ambiguity.
	//!
	//! Decomposition functions assume that matrix was composed from (in this order): scale, rotation and translation;
	//! so when composing matrices for later decomposition, use either <code>m = translation() * rotationByYawPitchRoll() * scale();</code>
	//! or slightly faster but otherwise identical <code>m = rotationByYawPitchRoll(); m.preScale(); m.postTranslate();</code>
	struct RR_API RRMatrix3x4
	{
		//! Direct access to matrix data.
		RRReal m[3][4];

		// Constructors
		RRMatrix3x4(); // uninitialized
		RRMatrix3x4(float* m3x4, bool transposed);
		RRMatrix3x4(double* m3x4, bool transposed);
		RRMatrix3x4(float m00, float m01, float m02, float m03, float m10, float m11, float m12, float m13, float m20, float m21, float m22, float m23);
		RRMatrix3x4(double m00, double m01, double m02, double m03, double m10, double m11, double m12, double m13, double m20, double m21, double m22, double m23);
		static RRMatrix3x4 identity();
		static RRMatrix3x4 scale(const RRVec3& scale);
		static RRMatrix3x4 translation(const RRVec3& translation);
		static RRMatrix3x4 rotationByYawPitchRoll(const RRVec3& yawPitchRollRad); ///< Yaw + Pitch + Roll = YXZ Euler angles as defined at http://en.wikipedia.org/wiki/Euler_angles
		static RRMatrix3x4 rotationByAxisAngle(const RRVec3& rotationAxis, RRReal rotationAngleRad);
		static RRMatrix3x4 rotationByQuaternion(const RRVec4& quaternion);

		static RRMatrix3x4 mirror(const RRVec4& plane);

		//! Returns position in 3d space transformed by matrix.
		RRVec3 getTransformedPosition(const RRVec3& a) const;
		//! Transforms position in 3d space by matrix.
		void transformPosition(RRVec3& a) const;
		//! Returns direction in 3d space transformed by matrix.
		RRVec3 getTransformedDirection(const RRVec3& a) const;
		//! Transforms direction in 3d space by matrix.
		void transformDirection(RRVec3& a) const;
		//! Returns plane in 3d space transformed by matrix.
		RRVec4 getTransformedPlane(const RRVec4& a) const;
		//! Transforms plane in 3d space by matrix.
		void transformPlane(RRVec4& a) const;

		bool operator ==(const RRMatrix3x4& a) const;
		//! A*B returns matrix that performs transformation B, then A.
		RRMatrix3x4 operator *(const RRMatrix3x4& a) const;
		//! A*=B adds transformation B at the beginning of transformations defined by A. If you want B at the end, do A=B*A;
		RRMatrix3x4& operator *=(const RRMatrix3x4& a);

		//! Tests whether matrix is identity.
		bool isIdentity() const;

		//! Returns translation component of matrix.
		RRVec3 getTranslation() const;
		//! Sets translation component of matrix.
		void setTranslation(const RRVec3& a);
		//! Applies translation on top of other transformations defined by this matrix, optimized *this=translation(a)**this;
		void postTranslate(const RRVec3& a);
		//! Returns the same transformation with pretranslation -center and posttranslation +center.
		RRMatrix3x4 centeredAround(const RRVec3& center) const;

		//! Returns scale component of matrix. Negative scale is supported. Use getScale().abs().avg() for absolute uniform scale.
		RRVec3 getScale() const;
		//! Applies scale before other transformations defined by this matrix; optimized *this*=scale(a);
		void preScale(const RRVec3& a);

		//! Returns rotation component of matrix as Yaw, Pitch and Roll = YXZ Euler angles as defined at http://en.wikipedia.org/wiki/Euler_angles
		//
		//! Works for matrices that represent scale, rotation and translation (in this order) of rigid body,
		//! result is undefined for non-orthogonal matrices.
		//!
		//! Yaw and roll are in -pi..pi range, pitch is in -pi/2..pi/2 range.
		RRVec3 getYawPitchRoll() const;

		//! Returns rotation component of matrix as rotation axis (xyz) and rotation angle in radians (w) .
		RRVec4 getAxisAngle() const;

		//! Returns rotation component of matrix as quaternion.
		RRVec4 getQuaternion() const;

		//! Returns i-th matrix column, for i=0,1,2,3.
		RRVec3 getColumn(unsigned i) const;
		//! Returns i-th matrix row, for i=0,1,2.
		RRVec4 getRow(unsigned i) const;
		//! Sets i-th matrix column, for i=0,1,2,3.
		void setColumn(unsigned i, const RRVec3& column);
		//! Sets i-th matrix row, for i=0,1,2.
		void setRow(unsigned i, const RRVec4& row);

		//! Returns determinant of first 3x3 elements.
		RRReal determinant3x3() const;
		//! Inverts matrix. Returns false in case of failure, not changing destination.
		bool invertedTo(RRMatrix3x4& destination) const;

		//! Fills this with linear interpolation of two samples; sample0 if blend=0, sample1 if blend=1.
		void blendLinear(const RRMatrix3x4& sample0, const RRMatrix3x4& sample1, RRReal blend);
		//! Fills this with Akima interpolation of samples, non uniformly scattered on time scale.
		//
		//! \param numSamples
		//!  Number of elements in samples and times arrays.
		//!  Interpolations uses at most 3 preceding and 3 following samples, providing more does not increase quality.
		//! \param samples
		//!  Pointers to numSamples samples.
		//! \param times
		//!  Array of times assigned to samples.
		//!  Sequence must be monotonic increasing, result is undefined if it is not.
		//! \param time
		//!  Time assigned to sample generated by this function.
		void blendAkima(unsigned numSamples, const RRMatrix3x4** samples, const RRReal* times, RRReal time);
	};

} // namespace

#endif
