#ifndef COLLIDER_RRCOLLIDER_H
#define COLLIDER_RRCOLLIDER_H

//////////////////////////////////////////////////////////////////////////////
// RRCollider - library for fast "ray x mesh" intersections
// version 2005.10.14
// http://dee.cz/rr
//
// - thread safe, you can calculate any number of intersections at the same time
// - you can select technique in range from maximal speed to zero memory allocated
// - up to 2^32 vertices and 2^30 triangles in mesh
// - builds helper-structures and stores them in cache on disk
//
// Copyright (C) Stepan Hrbek 1999-2005, Daniel Sykora 1999-2004
// This work is protected by copyright law, 
// using it without written permission from Stepan Hrbek is forbidden.
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// General rules
//
// If not otherwise specified, all inputs must be finite numbers.
// With Inf or NaN, result is undefined.
//
// Parameters that need to be destructed are always destructed by caller.
//////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
	#ifdef RRCOLLIDER_EXPORT
		// build dll
		#define RRCOLLIDER_API __declspec(dllexport)
	#else
	#ifdef RRCOLLIDER_IMPORT
		// use dll
		#define RRCOLLIDER_API __declspec(dllimport)
		#pragma comment(lib,"RRCollider.lib")
	#else
		// use static library
		#define RRCOLLIDER_API
		#ifdef NDEBUG
			#pragma comment(lib,"RRCollider_s.lib")
		#else
			#pragma comment(lib,"RRCollider_sd.lib")
		#endif
	#endif
	#endif
#else
	// use static library
	#define RRCOLLIDER_API
#endif

#include <new>

namespace rrCollider
{

	typedef float RRReal;


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRMeshImporter - abstract class for importing your mesh data into RR.
	//
	// Derive to import YOUR triangle mesh.
	// All results from RRMeshImporter must be constant in time.

	class RRCOLLIDER_API RRMeshImporter
	{
	public:
		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		virtual ~RRMeshImporter() {}

		// vertices
		struct Vertex        {RRReal m[3]; RRReal&operator[](int i){return m[i];} const RRReal&operator[](int i)const{return m[i];}};
		virtual unsigned     getNumVertices() const = 0;
		virtual void         getVertex(unsigned v, Vertex& out) const = 0;

		// triangles
		struct Triangle      {unsigned m[3]; unsigned&operator[](int i){return m[i];} const unsigned&operator[](int i)const{return m[i];}};
		virtual unsigned     getNumTriangles() const = 0;
		virtual void         getTriangle(unsigned t, Triangle& out) const = 0;

		// optional for faster access
		struct TriangleBody  {Vertex vertex0,side1,side2;};
		virtual void         getTriangleBody(unsigned t, TriangleBody& out) const;

		// optional for advanced importers
		//  post import number is always plain unsigned, 0..num-1
		//  pre import number is implementation defined
		//  all numbers in interface are post import, except for following preImportXxx:
		virtual unsigned     getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const {return postImportVertex;}
		virtual unsigned     getPostImportVertex(unsigned preImportVertex, unsigned preImportTriangle) const {return preImportVertex;}
		virtual unsigned     getPreImportTriangle(unsigned postImportTriangle) const {return postImportTriangle;}
		virtual unsigned     getPostImportTriangle(unsigned preImportTriangle) const {return preImportTriangle;}


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		// instance factory
		enum Format
		{
			UINT8   = 0, // uint8_t
			UINT16  = 1, // uint16_t
			UINT32  = 2, // uint32_t
			FLOAT16 = 3, // half
			FLOAT32 = 4, // float
			FLOAT64 = 5, // double
		};
		enum Flags
		{
			TRI_LIST            = 0,      // interpret data as triangle list
			TRI_STRIP           = (1<<0), // interpret data as triangle strip
			OPTIMIZED_VERTICES  = (1<<1), // remove identical and unused vertices
			OPTIMIZED_TRIANGLES = (1<<2), // remove degenerated triangles
		};
		static RRMeshImporter* create(unsigned flags, Format vertexFormat, void* vertexBuffer, unsigned vertexCount, unsigned vertexStride);
		static RRMeshImporter* createIndexed(unsigned flags, Format vertexFormat, void* vertexBuffer, unsigned vertexCount, unsigned vertexStride, Format indexFormat, void* indexBuffer, unsigned indexCount, float vertexStitchMaxDistance = 0);
		RRMeshImporter*        createCopy();
		static RRMeshImporter* createMultiMesh(RRMeshImporter* const* meshes, unsigned numMeshes);
		bool                   save(char* filename);
		static RRMeshImporter* load(char* filename);
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRMeshSurfaceImporter - defines non-trivial behaviour of mesh surfaces
	//
	// Derive to define behaviour of YOUR surfaces.
	// You need it only when you want to
	// - intersect mesh that contains both singlesided and twosided faces
	// - iterate over multiple intersections with mesh
	//
	// It's intentionally not part of RRMeshImporter, so you can easily combine
	// different surface behaviours with one geometry.

	class RRCOLLIDER_API RRMeshSurfaceImporter
	{
	public:
		virtual ~RRMeshSurfaceImporter() {}

		// acceptHit is called at each intersection with mesh
		// return false to continue to next intersection, true to end
		// for IT_BSP techniques, intersections are reported in order from the nearest one
		// for IT_LINEAR technique, intersections go unsorted
		virtual bool         acceptHit(const class RRRay* ray) = 0;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRAligned
	//
	// On some platforms, some structures need to be specially aligned in memory.
	// This helper base class helps to align them.

	struct RRCOLLIDER_API RRAligned
	{
		void* operator new(std::size_t n);
		void* operator new[](std::size_t n);
		void operator delete(void* p, std::size_t n);
		void operator delete[](void* p, std::size_t n);
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRRay - ray to intersect with object.
	//
	// Contains all inputs and outputs for RRCollider::intersect()
	// All fields of at least 3 floats are aligned.

	class RRCOLLIDER_API RRRay : public RRAligned
	{
	public:
		// create ray
		static RRRay* create(); // all is zeroed, all FILL flags on
		static RRRay* create(unsigned n); // creates array
		// inputs (never modified by collider)
		enum Flags
		{ 
			FILL_DISTANCE   =(1<<0), // which outputs to fill
			FILL_POINT3D    =(1<<1), // note: some outputs might be filled even when not requested by flag.
			FILL_POINT2D    =(1<<2),
			FILL_PLANE      =(1<<3),
			FILL_TRIANGLE   =(1<<4),
			FILL_SIDE       =(1<<5),
			TEST_SINGLESIDED=(1<<6), // detect collision only against outer side. default is to test both sides
		};
		RRReal          rayOrigin[4];   // i, (-Inf,Inf), ray origin. never modify last component, must stay 1
		RRReal          rayDirInv[4];   // i, <-Inf,Inf>, 1/ray direction. direction must be normalized
		RRReal          rayLengthMin;   // i, <0,Inf), test intersection in range <min,max>
		RRReal          rayLengthMax;   // i, <0,Inf), test intersection in range <min,max>
		unsigned        rayFlags;       // i, flags that specify the action
		RRMeshSurfaceImporter* surfaceImporter; // i, optional surface importer for user-defined surface behaviours
		// outputs (valid after positive test, undefined otherwise)
		RRReal          hitDistance;    // o, hit distance in object space
		unsigned        hitTriangle;    // o, triangle (postImport) that was hit
		RRReal          hitPoint2d[2];  // o, hit coordinate in triangle space (vertex[0]=0,0 vertex[1]=1,0 vertex[2]=0,1)
		RRReal          hitPlane[4];    // o, plane of hitTriangle in object space, [0..2] is normal
		RRReal          hitPoint3d[3];  // o, hit coordinate in object space
		bool            hitOuterSide;   // o, true = object was hit from the outer (common) side
		RRReal          hitPadding[8];  // o, undefined, never modify
	private:
		RRRay(); // intentionally private so one can't accidentally create unaligned instance
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRCollider - single object able to calculate ray x trimesh intersections.
	//
	// Mesh is defined by importer passed to create().
	// All results from importer must be constant in time, 
	// otherwise collision results are undefined.

	class RRCOLLIDER_API RRCollider
	{
	public:
		// create
		enum IntersectTechnique
		{
			IT_LINEAR,          // speed   1%, size  0
			IT_BSP_COMPACT,     // speed 100%, size  5 bytes per triangle
			IT_BSP_FAST,        // speed 175%, size 31
			IT_BSP_FASTEST,     // speed 200%, size 58
			IT_VERIFICATION,    // test using all techniques and compare
		};
		static RRCollider*   create(RRMeshImporter* importer, IntersectTechnique intersectTechnique, const char* cacheLocation=NULL, void* buildParams=0);

		// calculate intersections
		// (When intersection is detected, ray outputs are filled and true returned.
		// When no intersection is detected, ray outputs are undefined and false returned.)
		virtual bool         intersect(RRRay* ray) const = 0;

		// helpers
		virtual RRMeshImporter*    getImporter() const = 0; // identical to importer from create()
		virtual IntersectTechnique getTechnique() const = 0; // my differ from technique requested in create()
		virtual unsigned           getMemoryOccupied() const = 0; // total amount of system memory occupied by collider
		virtual ~RRCollider() {};
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRIntersectStats - statistics for library calls

	class RRCOLLIDER_API RRIntersectStats
	{
	public:
		static RRIntersectStats* getInstance();
		RRIntersectStats();
		// data
		unsigned loaded_triangles;
		unsigned invalid_triangles;
		// calls
		unsigned intersects;
		unsigned hits;
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
		// helper
		void getInfo(char *buf, unsigned len, unsigned level) const;
	};

} // namespace

#endif
