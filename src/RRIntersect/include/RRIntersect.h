#ifndef RRINTERSECT_RRINTERSECT_H
#define RRINTERSECT_RRINTERSECT_H

//////////////////////////////////////////////////////////////////////////////
// RRIntersect - library for fast "ray x mesh" intersections
//
// - thread safe, you can calculate any number of intersections at the same time
// - builds helper-structures and stores them in cache on disk
//////////////////////////////////////////////////////////////////////////////

namespace rrIntersect
{
	#ifdef _MSC_VER
	#pragma comment(lib,"RRIntersect.lib")
	#endif

	#define USE_BSP
	//#define USE_KD

	typedef unsigned TRIANGLE_HANDLE;
	typedef float    RRreal;


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRObjectImporter - abstract class for importing your data into RRObject.
	//
	// Derive to import YOUR geometry.
	// Derive from RRSceneObjectImporter if you want to calculate also radiosity.
	// Data must not change during object lifetime, all results must be constant.

	class RRObjectImporter
	{
	public:
		RRObjectImporter();
		virtual ~RRObjectImporter() {};

		// vertices
		virtual unsigned     getNumVertices() const = 0;
		virtual RRreal*      getVertex(unsigned i) const = 0;

		// triangles
		virtual unsigned     getNumTriangles() const = 0;
		virtual void         getTriangle(unsigned i, unsigned& v0, unsigned& v1, unsigned& v2) const = 0;

		// optional for faster access
		bool                 fastN   :1; // set true if you implement fast getTriangleN -> slower but much lower memory footprint Intersect may be used
		bool                 fastSRL :1;
		bool                 fastSRLN:1;
		struct TriangleN     {RRreal n[4];};
		struct TriangleSRL   {RRreal s[3],r[3],l[3];};
		struct TriangleSRLN  {RRreal s[3],r[3],l[3],n[4];};
		virtual void         getTriangleN(unsigned i, TriangleN* t) const;
		virtual void         getTriangleSRL(unsigned i, TriangleSRL* t) const;
		virtual void         getTriangleSRLN(unsigned i, TriangleSRLN* t) const;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRRay - ray to intersect with object.

	struct RRRay
	{
		RRreal          rayOrigin[3];
		RRreal          rayDir[3];
		RRreal          hitDistanceMin,hitDistanceMax;
		RRreal          hitDistance;   // changed even when no hit
		RRreal          hitPoint3d[3]; // returned wrong
		RRreal          hitPoint2d[2];
		TRIANGLE_HANDLE skip;
		TRIANGLE_HANDLE hitTriangle;
		bool            hitOuterSide;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRIntersect - single object able to calculate intersections.

	class RRIntersect
	{
	public:
		virtual bool intersect(RRRay* ray) const = 0;
		virtual ~RRIntersect() {};
	};

	RRIntersect* newIntersect(RRObjectImporter* importer);


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRIntersectStats - statistics for library calls

	class RRIntersectStats
	{
	public:
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
		unsigned intersect_kd;
		unsigned intersect_linear;
		// branches, many times per call
		unsigned intersect_triangleSRLNP;
		unsigned intersect_triangleNP;
		unsigned intersect_triangleP;
		// helper
		void getInfo(char *buf, unsigned len, unsigned level) const;
	};

	extern RRIntersectStats intersectStats;

}

#endif
