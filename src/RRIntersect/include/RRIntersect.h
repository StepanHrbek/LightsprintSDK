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
	#define CONST const

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
		// vertices
		virtual unsigned     getNumVertices() = 0;
		virtual RRreal*      getVertex(unsigned i) = 0;
		// optional for faster acces
		virtual RRreal*      getVertexBuffer(unsigned* stride) {return 0;}

		// triangles
		virtual unsigned     getNumTriangles() = 0;
		virtual void         getTriangle(unsigned i, unsigned& v0, unsigned& v1, unsigned& v2, unsigned& s) = 0;
		// optional for faster access
		RRObjectImporter();
		bool                 fastN   :1; // set true if you implement fast getTriangleN -> slower but much lower memory footprint Intersect may be used
		bool                 fastSRL :1;
		bool                 fastSRLN:1;
		struct TriangleN     {RRreal n[4];};
		struct TriangleSRL   {RRreal s[3],r[3],l[3];};
		struct TriangleSRLN  {RRreal s[3],r[3],l[3],n[4];};
		virtual void         getTriangleN(unsigned i, TriangleN* t);
		virtual void         getTriangleSRL(unsigned i, TriangleSRL* t);
		virtual void         getTriangleSRLN(unsigned i, TriangleSRLN* t);
		// optional for faster access
		virtual unsigned __int16* getTriangleList16()  {return 0;}
		virtual unsigned __int32* getTriangleList32()  {return 0;}
		virtual unsigned __int16* getTriangleStrip16() {return 0;}
		virtual unsigned __int32* getTriangleStrip32() {return 0;}
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
		//virtual ~Intersect() = 0;
		virtual bool intersect(RRRay* ray) CONST = 0;
	};

	RRIntersect* newIntersect(RRObjectImporter* importer);


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRIntersectStats - statistics for library calls

	class RRIntersectStats
	{
	public:
		RRIntersectStats();
		unsigned intersects;
		unsigned hits;
		unsigned bspSRLNP;
		unsigned bspNP;
		unsigned kd;
		unsigned linear;
		unsigned triangleSRLNP;
		unsigned triangleNP;
		unsigned triangleP;
	};

	extern RRIntersectStats intersectStats;
}

#endif
