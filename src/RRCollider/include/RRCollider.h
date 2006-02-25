#ifndef COLLIDER_RRCOLLIDER_H
#define COLLIDER_RRCOLLIDER_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRCollider.h
//! \brief RRCollider - library for fast "ray x mesh" intersections
//! \version 2006.2.26
//! \author Copyright (C) Stepan Hrbek 1999-2006, Daniel Sykora 1999-2004
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
	#ifdef RRCOLLIDER_STATIC
		// use static library
		#define RRCOLLIDER_API
		#ifdef NDEBUG
			#pragma comment(lib,"RRCollider_s.lib")
		#else
			#pragma comment(lib,"RRCollider_sd.lib")
		#endif
	#else
	#ifdef RRCOLLIDER_DLL_BUILD
		// build dll
		#define RRCOLLIDER_API __declspec(dllexport)
	#else
		// use dll
		#define RRCOLLIDER_API __declspec(dllimport)
		#pragma comment(lib,"RRCollider.lib")
	#endif
	#endif
#else
	// use static library
	#define RRCOLLIDER_API
#endif

#include <math.h>   // sqrt
#include <new>      // operators new/delete
#include <limits.h> // UNDEFINED

namespace rrCollider /// Encapsulates whole #RRCollider library.
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

	//! Vector of 2 real numbers.
	struct RRCOLLIDER_API RRVec2
	{
		RRReal    x;
		RRReal    y;
		RRReal& operator [](int i)            const {return ((RRReal*)this)[i];}
	};

	//! Vector of 3 real numbers.
	struct RRCOLLIDER_API RRVec3
	{
		RRReal    x;
		RRReal    y;
		RRReal    z;

		RRVec3()                                    {}
		explicit RRVec3(RRReal a)                   {x=a;y=a;z=a;}
		RRVec3(RRReal ax,RRReal ay,RRReal az)       {x=ax;y=ay;z=az;}
		RRVec3 operator + (const RRVec3& a)   const {return RRVec3(x+a.x,y+a.y,z+a.z);}
		RRVec3 operator - (const RRVec3& a)   const {return RRVec3(x-a.x,y-a.y,z-a.z);}
		RRVec3 operator * (RRReal f)          const {return RRVec3(x*f,y*f,z*f);}
		RRVec3 operator * (RRVec3 a)          const {return RRVec3(x*a.x,y*a.y,z*a.z);}
		RRVec3 operator / (RRReal f)          const {return RRVec3(x/f,y/f,z/f);}
		RRVec3 operator / (RRVec3 a)          const {return RRVec3(x/a.x,y/a.y,z/a.z);}
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
		RRReal& operator [](int i)            const {return ((RRReal*)this)[i];}
	};

	RRVec3 RRCOLLIDER_API operator -(const RRVec3& a);
	RRReal RRCOLLIDER_API size(const RRVec3& a);
	RRReal RRCOLLIDER_API size2(const RRVec3& a);
	RRVec3 RRCOLLIDER_API normalized(const RRVec3& a);
	RRReal RRCOLLIDER_API dot(const RRVec3& a,const RRVec3& b);
	RRVec3 RRCOLLIDER_API ortogonalTo(const RRVec3& a);
	RRVec3 RRCOLLIDER_API ortogonalTo(const RRVec3& a,const RRVec3& b);

	//! Vector of 4 real numbers.
	struct RRCOLLIDER_API RRVec4 : public RRVec3
	{
		RRReal    w;

		void operator =(const RRVec3 a)   {x=a.x;y=a.y;z=a.z;}
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRMeshImporter
	//! Common interface for any standard or proprietary triangle mesh structure.
	//
	//! \section s4 Creating instances
	//!
	//! #RRMeshImporter has built-in support for standard mesh formats used by
	//! rendering APIs - vertex and index buffers using triangle lists or
	//! triangle strips. See create() and createIndexed().
	//!
	//! #RRMeshImporter has built-in support for baking multiple meshes 
	//! into one mesh (without need for additional memory). 
	//! This may simplify mesh oprations or improve performance in some situations.
	//! See createMultiMesh().
	//!
	//! #RRMeshImporter has built-in support for creating self-contained mesh copies.
	//! See createCopy().
	//! While importers created from vertex buffer doesn't allocate more memory 
	//! and depend on vertex buffer, self-contained copy contains all mesh data
	//! and doesn't depend on any other objects.
	//!
	//! For proprietary mesh formats (heightfield, realtime generated etc), 
	//! you may easily derive from #RRMeshImporter and create your own importer.
	//! 
	//! \section s6 Optimizations
	//!
	//! #RRMeshImporter may help you with mesh optimizations if requested,
	//! for example by removing duplicate vertices or degenerated triangles.
	//! 
	//! \section s6 Constancy
	//!
	//! All data provided by #RRMeshImporter must be constant in time.
	//! Built-in importers guarantee constancy if you don't change
	//! their vertex/index buffers. Constancy of mesh copy is guaranteed always.
	//!
	//! \section s5 Indexing
	//!
	//! #RRMeshImporter operates with two types of vertex and triangle indices.
	//! -# PostImport indices, always 0..num-1 (where num=getNumTriangles
	//! or getNumVertices), these are used in most calls.
	//! \n Example: with 100-triangle mesh, triangle indices are 0..99.
	//! -# PreImport indices, optional, arbitrary numbers provided by 
	//! importer for your convenience.
	//! \n Example: could be offsets into your vertex buffer.
	//! \n Pre<->Post mapping is defined by importer and is arbitrary, but constant.
	//!
	//! All Pre-Post conversion functions must accept all unsigned values.
	//! When query makes no sense, they return UNDEFINED.
	//! This is because 
	//! -# valid PreImport numbers may be spread across whole unsigned 
	//! range and caller could be unaware of their validity.
	//! -# such queries are rare and not performance critical.
	//!
	//! All other function use PostImport numbers and may support only 
	//! their 0..num-1 range.
	//! When called with out of range value, result is undefined.
	//! Debug version may return arbitrary number or throw assert.
	//! Release version may return arbitrary number or crash.
	//! This is because 
	//! -# valid PostImport numbers are easy to ensure on caller side.
	//! -# such queries are very critical for performance.
	//////////////////////////////////////////////////////////////////////////////

	class RRCOLLIDER_API RRMeshImporter
	{
	public:
		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		virtual ~RRMeshImporter() {}

		// vertices
		typedef RRVec3 Vertex;
		virtual unsigned     getNumVertices() const = 0;
		virtual void         getVertex(unsigned v, Vertex& out) const = 0;

		// triangles
		struct Triangle      {unsigned m[3]; unsigned&operator[](int i){return m[i];} const unsigned&operator[](int i)const{return m[i];}};
		virtual unsigned     getNumTriangles() const = 0;
		virtual void         getTriangle(unsigned t, Triangle& out) const = 0;

		// optional for faster access
		struct TriangleBody  {Vertex vertex0,side1,side2;};
		typedef RRVec4 Plane;
		virtual void         getTriangleBody(unsigned t, TriangleBody& out) const;
		virtual bool         getTrianglePlane(unsigned t, Plane& out) const;
		virtual RRReal       getTriangleArea(unsigned t) const;

		// optional for advanced importers
		//  post import number is always plain unsigned, 0..num-1
		//  pre import number is implementation defined
		//  all numbers in interface are post import, except for following preImportXxx:
		virtual unsigned     getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const {return postImportVertex;}
		virtual unsigned     getPostImportVertex(unsigned preImportVertex, unsigned preImportTriangle) const {return preImportVertex;}
		virtual unsigned     getPreImportTriangle(unsigned postImportTriangle) const {return postImportTriangle;}
		virtual unsigned     getPostImportTriangle(unsigned preImportTriangle) const {return preImportTriangle;}
		enum {UNDEFINED = UINT_MAX};


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		// instance factory
		enum Format
		{
			UINT8   = 0, ///< uint8_t
			UINT16  = 1, ///< uint16_t
			UINT32  = 2, ///< uint32_t
			FLOAT16 = 3, ///< half
			FLOAT32 = 4, ///< float
			FLOAT64 = 5, ///< double
		};
		enum Flags
		{
			TRI_LIST            = 0,      ///< interpret data as triangle list
			TRI_STRIP           = (1<<0), ///< interpret data as triangle strip
			OPTIMIZED_VERTICES  = (1<<1), ///< remove identical and unused vertices
			OPTIMIZED_TRIANGLES = (1<<2), ///< remove degenerated triangles
		};
		static RRMeshImporter* create(unsigned flags, Format vertexFormat, void* vertexBuffer, unsigned vertexCount, unsigned vertexStride);
		static RRMeshImporter* createIndexed(unsigned flags, Format vertexFormat, void* vertexBuffer, unsigned vertexCount, unsigned vertexStride, Format indexFormat, void* indexBuffer, unsigned indexCount, float vertexStitchMaxDistance = 0);
		RRMeshImporter*        createCopy();
		static RRMeshImporter* createMultiMesh(RRMeshImporter* const* meshes, unsigned numMeshes);
		RRMeshImporter*        createOptimizedVertices(float vertexStitchMaxDistance = 0);
		bool                   save(char* filename);
		static RRMeshImporter* load(char* filename);

		// verification
		typedef void Reporter(const char* msg, void* context);
		unsigned               verify(Reporter* reporter, void* context); // returns number of reports
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRMeshSurfaceImporter
	//! Interface for non-trivial behaviour of mesh surfaces.
	//
	//! Derive to define behaviour of YOUR surfaces.
	//! You need it only when you want to
	//! - intersect mesh that contains both singlesided and twosided faces
	//! - iterate over multiple intersections with mesh
	//!
	//! It's intentionally not part of RRMeshImporter, so you can easily combine
	//! different surface behaviours with one geometry.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRCOLLIDER_API RRMeshSurfaceImporter
	{
	public:
		virtual ~RRMeshSurfaceImporter() {}

		//! acceptHit is called at each intersection with mesh
		//! return false to continue to next intersection, true to end
		//! for IT_BSP techniques, intersections are reported in order from the nearest one
		//! for IT_LINEAR technique, intersections go unsorted
		virtual bool         acceptHit(const class RRRay* ray) = 0;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRAligned
	//! Object aligned in memory as required by SIMD instructions.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRCOLLIDER_API RRAligned
	{
	public:
		void* operator new(std::size_t n);
		void* operator new[](std::size_t n);
		void operator delete(void* p, std::size_t n);
		void operator delete[](void* p, std::size_t n);
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRRay
	//! Ray to intersect with object.
	//
	//! Contains all inputs and outputs for RRCollider::intersect().
	//! All fields of at least 3 floats are aligned for SIMD.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRCOLLIDER_API RRRay : public RRAligned
	{
	public:
		// create ray
		static RRRay* create(); ///< Creates 1 RRRay. All is zeroed, all FILL flags on.
		static RRRay* create(unsigned n); ///< Creates array of RRRays.
		// inputs (never modified by collider)
		enum Flags ///< Flags define which outputs to fill. \detailed Some outputs may be filled even when not requested by flag.
		{ 
			FILL_DISTANCE   =(1<<0), 
			FILL_POINT3D    =(1<<1),
			FILL_POINT2D    =(1<<2),
			FILL_PLANE      =(1<<3),
			FILL_TRIANGLE   =(1<<4),
			FILL_SIDE       =(1<<5),
			TEST_SINGLESIDED=(1<<6), ///< detect collision only against front side. default is to test both sides
		};
		RRVec4          rayOrigin;      ///< in, (-Inf,Inf), ray origin. never modify last component, must stay 1
		RRVec4          rayDirInv;      ///< in, <-Inf,Inf>, 1/ray direction. direction must be normalized
		RRReal          rayLengthMin;   ///< in, <0,Inf), test intersection in range <min,max>
		RRReal          rayLengthMax;   ///< in, <0,Inf), test intersection in range <min,max>
		unsigned        rayFlags;       ///< in, flags that specify the action
		RRMeshSurfaceImporter* surfaceImporter; ///< i, optional surface importer for user-defined surface behaviours
		// outputs (valid after positive test, undefined otherwise)
		RRReal          hitDistance;    ///< out, hit distance in object space
		unsigned        hitTriangle;    ///< out, triangle (postImport) that was hit
		RRVec2          hitPoint2d;     ///< out, hit coordinate in triangle space (vertex[0]=0,0 vertex[1]=1,0 vertex[2]=0,1)
		RRVec4          hitPlane;       ///< out, plane of hitTriangle in object space, [0..2] is normal
		RRVec3          hitPoint3d;     ///< out, hit coordinate in object space
		bool            hitFrontSide;   ///< out, true = face was hit from the front side
		RRVec4          hitPadding[2];  ///< out, undefined, never modify
	private:
		RRRay(); // intentionally private so one can't accidentally create unaligned instance
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRCollider
	//! Single object able to calculate ray x trimesh intersections.
	//
	//! Mesh is defined by importer passed to create().
	//! All results from importer must be constant in time, 
	//! otherwise collision results are undefined.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRCOLLIDER_API RRCollider
	{
	public:
		// create
		enum IntersectTechnique
		{
			IT_LINEAR,          ///< speed   1%, size  0
			IT_BSP_COMPACT,     ///< speed 100%, size  5 bytes per triangle
			IT_BSP_FAST,        ///< speed 175%, size 31
			IT_BSP_FASTEST,     ///< speed 200%, size 58
			IT_VERIFICATION,    ///< test using all techniques and compare
		};
		static RRCollider*   create(RRMeshImporter* importer, IntersectTechnique intersectTechnique, const char* cacheLocation=NULL, void* buildParams=0);

		// calculate intersections
		// (When intersection is detected, ray outputs are filled and true returned.
		// When no intersection is detected, ray outputs are undefined and false returned.)
		virtual bool         intersect(RRRay* ray) const = 0;

		// helpers
		virtual RRMeshImporter*    getImporter() const = 0; ///< identical to importer from create()
		virtual IntersectTechnique getTechnique() const = 0; ///< my differ from technique requested in create()
		virtual unsigned           getMemoryOccupied() const = 0; ///< total amount of system memory occupied by collider
		virtual ~RRCollider() {};
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRIntersectStats
	//! Statistics for library calls. Retrieve by getInstance().
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRCOLLIDER_API RRIntersectStats
	{
	public:
		// data
		unsigned numTrianglesLoaded;
		unsigned numTrianglesInvalid;
		// calls
		unsigned intersect_mesh;
		unsigned hit_mesh;
		// branches, once per call
		unsigned intersect_bspSRLNP;
		unsigned intersect_bspNP;
		unsigned intersect_bsp;
		unsigned intersect_kd;
		unsigned intersect_linear;
		// branches, many times per call
		unsigned intersect_triangleSRLNP;
		unsigned intersect_triangleNP;
		unsigned intersect_triangle;
		// robustness
		unsigned diff_overlap;
		unsigned diff_precalc_miss;
		unsigned diff_tight_hit;
		unsigned diff_tight_miss;
		unsigned diff_parallel_hit;
		unsigned diff_parallel_miss;
		unsigned diff_clear_hit;
		unsigned diff_clear_miss_tested;
		unsigned diff_clear_miss_not_tested;
		// tools
		RRIntersectStats();
		void Reset();
		void getInfo(char *buf, unsigned len, unsigned level) const;
		static RRIntersectStats* getInstance();
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// tmp
	//
	//////////////////////////////////////////////////////////////////////////////

	extern unsigned RRCOLLIDER_API diagnosticLevel;


	//////////////////////////////////////////////////////////////////////////////
	//
	//! Provide your license number before any other work with library.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRCOLLIDER_API RRLicense
	{
	public:
		static void RRCOLLIDER_API registerLicense(char* licenseOwner, char* licenseNumber);
	};

} // namespace

#endif
